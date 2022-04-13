/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibIPC/ConnectionToServer.h>
#include <LoggingServer/LoggingClientEndpoint.h>
#include <LoggingServer/LoggingServerEndpoint.h>

class ConnectionToLoggingServer final
    : public IPC::ConnectionToServer<LoggingClientEndpoint, LoggingServerEndpoint>
    , public LoggingClientEndpoint {
    IPC_CLIENT_CONNECTION(ConnectionToLoggingServer, "/tmp/portal/logging")

public:
    virtual ~ConnectionToLoggingServer() override = default;

private:
    ConnectionToLoggingServer(NonnullOwnPtr<Core::Stream::LocalSocket> socket)
        : IPC::ConnectionToServer<LoggingClientEndpoint, LoggingServerEndpoint>(*this, move(socket))
    {
        dbgln("connection, other pid {}", m_socket->peer_pid());
    }
};
