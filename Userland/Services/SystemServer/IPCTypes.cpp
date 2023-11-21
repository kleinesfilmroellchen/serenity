/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <SystemServer/IPCTypes.h>

template<>
ErrorOr<void> IPC::encode(Encoder& encoder, SystemServer::ServiceInfo const& service_info)
{
    TRY(encoder.encode(service_info.name));
    TRY(encoder.encode(service_info.executable_path));
    TRY(encoder.encode(service_info.state));
    TRY(encoder.encode(service_info.pid));
    return {};
}

template<>
ErrorOr<SystemServer::ServiceInfo> IPC::decode(Decoder& decoder)
{
    SystemServer::ServiceInfo service_info;
    service_info.name = TRY(decoder.decode<String>());
    service_info.executable_path = TRY(decoder.decode<DeprecatedString>());
    service_info.state = TRY(decoder.decode<SystemServer::UnitState>());
    service_info.pid = TRY(decoder.decode<int>());
    return service_info;
}

template<>
ErrorOr<void> IPC::encode(Encoder& encoder, SystemServer::TargetInfo const& service_info)
{
    TRY(encoder.encode(service_info.name));
    TRY(encoder.encode(service_info.state));
    return {};
}

template<>
ErrorOr<SystemServer::TargetInfo> IPC::decode(Decoder& decoder)
{
    SystemServer::TargetInfo service_info;
    service_info.name = TRY(decoder.decode<String>());
    service_info.state = TRY(decoder.decode<SystemServer::UnitState>());
    return service_info;
}
