/*
 * Copyright (c) 2023, the SerenityOS developers.
 * Copyright (c) 2023, David Ganz <david.g.ganz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "EventManager.h"
#include <AK/JsonParser.h>
#include <AK/QuickSort.h>
#include <LibConfig/Client.h>
#include <LibCore/DateTime.h>
#include <LibDateTime/Format.h>
#include <LibDateTime/ISOCalendar.h>
#include <LibDateTime/ZonedDateTime.h>
#include <LibFileSystemAccessClient/Client.h>
#include <LibTimeZone/TimeZone.h>
#include <LibTimeZone/TimeZoneData.h>

namespace Calendar {

EventManager::EventManager()
{
}

OwnPtr<EventManager> EventManager::create()
{
    return adopt_own(*new EventManager());
}

void EventManager::add_event(Event event)
{
    m_events.append(move(event));
    quick_sort(m_events, [&](auto& a, auto& b) { return a.start.offset_to_utc_epoch() < b.start.offset_to_utc_epoch(); });
    m_dirty = true;
    on_events_change();
}

void EventManager::set_events(Vector<Event> events)
{
    m_events = move(events);
    quick_sort(m_events, [&](auto& a, auto& b) { return a.start.offset_to_utc_epoch() < b.start.offset_to_utc_epoch(); });
    m_dirty = true;
    on_events_change();
}

ErrorOr<void> EventManager::save(FileSystemAccessClient::File& file)
{
    set_filename(file.filename());

    auto stream = file.release_stream();
    auto json = TRY(serialize_events()).to_byte_string();
    TRY(stream->write_some(json.bytes()));
    stream->close();

    m_dirty = false;

    return {};
}

ErrorOr<JsonArray> EventManager::serialize_events()
{
    JsonArray result;
    for (auto const& event : m_events) {
        JsonObject object;
        object.set("start", JsonValue(TRY(event.start.format(DateTime::ISO8601_SHORT_FORMAT))));
        object.set("end", JsonValue(TRY(event.end.format(DateTime::ISO8601_SHORT_FORMAT))));
        object.set("summary", JsonValue(event.summary));
        TRY(result.append(object));
    }

    return result;
}

ErrorOr<Vector<Event>> EventManager::deserialize_events(JsonArray const& json)
{
    Vector<Event> result;

    auto local_timezone = TRY(DateTime::ZonedDateTime::now().format("{0z}"sv));

    for (auto const& value : json.values()) {
        auto const& object = value.as_object();
        if (!object.has("summary"sv) || !object.has("start"sv) || !object.has("end"sv))
            continue;

        auto summary = TRY(String::from_byte_string(object.get("summary"sv).release_value().as_string()));
        // FIXME: Implement and use a DateTime::LocalDateTime parser.
        //        Get rid of the timezone hack which is currently needed to prevent UTC-localtime adjustments.
        auto start = Core::DateTime::parse("%Y-%m-%dT%H:%M:%S%z"sv, object.get("start"sv).release_value().as_string());
        if (!start.has_value())
            continue;

        auto end = Core::DateTime::parse("%Y-%m-%dT%H:%M:%S%z"sv, object.get("end"sv).release_value().as_string());
        if (!end.has_value())
            continue;

        Event event = {
            .summary = summary,
            // HACK: the DateTime parser removes timezone adjustments to be in UTC,
            //       so we tell ZonedDateTime about this and then readjust into the local time zone.
            .start = DateTime::LocalDateTime { start.release_value() }.with_time_zone(TimeZone::TimeZone::UTC).in_time_zone(TimeZone::time_zone_from_string(TimeZone::current_time_zone()).value()),
            .end = DateTime::LocalDateTime { end.release_value() }.with_time_zone(TimeZone::TimeZone::UTC).in_time_zone(TimeZone::time_zone_from_string(TimeZone::current_time_zone()).value()),
        };
        result.append(event);
    }

    return result;
}

Optional<DateTime::ZonedDateTime> EventManager::format_icalendar_vevent_datetime(String const& parameter)
{
    auto date_time_bytes = parameter.bytes();

    // https://datatracker.ietf.org/doc/html/rfc5545#section-3.3.5
    // 3.3.5.  Date-Time
    //     date-time  = date "T" time ;As specified in the DATE and TIME
    //                                ;value definitions
    if (date_time_bytes.size() < 15 || date_time_bytes[8] != 'T')
        return {};

    auto formatted_string = String::formatted("{:c}-{:c}-{:c}T{:c}:{:c}:{:c}",
        date_time_bytes.slice(0, 4), date_time_bytes.slice(4, 2),
        date_time_bytes.slice(6, 2), date_time_bytes.slice(9, 2),
        date_time_bytes.slice(11, 2), date_time_bytes.slice(13, 2));
    if (formatted_string.is_error())
        return {};
    auto parsed_datetime = Core::DateTime::parse("%Y-%m-%dT%H:%M:%S"sv, formatted_string.value());
    if (!parsed_datetime.has_value())
        return {};
    DateTime::LocalDateTime datetime { parsed_datetime.value() };

    // FORM #1: DATE WITH LOCAL TIME
    if (date_time_bytes.size() == 15)
        return datetime.with_current_time_zone();

    // FORM #2: DATE WITH UTC TIME
    if (date_time_bytes.size() == 16 && date_time_bytes[15] == 'Z') {
        return datetime.with_time_zone(TimeZone::TimeZone::UTC);
    }

    // FIXME: Implement FORM #3: DATE WITH LOCAL TIME AND TIME ZONE REFERENCE
    return {};
}

// https://datatracker.ietf.org/doc/html/rfc5545
ErrorOr<Vector<Event>> EventManager::parse_icalendar_vevents(ByteBuffer const& content)
{
    Event event;
    Vector<Event> events;
    ICalendarParserState state = ICalendarParserState::Idle;
    auto lines = StringView(content.bytes()).split_view('\n');
    for (size_t i = 0; i < lines.size(); i++) {
        auto section = TRY(String::formatted("{}", lines[i]).value().split_limit(':', 2));
        if (section.size() > 1) {
            auto property = section[0];
            auto parameter = TRY(section[1].trim_ascii_whitespace());
            switch (state) {
            case ICalendarParserState::InVEvent:
                if (property.bytes().starts_with("DTSTART"sv.bytes())) {
                    auto const start = format_icalendar_vevent_datetime(parameter);
                    if (start.has_value())
                        event.start = start.value();
                }
                if (property.bytes().starts_with("DTEND"sv.bytes())) {
                    auto const end = format_icalendar_vevent_datetime(parameter);
                    if (end.has_value())
                        event.end = end.value();
                }
                if (property == "SUMMARY")
                    event.summary = parameter;
                if (property == "END" && parameter == "VEVENT") {
                    if (event.start.to_parts<DateTime::ISOCalendar>().year && event.end.to_parts<DateTime::ISOCalendar>().year)
                        events.append(event);
                    state = ICalendarParserState::Idle;
                }
                break;
            case ICalendarParserState::Idle:
                if (property == "BEGIN" && parameter == "VEVENT")
                    state = ICalendarParserState::InVEvent;
                break;
            default:
                break;
            }
        }
    }
    return events;
}

ErrorOr<Vector<Event>> EventManager::parse_events(ByteBuffer const& content)
{
    // If content is iCalendar format, try to parse VEVENTs
    if (content.span().starts_with("BEGIN:VCALENDAR"sv.bytes())) {
        set_filename("");
        return parse_icalendar_vevents(content);
    }
    // Otherwise, try to parse content as JSON
    auto json = TRY(AK::JsonParser(content).parse());
    return deserialize_events(json.as_array());
}

ErrorOr<void> EventManager::load_file(FileSystemAccessClient::File& file)
{
    set_filename(file.filename());

    auto content = TRY(file.stream().read_until_eof());
    auto events = parse_events(content);
    set_events(TRY(events));

    m_dirty = false;

    return {};
}

}
