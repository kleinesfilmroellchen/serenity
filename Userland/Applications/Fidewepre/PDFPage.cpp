/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "PDFPage.h"
#include "SlideObject.h"
#include <LibCore/File.h>
#include <LibGUI/Object.h>
#include <LibGfx/Painter.h>
#include <LibPDF/Document.h>
#include <LibPDF/Error.h>
#include <LibPDF/Renderer.h>

static String format_pdf_errors(PDF::Errors errors)
{
    StringBuilder builder;
    for (auto const& error : errors.errors()) {
        builder.appendff("{}\n", error.message());
    }
    return MUST(builder.to_string());
}

PDFPage::PDFPage(NonnullRefPtr<GUI::Window> window, String presentation_path)
    : m_presentation_path(move(presentation_path))
    , m_window(move(window))
{
    REGISTER_STRING_PROPERTY("path", pdf_path, set_pdf_path);
    REGISTER_INT_PROPERTY("page", get_page_number, set_page)
}

void PDFPage::paint(Gfx::Painter& painter, Gfx::FloatSize display_scale) const
{
    if (m_pdf_document.is_locked())
        return;

    auto maybe_render_error = m_pdf_document.with_locked([&](auto document) -> PDF::PDFErrorsOr<void> {
        if (!document)
            return {};

        auto const transformed_bounding_box = this->transformed_bounding_box(painter.clip_rect(), display_scale);

        auto page_index = m_page_index;
        if (document->get_page_count() <= page_index) {
            // Empty document, nothing to draw.
            if (document->get_page_count() == 0)
                return {};
            dbgln("Document has {} pages but page {} was requested, using last page instead.", document->get_page_count(), m_page_index);
            page_index = document->get_page_count() - 1;
        }
        auto page = TRY(document->get_page(page_index));

        Gfx::FloatSize original_size { page.media_box.width(), page.media_box.height() };
        auto page_aspect_ratio = original_size.aspect_ratio();

        auto pdf_box = transformed_bounding_box;
        auto width_corresponding_to_height = pdf_box.height() * page_aspect_ratio;
        auto direction_to_preserve_for_fit = width_corresponding_to_height > pdf_box.width() ? Orientation::Horizontal : Orientation::Vertical;
        pdf_box.set_size(pdf_box.size().match_aspect_ratio(page_aspect_ratio, direction_to_preserve_for_fit));
        pdf_box = pdf_box.centered_within(transformed_bounding_box);

        auto maybe_rendered_page = Gfx::Bitmap::create(Gfx::BitmapFormat::BGRA8888, original_size.to_rounded<int>());
        if (maybe_rendered_page.is_error())
            return PDF::Error(maybe_rendered_page.release_error());
        auto rendered_page = maybe_rendered_page.value();
        TRY(PDF::Renderer::render(*document, page, rendered_page,
            PDF::RenderingPreferences {
                .show_clipping_paths = false,
                .show_images = true,
            }));

        auto original_clip_rect = painter.clip_rect();
        painter.clear_clip_rect();
        painter.add_clip_rect(pdf_box);

        painter.draw_scaled_bitmap(pdf_box, *rendered_page, rendered_page->rect(), 1.0f, Gfx::Painter::ScalingMode::BilinearBlend);

        painter.clear_clip_rect();
        painter.add_clip_rect(original_clip_rect);

        return {};
    });

    if (maybe_render_error.is_error())
        dbgln("Error rendering PDF: {}", format_pdf_errors(maybe_render_error.release_error()));
}

void PDFPage::set_pdf_path(String pdf_path)
{
    m_pdf_path = LexicalPath { pdf_path.to_deprecated_string() };
    execute_rerender();
    m_invalidated = true;
}

void PDFPage::set_page(u32 page)
{
    if (page == 0)
        return;
    m_page_index = page - 1;
    execute_rerender();
    m_invalidated = true;
}

// HACK: We can't store a runtime text in an error.
static String last_pdf_error;

void PDFPage::execute_rerender() const
{
    if (m_render_action)
        m_render_action->cancel();

    auto render_action = Threading::BackgroundAction<int>::try_create(
        [this](auto&) -> ErrorOr<int> {
            // FIXME: shouldn't be necessary
            Core::EventLoop loop;

            auto result = this->reload_document();
            if (result.is_error()) {
                last_pdf_error = format_pdf_errors(result.release_error());
                return Error::from_string_view(last_pdf_error.bytes_as_string_view());
            }
            return { 0 };
        },
        [this](auto) -> ErrorOr<void> {
            m_invalidated = true;
            m_window->update();
            return {};
        },
        [this](auto error) {
            if (error.code() == ECANCELED)
                return;
            if (auto text = String::formatted("Loading PDF {} failed: {}", m_pdf_path, error); !text.is_error())
                GUI::MessageBox::show_error(m_window, text.release_value().bytes_as_string_view());
        });

    if (render_action.is_error()) {
        auto result = this->reload_document();
        if (result.is_error()) {
            if (auto text = String::formatted("Loading PDF {} failed: {}", m_pdf_path, format_pdf_errors(result.release_error())); !text.is_error())
                GUI::MessageBox::show_error(m_window, text.release_value().bytes_as_string_view());
            else
                dbgln("{}", format_pdf_errors(result.release_error()));
        }
    } else {
        m_render_action = render_action.release_value();
    }
}

PDF::PDFErrorOr<void> PDFPage::reload_document() const
{
    auto pdf_path = LexicalPath::absolute_path(LexicalPath { m_presentation_path.to_deprecated_string() }.parent().string(),
        m_pdf_path.string());
    auto file = TRY(Core::File::open(pdf_path, Core::File::OpenMode::Read));

    m_document_bytes = TRY(file->read_until_eof());
    auto document = TRY(PDF::Document::create(m_document_bytes));
    TRY(document->initialize());
    m_pdf_document.with_locked([&](auto& pdf_document) {
        pdf_document = move(document);
    });
    return {};
}
