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
#include <LibCore/EventReceiver.h>
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
#include <LibSyntax/Document.h>
#include <LibSyntax/HighlighterClient.h>
#include <LibSyntax/Language.h>
#include <LibThreading/BackgroundAction.h>
#include <LibThreading/MutexProtected.h>

enum class ObjectRole {
    Default,
    TitleObject,
};

class Presentation;

// Anything that can be on a slide.
// For properties set in the file, we re-use the GUI::Object property facility.
class SlideObject : public GUI::Object {
    C_OBJECT_ABSTRACT(SlideObject);

public:
    virtual ~SlideObject() = default;

    static ErrorOr<NonnullRefPtr<SlideObject>> parse_slide_object(JsonObject const& slide_object_json, Presentation const&, HashMap<String, JsonObject> const& templates, NonnullRefPtr<GUI::Window> window);

    bool is_visible_during_frame([[maybe_unused]] unsigned frame_number) const { return m_frames.is_empty() || m_frames.contains(frame_number); }

    virtual void paint(Gfx::Painter&, Gfx::FloatSize display_scale) const;
    Gfx::IntRect transformed_bounding_box(Gfx::IntRect clip_rect, Gfx::FloatSize display_scale) const;

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

    mutable Atomic<bool> m_invalidated { false };
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

class Text : public GraphicsObject
    , public Syntax::HighlighterClient {
    C_OBJECT(SlideObject);

public:
    Text();
    virtual ~Text() = default;

    virtual void paint(Gfx::Painter&, Gfx::FloatSize display_scale) const override;

    void set_font(String font)
    {
        m_font = font;
    }
    StringView font() const { return m_font; }
    void set_font_size(int font_size) { m_font_size = font_size; }
    int font_size() const { return m_font_size; }
    void set_font_weight(unsigned font_weight) { m_font_weight = font_weight; }
    unsigned font_weight() const { return m_font_weight; }
    void set_text_alignment(Gfx::TextAlignment text_alignment) { m_text_alignment = text_alignment; }
    Gfx::TextAlignment text_alignment() const { return m_text_alignment; }
    void set_text(String text)
    {
        m_text = text;
        update_document();
    }
    StringView text() const { return m_text; }
    void set_font_style(String font_style)
    {
        m_font_style = font_style;
    }
    StringView font_style() const { return m_font_style; }
    StringView syntax_highlight() const
    {
        return m_syntax_highlight.map([](auto language) { return Syntax::language_to_string(language); }).value_or("none"sv);
    }
    void set_syntax_highlight(StringView language);

    // ^HighlighterClient
    virtual Vector<Syntax::TextDocumentSpan> const& spans() const override { return m_highlighting_document->spans(); }
    virtual void set_span_at_index(size_t index, Syntax::TextDocumentSpan span) override { m_highlighting_document->set_span_at_index(index, span); }
    virtual Vector<Syntax::TextDocumentFoldingRegion>& folding_regions() override { return m_highlighting_document->folding_regions(); }
    virtual Vector<Syntax::TextDocumentFoldingRegion> const& folding_regions() const override { return m_highlighting_document->folding_regions(); }
    virtual DeprecatedString highlighter_did_request_text() const override { return m_text.to_deprecated_string(); }
    virtual void highlighter_did_request_update() override { m_invalidated = true; }
    virtual Syntax::Document& highlighter_did_request_document() override { return m_highlighting_document; }
    virtual Syntax::TextPosition highlighter_did_request_cursor() const override { return {}; }
    virtual void highlighter_did_set_spans(Vector<Syntax::TextDocumentSpan> spans) override { m_highlighting_document->set_spans(Syntax::HighlighterClient::span_collection_index, spans); }
    virtual void highlighter_did_set_folding_regions(Vector<Syntax::TextDocumentFoldingRegion> regions) override { m_highlighting_document->set_folding_regions(regions); }

protected:
    void update_document();

    String m_text;
    // The font family, technically speaking.
    String m_font;
    int m_font_size { 18 };
    unsigned m_font_weight { Gfx::FontWeight::Regular };
    Gfx::TextAlignment m_text_alignment { Gfx::TextAlignment::CenterLeft };
    String m_font_style;

    // Just used to make HighlighterClient work.
    class Document : public Syntax::Document {
    public:
        Document() = default;
        virtual ~Document() = default;

        virtual Syntax::TextDocumentLine const& line(size_t line_index) const override { return m_lines[line_index]; }
        virtual Syntax::TextDocumentLine& line(size_t line_index) override { return m_lines[line_index]; }
        virtual void update_views(Badge<Syntax::TextDocumentLine>) override { }

        void add_line(String const& line) { m_lines.empend(*this, line.bytes_as_string_view()); }
        void clear() { m_lines.clear(); }
        ReadonlySpan<Syntax::TextDocumentLine> lines() const { return m_lines.span(); }

    private:
        Vector<Syntax::TextDocumentLine> m_lines;
    };

    NonnullRefPtr<Document> m_highlighting_document { make_ref_counted<Document>() };
    OwnPtr<Syntax::Highlighter> m_highlighter;
    Optional<Syntax::Language> m_syntax_highlight;
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

    void set_image_path(String image_path);
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
    ErrorOr<int> reload_image();

    Threading::MutexProtected<RefPtr<Gfx::Bitmap>> m_currently_loaded_image;
    NonnullRefPtr<GUI::Window> m_window;
    RefPtr<Threading::BackgroundAction<int>> m_image_load_action;
};
