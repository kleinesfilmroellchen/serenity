/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/NonnullOwnPtr.h>
#include <LibCore/Object.h>
#include <LibCore/Stream.h>
#include <LibIPC/ConnectionFromClient.h>
#include <LoggingServer/LoggingClientEndpoint.h>
#include <LoggingServer/LoggingServerEndpoint.h>

namespace LoggingServer {

class ConnectionFromClient final : public IPC::ConnectionFromClient<LoggingClientEndpoint, LoggingServerEndpoint> {
    C_OBJECT(ConnectionFromClient)
public:
    ~ConnectionFromClient() override = default;

    virtual void die() override;

private:
    explicit ConnectionFromClient(NonnullOwnPtr<Core::Stream::LocalSocket>, int client_id);

    virtual Messages::LoggingServer::CreateLoggedApplicationResponse create_logged_application(String const& application_name) override;
};

}
