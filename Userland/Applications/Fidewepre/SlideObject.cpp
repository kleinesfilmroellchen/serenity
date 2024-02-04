/*
 * Copyright (c) 2022-2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 * Copyright (c) 2022, kubczakn <kubczakn@umich.edu>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "SlideObject.h"
#include "PDFPage.h"
#include "Presentation.h"
#include <AK/JsonObject.h>
#include <AK/LexicalPath.h>
#include <AK/RefPtr.h>
#include <LibCMake/CMakeCache/SyntaxHighlighter.h>
#include <LibCMake/SyntaxHighlighter.h>
#include <LibCore/EventReceiver.h>
#include <LibCpp/SyntaxHighlighter.h>
#include <LibGUI/GML/SyntaxHighlighter.h>
#include <LibGUI/GitCommitSyntaxHighlighter.h>
#include <LibGUI/INISyntaxHighlighter.h>
#include <LibGUI/Margins.h>
#include <LibGfx/Font/Font.h>
#include <LibGfx/Font/FontDatabase.h>
#include <LibGfx/Font/FontStyleMapping.h>
#include <LibGfx/Forward.h>
#include <LibGfx/Orientation.h>
#include <LibGfx/Painter.h>
#include <LibGfx/Size.h>
#include <LibGfx/TextWrapping.h>
#include <LibImageDecoderClient/Client.h>
#include <LibJS/SyntaxHighlighter.h>
#include <LibMarkdown/SyntaxHighlighter.h>
#include <LibSQL/AST/SyntaxHighlighter.h>
#include <LibSyntax/Highlighter.h>
#include <LibWeb/CSS/SyntaxHighlighter/SyntaxHighlighter.h>
#include <LibWeb/HTML/SyntaxHighlighter/SyntaxHighlighter.h>
#include <Shell/SyntaxHighlighter.h>

ErrorOr<NonnullRefPtr<SlideObject>> SlideObject::parse_slide_object(JsonObject const& slide_object_json, Presentation const& presentation, HashMap<String, JsonObject> const& templates, NonnullRefPtr<GUI::Window> window)
{
    auto const& maybe_type = slide_object_json.get_byte_string("type"sv);
    if (!maybe_type.has_value())
        return Error::from_string_view("Slide object must have a type"sv);

    auto type = maybe_type.value();
    RefPtr<SlideObject> object;
    if (type == "text"sv)
        object = TRY(try_make_ref_counted<Text>());
    else if (type == "image"sv)
        object = TRY(try_make_ref_counted<Image>(window, presentation.path()));
    else if (type == "pdf"sv)
        object = TRY(try_make_ref_counted<PDFPage>(window, presentation.path()));
    else
        return Error::from_string_view("Unsupported slide object type"sv);

    auto assign_property = [&](auto const& key, auto const& value) {
        if (key == "type"sv || key == "templates"sv)
            return;
        auto successful = object->set_property(key, value);
        if (!successful)
            dbgln("Storing {:15} = {:20} on slide object type {:8} failed, ignoring.", key, value, type);
    };

    // First, assign properties from the templates.
    auto const& used_templates = slide_object_json.get("templates"sv);
    if (used_templates.has_value()) {
        if (!used_templates->is_array())
            return Error::from_string_view("Slide object templates are not an array"sv);

        Vector<String> used_template_ids;
        used_template_ids.ensure_capacity(used_templates->as_array().size());
        TRY(used_templates->as_array().try_for_each([&](auto const& template_id) -> ErrorOr<void> {
            used_template_ids.append(TRY(String::from_byte_string(template_id.as_string())));
            return {};
        }));

        for (auto const& template_id : used_template_ids) {
            auto referenced_template = templates.get(template_id);
            if (!referenced_template.has_value())
                return Error::from_string_view("Undefined template used in a slide object"sv);

            referenced_template.value().for_each_member(assign_property);
        }
    }

    // Then, assign properties from the object itself, which always have priority.
    slide_object_json.for_each_member(assign_property);

    return object.release_nonnull();
}

SlideObject::SlideObject()
{
    REGISTER_RECT_PROPERTY("rect", rect, set_rect);
    REGISTER_ENUM_PROPERTY("role", role, set_role, ObjectRole,
        { ObjectRole::Default, "default" },
        { ObjectRole::TitleObject, "title" }, );

    register_property(
        "frames"sv,
        [this] {
            auto const& frames = this->frames();
            JsonArray json_frames;
            for (auto element : frames) {
                auto result = json_frames.append(element);
                if (result.is_error()) {
                    dbgln("OOM while allocating slide object frames list");
                    return json_frames;
                }
            }

            return json_frames;
        },
        // We can’t really do anything here except accept any value.
        [](auto& id) -> ErrorOr<JsonValue> { return id; },
        [this](auto value) {
            if (!value.is_array())
                return;

            HashTable<unsigned> frames;
            auto const& values = value.as_array().values();
            for (auto const& element : values)
                frames.set(element.template get_integer<unsigned>().value_or(0));
            this->set_frames(frames);

            return;
        });
}

// FIXME: Consider drawing a placeholder box instead.
void SlideObject::paint(Gfx::Painter&, Gfx::FloatSize) const { }

Gfx::IntRect SlideObject::transformed_bounding_box(Gfx::IntRect clip_rect, Gfx::FloatSize display_scale) const
{
    return m_rect.to_type<float>().scaled(display_scale.width(), display_scale.height()).to_rounded<int>().translated(clip_rect.top_left());
}

bool SlideObject::fetch_and_reset_invalidation()
{
    auto invalidated = m_invalidated.exchange(false);
    return invalidated;
}

GraphicsObject::GraphicsObject()
{
    register_property(
        "color"sv, [this]() { return this->color().to_byte_string(); },
        [](auto& id) -> ErrorOr<JsonValue> { return id; },
        [this](auto value) {
            auto color = Color::from_string(value.as_string());
            if (color.has_value()) {
                this->set_color(color.value());
                return;
            }
            return;
        });
}

Text::Text()
{
    REGISTER_STRING_PROPERTY("text", text, set_text);
    REGISTER_FONT_WEIGHT_PROPERTY("font-weight", font_weight, set_font_weight);
    REGISTER_TEXT_ALIGNMENT_PROPERTY("text-alignment", text_alignment, set_text_alignment);
    REGISTER_INT_PROPERTY("font-size", font_size, set_font_size);
    REGISTER_STRING_PROPERTY("font", font, set_font);
    REGISTER_STRING_PROPERTY("font-style", font_style, set_font_style);
    REGISTER_STRING_PROPERTY("syntax-highlight", syntax_highlight, set_syntax_highlight);
}

void Text::paint(Gfx::Painter& painter, Gfx::FloatSize display_scale) const
{
    auto scaled_bounding_box = this->transformed_bounding_box(painter.clip_rect(), display_scale);

    auto scaled_font_size = display_scale.height() * static_cast<float>(m_font_size);
    auto font = Gfx::FontDatabase::the().get(m_font, scaled_font_size, m_font_weight, Gfx::FontWidth::Normal, Gfx::name_to_slope(m_font_style), Gfx::Font::AllowInexactSizeMatch::Yes);
    if (font.is_null())
        font = Gfx::FontDatabase::default_font();
    if (m_highlighting_document->has_spans()) {
        auto draw_text = [&](auto const& rect, auto const& raw_text, Gfx::Font const& draw_font, Gfx::TextAttributes attributes) {
            painter.draw_text(rect, raw_text, draw_font, m_text_alignment, attributes.color);

            if (attributes.underline_style.has_value()) {
                auto bottom_left = [&]() {
                    auto point = rect.bottom_left();

                    if constexpr (IsSame<RemoveCVReference<decltype(rect)>, Gfx::IntRect>)
                        return point;
                    else
                        return point.template to_type<int>();
                };

                auto bottom_right = [&]() {
                    auto point = rect.bottom_right().translated(-1, 0);

                    if constexpr (IsSame<RemoveCVReference<decltype(rect)>, Gfx::IntRect>)
                        return point;
                    else
                        return point.template to_type<int>();
                };

                if (attributes.underline_style == Gfx::TextAttributes::UnderlineStyle::Solid)
                    painter.draw_line(bottom_left(), bottom_right(), attributes.underline_color.value_or(attributes.color));
                if (attributes.underline_style == Gfx::TextAttributes::UnderlineStyle::Wavy)
                    painter.draw_triangle_wave(bottom_left(), bottom_right(), attributes.underline_color.value_or(attributes.color), 2);
            }
        };
        Gfx::TextAttributes unspanned_text_attributes;
        unspanned_text_attributes.color = m_color;

        auto const line_height = font->pixel_metrics().line_spacing();
        size_t span_index = 0;
        auto spans = m_highlighting_document->spans();

        for (size_t line_index = 0; line_index < m_highlighting_document->lines().size(); ++line_index) {
            auto const line = m_highlighting_document->lines()[line_index].view();
            Gfx::FloatPoint const line_position = scaled_bounding_box.top_left().to_type<float>().moved_down(static_cast<float>(line_index) * line_height);
            // This is a simplified copy of the text editor syntax highlighting code (no collapsed range handling or wrapping).
            size_t next_column = 0;
            Gfx::FloatRect span_rect = { line_position, { 0, line_height } };
            auto draw_text_helper = [&](size_t start, size_t end, Gfx::Font const& draw_font, Gfx::TextAttributes text_attributes) {
                size_t length = end - start;
                if (length == 0)
                    return;
                auto text = line.substring_view(start, length);
                span_rect.set_width(draw_font.width(text) + draw_font.glyph_spacing());
                if (text_attributes.background_color.has_value()) {
                    painter.fill_rect(span_rect.to_type<int>(), text_attributes.background_color.value());
                }
                draw_text(span_rect, text, draw_font, text_attributes);
                span_rect.translate_by(span_rect.width(), 0);
            };

            while (span_index < spans.size()) {
                auto& span = spans[span_index];
                // Skip spans that have ended before this point.
                if (span.range.end().line() < line_index) {
                    ++span_index;
                    continue;
                }
                if (span.range.start().line() > line_index) {
                    // no more spans in this line, moving on
                    break;
                }
                size_t span_start;
                if (span.range.start().line() < line_index) {
                    span_start = 0;
                } else {
                    span_start = span.range.start().column();
                }
                size_t span_end;
                bool span_consumed;
                if (span.range.end().line() > line_index || span.range.end().column() > line.length()) {
                    span_end = line.length();
                    span_consumed = false;
                } else {
                    span_end = span.range.end().column();
                    span_consumed = true;
                }

                if (span_start != next_column) {
                    // draw unspanned text between spans
                    draw_text_helper(next_column, span_start, *font, unspanned_text_attributes);
                }
                auto& span_font = span.attributes.bold ? font->bold_variant() : *font;
                draw_text_helper(span_start, span_end, span_font, span.attributes);
                next_column = span_end;
                if (!span_consumed) {
                    // continue with same span on next line
                    break;
                } else {
                    ++span_index;
                }
            }
            // draw unspanned text after last span
            if (next_column < line.length()) {
                draw_text_helper(next_column, line.length(), *font, unspanned_text_attributes);
            }
        }

    } else {
        painter.draw_text(scaled_bounding_box, m_text.bytes_as_string_view(), *font, m_text_alignment, m_color, Gfx::TextElision::None, Gfx::TextWrapping::Wrap);
    }
}

void Text::set_syntax_highlight(StringView language)
{
    m_syntax_highlight = Syntax::language_from_name(language);

    if (m_syntax_highlight.has_value()) {
        auto const language = m_syntax_highlight.release_value();

        switch (language) {
        case Syntax::Language::Cpp:
            m_highlighter = make<Cpp::SyntaxHighlighter>();
            break;
        case Syntax::Language::CMake:
            m_highlighter = make<CMake::SyntaxHighlighter>();
            break;
        case Syntax::Language::CMakeCache:
            m_highlighter = make<CMake::Cache::SyntaxHighlighter>();
            break;
        case Syntax::Language::CSS:
            m_highlighter = make<Web::CSS::SyntaxHighlighter>();
            break;
        case Syntax::Language::GitCommit:
            m_highlighter = make<GUI::GitCommitSyntaxHighlighter>();
            break;
        case Syntax::Language::GML:
            m_highlighter = make<GUI::GML::SyntaxHighlighter>();
            break;
        case Syntax::Language::HTML:
            m_highlighter = make<Web::HTML::SyntaxHighlighter>();
            break;
        case Syntax::Language::INI:
            m_highlighter = make<GUI::IniSyntaxHighlighter>();
            break;
        case Syntax::Language::JavaScript:
            m_highlighter = make<JS::SyntaxHighlighter>();
            break;
        case Syntax::Language::Markdown:
            m_highlighter = make<Markdown::SyntaxHighlighter>();
            break;
        case Syntax::Language::Shell:
            m_highlighter = make<Shell::SyntaxHighlighter>();
            break;
        case Syntax::Language::SQL:
            m_highlighter = make<SQL::AST::SyntaxHighlighter>();
            break;
        default:
            m_syntax_highlight = {};
            m_highlighter = nullptr;
        }
    } else {
        m_highlighter = nullptr;
    }

    if (m_highlighter != nullptr)
        m_highlighter->attach(*this);

    update_document();
}

void Text::update_document()
{
    m_highlighting_document->clear();
    auto const lines = MUST(m_text.split('\n', SplitBehavior::KeepEmpty));
    for (auto const& line : lines)
        m_highlighting_document->add_line(line);

    if (m_highlighter != nullptr) {
        m_highlighter->rehighlight(GUI::Application::the()->palette());
        m_invalidated = true;
    }
}

Image::Image(NonnullRefPtr<GUI::Window> window, String presentation_path)
    : m_presentation_path(move(presentation_path))
    , m_window(move(window))
{
    REGISTER_STRING_PROPERTY("path", image_path, set_image_path);
    REGISTER_ENUM_PROPERTY("scaling", scaling, set_scaling, ImageScaling,
        { ImageScaling::FitSmallest, "fit-smallest" },
        { ImageScaling::FitLargest, "fit-largest" },
        { ImageScaling::Stretch, "stretch" }, );
    REGISTER_ENUM_PROPERTY("scaling-mode", scaling_mode, set_scaling_mode, Gfx::Painter::ScalingMode,
        { Gfx::Painter::ScalingMode::SmoothPixels, "smooth-pixels" },
        { Gfx::Painter::ScalingMode::NearestNeighbor, "nearest-neighbor" },
        { Gfx::Painter::ScalingMode::BilinearBlend, "bilinear-blend" }, );
}

ErrorOr<int> Image::reload_image()
{
    auto image_path = LexicalPath::absolute_path(LexicalPath { m_presentation_path.to_byte_string() }.parent().string(),
        m_image_path.string());

    auto file = TRY(Core::File::open(image_path, Core::File::OpenMode::Read));
    auto data = TRY(file->read_until_eof());
    auto client = TRY(ImageDecoderClient::Client::try_create());
    auto maybe_decoded = client->decode_image(data);
    // Manually make sure we don't keep the ImageDecoder process around.
    client->shutdown();
    if (!maybe_decoded.has_value() || maybe_decoded.value().frames.size() < 1)
        return Error::from_string_view("Could not decode image"sv);
    // FIXME: Handle multi-frame images.
    m_currently_loaded_image.with_locked([&](auto& image) { image = maybe_decoded.value().frames.first().bitmap; });
    m_invalidated.store(true);
    return 0;
}

void Image::set_image_path(String image_path)
{
    m_image_path = LexicalPath { image_path.to_byte_string() };

    auto image_load_action = Threading::BackgroundAction<int>::try_create(
        [this](auto&) {
            // FIXME: shouldn't be necessary
            Core::EventLoop loop;
            return this->reload_image();
        },
        [this](auto) -> ErrorOr<void> {
            m_invalidated = true;
            m_window->update();
            return {};
        },
        [this](auto error) {
            if (auto text = String::formatted("Loading image {} failed: {}", m_image_path, error); !text.is_error())
                GUI::MessageBox::show_error(m_window, text.release_value().bytes_as_string_view());
        });

    if (image_load_action.is_error()) {
        // Try to load the image synchronously instead.
        auto result = this->reload_image();
        if (result.is_error()) {
            if (auto text = String::formatted("Loading image {} failed: {}", m_image_path, result.error()); !text.is_error())
                GUI::MessageBox::show_error(m_window, text.release_value().bytes_as_string_view());
            else
                dbgln("{}", result.error());
        }
    } else {
        m_image_load_action = image_load_action.release_value();
    }
}

void Image::paint(Gfx::Painter& painter, Gfx::FloatSize display_scale) const
{
    // Const-cast is fine here, as we never modify the loaded image.
    const_cast<Threading::MutexProtected<RefPtr<Gfx::Bitmap>>*>(&m_currently_loaded_image)->with_locked([&, this](auto const& image) {
        if (!image) {
            dbgln("Image {} is not loaded yet, skipping", m_image_path);
            return;
        }

        auto transformed_bounding_box = this->transformed_bounding_box(painter.clip_rect(), display_scale);

        auto image_size = image->size();
        auto image_aspect_ratio = image_size.aspect_ratio();

        auto image_box = transformed_bounding_box;
        if (m_scaling != ImageScaling::Stretch) {
            auto width_corresponding_to_height = image_box.height() * image_aspect_ratio;
            auto direction_to_preserve_for_fit = width_corresponding_to_height > image_box.width() ? Orientation::Horizontal : Orientation::Vertical;
            // Fit largest and fit smallest are the same, except with inverted preservation conditions.
            if (m_scaling == ImageScaling::FitLargest)
                direction_to_preserve_for_fit = direction_to_preserve_for_fit == Orientation::Vertical ? Orientation::Horizontal : Orientation::Vertical;

            image_box.set_size(image_box.size().match_aspect_ratio(image_aspect_ratio, direction_to_preserve_for_fit));
        }

        image_box = image_box.centered_within(transformed_bounding_box);

        auto original_clip_rect = painter.clip_rect();
        painter.clear_clip_rect();
        painter.add_clip_rect(image_box);

        painter.draw_scaled_bitmap(image_box, *image, image->rect(), 1.0f, m_scaling_mode);

        painter.clear_clip_rect();
        painter.add_clip_rect(original_clip_rect);
    });
}
