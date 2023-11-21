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
#include <LibCore/File.h>
#include <LibCore/System.h>

static constexpr StringView text_system_mode = "text"sv;
static constexpr StringView selftest_system_mode = "self-test"sv;
static constexpr StringView graphical_system_mode = "graphical"sv;

static Singleton<UnitManagement> s_instance;
UnitManagement& UnitManagement::the() { return s_instance; }

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
        if (auto result = service->did_exit(status); result.is_error())
            dbgln("{}: {}", service->name(), result.release_error());
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

ErrorOr<void> UnitManagement::activate_target(Target& target)
{
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
    auto add_to_services = [service = NonnullRefPtr(service), this]() {
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
    if (m_system_mode == graphical_system_mode) {
        auto result = wait_for_gpu();
        if (result.is_error())
            m_system_mode = MUST(String::from_utf8(text_system_mode));
    }
}
