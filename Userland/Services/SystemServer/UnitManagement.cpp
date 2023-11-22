/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Peter Elliott <pelliott@serenityos.org>
 * Copyright (c) 2023, Liav A. <liavalb@hotmail.co.il>
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "UnitManagement.h"
#include <AK/Debug.h>
#include <LibCore/ConfigFile.h>
#include <LibCore/EventLoop.h>
#include <LibCore/File.h>
#include <LibCore/ProcessStatisticsReader.h>
#include <LibCore/System.h>

static constexpr StringView text_system_mode = "text"sv;
static constexpr StringView selftest_system_mode = "self-test"sv;
static constexpr StringView graphical_system_mode = "graphical"sv;

static Singleton<UnitManagement> s_instance;
UnitManagement& UnitManagement::the() { return s_instance; }

UnitManagement::UnitManagement()
{
    populate_special_targets();
}

void UnitManagement::populate_special_targets()
{
    m_units.append(MUST(Target::create_special_target(Target::SpecialTargetKind::Shutdown)));
}

void UnitManagement::sigchld_handler()
{
    for (;;) {
        int status = 0;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid < 0) {
            perror("waitpid");
            break;
        }
        if (pid == 0)
            break;

        dbgln_if(SYSTEMSERVER_DEBUG, "Reaped child with pid {}, exit status {}", pid, status);

        auto maybe_service = find_service_by_pid(pid);
        if (!maybe_service.has_value()) {
            // This can happen for multi-instance services.
            continue;
        }
        auto service = maybe_service.value();

        m_services_by_pid.remove(service->pid());

        // Don't restart services during shutdown.
        if (m_in_shutdown)
            continue;

        if (auto result = service->did_exit(status); result.is_error())
            dbgln("{}: {}", service->name(), result.release_error());

        // If service restarted a process, add it back to our list.
        if (service->pid() > 0)
            m_services_by_pid.set(service->pid(), service);
    }
}

Optional<NonnullRefPtr<Service>> UnitManagement::find_service_by_pid(pid_t pid)
{
    auto it = m_services_by_pid.find(pid);
    if (it == m_services_by_pid.end())
        return {};
    return (*it).value;
}

Optional<Target&> UnitManagement::find_target_for(StringView target_name)
{
    for (auto& unit : m_units) {
        if (is<Target>(*unit)) {
            auto target = static_ptr_cast<Target>(unit);
            if (target->name() == target_name)
                return *target;
        }
    }
    return {};
}

ErrorOr<void> UnitManagement::handle_special_target(StringView special_target_name)
{
    if (special_target_name == "shutdown") {
        m_in_shutdown = true;

        if (m_server_mode == ServerMode::System)
            dbgln("SerenityOS shutting down...");
        else {
            dbgln("Exiting session...");
            // dbgln("FUCKERY! Gonna die right now!");
            // exit(0);
        }

        for (auto const& service : m_services_by_pid) {
            auto result = Core::System::kill(service.key, SIGTERM);
            if (result.is_error()) {
                dbgln("Failed to kill {}: {}", service.key, result.error());
                // Try harder to be sure.
                (void)Core::System::kill(service.key, SIGKILL);
            }
        }

        // After killing services, let's kill all other processes too.
        // This is particularly important since session SystemServers are not one of our services.
        // FIXME: This feels like a hack. Why doesn't LoginServer close down its SystemServers properly.
        auto process_list = TRY(Core::ProcessStatisticsReader::get_all(false));

        size_t waited_count = 0;
        while (!m_services_by_pid.is_empty() && waited_count < 10) {
            dbgln("Waiting on {} services ({})...", m_services_by_pid.size(), waited_count);
            usleep(1'000'000);
            // Calls signal handlers so that services get cleaned up.
            Core::EventLoop::current().pump(Core::EventLoop::WaitMode::PollForEvents);
            waited_count++;
        }

        // Don't turn off the system in user mode.
        if (m_server_mode == ServerMode::User)
            return {};

        dbgln("All processes have exited. Turning off the computer...");

        auto file = TRY(Core::File::open("/sys/kernel/power_state"sv, Core::File::OpenMode::Write));
        constexpr auto file_contents = "2"sv;
        TRY(file->write_until_depleted(file_contents.bytes()));
        file->close();
        // If we’re not dead yet, let’s just exit.
        exit(0);
    }
    return ENOENT;
}

ErrorOr<void> UnitManagement::activate_target(Target& target)
{
    if (target.is_special())
        return handle_special_target(target.name());

    // By checking and marking a target as started immediately, circular dependencies will not hang SystemServer.
    // However, they may cause weird behavior since one target *needs* to be started first
    // (and this may not be predictable to the user).
    if (target.has_been_activated()) {
        dbgln_if(SYSTEMSERVER_DEBUG, "Skipping target {} as it is already started.", target.name());
        return {};
    }
    target.set_start_in_progress();

    // First start all target dependencies.
    auto dependencies = target.depends_on();
    for (auto const& dependency_target_name : dependencies) {
        auto dependency = find_target_for(dependency_target_name);
        // FIXME: What to do if the target doesn't exist?
        TRY(activate_target(dependency.value()));
    }

    dbgln("Activating services for target {}...", target.name());
    for (auto& unit : m_units) {
        if (is<Service>(*unit)) {
            auto service = static_ptr_cast<Service>(unit);
            if (service->is_required_for_target(target.name()) && !service->has_been_activated()) {
                if (auto result = activate_service(service); result.is_error())
                    dbgln("{}: {}", service->name(), result.release_error());
            }
        }
    }

    target.set_reached();
    dbgln("Reached target {}.", target.name());
    return {};
}

ErrorOr<void> UnitManagement::activate_service(Service& service)
{
    dbgln_if(SYSTEMSERVER_DEBUG, "Activating {}", service.name());
    // Make sure to keep track of the service, no matter how broken its setup was.
    ScopeGuard add_to_services = [service = NonnullRefPtr(service), this]() {
        auto pid = service->pid();
        if (pid > 0)
            m_services_by_pid.set(pid, service);
    };

    TRY(service.setup_sockets());
    TRY(service.activate());

    return {};
}

ErrorOr<void> UnitManagement::load_config(Core::ConfigFile const& config)
{
    for (auto const& name : config.groups()) {
        auto unit = TRY(Unit::try_create(config, name));
        m_units.append(move(unit));
    }
    return {};
}

ErrorOr<void> UnitManagement::activate_system_mode_target()
{
    // This will intentionally crash if the system mode has no associated target.
    auto& target = find_target_for(m_system_mode).value();
    return activate_target(target);
}

ErrorOr<void> UnitManagement::determine_system_mode()
{
    ArmedScopeGuard declare_text_mode_on_failure([&] {
        // Note: Only if the mode is not set to self-test, degrade it to text mode.
        if (m_system_mode != selftest_system_mode)
            m_system_mode = MUST(String::from_utf8(text_system_mode));
    });

    auto file_or_error = Core::File::open("/sys/kernel/system_mode"sv, Core::File::OpenMode::Read);
    if (file_or_error.is_error()) {
        dbgln("Failed to read system_mode: {}", file_or_error.error());
        // Continue and assume "text" mode.
        return {};
    }
    auto const system_mode_buf_or_error = file_or_error.value()->read_until_eof();
    if (system_mode_buf_or_error.is_error()) {
        dbgln("Failed to read system_mode: {}", system_mode_buf_or_error.error());
        // Continue and assume "text" mode.
        return {};
    }

    m_system_mode = TRY(TRY(String::from_utf8(system_mode_buf_or_error.value())).trim_ascii_whitespace());
    declare_text_mode_on_failure.disarm();

    dbgln("Read system_mode: {}", m_system_mode);
    return {};
}

// Waits for up to 10 seconds for a graphics device to be detected.
static ErrorOr<void> wait_for_gpu()
{
    for (int attempt = 0; attempt < 10; attempt++) {
        auto file_state = Core::System::lstat("/dev/gpu/connector0"sv);
        if (!file_state.is_error())
            return {};

        usleep(1'000'000);
    }
    dbgln("WARNING: No device nodes at /dev/gpu/ directory after 10 seconds. This is probably a sign of disabled graphics functionality.");
    dbgln("To cope with this, graphical mode will not be enabled.");
    return ENOENT;
}

void UnitManagement::wait_for_gpu_if_needed()
{
    if (m_server_mode == ServerMode::System && m_system_mode == graphical_system_mode) {
        auto result = wait_for_gpu();
        if (result.is_error())
            m_system_mode = MUST(String::from_utf8(text_system_mode));
    }
}
