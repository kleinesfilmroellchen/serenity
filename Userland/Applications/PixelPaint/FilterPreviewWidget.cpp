/*
 * Copyright (c) 2022, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "FilterPreviewWidget.h"
#include "LibCore/EventLoop.h"
#include "LibGUI/Icon.h"
#include "LibGfx/Bitmap.h"
#include <LibGUI/Painter.h>
#include <LibGfx/Palette.h>
#include <LibGfx/Rect.h>

REGISTER_WIDGET(PixelPaint, FilterPreviewWidget);

namespace PixelPaint {

FilterPreviewWidget::FilterPreviewWidget()
    : m_progress_icon(GUI::Icon::default_icon("wait"sv))
{
}

FilterPreviewWidget::~FilterPreviewWidget()
{
}

void FilterPreviewWidget::set_bitmap(RefPtr<Gfx::Bitmap> const& bitmap)
{
    m_bitmap = bitmap;
    clear_filter();
}

FilterPreviewCommand::FilterPreviewCommand(NonnullRefPtr<FilterPreviewWidget> preview_widget, NonnullRefPtr<Filter> filter)
    : m_preview_widget(move(preview_widget))
    , m_filter(move(filter))
    , m_event_loop(Core::EventLoop::current())
{
}

void FilterPreviewCommand::execute()
{
    m_preview_widget->set_filtering_in_progress(true);
    m_filter->cancellation_requested = &m_cancelled;
    m_filter->apply(*m_preview_widget->m_filtered_bitmap, *m_preview_widget->m_bitmap);
    m_preview_widget->set_filtering_in_progress(false);
    m_event_loop.deferred_invoke([strong_this = NonnullRefPtr(*this)]() {
        (*const_cast<NonnullRefPtr<FilterPreviewCommand>*>(&strong_this))->m_preview_widget->repaint();
    });
}

void FilterPreviewWidget::set_filter(Filter* filter)
{
    if (filter) {
        set_filtering_in_progress(true);
        // When we get a new filter, remove the old filter.
        m_currently_computing_preview.with_locked([&, this](auto& preview) {
            if (preview) {
                preview->cancel();
                preview = nullptr;
            }
            auto new_preview = make_ref_counted<FilterPreviewCommand>(*this, *filter);
            MUST(ImageProcessor::the()->enqueue_command(new_preview));
            preview = new_preview;
        });
        repaint();
    } else {
        m_filtered_bitmap = m_bitmap->clone().release_value();
        set_filtering_in_progress(false);
        repaint();
    }
}

void FilterPreviewWidget::clear_filter()
{
    set_filter(nullptr);
}

void FilterPreviewWidget::set_filtering_in_progress(bool filtering_in_progress)
{
    m_filtering_in_progress = filtering_in_progress;

    // Whenever filtering gets set to "not running", cancel the currently computing filter preview.
    if (!m_filtering_in_progress) {
        m_currently_computing_preview.with_locked([](auto& preview) {
            if (preview) {
                preview->cancel();
                preview = nullptr;
            }
        });
    }
}

void FilterPreviewWidget::paint_event(GUI::PaintEvent& event)
{
    GUI::Painter painter(*this);
    painter.add_clip_rect(event.rect());
    auto preview_rect = event.rect();
    auto bitmap_rect = m_filtered_bitmap->rect();

    int scaled_width, scaled_height, dx = 0, dy = 0;
    if (preview_rect.height() > preview_rect.width()) {
        scaled_width = preview_rect.width();
        scaled_height = ((float)bitmap_rect.height() / bitmap_rect.width()) * scaled_width;
        dy = (preview_rect.height() - scaled_height) / 2;
    } else {
        scaled_height = preview_rect.height();
        scaled_width = ((float)bitmap_rect.width() / bitmap_rect.height()) * scaled_height;
        dx = (preview_rect.width() - scaled_width) / 2;
    }

    Gfx::IntRect scaled_rect(preview_rect.x() + dx, preview_rect.y() + dy, scaled_width, scaled_height);

    painter.draw_scaled_bitmap(scaled_rect, *m_filtered_bitmap, m_filtered_bitmap->rect());

    if (m_filtering_in_progress) {
        auto const& progress_image = *m_progress_icon.bitmap_for_size(min(preview_rect.width(), preview_rect.height()));
        painter.blit(scaled_rect.top_left().translated(preview_rect.width() / 2, preview_rect.height() / 2).translated(-progress_image.width() / 2, -progress_image.height() / 2),
            progress_image, progress_image.rect());
    }
}

}
