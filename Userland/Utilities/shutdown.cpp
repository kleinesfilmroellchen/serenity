/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Liav A. <liavalb@hotmail.co.il>
 * Copyright (c) 2022, Alex Major
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteString.h>
#include <LibCore/EventLoop.h>
#include <LibCore/File.h>
#include <LibMain/Main.h>
#include <SystemServer/ConnectionToServer.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static ErrorOr<void> try_system_server_shutdown()
{
    auto server_connection = TRY(SystemServer::ControlClient::connect_to_init_system_server());
    auto result = server_connection->activate_target("shutdown"_string);
    if (result.has_value())
        return ENOENT;
    return {};
}

ErrorOr<int> serenity_main(Main::Arguments)
{
    Core::EventLoop loop;

    auto result = try_system_server_shutdown();

    if (result.is_error()) {
        outln("warning: can’t shutdown via SystemServer, performing a hard kernel shutdown instead.");

        auto file = TRY(Core::File::open("/sys/kernel/power_state"sv, Core::File::OpenMode::Write));

        ByteString const file_contents = "2";
        TRY(file->write_until_depleted(file_contents.bytes()));
        file->close();
    }

    return 0;
}
