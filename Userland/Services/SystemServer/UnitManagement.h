/*
 * Copyright (c) 2018-2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "Service.h"
#include <AK/Singleton.h>

class UnitManagement final {
public:
    static UnitManagement& the();

    Optional<NonnullRefPtr<Service>> find_service_by_pid(pid_t pid);
    Optional<Target&> find_target_for(StringView target_name);

    ErrorOr<void> determine_system_mode();
    void wait_for_gpu_if_needed();

    ErrorOr<void> load_config(Core::ConfigFile const&);
    ErrorOr<void> activate_target(Target& target);
    ErrorOr<void> activate_service(Service& service);
    ErrorOr<void> activate_system_mode_target();

    void sigterm_handler();
    void sigchld_handler();

private:
    Vector<NonnullRefPtr<Unit>> m_units;
    HashMap<pid_t, NonnullRefPtr<Service>> m_services_by_pid;
    String m_system_mode;
};
