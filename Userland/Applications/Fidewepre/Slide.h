/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "SlideObject.h"
#include <AK/Forward.h>
#include <AK/NonnullOwnPtrVector.h>
#include <AK/String.h>
#include <LibGfx/Forward.h>

class Presentation;

// A single slide of a presentation.
class Slide final {
public:
    static ErrorOr<Slide> parse_slide(JsonObject const& slide_json, Presentation const& presentation, HashMap<String, JsonObject> const& templates, NonnullRefPtr<GUI::Window> window);

    unsigned frame_count() const { return m_frame_count; }
    StringView title() const { return m_title; }

    void paint(Gfx::Painter&, unsigned current_frame, Gfx::FloatSize display_scale) const;

    bool fetch_and_reset_invalidation();

private:
    Slide(Vector<NonnullRefPtr<SlideObject>> slide_objects, String title, unsigned frame_count);

    Vector<NonnullRefPtr<SlideObject>> m_slide_objects;
    String m_title;
    unsigned m_frame_count;
};
