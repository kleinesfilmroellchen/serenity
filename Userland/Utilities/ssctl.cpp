/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/DeprecatedString.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/EventLoop.h>
#include <LibCore/File.h>
#include <LibMain/Main.h>
#include <SystemServer/ConnectionToServer.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static constexpr StringView state_to_string(SystemServer::UnitState state)
{
    switch (state) {
    case SystemServer::Inactive:
        return "Inactive"sv;
    case SystemServer::ActiveLazy:
        return "ActiveLazy"sv;
    case SystemServer::ActiveDead:
        return "ActiveDead"sv;
    case SystemServer::ActiveRunning:
        return "ActiveRunning"sv;
    }
    VERIFY_NOT_REACHED();
}

static ErrorOr<void> run_list_command(ReadonlySpan<String>)
{
#define table_format "{:<25} {:<7} {:<15} {:<20}"
    outln(table_format, "name", "type", "state", "executable");
    outln(table_format, "----", "----", "----", "----");
    auto print_info = [](auto const& result) {
        result.visit(
            [&](SystemServer::UnitError const& error) {
                dbgln("Error: {}", to_underlying(error));
            },
            [&](auto const& unit_info) {
                unit_info.visit(
                    [&](SystemServer::ServiceInfo const& service_info) {
                        outln(table_format, service_info.name, "service", state_to_string(service_info.state), service_info.executable_path);
                    },
                    [&](SystemServer::TargetInfo const& target_info) {
                        outln(table_format, target_info.name, "target", state_to_string(target_info.state), "");
                    });
            },
            [&](Empty const&) {});
    };

    auto init_server = TRY(SystemServer::ControlClient::connect_to_init_system_server());
    auto units = init_server->list_units();
    for (auto const& unit : units) {
        auto result = init_server->query_unit_info(unit);
        print_info(result);
    }

    outln(table_format, "----", "----", "----", "----");

    auto session_server = TRY(SystemServer::ControlClient::connect_to_session_system_server());
    units = session_server->list_units();
    for (auto const& unit : units) {
        auto result = session_server->query_unit_info(unit);
        print_info(result);
    }

    return {};
}

static ErrorOr<void> run_activate_command(ReadonlySpan<String> args)
{
    if (args.size() != 1)
        return Error::from_string_view("One target to activate must be given"sv);
    auto target_name = args.first();

    // Try activating a target on the session server first.
    auto session_server = TRY(SystemServer::ControlClient::connect_to_session_system_server());
    auto result = session_server->activate_target(target_name);
    if (result.has_value()) {
        // Try again, this time with the init server.
        auto init_server = TRY(SystemServer::ControlClient::connect_to_init_system_server());
        result = init_server->activate_target(target_name);

        if (result.has_value()) {
            if (result.value() == SystemServer::UnitError::UnitNotFound)
                return Error::from_string_view("Target with this name does not exist"sv);
            if (result.has_value())
                return Error::from_string_view("Unknown error while activating target"sv);
        }
    }
    return {};
}

enum class Command {
    List,
    Activate,
};

static Optional<Command> parse_command(StringView command_name)
{
    if (command_name == "list"sv) {
        return Command::List;
    } else if (command_name == "activate"sv) {
        return Command::Activate;
    }
    return {};
}

ErrorOr<int> serenity_main(Main::Arguments args)
{
    TRY(Core::System::pledge("stdio rpath cpath unix"));
    TRY(Core::System::unveil("/tmp/portal/systemserver", "rw"));
    TRY(Core::System::unveil(TRY(Core::SessionManagement::parse_path_with_sid("/tmp/session/%sid/portal/systemserver"sv)), "rw"sv));
    TRY(Core::System::unveil(nullptr, nullptr));

    Core::ArgsParser parser;
    parser.set_general_help("Interface with and send commands to SystemServer");
    parser.set_stop_on_first_non_option(false);
    Optional<Command> command;
    Vector<String> sub_args;

    parser.add_positional_argument({
        .help_string = "Command to execute.\n\tAvailable commands: list, activate",
        .name = "command",
        .min_values = 1,
        .max_values = 1,
        .accept_value = [&](auto const& string_command) {
            command = parse_command(string_command);
            return command.has_value();
        },
    });
    parser.add_positional_argument(sub_args, "Arguments to subcommands", "subcommand args");

    parser.parse(args);
    VERIFY(command.has_value());

    Core::EventLoop loop;

    switch (command.value()) {
    case Command::List:
        TRY(run_list_command(sub_args));
        break;
    case Command::Activate:
        TRY(run_activate_command(sub_args));
    }

    return 0;
}
