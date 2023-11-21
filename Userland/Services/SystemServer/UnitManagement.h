/*
 * Copyright (c) 2018-2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "Service.h"
#include <AK/IterationDecision.h>
#include <AK/Singleton.h>

class UnitManagement final {
    AK_MAKE_NONCOPYABLE(UnitManagement);
    AK_MAKE_NONMOVABLE(UnitManagement);

public:
    UnitManagement();

    static UnitManagement& the();

    Optional<NonnullRefPtr<Service>> find_service_by_pid(pid_t pid);
    Optional<Target&> find_target_for(StringView target_name);

    ErrorOr<void> determine_system_mode();
    void wait_for_gpu_if_needed();

    ErrorOr<void> load_config(Core::ConfigFile const&);
    void set_is_user_mode(bool is_user_mode) { m_server_mode = is_user_mode ? ServerMode::User : ServerMode::System; }
    ErrorOr<void> activate_target(Target& target);
    ErrorOr<void> activate_service(Service& service);
    ErrorOr<void> activate_system_mode_target();

    void sigterm_handler();
    void sigchld_handler();

    template<typename Callback>
    IterationDecision for_each_unit(Callback function) const
    {
        for (auto const& unit_ptr : m_units) {
            auto const& unit = *unit_ptr;
            auto decision = function(unit);
            if (decision == IterationDecision::Break)
                return IterationDecision::Break;
        }
        return IterationDecision::Continue;
    }

private:
    enum class ServerMode : bool {
        System,
        User,
    };

    void populate_special_targets();
    ErrorOr<void> handle_special_target(StringView special_target_name);

    Vector<NonnullRefPtr<Unit>> m_units;
    HashMap<pid_t, NonnullRefPtr<Service>> m_services_by_pid;
    String m_system_mode;
    ServerMode m_server_mode { ServerMode::System };

    bool m_in_shutdown { false };
};
