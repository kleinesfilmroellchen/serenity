/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 * Copyright (c) 2022, kubczakn <kubczakn@umich.edu>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Forward.h>
#include <AK/LexicalPath.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/NonnullRefPtr.h>
#include <AK/String.h>
#include <LibCore/Object.h>
#include <LibGUI/Application.h>
#include <LibGUI/MessageBox.h>
#include <LibGUI/Window.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/Color.h>
#include <LibGfx/Font/Font.h>
#include <LibGfx/Forward.h>
#include <LibGfx/Painter.h>
#include <LibGfx/Rect.h>
#include <LibGfx/TextAlignment.h>
#include <LibImageDecoderClient/Client.h>
#include <LibThreading/BackgroundAction.h>

enum class ObjectRole {
    Default,
    TitleObject,
};

class Presentation;

// Anything that can be on a slide.
// For properties set in the file, we re-use the Core::Object property facility.
class SlideObject : public Core::Object {
    C_OBJECT_ABSTRACT(SlideObject);

public:
    virtual ~SlideObject() = default;

    static ErrorOr<NonnullRefPtr<SlideObject>> parse_slide_object(JsonObject const& slide_object_json, Presentation const&, HashMap<String, JsonObject> const& templates, NonnullRefPtr<GUI::Window> window);

    bool is_visible_during_frame([[maybe_unused]] unsigned frame_number) const { return m_frames.is_empty() || m_frames.contains(frame_number); }

    virtual void paint(Gfx::Painter&, Gfx::FloatSize display_scale) const;
    ALWAYS_INLINE Gfx::IntRect transformed_bounding_box(Gfx::IntRect clip_rect, Gfx::FloatSize display_scale) const;

    void set_rect(Gfx::IntRect rect) { m_rect = rect; }
    Gfx::IntRect rect() const { return m_rect; }

    HashTable<unsigned> const& frames() const { return m_frames; }
    void set_frames(HashTable<unsigned> frames) { m_frames = move(frames); }

    ObjectRole role() const { return m_role; }
    void set_role(ObjectRole role) { m_role = role; }

    // Returns whether the slide object was invalidated, then resets the invalidation state.
    bool fetch_and_reset_invalidation();

protected:
    SlideObject();

    Gfx::IntRect m_rect;
    HashTable<unsigned> m_frames {};
    ObjectRole m_role { ObjectRole::Default };

    Atomic<bool> m_invalidated { false };
};

// Objects with a foreground color.
class GraphicsObject : public SlideObject {
    C_OBJECT_ABSTRACT(SlideObject);

public:
    virtual ~GraphicsObject() = default;

    void set_color(Gfx::Color color) { m_color = color; }
    Gfx::Color color() const { return m_color; }

protected:
    GraphicsObject();

    // FIXME: Change the default color based on the color scheme
    Gfx::Color m_color { Gfx::Color::Black };
};

class Text : public GraphicsObject {
    C_OBJECT(SlideObject);

public:
    Text();
    virtual ~Text() = default;

    virtual void paint(Gfx::Painter&, Gfx::FloatSize display_scale) const override;

    void set_font(DeprecatedString font)
    {
        if (auto proper_font = String::from_deprecated_string(font); !proper_font.is_error())
            m_font = proper_font.release_value();
    }
    StringView font() const { return m_font; }
    void set_font_size(int font_size) { m_font_size = font_size; }
    int font_size() const { return m_font_size; }
    void set_font_weight(unsigned font_weight) { m_font_weight = font_weight; }
    unsigned font_weight() const { return m_font_weight; }
    void set_text_alignment(Gfx::TextAlignment text_alignment) { m_text_alignment = text_alignment; }
    Gfx::TextAlignment text_alignment() const { return m_text_alignment; }
    void set_text(DeprecatedString text)
    {
        if (auto proper_text = String::from_deprecated_string(text); !proper_text.is_error())
            m_text = proper_text.release_value();
    }
    StringView text() const { return m_text; }
    void set_font_style(DeprecatedString font_style)
    {
        if (auto proper_font_style = String::from_deprecated_string(font_style); !proper_font_style.is_error())
            m_font_style = proper_font_style.release_value();
    }
    StringView font_style() const { return m_font_style; }

protected:
    String m_text;
    // The font family, technically speaking.
    String m_font;
    int m_font_size { 18 };
    unsigned m_font_weight { Gfx::FontWeight::Regular };
    Gfx::TextAlignment m_text_alignment { Gfx::TextAlignment::CenterLeft };
    String m_font_style;
};

// How to scale an image object.
enum class ImageScaling {
    // Fit the image into the bounding box, preserving its aspect ratio.
    FitSmallest,
    // Match the bounding box in width and height exactly; this will change the image's aspect ratio if the aspect ratio of the bounding box is not exactly the same.
    Stretch,
    // Make the image fill the bounding box, preserving its aspect ratio. This means that the image will be cut off on the top and bottom or left and right, depending on which dimension is "too large".
    FitLargest,
};

class Image : public SlideObject {
    C_OBJECT(Image);

public:
    Image(NonnullRefPtr<GUI::Window>, String presentation_path);
    virtual ~Image() = default;

    virtual void paint(Gfx::Painter&, Gfx::FloatSize display_scale) const override;

    void set_image_path(DeprecatedString image_path)
    {
        m_image_path = LexicalPath { move(image_path) };

        auto image_load_action = Threading::BackgroundAction<ErrorOr<void>>::try_create(
            [this](auto&) {
                // FIXME: shouldn't be necessary
                Core::EventLoop loop;
                return this->reload_image();
            },
            [this](auto result) -> ErrorOr<void> {
                if (result.is_error()) {
                    if (auto text = String::formatted("Loading image {} failed: {}", m_image_path, result.error()); !text.is_error())
                        GUI::MessageBox::show_error(m_window, text.release_value().bytes_as_string_view());
                } else {
                    // This should cause us to redraw.
                    m_window->update();
                }
                return {};
            });

        if (image_load_action.is_error()) {
            // Try to load the image synchronously instead.
            auto result = this->reload_image();
            if (result.is_error()) {
                if (auto text = String::formatted("Loading image {} failed: {}", m_image_path, result.error()); !text.is_error())
                    GUI::MessageBox::show_error(m_window, text.release_value().bytes_as_string_view());
            }
        } else {
            m_image_load_action = image_load_action.release_value();
        }
    }
    StringView image_path() const { return m_image_path.string(); }
    void set_scaling(ImageScaling scaling) { m_scaling = scaling; }
    ImageScaling scaling() const { return m_scaling; }
    void set_scaling_mode(Gfx::Painter::ScalingMode scaling_mode) { m_scaling_mode = scaling_mode; }
    Gfx::Painter::ScalingMode scaling_mode() const { return m_scaling_mode; }

protected:
    LexicalPath m_image_path { ""sv };
    String m_presentation_path;
    ImageScaling m_scaling { ImageScaling::FitSmallest };
    Gfx::Painter::ScalingMode m_scaling_mode { Gfx::Painter::ScalingMode::SmoothPixels };

private:
    ErrorOr<void> reload_image();

    Threading::MutexProtected<RefPtr<Gfx::Bitmap>> m_currently_loaded_image;
    NonnullRefPtr<GUI::Window> m_window;
    RefPtr<Threading::BackgroundAction<ErrorOr<void>>> m_image_load_action;
};
