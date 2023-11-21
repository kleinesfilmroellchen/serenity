/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <SystemServer/ConnectionFromClient.h>
#include <SystemServer/UnitManagement.h>

HashMap<int, NonnullRefPtr<ConnectionFromClient>> s_clients;

ConnectionFromClient::ConnectionFromClient(NonnullOwnPtr<Core::LocalSocket> socket, int client_id)
    : IPC::ConnectionFromClient<SystemControlClientEndpoint, SystemControlServerEndpoint>(*this, move(socket), client_id)
{
    s_clients.set(client_id, *this);
}

void ConnectionFromClient::die()
{
    s_clients.remove(client_id());
}

Messages::SystemControlServer::ActivateTargetResponse ConnectionFromClient::activate_target(String const& target_name)
{
    auto& unit_management = UnitManagement::the();
    auto maybe_target = unit_management.find_target_for(target_name.bytes_as_string_view());
    if (!maybe_target.has_value())
        return { SystemServer::UnitError::UnitNotFound };

    auto result = unit_management.activate_target(maybe_target.value());
    if (result.is_error()) {
        dbgln("Error during client activation of {}: {}", target_name, result.error());
        return { SystemServer::UnitError::OSError };
    }
    return { {} };
}

Messages::SystemControlServer::ListUnitsResponse ConnectionFromClient::list_units()
{
    auto& unit_management = UnitManagement::the();
    Vector<String> unit_names;
    unit_management.for_each_unit([&](auto const& unit) {
        // Don't OOM crash the SystemServer due to a client request; returning an incomplete list is safer in this case.
        auto name = String::from_deprecated_string(unit.name());
        if (name.is_error())
            return IterationDecision::Break;
        auto result = unit_names.try_append(name.release_value());
        if (result.is_error())
            return IterationDecision::Break;
        return IterationDecision::Continue;
    });
    return { unit_names };
}

Messages::SystemControlServer::QueryUnitInfoResponse ConnectionFromClient::query_unit_info(String const& unit_name)
{
    auto& unit_management = UnitManagement::the();
    Optional<Unit const&> maybe_unit;
    unit_management.for_each_unit([&](auto const& unit) {
        if (unit.name() == unit_name) {
            maybe_unit = unit;
            return IterationDecision::Break;
        }
        return IterationDecision::Continue;
    });
    if (!maybe_unit.has_value())
        return { SystemServer::UnitError::UnitNotFound };

    auto const& unit = maybe_unit.value();

    if (is<Service>(unit)) {
        auto const& service = static_cast<Service const&>(unit);

        auto state = SystemServer::UnitState::Inactive;
        // FIXME: A dead service due to too many restarts counts as not activated.
        //        This is a problem in the service state management that should be fixed there.
        if (service.has_been_activated()) {
            if (service.is_dead())
                state = SystemServer::UnitState::ActiveDead;
            else if (service.is_lazy() && service.pid() < 0)
                state = SystemServer::UnitState::ActiveLazy;
            else
                state = SystemServer::UnitState::ActiveRunning;
        }

        auto name = String::from_deprecated_string(service.name());
        if (name.is_error())
            return { SystemServer::UnitError::OSError };
        return { SystemServer::ServiceInfo {
            .name = name.release_value(),
            .executable_path = service.executable_path(),
            .state = state,
            .pid = service.pid(),
        } };
    } else if (is<Target>(unit)) {
        auto const& target = static_cast<Target const&>(unit);

        SystemServer::UnitState state;
        switch (target.state()) {
        case Target::State::Inactive:
            state = SystemServer::UnitState::Inactive;
            break;
        case Target::State::StartInProgress:
        case Target::State::Reached:
            state = SystemServer::UnitState::ActiveRunning;
            break;
        }

        auto name = String::from_deprecated_string(target.name());
        if (name.is_error())
            return { SystemServer::UnitError::OSError };
        return { SystemServer::TargetInfo {
            .name = name.release_value(),
            .state = state,
        } };
    } else {
        VERIFY_NOT_REACHED();
    }
}
