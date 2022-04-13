/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/EventLoop.h>
#include <LibCore/System.h>
#include <LibIPC/MultiServer.h>
#include <LibMain/Main.h>
#include <LoggingServer/ConnectionFromClient.h>

ErrorOr<int> serenity_main(Main::Arguments)
{
    dbgln("LoggingServer starting...");
    Core::EventLoop loop;
    auto server = TRY(IPC::MultiServer<LoggingServer::ConnectionFromClient>::try_create());

    TRY(Core::System::pledge("stdio accept wpath rpath cpath unix"));

    return loop.exec();
}
