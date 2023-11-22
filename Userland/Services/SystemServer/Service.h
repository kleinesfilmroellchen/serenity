/*
 * Copyright (c) 2019-2020, Sergey Bugaev <bugaevc@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteString.h>
#include <AK/RefPtr.h>
#include <AK/String.h>
#include <LibCore/Account.h>
#include <LibCore/ElapsedTimer.h>
#include <LibCore/EventReceiver.h>
#include <LibCore/Notifier.h>

/// SocketDescriptor describes the details of a single socket that was
/// requested by a service.
struct SocketDescriptor {
    /// The path of the socket.
    ByteString path;
    /// File descriptor of the socket. -1 if the socket hasn't been opened.
    int fd { -1 };
    /// File permissions of the socket.
    mode_t permissions;
};

// Unit is a generalization of services, targets, cronjobs etc.
// We borrow this name from systemd, but it doesn't *quite* mean the same thing for SystemServer.
class Unit : public Core::EventReceiver {
    C_OBJECT_ABSTRACT(Unit)

public:
    virtual ~Unit() = default;
    virtual bool has_been_activated() const = 0;

    static ErrorOr<NonnullRefPtr<Unit>> try_create(Core::ConfigFile const& config, StringView name);
};

class Service final : public Unit {
    C_OBJECT_ABSTRACT(Service)

public:
    enum class Mode {
        Normal,
        // We should only spawn this service once somebody connects to the socket.
        Lazy,
        // Several instances of this service can run at once.
        MultiInstance,
    };

    virtual ~Service();

    bool is_required_for_target(StringView target_name) const;
    ErrorOr<void> activate();
    virtual bool has_been_activated() const override;
    // Note: This is a `status` as in POSIX's wait syscall, not an exit-code.
    ErrorOr<void> did_exit(int status);

    ErrorOr<void> setup_sockets();

    // May be -1 if this service has no associated PID.
    int pid() const { return m_pid; }
    // Whether the service is either not activated or has exited.
    // No process has to be running for the service right now for it to be alive, but one will run in the near future.
    bool is_dead() const;
    Mode mode() const { return m_mode; }
    StringView executable_path() const { return m_executable_path; }

    // Configuration APIs only used by Unit.
    Service(Badge<Unit>, StringView name, ByteString executable_path, ByteString extra_arguments, Mode mode, int priority, bool keep_alive, ByteString environment, Vector<ByteString> targets);
    void set_stdio_file_path(Badge<Unit>, ByteString stdio_file_path) { m_stdio_file_path = move(stdio_file_path); }
    void set_user(Badge<Unit>, ByteString user);
    void set_sockets(Badge<Unit>, Vector<SocketDescriptor> sockets) { m_sockets = move(sockets); }
    void set_working_directory(Badge<Unit>, ByteString working_directory) { m_working_directory = working_directory; }

private:
    ErrorOr<void> spawn(int socket_fd = -1);

    ErrorOr<void> determine_account(int fd);

    ErrorOr<void> change_privileges();

    // Path to the executable. By default this is /bin/{m_name}.
    ByteString m_executable_path;
    // Extra arguments, starting from argv[1], to pass when exec'ing.
    ByteString m_extra_arguments;
    // File path to open as stdio fds.
    Optional<ByteString> m_stdio_file_path {};
    int m_priority;
    // Whether we should re-launch it if it exits.
    bool m_keep_alive;
    // The service's mode, which determines how it is started.
    Mode m_mode;
    // The name of the user we should run this service as.
    Optional<ByteString> m_user {};
    // The working directory in which to spawn the service.
    Optional<ByteString> m_working_directory {};
    // Targets in which to run this service.
    Vector<ByteString> m_targets;
    // Environment variables to pass to the service.
    ByteString m_environment;
    // Socket descriptors for this service.
    Vector<SocketDescriptor> m_sockets;

    // The resolved user account to run this service as.
    Optional<Core::Account> m_account;
    bool m_must_login { false };

    // For single-instance services, PID of the running instance of this service.
    pid_t m_pid { -1 };
    RefPtr<Core::Notifier> m_socket_notifier;

    // Timer since we last spawned the service.
    Core::ElapsedTimer m_run_timer;
    // How many times we have tried to restart this service, only counting those
    // times where it has exited unsuccessfully and too quickly.
    int m_restart_attempts { 0 };

    ErrorOr<void> setup_socket(SocketDescriptor&);
    void setup_notifier();
    ErrorOr<void> handle_socket_connection();
};

// A target is a defined system state in terms of service and kernel state controlled by SystemServer.
// For instance, all system boot modes, and power states like shutdown are targets.
class Target final : public Unit {
    C_OBJECT_ABSTRACT(Target)

public:
    enum class State {
        // Target has not been started.
        Inactive,
        // Target's start is currently in progress.
        StartInProgress,
        // Target has been reached.
        Reached,
    };

    enum class SpecialTargetKind {
        Shutdown,
    };
    static ErrorOr<NonnullRefPtr<Target>> create_special_target(SpecialTargetKind);

    virtual ~Target() = default;
    virtual bool has_been_activated() const override;

    bool is_system_mode() const { return m_is_system_mode; }
    bool is_special() const { return m_is_special; }
    ReadonlySpan<ByteString> depends_on() const { return m_depends_on; }
    State state() const { return m_state; }

    void set_start_in_progress()
    {
        VERIFY(m_state == State::Inactive);
        m_state = State::StartInProgress;
    }
    void set_reached()
    {
        VERIFY(m_state == State::StartInProgress);
        m_state = State::Reached;
    }

    Target(Badge<Unit>, StringView name, bool is_system_mode, Vector<ByteString> depends_on)
        : Target(name, is_system_mode, move(depends_on))
    {
    }

private:
    Target(
        StringView name, bool is_system_mode, Vector<ByteString> depends_on, bool is_special = false)
        : m_is_system_mode(is_system_mode)
        , m_is_special(is_special)
        , m_depends_on(move(depends_on))
    {
        set_name(name);
    }

    bool m_is_system_mode;
    bool m_is_special;
    Vector<ByteString> m_depends_on;

    State m_state { State::Inactive };
};
