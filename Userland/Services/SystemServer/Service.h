/*
 * Copyright (c) 2019-2020, Sergey Bugaev <bugaevc@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "IPCTypes.h"
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

enum class ServiceMode : u8 {
    Normal,
    // We should only spawn this service once somebody connects to the socket.
    Lazy,
    // Several instances of this service can run at once.
    MultiInstance,
};

// Unit is a generalization of services, targets, cronjobs etc.
// We borrow this name from systemd, but it doesn't *quite* mean the same thing for SystemServer.
class Unit : public Core::EventReceiver {
    C_OBJECT_ABSTRACT(Unit)

public:
    // Unit state machine.
    struct State {
        SystemServer::UnitState state { SystemServer::UnitState::Inactive };

        constexpr bool has_been_activated() const { return state != SystemServer::UnitState::Inactive && state != SystemServer::UnitState::Stopping; }
        constexpr bool is_active() const { return state == SystemServer::UnitState::ActiveLazy || state == SystemServer::UnitState::ActiveMultiInstance || state == SystemServer::UnitState::ActiveRunning; }
        constexpr bool operator==(SystemServer::UnitState const& other) const { return state == other; }
        constexpr bool operator==(State const& other) const { return state == other.state; }

        constexpr void activate()
        {
            VERIFY(!has_been_activated());
            state = SystemServer::UnitState::Activating;
        }
        constexpr void set_active(ServiceMode mode)
        {
            switch (mode) {
            case ServiceMode::Normal:
                return set_running();
            case ServiceMode::Lazy:
                return set_running_lazy();
            case ServiceMode::MultiInstance:
                return set_running_multi_instance();
            }
        }
        constexpr void set_running()
        {
            VERIFY(state == SystemServer::UnitState::Activating || state == SystemServer::UnitState::Restarting || state == SystemServer::UnitState::ActiveLazy);
            state = SystemServer::UnitState::ActiveRunning;
        }
        constexpr void set_running_lazy()
        {
            VERIFY(state == SystemServer::UnitState::Activating || state == SystemServer::UnitState::Restarting);
            state = SystemServer::UnitState::ActiveLazy;
        }
        constexpr void set_running_multi_instance()
        {
            VERIFY(state == SystemServer::UnitState::Activating);
            state = SystemServer::UnitState::ActiveMultiInstance;
        }
        constexpr void set_dead()
        {
            // It's fine to "kill" a service that's already dead. set_dead is used as a scope guard in various places.
            VERIFY(has_been_activated() || state == SystemServer::UnitState::ActiveDead);
            state = SystemServer::UnitState::ActiveDead;
        }
        constexpr void set_restarting()
        {
            VERIFY(is_active());
            state = SystemServer::UnitState::Restarting;
        }
    };

    virtual ~Unit() = default;

    bool has_been_activated() const { return m_state.has_been_activated(); }

    State state() const { return m_state; }

    static ErrorOr<NonnullRefPtr<Unit>> try_create(Core::ConfigFile const& config, StringView name);

protected:
    State m_state;
};

class Service final : public Unit {
    C_OBJECT_ABSTRACT(Service)

public:
    virtual ~Service();

    bool is_required_for_target(StringView target_name) const;
    ErrorOr<void> activate();
    // Note: This is a `status` as in POSIX's wait syscall, not an exit-code.
    ErrorOr<void> did_exit(int status);

    ErrorOr<void> setup_sockets();

    // May be -1 if this service has no associated PID.
    int pid() const { return m_pid; }
    ServiceMode mode() const { return m_mode; }
    StringView executable_path() const { return m_executable_path; }

    // Configuration APIs only used by Unit.
    Service(Badge<Unit>, StringView name, ByteString executable_path, ByteString extra_arguments, ServiceMode mode, int priority, bool keep_alive, ByteString environment, Vector<ByteString> targets);
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
    ServiceMode m_mode;
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
    enum class SpecialTargetKind {
        Shutdown,
    };
    static ErrorOr<NonnullRefPtr<Target>> create_special_target(SpecialTargetKind);

    virtual ~Target() = default;

    bool is_system_mode() const { return m_is_system_mode; }
    bool is_special() const { return m_is_special; }
    ReadonlySpan<ByteString> depends_on() const { return m_depends_on; }

    void set_start_in_progress() { m_state.activate(); }
    void set_reached() { m_state.set_running(); }

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
};
