/*
 * Copyright (c) 2021, Peter Elliott <pelliott@serenityos.org>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/Account.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/SessionManagement.h>
#include <LibCore/System.h>
#include <LibGUI/Application.h>
#include <LibGUI/MessageBox.h>
#include <LibMain/Main.h>
#include <LibThreading/MutexProtected.h>
#include <Services/LoginServer/LoginWindow.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// Shared state between signal handlers and the GUI event loop.
static Threading::MutexProtected<RefPtr<LoginServer::LoginWindow>> g_window;
static Atomic<pid_t> g_system_server_pid { 0 };
static Atomic<bool> g_should_exit { false };

static void exit_on_termination(int)
{
    dbgln("THE USER SIGNAL!");
    g_should_exit.store(true, AK::memory_order_acquire);
    (void)Core::System::kill(g_system_server_pid, SIGTERM);
    dbgln("Gonna exit soon...");
}

static void child_process(Core::Account const& account)
{
    pid_t rc = setsid();
    if (rc == -1) {
        dbgln("failed to setsid: {}", strerror(errno));
        exit(1);
    }
    auto result = Core::SessionManagement::create_session_temporary_directory_if_needed(account.uid(), account.gid());
    if (result.is_error()) {
        dbgln("Failed to create temporary directory for session: {}", result.error());
        exit(1);
    }

    if (auto const result = account.login(); result.is_error()) {
        dbgln("failed to switch users: {}", result.error());
        exit(1);
    }

    setenv("HOME", account.home_directory().characters(), true);
    dbgln("login with sid={}", rc);

    execlp("/bin/SystemServer", "SystemServer", "--user", nullptr);
    dbgln("failed to exec SystemServer --user: {}", strerror(errno));
    exit(127);
}

static void sigchld_handler(int)
{
    for (;;) {
        auto result = Core::System::waitpid(-1);
        if (result.is_error()) {
            dbgln("Error while handling child termination: {}", result.error());
            break;
        }
        auto info = result.value();
        if (info.pid == 0)
            break;

        dbgln("Child {} died.", info.pid);

        if (WIFEXITED(info.status) && WEXITSTATUS(info.status) != 0)
            dbgln("SystemServer exited with non-zero status: {}", WEXITSTATUS(info.status));

        g_system_server_pid = 0;

        g_window.with_locked([](auto const& window) {
            if (window != nullptr)
                window->show();
        });
    }

    if (g_should_exit.load(AK::memory_order_release)) {
        dbgln("Exiting now.");
        Core::EventLoop::current().quit(0);
    }
}

static void login(Core::Account const& account)
{
    pid_t pid = fork();
    if (pid == 0)
        child_process(account);

    g_system_server_pid = pid;
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    auto app = TRY(GUI::Application::create(arguments));

    TRY(Core::System::pledge("stdio recvfd sendfd cpath chown rpath exec proc id sigaction"));
    TRY(Core::System::unveil("/home", "r"));
    TRY(Core::System::unveil("/tmp", "c"));
    TRY(Core::System::unveil("/etc/passwd", "r"));
    TRY(Core::System::unveil("/etc/shadow", "r"));
    TRY(Core::System::unveil("/etc/group", "r"));
    TRY(Core::System::unveil("/bin/SystemServer", "x"));
    TRY(Core::System::unveil("/res", "r"));
    TRY(Core::System::unveil(nullptr, nullptr));

    auto window = LoginServer::LoginWindow::construct();
    g_window.with_locked([&](auto& global_window) { global_window = window; });
    window->on_submit = [&]() {
        auto username = window->username();
        auto password = Core::SecretString::take_ownership(window->password().to_byte_buffer());

        window->set_password(""sv);

        auto fail_message = "Can't log in: invalid username or password."sv;

        auto account = Core::Account::from_name(username);
        if (account.is_error()) {
            window->set_fail_message(fail_message);
            dbgln("failed graphical login for user {}: {}", username, account.error());
            return;
        }

        if (!account.value().authenticate(password)) {
            window->set_fail_message(fail_message);
            dbgln("failed graphical login for user {}: invalid password", username);
            return;
        }

        window->set_username(""sv);
        window->hide();

        login(account.value());
    };

    StringView auto_login;

    Core::ArgsParser args_parser;
    args_parser.add_option(auto_login, "automatically log in with no prompt", "auto-login", 'a', "username");
    args_parser.parse(arguments);

    if (auto_login.is_empty()) {
        window->show();
    } else {
        auto account = Core::Account::from_name(auto_login);
        if (account.is_error()) {
            dbgln("failed auto-login for user {}: {}", auto_login, account.error());
            return 1;
        }

        login(account.value());
    }

    // Note: This ensures that any session SystemServer gets closed properly.
    Core::EventLoop::register_signal(SIGINFO, exit_on_termination);
    Core::EventLoop::register_signal(SIGTERM, exit_on_termination);
    Core::EventLoop::register_signal(SIGCHLD, sigchld_handler);

    return app->exec();
}
