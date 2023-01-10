/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "LibThreading/ConditionVariable.h"
#include "Slide.h"
#include <AK/DeprecatedString.h>
#include <AK/Forward.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Vector.h>
#include <LibCore/DateTime.h>
#include <LibGfx/Painter.h>
#include <LibGfx/Size.h>

static constexpr int const PRESENTATION_FORMAT_VERSION = 1;

static constexpr size_t const DEFAULT_CACHE_SIZE = 10;

// In-memory representation of the presentation stored in a file.
// This class also contains all the parser code for loading .presenter files.
class Presentation {
public:
    ~Presentation() = default;

    // We can't pass this class directly in an ErrorOr because some of the components are not properly moveable under these conditions.
    static ErrorOr<NonnullOwnPtr<Presentation>> load_from_file(StringView file_name, NonnullRefPtr<GUI::Window> window);

    StringView title() const;
    StringView author() const;
    Core::DateTime last_modified() const;
    Gfx::IntSize normative_size() const { return m_normative_size; }

    Slide const& current_slide() const { return m_slides[m_current_slide.value()]; }
    Span<Slide const> slides() const { return m_slides; }
    unsigned current_slide_number() const { return m_current_slide.value(); }
    unsigned current_frame_in_slide_number() const { return m_current_frame_in_slide.value(); }

    unsigned total_frame_count() const;
    unsigned total_slide_count() const;
    unsigned global_frame_number() const;

    void next_frame();
    void previous_frame();
    void go_to_first_slide();
    void go_to_slide(unsigned slide_index);

    // This assumes that the caller has clipped the painter to exactly the display area.
    void paint(Gfx::Painter& painter);

    // Formats a footer with user-supplied formatting parameters.
    // {presentation_title}, {slide_title}, {author}, {slides_total}, {frames_total}, {date}
    // {slide_number}: Slide number
    // {slide_frame_number}: Number of frame within slide
    // {slide_frames_total}: Total number of frames within the current slide
    // {frame_number}: Counts all frames on all slides
    DeprecatedString format_footer(StringView format) const;

    Optional<DeprecatedString> footer_text() const;

    // Note that if the cache is larger than the given value, old slides will be evicted from the cache only once new slides are pushed to the cache.
    void set_cache_size(size_t cache_size);

    void predraw_slide();

private:
    static HashMap<DeprecatedString, DeprecatedString> parse_metadata(JsonObject const& metadata_object);
    static ErrorOr<Gfx::IntSize> parse_presentation_size(JsonObject const& metadata_object);

    Presentation(Gfx::IntSize normative_size, HashMap<DeprecatedString, DeprecatedString> metadata, HashMap<DeprecatedString, JsonObject> templates);
    static NonnullOwnPtr<Presentation> construct(Gfx::IntSize normative_size, HashMap<DeprecatedString, DeprecatedString> metadata, HashMap<DeprecatedString, JsonObject> templates);

    void append_slide(Slide slide);

    void cache(unsigned slide_index, unsigned frame_index, NonnullRefPtr<Gfx::Bitmap> slide_render);
    // Also marks the found entry as "hit" if possible.
    RefPtr<Gfx::Bitmap> find_in_cache(unsigned slide_index, unsigned frame_index);
    void clear_cache();

    Vector<Slide> m_slides {};
    // This is not a pixel size, but an abstract size used by the slide objects for relative positioning.
    Gfx::IntSize m_normative_size;
    HashMap<DeprecatedString, DeprecatedString> m_metadata;
    HashMap<DeprecatedString, JsonObject> m_templates;

    Checked<unsigned> m_current_slide { 0 };
    Checked<unsigned> m_current_frame_in_slide { 0 };

    Gfx::FloatSize m_last_scale {};
    // This variable mighht seem to have TOCTOU bugs, but it actually doesn't matter if we accidentally overfill or underfill the cache once.
    Atomic<size_t> m_slide_cache_size { DEFAULT_CACHE_SIZE };
    // This variable however must be handled very carefully in the multi-threaded environment!
    Atomic<unsigned> m_cache_time { 0 };

    struct SlideCacheEntry {
        unsigned slide_index;
        unsigned frame_index;
        NonnullRefPtr<Gfx::Bitmap> slide_render;
        // Set to the current cache_time whenever the value is used.
        // The entry with the lowest freshness is evicted from the cache first.
        unsigned freshness;
    };
    // An LRU (least recently used) cache of rendered slides.
    // We use a vector as it is most memory efficient, there is not one preferred key (so a HashMap makes little sense) and looping over the Vector is decently fast at the used sizes anyways.
    Threading::MutexProtected<Vector<SlideCacheEntry, DEFAULT_CACHE_SIZE>> m_slide_cache;
};
