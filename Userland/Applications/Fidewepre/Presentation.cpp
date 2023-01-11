/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Presentation.h"
#include <AK/Forward.h>
#include <AK/JsonObject.h>
#include <AK/SourceGenerator.h>
#include <LibConfig/Client.h>
#include <LibCore/Stream.h>
#include <LibGUI/Window.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/Forward.h>
#include <errno_codes.h>

Presentation::Presentation(Gfx::IntSize normative_size, HashMap<DeprecatedString, DeprecatedString> metadata, HashMap<DeprecatedString, JsonObject> templates)
    : m_normative_size(normative_size)
    , m_metadata(move(metadata))
    , m_templates(move(templates))
{
    m_prerender_count = Config::read_u32("Presenter"sv, "Performance"sv, "PrerenderCount"sv, 1);
    m_slide_cache_size = Config::read_u32("Presenter"sv, "Performance"sv, "CacheSize"sv, DEFAULT_CACHE_SIZE);
}

NonnullOwnPtr<Presentation> Presentation::construct(Gfx::IntSize normative_size, HashMap<DeprecatedString, DeprecatedString> metadata, HashMap<DeprecatedString, JsonObject> templates)
{
    return NonnullOwnPtr<Presentation>(NonnullOwnPtr<Presentation>::Adopt, *new Presentation(normative_size, move(metadata), move(templates)));
}

void Presentation::append_slide(Slide slide)
{
    m_slides.append(move(slide));
}

StringView Presentation::title() const
{
    return m_metadata.get("title"sv).value_or(m_metadata.get("file-name"sv).value_or("Untitled Presentation"sv));
}

StringView Presentation::author() const
{
    if (m_metadata.contains("author"sv))
        return m_metadata.get("author"sv)->view();
    return "Unknown Author"sv;
}

Core::DateTime Presentation::last_modified() const
{
    auto maybe_time = m_metadata.get("last-modified"sv);
    Optional<Core::DateTime> maybe_parsed_time;
    if (maybe_time.has_value()) {
        // FIXME: possibly allow more ISO 8601 formats, for now only full date+time is possible.
        maybe_parsed_time = Core::DateTime::parse("%Y-%m-%dT%H:%M:%S"sv, maybe_time.value());
    }

    if (!maybe_parsed_time.has_value())
        maybe_parsed_time = Core::DateTime::now();

    return maybe_parsed_time.release_value();
}

unsigned Presentation::total_frame_count() const
{
    // FIXME: This would be nicer with a reduction function.
    unsigned frame_count = 0;
    for (auto const& slide : m_slides)
        frame_count += slide.frame_count();
    return frame_count;
}

unsigned Presentation::total_slide_count() const
{
    return m_slides.size();
}

unsigned Presentation::global_frame_number() const
{
    unsigned frame_count = 0;
    for (size_t i = 0; i < m_current_slide.value(); ++i)
        frame_count += m_slides[i].frame_count();
    frame_count += m_current_frame_in_slide.value() + 1;
    return frame_count;
}

void Presentation::next_frame()
{
    m_current_frame_in_slide++;
    if (m_current_frame_in_slide >= current_slide().frame_count()) {
        // If we're on the last frame of the last slide, don't change anything.
        if (m_current_slide.value() + 1 >= m_slides.size()) {
            m_current_frame_in_slide--;
            return;
        }
        m_current_frame_in_slide = 0;
        m_current_slide = min(m_current_slide.value() + 1u, m_slides.size() - 1);
    }
}

void Presentation::previous_frame()
{
    m_current_frame_in_slide.sub(1);
    if (m_current_frame_in_slide.has_overflow()) {
        m_current_slide.saturating_sub(1);
        m_current_frame_in_slide = m_current_slide == 0u ? 0 : current_slide().frame_count() - 1;
    }
}

void Presentation::go_to_first_slide()
{
    m_current_frame_in_slide = 0;
    m_current_slide = 0;
}

void Presentation::go_to_slide(unsigned slide_index)
{
    VERIFY(m_slides.size() > slide_index);
    m_current_slide = slide_index;
    m_current_frame_in_slide = 0;
}

void Presentation::set_cache_size(size_t cache_size)
{
    m_slide_cache_size = cache_size;
}

void Presentation::set_prerender_count(size_t prerender_count)
{
    m_prerender_count = prerender_count;
}

void Presentation::cache(unsigned slide_index, unsigned frame_index, NonnullRefPtr<Gfx::Bitmap> slide_render)
{
    if (m_slide_cache_size == 0)
        return;

    auto freshness = m_cache_time.fetch_add(1);

    m_slide_cache.with_locked([&, this](auto& slide_cache) {
        SlideCacheEntry new_entry {
            .slide_index = slide_index,
            .frame_index = frame_index,
            .slide_render = move(slide_render),
            .freshness = freshness,
        };

        // If we find an identical entry, replace the other entry!
        // There are in fact some circumstances where we can't check the cache carefully, but this will prevent any of them to wreak havoc.
        slide_cache.remove_all_matching([&](auto& value) { return value.slide_index == slide_index && value.frame_index == frame_index; });

        // Empty the cache by evicting old entries. This also handles an overfull cache correctly.
        while (slide_cache.size() + 1 > m_slide_cache_size) {
            size_t oldest_cache_index = 0;
            auto& oldest_cache_entry = slide_cache[oldest_cache_index];
            for (size_t i = 0; i < slide_cache.size(); ++i) {
                auto const& cache_entry = slide_cache[i];
                if (cache_entry.freshness < oldest_cache_entry.freshness) {
                    oldest_cache_entry = cache_entry;
                    oldest_cache_index = i;
                }
            }
            // We have reached the last entry we need to remove, so we can simply reassign the oldest entry.
            if (slide_cache.size() == m_slide_cache_size) {
                slide_cache[oldest_cache_index] = move(new_entry);
                return;
            }
            slide_cache.remove(oldest_cache_index);
        }

        slide_cache.append(move(new_entry));
    });
}

RefPtr<Gfx::Bitmap> Presentation::find_in_cache(unsigned slide_index, unsigned frame_index)
{
    auto freshness = m_cache_time.fetch_add(1);

    return m_slide_cache.with_locked([&](auto& slide_cache) -> RefPtr<Gfx::Bitmap> {
        for (auto& entry : slide_cache) {
            // This order should improve speed, as the slide index is different more often than the frame index.
            if (entry.slide_index == slide_index && entry.frame_index == frame_index) {
                entry.freshness = freshness;
                return entry.slide_render;
            }
        }
        return {};
    });
}

void Presentation::clear_cache()
{
    m_cache_time = 0;
    m_slide_cache.with_locked([](auto& slide_cache) { slide_cache.clear(); });
}

bool Presentation::predraw_slide()
{
    // In order to prevent data races, we extract the current frame and slide once and then use that to predraw a slide.
    auto current_frame = m_current_frame_in_slide;
    auto current_slide = m_current_slide;
    size_t prerender_amount = 0;

    // Go to the next frame as far as necessary.
    // We either look for the first not-prerendered slide, or stop once we reached the prerender count.
    // Note that we lock the slide cache several times here independently.
    // If a slide is incorrectly found to be non-cached, we will find that it is already cached down below.
    do {
        current_frame++;
        prerender_amount++;
        if (current_frame >= m_slides[current_slide.value()].frame_count()) {
            if (current_slide.value() + 1 >= m_slides.size()) {
                current_slide--;
            } else {
                current_frame = 0;
                current_slide = min(current_slide.value() + 1u, m_slides.size() - 1);
            }
        }
    } while (!find_in_cache(current_slide.value(), current_frame.value()).is_null() && prerender_amount < m_prerender_count);

    // Make sure that both accesses to the cache happen atomically, otherwise we might put a slide into the cache twice!
    return m_slide_cache.with_locked([&](auto) -> bool {
        auto possible_cached_slide = find_in_cache(current_slide.value(), current_frame.value());
        if (possible_cached_slide.is_null()) {
            auto display_size = m_normative_size.to_type<float>().scaled_by(m_last_scale.width(), m_last_scale.height()).to_type<int>();
            auto maybe_slide_bitmap = Gfx::Bitmap::try_create(Gfx::BitmapFormat::BGRA8888, display_size);
            if (maybe_slide_bitmap.is_error())
                return false;
            auto slide_bitmap = maybe_slide_bitmap.release_value();
            Gfx::Painter slide_bitmap_painter { slide_bitmap };
            m_slides[current_slide.value()].paint(slide_bitmap_painter, current_frame.value(), m_last_scale);
            cache(current_slide.value(), current_frame.value(), slide_bitmap);
            return true;
        }
        return false;
    });
}

ErrorOr<NonnullOwnPtr<Presentation>> Presentation::load_from_file(StringView file_name, NonnullRefPtr<GUI::Window> window)
{
    auto start_time = Time::now_realtime();

    if (file_name.is_empty())
        return ENOENT;
    auto file = TRY(Core::Stream::File::open_file_or_standard_stream(file_name, Core::Stream::OpenMode::Read));
    auto contents = TRY(file->read_until_eof());
    auto content_string = StringView { contents };
    auto json = TRY(JsonValue::from_string(content_string));

    if (!json.is_object())
        return Error::from_string_view("Presentation must contain a global JSON object"sv);

    auto const& global_object = json.as_object();
    if (!global_object.has_number("version"sv))
        return Error::from_string_view("Presentation file is missing a version specification"sv);

    auto const version = global_object.get("version"sv).to_int(-1);
    if (version != PRESENTATION_FORMAT_VERSION)
        return Error::from_string_view("Presentation file has incompatible version"sv);

    auto const& maybe_metadata = global_object.get("metadata"sv);
    auto const& maybe_slides = global_object.get("slides"sv);

    if (maybe_metadata.is_null() || !maybe_metadata.is_object() || maybe_slides.is_null() || !maybe_slides.is_array())
        return Error::from_string_view("Metadata or slides in incorrect format"sv);

    auto const& maybe_templates = global_object.get("templates"sv);
    if (!maybe_templates.is_null() && !maybe_templates.is_object())
        return Error::from_string_view("Templates are not an object"sv);
    HashMap<DeprecatedString, JsonObject> templates;

    if (!maybe_templates.is_null()) {
        auto json_templates = maybe_templates.as_object();
        TRY(json_templates.try_for_each_member([&](auto const& template_id, auto const& template_data) -> ErrorOr<void> {
            if (!template_data.is_object())
                return Error::from_string_view("Template is not an object"sv);
            templates.set(template_id, template_data.as_object());
            return {};
        }));
    }

    auto const& raw_metadata = maybe_metadata.as_object();
    auto metadata = parse_metadata(raw_metadata);
    auto size = TRY(parse_presentation_size(raw_metadata));
    metadata.set("file-name", file_name);

    auto presentation = Presentation::construct(size, metadata, templates);

    auto const& slides = maybe_slides.as_array();
    for (auto const& maybe_slide : slides.values()) {
        if (!maybe_slide.is_object())
            return Error::from_string_view("Slides must be objects"sv);
        auto const& slide_object = maybe_slide.as_object();

        auto slide = TRY(Slide::parse_slide(slide_object, templates, window));
        presentation->append_slide(move(slide));
    }

    dbgln("Took {} ms to load presentation.", (Time::now_realtime() - start_time).to_milliseconds());

    return presentation;
}

HashMap<DeprecatedString, DeprecatedString> Presentation::parse_metadata(JsonObject const& metadata_object)
{
    HashMap<DeprecatedString, DeprecatedString> metadata;

    metadata_object.for_each_member([&](auto const& key, auto const& value) {
        metadata.set(key, value.to_deprecated_string());
    });

    return metadata;
}

ErrorOr<Gfx::IntSize> Presentation::parse_presentation_size(JsonObject const& metadata_object)
{
    auto const& maybe_width = metadata_object.get("width"sv);
    auto const& maybe_aspect = metadata_object.get("aspect"sv);

    if (maybe_width.is_null() || !maybe_width.is_number() || maybe_aspect.is_null() || !maybe_aspect.is_string())
        return Error::from_string_view("Width or aspect in incorrect format"sv);

    // We intentionally discard floating-point data here. If you need more resolution, just use a larger width.
    auto const width = maybe_width.to_int();
    auto const aspect_parts = maybe_aspect.as_string().split_view(':');
    if (aspect_parts.size() != 2)
        return Error::from_string_view("Aspect specification must have the exact format `width:height`"sv);
    auto aspect_width = aspect_parts[0].to_int<int>();
    auto aspect_height = aspect_parts[1].to_int<int>();
    if (!aspect_width.has_value() || !aspect_height.has_value() || aspect_width.value() == 0 || aspect_height.value() == 0)
        return Error::from_string_view("Aspect width and height must be non-zero integers"sv);

    auto aspect_ratio = static_cast<double>(aspect_height.value()) / static_cast<double>(aspect_width.value());
    return Gfx::IntSize {
        width,
        static_cast<int>(round(static_cast<double>(width) * aspect_ratio)),
    };
}

void Presentation::paint(Gfx::Painter& painter)
{
    auto display_area = painter.clip_rect();
    // These two should be the same, but better be safe than sorry.
    auto width_scale = static_cast<double>(display_area.width()) / static_cast<double>(m_normative_size.width());
    auto height_scale = static_cast<double>(display_area.height()) / static_cast<double>(m_normative_size.height());
    auto scale = Gfx::FloatSize { static_cast<float>(width_scale), static_cast<float>(height_scale) };

    // FIXME: Fill the background with a color depending on the color scheme
    painter.clear_rect(painter.clip_rect(), Color::White);

    auto start_time = Time::now_realtime();

    if (m_last_scale != scale)
        clear_cache();
    m_last_scale = scale;

    m_slide_cache.with_locked([&, this](auto) {
        auto possible_cached_slide = find_in_cache(m_current_slide.value(), m_current_frame_in_slide.value());
        if (possible_cached_slide.is_null()) {
            auto maybe_slide_bitmap = Gfx::Bitmap::try_create(Gfx::BitmapFormat::BGRA8888, display_area.size());
            if (maybe_slide_bitmap.is_error()) {
                // If we're OOM, at least paint directly which usually doesn't allocate as much as an entire bitmap.
                current_slide().paint(painter, m_current_frame_in_slide.value(), scale);
            } else {
                auto slide_bitmap = maybe_slide_bitmap.release_value();
                Gfx::Painter slide_bitmap_painter { slide_bitmap };
                current_slide().paint(slide_bitmap_painter, m_current_frame_in_slide.value(), scale);
                cache(m_current_slide.value(), m_current_frame_in_slide.value(), slide_bitmap);
                painter.blit(painter.clip_rect().top_left(), slide_bitmap, slide_bitmap->rect());
            }
        } else {
            painter.blit(painter.clip_rect().top_left(), *possible_cached_slide, possible_cached_slide->rect());
        }
    });

    dbgln("Took {} ms to draw slide", (Time::now_realtime() - start_time).to_milliseconds());

    auto footer_text = this->footer_text();
    if (footer_text.has_value())
        painter.draw_text(painter.clip_rect(), format_footer(*footer_text), Gfx::TextAlignment::BottomCenter);
}

Optional<DeprecatedString> Presentation::footer_text() const
{
    auto override_enabled = Config::read_bool("Presenter"sv, "Footer"sv, "OverrideFooter"sv, false);
    if (override_enabled) {
        auto footer_enabled = Config::read_bool("Presenter"sv, "Footer"sv, "EnableFooter"sv, true);
        if (!footer_enabled)
            return {};
        return Config::read_string("Presenter"sv, "Footer"sv, "FooterText"sv, "{presentation_title}: {slide_title} ({slide_number}/{slides_total}), frame {slide_frame_number}, last modified {date}"sv);
    }
    return m_metadata.get("footer-center");
}

DeprecatedString Presentation::format_footer(StringView format) const
{
    StringBuilder footer;
    SourceGenerator footer_generator { footer, '{', '}' };
    footer_generator.set("presentation_title"sv, title());
    footer_generator.set("slide_title"sv, current_slide().title());
    footer_generator.set("author"sv, author());
    footer_generator.set("slides_total"sv, DeprecatedString::number(m_slides.size()));
    footer_generator.set("frames_total"sv, DeprecatedString::number(total_frame_count()));
    footer_generator.set("frame_number"sv, DeprecatedString::number(global_frame_number()));
    footer_generator.set("slide_number"sv, DeprecatedString::number(current_slide_number() + 1));
    footer_generator.set("slide_frames_total"sv, DeprecatedString::number(current_slide().frame_count()));
    footer_generator.set("slide_frame_number"sv, DeprecatedString::number(current_frame_in_slide_number() + 1));
    footer_generator.set("date"sv, last_modified().to_deprecated_string());

    footer_generator.append(format);
    return footer_generator.as_string();
}

void Presentation::config_u32_did_change(DeprecatedString const& domain, DeprecatedString const& group, DeprecatedString const& key, u32 value)
{
    if (domain != "Presenter")
        return;
    if (group != "Performance")
        return;

    if (key == "PrerenderCount")
        set_prerender_count(value);
    else if (key == "CacheSize")
        set_cache_size(value);
}
