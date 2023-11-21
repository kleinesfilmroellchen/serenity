/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/LexicalPath.h>
#include <LibCore/Directory.h>
#include <LibCore/EventReceiver.h>
#include <LibCore/LocalServer.h>
#include <LibCore/SessionManagement.h>
#include <LibIPC/ConnectionFromClient.h>
#include <SystemServer/SystemControlClientEndpoint.h>
#include <SystemServer/SystemControlServerEndpoint.h>

class ConnectionFromClient final
    : public IPC::ConnectionFromClient<SystemControlClientEndpoint, SystemControlServerEndpoint> {
    C_OBJECT(ConnectionFromClient);

public:
    virtual ~ConnectionFromClient() override = default;

    virtual void die() override;

    virtual Messages::SystemControlServer::ActivateTargetResponse activate_target(String const& target_name) override;
    virtual Messages::SystemControlServer::ListUnitsResponse list_units() override;
    virtual Messages::SystemControlServer::QueryUnitInfoResponse query_unit_info(String const& unit_name) override;

private:
    ConnectionFromClient(NonnullOwnPtr<Core::LocalSocket> socket, int client_id);
};

class ControlServer {
public:
    static ErrorOr<NonnullOwnPtr<ControlServer>> try_create(StringView socket_path)
    {
        auto server = TRY(Core::LocalServer::try_create());

        // Note: Since we're the SystemServer, we need to set up the socket and its directories manually.
        auto path = TRY(Core::SessionManagement::parse_path_with_sid(socket_path));
        TRY(Core::Directory::create(LexicalPath { path }.parent(), Core::Directory::CreateDirectories::Yes));
        auto successful = server->listen(path);
        if (!successful)
            return EIO;
        TRY(Core::System::chmod(path, 0666));
        return adopt_nonnull_own_or_enomem(new (nothrow) ControlServer(move(server)));
    }

private:
    explicit ControlServer(NonnullRefPtr<Core::LocalServer> server)
        : m_server(move(server))
    {
        m_server->on_accept = [&](auto client_socket) {
            auto client_id = ++m_next_client_id;
            // Client will add itself to a global client list and stay alive.
            (void)IPC::new_client_connection<ConnectionFromClient>(move(client_socket), client_id);
        };
    }

    int m_next_client_id { 0 };
    RefPtr<Core::LocalServer> m_server;
};
