/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Slide.h"
#include "Presentation.h"
#include <AK/JsonObject.h>
#include <AK/TypeCasts.h>
#include <LibGUI/Window.h>
#include <LibGfx/Painter.h>
#include <LibGfx/Size.h>
#include <LibGfx/TextAlignment.h>

Slide::Slide(Vector<NonnullRefPtr<SlideObject>> slide_objects, String title, unsigned frame_count)
    : m_slide_objects(move(slide_objects))
    , m_title(move(title))
    , m_frame_count(frame_count)
{
}

ErrorOr<Slide> Slide::parse_slide(JsonObject const& slide_json, Presentation const& presentation, HashMap<String, JsonObject> const& templates, NonnullRefPtr<GUI::Window> window)
{
    auto frame_count = slide_json.get_integer<unsigned>("frames"sv).value_or(1);

    auto const& maybe_slide_objects = slide_json.get("objects"sv);
    if (!maybe_slide_objects.has_value() || !maybe_slide_objects->is_array())
        return Error::from_string_view("Slide objects must be an array"sv);

    auto const& json_slide_objects = maybe_slide_objects->as_array();
    Vector<NonnullRefPtr<SlideObject>> slide_objects;
    for (auto const& maybe_slide_object_json : json_slide_objects.values()) {
        if (!maybe_slide_object_json.is_object())
            return Error::from_string_view("Slides must be objects"sv);
        auto const& slide_object_json = maybe_slide_object_json.as_object();

        auto slide_object = TRY(SlideObject::parse_slide_object(slide_object_json, presentation, templates, window));
        slide_objects.append(move(slide_object));
    }

    // For the title, we either use the slide's explicit title, or the text of a "role=title" text object, or a fallback of "Untitled slide".
    auto maybe_title = TRY(slide_json.get_byte_string("title"sv).flat_map([&](auto title) -> ErrorOr<String> { return String::from_byte_string(title); }));
    auto title = maybe_title.value_or(TRY(
        slide_objects.first_matching([&](auto const& object) { return object->role() == ObjectRole::TitleObject; })
            .flat_map([&](auto object) -> Optional<ErrorOr<String>> {
                return is<Text>(*object) ? String::from_utf8(static_ptr_cast<Text>(object)->text()) : Optional<ErrorOr<String>> {};
            })
            .value_or(TRY(String::from_utf8("Untitled slide"sv)))));

    return Slide { move(slide_objects), title, frame_count };
}

void Slide::paint(Gfx::Painter& painter, unsigned int current_frame, Gfx::FloatSize display_scale) const
{
    for (auto const& object : m_slide_objects) {
        if (object->is_visible_during_frame(current_frame))
            object->paint(painter, display_scale);
    }
}

bool Slide::fetch_and_reset_invalidation()
{
    auto invalidated { false };
    for (auto& object : m_slide_objects)
        invalidated |= object->fetch_and_reset_invalidation();

    return invalidated;
}
