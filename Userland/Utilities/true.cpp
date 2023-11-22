/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Atomic.h>
#include <AK/Format.h>
#include <LibCore/System.h>
#include <LibMain/Main.h>
#include <unistd.h>

/// ------------------------
/// Child code

static Atomic<bool> child_should_exit_later = false;

static void child_termination_handler(int)
{
    dbgln("Child received SIGTERM! But we'll wait a second before exiting...");
    child_should_exit_later = true;
}

static void child_process()
{
    (void)signal(SIGTERM, child_termination_handler);
    dbgln("Child has finished setup.");

    while (!child_should_exit_later)
        usleep(1'000);

    dbgln("The signal handler told us to exit, so let's do that in a second!");
    usleep(1'000'000);
    exit(0);
}

/// ------------------------
/// Parent code

static Atomic<bool> child_has_exited = false;

static void parent_sigchld_handler(int)
{
    for (;;) {
        auto result = Core::System::waitpid(-1);
        if (result.is_error()) {
            dbgln("Parent error while handling child termination: {}", result.error());
            break;
        }
        auto info = result.value();
        if (info.pid == 0)
            break;

        dbgln("Parent: Child {} died.", info.pid);

        child_has_exited = true;
        break;
    }
}

ErrorOr<int> serenity_main(Main::Arguments)
{

    auto pid = TRY(Core::System::fork());

    if (pid == 0)
        child_process();

    // Only set up sigchld for parent.
    (void)signal(SIGCHLD, parent_sigchld_handler);

    // Wait for child to finish its setup...
    usleep(1'000'000);
    dbgln("Parent will SIGTERM the child now");
    TRY(Core::System::kill(pid, SIGTERM));

    // Wait for sigchld to fire...
    while (!child_has_exited)
        usleep(1'000);

    return 0;
}
