/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Peter Elliott <pelliott@serenityos.org>
 * Copyright (c) 2023, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ConnectionFromClient.h"
#include "Service.h"
#include "UnitManagement.h"
#include <AK/Assertions.h>
#include <AK/ByteBuffer.h>
#include <AK/Debug.h>
#include <AK/String.h>
#include <Kernel/API/DeviceEvent.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/ConfigFile.h>
#include <LibCore/DirIterator.h>
#include <LibCore/Event.h>
#include <LibCore/EventLoop.h>
#include <LibCore/File.h>
#include <LibCore/Process.h>
#include <LibCore/System.h>
#include <LibIPC/MultiServer.h>
#include <LibMain/Main.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void sigchld_handler(int) { UnitManagement::the().sigchld_handler(); }

// NOTE: This handler ensures that the destructor of UnitManagement is called.
static void sigterm_handler(int)
{
    exit(0);
}

namespace SystemServer {

static ErrorOr<void> prepare_bare_minimum_filesystem_mounts()
{
    TRY(Core::System::remount("/"sv, MS_NODEV | MS_NOSUID | MS_RDONLY));
    // FIXME: Find a better way to all of this stuff, without hardcoding all of this!
    TRY(Core::System::mount(-1, "/proc"sv, "proc"sv, MS_NOSUID));
    TRY(Core::System::mount(-1, "/sys"sv, "sys"sv, 0));
    TRY(Core::System::mount(-1, "/dev"sv, "ram"sv, MS_NOSUID | MS_NOEXEC | MS_NOREGULAR));
    TRY(Core::System::mount(-1, "/tmp"sv, "ram"sv, MS_NOSUID | MS_NODEV));
    // NOTE: Set /tmp to have a sticky bit with 0777 permissions.
    TRY(Core::System::chmod("/tmp"sv, 01777));
    return {};
}

static ErrorOr<void> prepare_bare_minimum_devtmpfs_directory_structure()
{
    TRY(Core::System::mkdir("/dev/audio"sv, 0755));
    TRY(Core::System::mkdir("/dev/input"sv, 0755));
    TRY(Core::System::mkdir("/dev/input/keyboard"sv, 0755));
    TRY(Core::System::mkdir("/dev/input/mouse"sv, 0755));
    TRY(Core::System::symlink("/proc/self/fd/0"sv, "/dev/stdin"sv));
    TRY(Core::System::symlink("/proc/self/fd/1"sv, "/dev/stdout"sv));
    TRY(Core::System::symlink("/proc/self/fd/2"sv, "/dev/stderr"sv));
    TRY(Core::System::mkdir("/dev/gpu"sv, 0755));
    TRY(Core::System::mkdir("/dev/pts"sv, 0755));
    TRY(Core::System::mount(-1, "/dev/pts"sv, "devpts"sv, 0));
    TRY(Core::System::mkdir("/dev/loop"sv, 0755));
    TRY(Core::System::mount(-1, "/dev/loop"sv, "devloop"sv, 0));

    mode_t old_mask = umask(0);
    TRY(Core::System::create_char_device("/dev/devctl"sv, 0660, 2, 10));
    TRY(Core::System::create_char_device("/dev/zero"sv, 0666, 1, 5));
    TRY(Core::System::create_char_device("/dev/mem"sv, 0600, 1, 1));
    TRY(Core::System::create_char_device("/dev/null"sv, 0666, 1, 3));
    TRY(Core::System::create_char_device("/dev/full"sv, 0666, 1, 7));
    TRY(Core::System::create_char_device("/dev/random"sv, 0666, 1, 8));
    TRY(Core::System::create_char_device("/dev/input/mice"sv, 0666, 12, 0));
    TRY(Core::System::create_char_device("/dev/console"sv, 0666, 5, 1));
    TRY(Core::System::create_char_device("/dev/ptmx"sv, 0666, 5, 2));
    TRY(Core::System::create_char_device("/dev/tty"sv, 0666, 5, 0));
    TRY(Core::System::create_char_device("/dev/fuse"sv, 0666, 10, 229));
#ifdef ENABLE_KERNEL_COVERAGE_COLLECTION
    TRY(Core::System::create_block_device("/dev/kcov"sv, 0666, 30, 0));
#endif
    umask(old_mask);
    TRY(Core::System::symlink("/dev/random"sv, "/dev/urandom"sv));
    TRY(Core::System::chmod("/dev/urandom"sv, 0666));
    return {};
}

static ErrorOr<void> spawn_device_mapper_process()
{
    TRY(Core::Process::spawn("/bin/DeviceMapper"sv, ReadonlySpan<StringView> {}, {}, Core::Process::KeepAsChild::No));
    return {};
}

static ErrorOr<void> prepare_synthetic_filesystems()
{
    TRY(prepare_bare_minimum_filesystem_mounts());
    TRY(prepare_bare_minimum_devtmpfs_directory_structure());
    TRY(spawn_device_mapper_process());
    return {};
}

static ErrorOr<void> mount_all_filesystems()
{
    dbgln("Spawning mount -a to mount all filesystems.");
    pid_t pid = TRY(Core::System::fork());

    if (pid == 0)
        TRY(Core::System::exec("/bin/mount"sv, Vector { "mount"sv, "-a"sv }, Core::System::SearchInPath::No));

    wait(nullptr);
    return {};
}

static ErrorOr<void> create_tmp_coredump_directory()
{
    dbgln("Creating /tmp/coredump directory");
    auto old_umask = umask(0);
    // FIXME: the coredump directory should be made read-only once CrashDaemon is no longer responsible for compressing coredumps
    TRY(Core::System::mkdir("/tmp/coredump"sv, 0777));
    umask(old_umask);
    return {};
}

static ErrorOr<void> set_default_coredump_directory()
{
    dbgln("Setting /tmp/coredump as the coredump directory");
    auto sysfs_coredump_directory_variable_fd = TRY(Core::System::open("/sys/kernel/conf/coredump_directory"sv, O_RDWR));
    ScopeGuard close_on_exit([&] {
        close(sysfs_coredump_directory_variable_fd);
    });
    auto tmp_coredump_directory_path = "/tmp/coredump"sv;
    auto nwritten = TRY(Core::System::write(sysfs_coredump_directory_variable_fd, tmp_coredump_directory_path.bytes()));
    VERIFY(static_cast<size_t>(nwritten) == tmp_coredump_directory_path.length());
    return {};
}

static ErrorOr<void> create_tmp_semaphore_directory()
{
    dbgln("Creating /tmp/semaphore directory");
    auto old_umask = umask(0);
    TRY(Core::System::mkdir("/tmp/semaphore"sv, 0777));
    umask(old_umask);
    return {};
}

static ErrorOr<void> reopen_base_file_descriptors()
{
    // Note: We open the /dev/null device and set file descriptors 0, 1, 2 to it
    // because otherwise these file descriptors won't have a custody, making
    // the ProcFS file descriptor links (at /proc/PID/fd/{0,1,2}) to have an
    // absolute path of "device:1,3" instead of something like "/dev/null".
    // This affects also every other process that inherits the file descriptors
    // from SystemServer, so it is important for other things (also for ProcFS
    // tests that are running in CI mode).
    int stdin_new_fd = TRY(Core::System::open("/dev/null"sv, O_NONBLOCK));
    TRY(Core::System::dup2(stdin_new_fd, 0));
    TRY(Core::System::dup2(stdin_new_fd, 1));
    TRY(Core::System::dup2(stdin_new_fd, 2));
    return {};
}

};

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    bool user = false;
    Core::ArgsParser args_parser;
    args_parser.add_option(user, "Run in user-mode", "user", 'u');
    args_parser.parse(arguments);

    UnitManagement::the().set_is_user_mode(user);

    if (!user) {
        TRY(SystemServer::mount_all_filesystems());
        TRY(SystemServer::prepare_synthetic_filesystems());
        TRY(SystemServer::reopen_base_file_descriptors());
    }

    TRY(Core::System::pledge("stdio proc exec tty accept unix rpath wpath cpath chown fattr id sigaction"));

    if (!user) {
        TRY(SystemServer::create_tmp_coredump_directory());
        TRY(SystemServer::set_default_coredump_directory());
        TRY(SystemServer::create_tmp_semaphore_directory());
    }

    TRY(UnitManagement::the().determine_system_mode());
    UnitManagement::the().wait_for_gpu_if_needed();

    Core::EventLoop event_loop;

    auto const socket_path = user ? "/tmp/session/%sid/portal/systemserver"sv : "/tmp/portal/systemserver"sv;
    auto server = TRY(ControlServer::try_create(socket_path));

    event_loop.register_signal(SIGCHLD, sigchld_handler);
    event_loop.register_signal(SIGTERM, sigterm_handler);

    auto config = user ? TRY(Core::ConfigFile::open_for_app("SystemServer")) : TRY(Core::ConfigFile::open_for_system("SystemServer"));
    TRY(UnitManagement::the().load_config(config));

    TRY(UnitManagement::the().activate_system_mode_target());

    return event_loop.exec();
}
