/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "Slide.h"
#include <AK/Forward.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <LibConfig/Listener.h>
#include <LibCore/DateTime.h>
#include <LibGfx/Painter.h>
#include <LibGfx/Size.h>
#include <LibThreading/ConditionVariable.h>

static constexpr int const PRESENTATION_FORMAT_VERSION = 1;

static constexpr size_t const DEFAULT_CACHE_SIZE = 20;

// In-memory representation of the presentation stored in a file.
// This class also contains all the parser code for loading .presenter files.
class Presentation : public Config::Listener {
public:
    ~Presentation() = default;

    // We can't pass this class directly in an ErrorOr because some of the components are not properly moveable under these conditions.
    static ErrorOr<NonnullOwnPtr<Presentation>> load_from_file(StringView file_name, NonnullRefPtr<GUI::Window> window);

    Utf8View title() const;
    Utf8View author() const;
    Core::DateTime last_modified() const;
    Gfx::IntSize normative_size() const { return m_normative_size; }

    String path() const { return m_file_name; }

    Slide const& current_slide() const { return m_slides[m_current_slide.value()]; }
    Span<Slide const> slides() const { return m_slides; }
    unsigned current_slide_number() const { return m_current_slide.value(); }
    unsigned current_frame_in_slide_number() const { return m_current_frame_in_slide.value(); }

    unsigned total_frame_count() const;
    unsigned total_slide_count() const;
    unsigned global_frame_number() const;

    void next_frame();
    void previous_frame();
    bool has_next_frame() const;
    bool has_previous_frame() const;
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
    String format_footer(Utf8View format) const;

    Optional<String> footer_text() const;

    // Note that if the cache is larger than the given value, old slides will be evicted from the cache only once new slides are pushed to the cache.
    void set_cache_size(size_t cache_size);
    void set_prerender_count(size_t prerender_count);

    // Returns whether predrawing was successful.
    bool predraw_slide();

    virtual void config_u32_did_change(StringView domain, StringView group, StringView key, u32 value) override;

private:
    static ErrorOr<HashMap<String, String>> parse_metadata(JsonObject const& metadata_object);
    static ErrorOr<Gfx::IntSize> parse_presentation_size(JsonObject const& metadata_object);

    Presentation(String file_name, Gfx::IntSize normative_size, HashMap<String, String> metadata, HashMap<String, JsonObject> templates);
    static NonnullOwnPtr<Presentation> construct(String file_name, Gfx::IntSize normative_size, HashMap<String, String> metadata, HashMap<String, JsonObject> templates);

    void append_slide(Slide slide);

    void cache(unsigned slide_index, unsigned frame_index, NonnullRefPtr<Gfx::Bitmap> slide_render);
    // Also marks the found entry as "hit" if possible.
    RefPtr<Gfx::Bitmap> find_in_cache(unsigned slide_index, unsigned frame_index);
    void clear_cache();

    String m_file_name {};

    Vector<Slide> m_slides {};
    // This is not a pixel size, but an abstract size used by the slide objects for relative positioning.
    Gfx::IntSize m_normative_size;
    HashMap<String, String> m_metadata;
    HashMap<String, JsonObject> m_templates;

    Checked<unsigned> m_current_slide { 0 };
    Checked<unsigned> m_current_frame_in_slide { 0 };

    Gfx::FloatSize m_last_scale {};
    // This variable might seem to have TOCTOU bugs, but it actually doesn't matter if we accidentally overfill or underfill the cache once.
    Atomic<size_t> m_slide_cache_size { DEFAULT_CACHE_SIZE };
    Atomic<size_t> m_prerender_count { 1 };
    // This variable however must be handled very carefully in the multi-threaded environment!
    Atomic<unsigned> m_cache_time { 0 };

    // Marker type to identify a cache entry that's not quite rendered yet.
    struct [[gnu::packed]] IsMarkedForRendering { };

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
