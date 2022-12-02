/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "SlideObject.h"
#include <AK/DeprecatedString.h>
#include <AK/Forward.h>
#include <AK/NonnullOwnPtrVector.h>
#include <LibGfx/Forward.h>

// A single slide of a presentation.
class Slide final {
public:
    static ErrorOr<Slide> parse_slide(JsonObject const& slide_json, NonnullRefPtr<GUI::Window> window);

    unsigned frame_count() const { return m_frame_count; }
    StringView title() const { return m_title; }

    void paint(Gfx::Painter&, unsigned current_frame, Gfx::FloatSize display_scale) const;

private:
    Slide(NonnullRefPtrVector<SlideObject> slide_objects, DeprecatedString title, unsigned frame_count);

    NonnullRefPtrVector<SlideObject> m_slide_objects;
    DeprecatedString m_title;
    unsigned m_frame_count;
};
