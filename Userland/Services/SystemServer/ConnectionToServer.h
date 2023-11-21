/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibIPC/ConnectionToServer.h>
#include <SystemServer/SystemControlClientEndpoint.h>
#include <SystemServer/SystemControlServerEndpoint.h>

namespace SystemServer {

// ControlClient is used to connect to two different SystemServer instances:
// the init process SystemServer (at /tmp/portal/systemserver) and the user session SystemServer (at /tmp/session/%sid/portal/systemserver).
class ControlClient
    : public IPC::ConnectionToServer<SystemControlClientEndpoint, SystemControlServerEndpoint>
    , public SystemControlClientEndpoint {
public:
    static ErrorOr<NonnullRefPtr<ControlClient>> connect_to_init_system_server()
    {
        auto socket = TRY(Core::LocalSocket::connect("/tmp/portal/systemserver"sv));
        TRY(socket->set_blocking(true));

        return adopt_nonnull_ref_or_enomem(new (nothrow) ControlClient(move(socket)));
    }

    static ErrorOr<NonnullRefPtr<ControlClient>> connect_to_session_system_server()
    {
        auto parsed_socket_path = TRY(Core::SessionManagement::parse_path_with_sid("/tmp/session/%sid/portal/systemserver"sv));
        auto socket = TRY(Core::LocalSocket::connect(parsed_socket_path));
        TRY(socket->set_blocking(true));

        return adopt_nonnull_ref_or_enomem(new (nothrow) ControlClient(move(socket)));
    }

    virtual ~ControlClient() = default;

private:
    explicit ControlClient(NonnullOwnPtr<Core::LocalSocket> socket)
        : IPC::ConnectionToServer<SystemControlClientEndpoint, SystemControlServerEndpoint>(*this, move(socket))
    {
    }
};

}
