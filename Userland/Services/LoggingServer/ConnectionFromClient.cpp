/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ConnectionFromClient.h"

namespace LoggingServer {

ConnectionFromClient::ConnectionFromClient(NonnullOwnPtr<Core::Stream::LocalSocket> socket, int client_id)
    : IPC::ConnectionFromClient<LoggingClientEndpoint, LoggingServerEndpoint>(*this, move(socket), client_id)
{
    dbgln("created client!! other pid {}", m_socket->peer_pid());
}

void ConnectionFromClient::die()
{
    dbgln("ded");
}

Messages::LoggingServer::CreateLoggedApplicationResponse ConnectionFromClient::create_logged_application(String const& application_name)
{
    dbgln("Creating logged application {}", application_name);
    return Messages::LoggingServer::CreateLoggedApplicationResponse { 0, 0 };
}

}
