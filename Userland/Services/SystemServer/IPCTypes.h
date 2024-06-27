/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Types.h>
#include <LibIPC/Decoder.h>
#include <LibIPC/Encoder.h>

namespace SystemServer {

enum UnitError : u8 {
    UnitNotFound,
    OSError,
};
// NOTE: Due to IPC constraints, we cannot use ErrorOr, since it is not default constructible even if the value type is void.
using MaybeUnitError = Optional<UnitError>;

enum UnitState : u8 {
    // Unit has not been activated.
    Inactive,
    // Unit is currently being activated.
    Activating,
    // Unit has been activated, but no associated program is running since it's a lazy service.
    ActiveLazy,
    // A multi-instance service that has been activated. Any number (including none) of process instances may be running.
    ActiveMultiInstance,
    // Unit has a process running. This does not apply to multi-instance services.
    ActiveRunning,
    // A unit is not currently running, but active and will be restarted as soon as possible.
    Restarting,
    // Unit has exited and will not be restarted, either because it's not configured to be restarted, or because three restarts have been attempted.
    ActiveDead,
    // Unit is being deactivated.
    Stopping,
};

struct ServiceInfo {
    String name;
    ByteString executable_path;
    UnitState state;
    int pid;
};

struct TargetInfo {
    String name;
    UnitState state;
};

using UnitInfo = Variant<ServiceInfo, TargetInfo>;
// NOTE: Due to various IPC constraints we cannot use ErrorOr<UnitInfo, UnitError>.
//       For the same reason, the Variant must be able to hold Empty as it is not default constructible otherwise.
using ErrorOrUnitInfo = Variant<UnitInfo, UnitError, Empty>;

}

namespace IPC {

template<>
ErrorOr<void> encode(Encoder&, SystemServer::ServiceInfo const&);

template<>
ErrorOr<SystemServer::ServiceInfo> decode(Decoder&);

template<>
ErrorOr<void> encode(Encoder&, SystemServer::TargetInfo const&);

template<>
ErrorOr<SystemServer::TargetInfo> decode(Decoder&);

}
