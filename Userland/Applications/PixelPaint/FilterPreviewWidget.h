/*
 * Copyright (c) 2022, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "Filters/Filter.h"
#include "ImageEditor.h"
#include "ImageProcessor.h"
#include "Layer.h"
#include <AK/NonnullRefPtr.h>
#include <LibCore/Forward.h>
#include <LibGUI/Frame.h>
#include <LibGUI/Icon.h>
#include <LibGfx/Bitmap.h>
#include <LibThreading/Mutex.h>

namespace PixelPaint {

class FilterPreviewCommand;

class FilterPreviewWidget final : public GUI::Frame {
    C_OBJECT(FilterPreviewWidget);

public:
    virtual ~FilterPreviewWidget() override;
    void set_bitmap(RefPtr<Gfx::Bitmap> const& bitmap);
    void set_filter(Filter* filter);
    void clear_filter();

    void set_filtering_in_progress(bool filtering_in_progress);

private:
    friend class FilterPreviewCommand;
    explicit FilterPreviewWidget();

    RefPtr<Gfx::Bitmap> m_bitmap;
    RefPtr<Gfx::Bitmap> m_filtered_bitmap;

    Threading::MutexProtected<RefPtr<FilterPreviewCommand>> m_currently_computing_preview;

    bool m_filtering_in_progress { false };
    GUI::Icon const m_progress_icon;

    virtual void paint_event(GUI::PaintEvent&) override;
};

class FilterPreviewCommand : public ImageProcessingCommand {
public:
    FilterPreviewCommand(NonnullRefPtr<FilterPreviewWidget>, NonnullRefPtr<Filter>);

    virtual ~FilterPreviewCommand() = default;
    virtual void execute() override;

private:
    NonnullRefPtr<FilterPreviewWidget> m_preview_widget;
    NonnullRefPtr<Filter> m_filter;
    Core::EventLoop& m_event_loop;
};

}
