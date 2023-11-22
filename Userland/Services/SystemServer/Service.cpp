/*
 * Copyright (c) 2019-2020, Sergey Bugaev <bugaevc@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Service.h"
#include <AK/Debug.h>
#include <AK/HashMap.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/String.h>
#include <AK/StringBuilder.h>
#include <LibCore/ConfigFile.h>
#include <LibCore/Directory.h>
#include <LibCore/Environment.h>
#include <LibCore/SessionManagement.h>
#include <LibCore/SocketAddress.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

ErrorOr<void> Service::setup_socket(SocketDescriptor& socket)
{
    VERIFY(socket.fd == -1);

    // Note: The purpose of this syscall is to remove potential left-over of previous portal.
    // The return value is discarded as sockets are not always there, and unlinking a non-existent path is considered as a failure.
    (void)Core::System::unlink(socket.path);

    TRY(Core::Directory::create(LexicalPath(socket.path).parent(), Core::Directory::CreateDirectories::Yes));

    // Note: we use SOCK_CLOEXEC here to make sure we don't leak every socket to
    // all the clients. We'll make the one we do need to pass down !CLOEXEC later
    // after forking off the process.
    int const socket_fd = TRY(Core::System::socket(AF_LOCAL, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
    socket.fd = socket_fd;

    if (m_account.has_value()) {
        auto& account = m_account.value();
        TRY(Core::System::fchown(socket_fd, account.uid(), account.gid()));
    }

    TRY(Core::System::fchmod(socket_fd, socket.permissions));

    auto socket_address = Core::SocketAddress::local(socket.path);
    auto un_optional = socket_address.to_sockaddr_un();
    if (!un_optional.has_value()) {
        dbgln("Socket name {} is too long. BUG! This should have failed earlier!", socket.path);
        VERIFY_NOT_REACHED();
    }
    auto un = un_optional.value();

    TRY(Core::System::bind(socket_fd, (sockaddr const*)&un, sizeof(un)));
    TRY(Core::System::listen(socket_fd, 16));
    return {};
}

ErrorOr<void> Service::setup_sockets()
{
    for (SocketDescriptor& socket : m_sockets)
        TRY(setup_socket(socket));
    return {};
}

void Service::setup_notifier()
{
    VERIFY(m_mode != Mode::Normal);
    VERIFY(m_sockets.size() == 1);
    VERIFY(!m_socket_notifier);

    m_socket_notifier = Core::Notifier::construct(m_sockets[0].fd, Core::Notifier::Type::Read, this);
    m_socket_notifier->on_activation = [this] {
        if (auto result = handle_socket_connection(); result.is_error())
            dbgln("{}", result.release_error());
    };
}

ErrorOr<void> Service::handle_socket_connection()
{
    VERIFY(m_sockets.size() == 1);
    dbgln_if(SERVICE_DEBUG, "Ready to read on behalf of {}", name());

    int socket_fd = m_sockets[0].fd;

    if (m_mode == Mode::MultiInstance) {
        auto const accepted_fd = TRY(Core::System::accept(socket_fd, nullptr, nullptr));

        TRY(determine_account(accepted_fd));
        TRY(spawn(accepted_fd));
        TRY(Core::System::close(accepted_fd));
    } else {
        remove_child(*m_socket_notifier);
        m_socket_notifier = nullptr;
        TRY(spawn(socket_fd));
    }
    return {};
}

ErrorOr<void> Service::activate()
{
    VERIFY(!has_been_activated());

    if (m_mode != Mode::Normal)
        setup_notifier();
    else
        TRY(spawn());
    return {};
}

bool Service::has_been_activated() const
{
    return m_pid > 0 || m_socket_notifier != nullptr;
}

bool Service::is_dead() const
{
    if (m_keep_alive)
        return has_been_activated() && m_restart_attempts < 2;

    return !has_been_activated();
}

ErrorOr<void> Service::change_privileges()
{
    // NOTE: Dropping privileges makes sense when SystemServer is running
    // for a root session.
    // This could happen when we need to spawn a Service to serve a client with non-user UID/GID.
    // However, in case the user explicitly specified a username via the User= option, then we must
    // try to login as at that user, so we can't ignore the failure when it was requested to change
    // privileges.
    if (auto current_uid = getuid(); m_account.has_value() && m_account.value().uid() != current_uid) {
        if (current_uid != 0 && !m_must_login)
            return {};
        auto& account = m_account.value();
        if (auto error_or_void = account.login(); error_or_void.is_error()) {
            dbgln("Failed to drop privileges (tried to change to GID={}, UID={}), due to {}\n", account.gid(), account.uid(), error_or_void.error());
            exit(1);
        }
        TRY(Core::Environment::set("HOME"sv, account.home_directory(), Core::Environment::Overwrite::Yes));
    }
    return {};
}

ErrorOr<void> Service::spawn(int socket_fd)
{
    if (!FileSystem::exists(m_executable_path)) {
        dbgln("{}: binary \"{}\" does not exist, skipping service.", name(), m_executable_path);
        return Error::from_errno(ENOENT);
    }

    dbgln_if(SERVICE_DEBUG, "Spawning {}", name());

    m_run_timer.start();
    pid_t pid = TRY(Core::System::fork());

    if (pid == 0) {
        // We are the child.
        if (m_working_directory.has_value())
            TRY(Core::System::chdir(*m_working_directory));

        struct sched_param p;
        p.sched_priority = m_priority;
        int rc = sched_setparam(0, &p);
        if (rc < 0) {
            perror("sched_setparam");
            VERIFY_NOT_REACHED();
        }

        if (m_stdio_file_path.has_value()) {
            close(STDIN_FILENO);
            auto const fd = TRY(Core::System::open(*m_stdio_file_path, O_RDWR, 0));
            VERIFY(fd == 0);

            dup2(STDIN_FILENO, STDOUT_FILENO);
            dup2(STDIN_FILENO, STDERR_FILENO);

            if (isatty(STDIN_FILENO)) {
                ioctl(STDIN_FILENO, TIOCSCTTY);
            }
        } else {
            if (isatty(STDIN_FILENO)) {
                ioctl(STDIN_FILENO, TIOCNOTTY);
            }
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            auto const fd = TRY(Core::System::open("/dev/null"sv, O_RDWR));
            VERIFY(fd == STDIN_FILENO);
            dup2(STDIN_FILENO, STDOUT_FILENO);
            dup2(STDIN_FILENO, STDERR_FILENO);
        }

        StringBuilder socket_takeover_builder;

        if (socket_fd >= 0) {
            // We were spawned by socket activation. We currently only support
            // single sockets for socket activation, so make sure that's the case.
            VERIFY(m_sockets.size() == 1);

            int fd = dup(socket_fd);
            TRY(socket_takeover_builder.try_appendff("{}:{}", m_sockets[0].path, fd));
        } else {
            // We were spawned as a regular process, so dup every socket for this
            // service and let the service know via SOCKET_TAKEOVER.
            for (unsigned i = 0; i < m_sockets.size(); i++) {
                SocketDescriptor& socket = m_sockets.at(i);

                int new_fd = dup(socket.fd);
                if (i != 0)
                    TRY(socket_takeover_builder.try_append(';'));
                TRY(socket_takeover_builder.try_appendff("{}:{}", socket.path, new_fd));
            }
        }

        if (!m_sockets.is_empty()) {
            // The new descriptor is !CLOEXEC here.
            TRY(Core::Environment::set("SOCKET_TAKEOVER"sv, socket_takeover_builder.string_view(), Core::Environment::Overwrite::Yes));
        }

        TRY(change_privileges());

        TRY(m_environment.view().for_each_split_view(' ', SplitBehavior::Nothing, [&](auto env) {
            return Core::Environment::put(env);
        }));

        Vector<StringView, 10> arguments;
        TRY(arguments.try_append(m_executable_path));
        TRY(m_extra_arguments.view().for_each_split_view(' ', SplitBehavior::Nothing, [&](auto arg) {
            return arguments.try_append(arg);
        }));

        TRY(Core::System::exec(m_executable_path, arguments, Core::System::SearchInPath::No));
    } else if (m_mode != Mode::MultiInstance) {
        // We are the parent.
        m_pid = pid;
    }

    return {};
}

ErrorOr<void> Service::did_exit(int status)
{
    using namespace AK::TimeLiterals;

    VERIFY(m_pid > 0);
    VERIFY(m_mode != Mode::MultiInstance);

    if (WIFEXITED(status))
        dbgln("Service {} has exited with exit code {}", name(), WEXITSTATUS(status));
    if (WIFSIGNALED(status))
        dbgln("Service {} terminated due to signal {}", name(), WTERMSIG(status));

    m_pid = -1;

    if (!m_keep_alive)
        return {};

    auto run_time = m_run_timer.elapsed_time();
    bool exited_successfully = WIFEXITED(status) && WEXITSTATUS(status) == 0;

    if (!exited_successfully && run_time < 1_sec) {
        switch (m_restart_attempts) {
        case 0:
            dbgln("Trying again");
            break;
        case 1:
            dbgln("Third time's the charm?");
            break;
        default:
            dbgln("Giving up on {}. Good luck!", name());
            return {};
        }
        m_restart_attempts++;
    }

    TRY(activate());
    return {};
}

Service::Service(Badge<Unit>, StringView name, ByteString executable_path, ByteString extra_arguments, Mode mode, int priority, bool keep_alive, ByteString environment, Vector<ByteString> targets)
    : m_executable_path(move(executable_path))
    , m_extra_arguments(move(extra_arguments))
    , m_priority(priority)
    , m_keep_alive(keep_alive)
    , m_mode(mode)
    , m_targets(move(targets))
    , m_environment(move(environment))
{
    set_name(name);
}

void Service::set_user(Badge<Unit>, ByteString user)
{
    m_user = move(user);
    auto result = Core::Account::from_name(*m_user, Core::Account::Read::PasswdOnly);
    if (result.is_error()) {
        warnln("Failed to resolve user {}: {}", m_user, result.error());
    } else {
        m_must_login = true;
        m_account = result.value();
    }
}

ErrorOr<NonnullRefPtr<Unit>> Unit::try_create(Core::ConfigFile const& config, StringView name)
{
    if (!config.has_group(name))
        return Error::from_string_view("Unit's config file group not found"sv);

    auto type = config.read_entry(name, "Type", "Service");

    if (type == "Target") {
        auto is_system_mode = config.read_bool_entry(name, "IsSystemMode");
        auto depends_on = config.read_entry(name, "DependsOn").split(',');
        return adopt_nonnull_ref_or_enomem(new (nothrow) Target({}, name, is_system_mode, depends_on));
    } else if (type == "Service") {
        auto executable_path = config.read_entry(name, "Executable", ByteString::formatted("/bin/{}", name));
        auto extra_arguments = config.read_entry(name, "Arguments");
        auto keep_alive = config.read_bool_entry(name, "KeepAlive");
        auto environment = config.read_entry(name, "Environment");
        auto targets = config.read_entry(name, "Targets", "graphical").split(',');

        auto mode_text = config.read_entry(name, "ServiceType", "Normal");
        Service::Mode mode;
        if (mode_text == "Normal"sv)
            mode = Service::Mode::Normal;
        else if (mode_text == "Lazy"sv)
            mode = Service::Mode::Lazy;
        else if (mode_text == "MultiInstance"sv)
            mode = Service::Mode::MultiInstance;
        else
            return Error::from_string_view("Mode is not Normal, Lazy or MultiInstance"sv);

        auto priority_string = config.read_entry_optional(name, "Priority");
        int priority;
        if (priority_string == "low")
            priority = 10;
        else if (priority_string == "normal" || !priority_string.has_value())
            priority = 30;
        else if (priority_string == "high")
            priority = 50;
        else
            return Error::from_string_view("Priority must be either low, normal, or high"sv);

        auto service = TRY(adopt_nonnull_ref_or_enomem(new (nothrow) Service({}, name, executable_path, extra_arguments, mode, priority, keep_alive, environment, targets)));

        auto stdio_file_path = config.read_entry_optional(name, "StdIO");
        if (stdio_file_path.has_value())
            service->set_stdio_file_path({}, stdio_file_path.release_value());

        auto user = config.read_entry_optional(name, "User");
        if (user.has_value())
            service->set_user({}, user.release_value());

        auto working_directory = config.read_entry_optional(name, "WorkingDirectory");
        if (working_directory.has_value())
            service->set_working_directory({}, working_directory.release_value());

        ByteString socket_entry = config.read_entry(name, "Socket");
        ByteString socket_permissions_entry = config.read_entry(name, "SocketPermissions", "0600");

        Vector<SocketDescriptor> sockets;
        if (!socket_entry.is_empty()) {
            Vector<ByteString> socket_paths = socket_entry.split(',');
            Vector<ByteString> socket_perms = socket_permissions_entry.split(',');
            sockets.ensure_capacity(socket_paths.size());

            // Need i here to iterate along with all other vectors.
            for (unsigned i = 0; i < socket_paths.size(); i++) {
                auto const path = Core::SessionManagement::parse_path_with_sid(socket_paths.at(i));
                if (path.is_error()) {
                    // FIXME: better error handling for this case.
                    TODO();
                }

                // Socket path (plus NUL) must fit into the structs sent to the Kernel.
                VERIFY(path.value().length() < UNIX_PATH_MAX);

                // This is done so that the last permission repeats for every other
                // socket. So you can define a single permission, and have it
                // be applied for every socket.
                mode_t permissions = strtol(socket_perms.at(min(socket_perms.size() - 1, (long unsigned)i)).characters(), nullptr, 8) & 0777;

                sockets.empend(path.value(), -1, permissions);
            }
        }

        if (mode != Service::Mode::Normal && sockets.size() != 1)
            return Error::from_string_view("Lazy and MultiInstance requires Socket, but only one."sv);

        if (mode == Service::Mode::MultiInstance && keep_alive)
            return Error::from_string_view("MultiInstance doesn't work with KeepAlive."sv);

        service->set_sockets({}, move(sockets));

        return service;

    } else {
        return Error::from_string_view("Unsupported unit type"sv);
    }
}

bool Service::is_required_for_target(StringView target_name) const
{
    return m_targets.contains_slow(target_name);
}

ErrorOr<void> Service::determine_account(int fd)
{
    struct ucred creds = {};
    socklen_t creds_size = sizeof(creds);
    TRY(Core::System::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &creds, &creds_size));

    m_account = TRY(Core::Account::from_uid(creds.uid, Core::Account::Read::PasswdOnly));
    return {};
}

Service::~Service()
{
    for (auto& socket : m_sockets) {
        if (auto rc = remove(socket.path.characters()); rc != 0)
            dbgln("{}", Error::from_errno(errno));
    }
}

bool Target::has_been_activated() const
{
    return m_state != State::Inactive;
}

ErrorOr<NonnullRefPtr<Target>> Target::create_special_target(SpecialTargetKind kind)
{
    switch (kind) {
    case SpecialTargetKind::Shutdown:
        return adopt_nonnull_ref_or_enomem(new (nothrow) Target("shutdown"sv, false, {}, true));
    }
    VERIFY_NOT_REACHED();
}
