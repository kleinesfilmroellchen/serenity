/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibGUI/CheckBox.h>
#include <LibGUI/SettingsWindow.h>

class PresenterWidget;

namespace Fidewepre {

class FooterWidget : public GUI::SettingsWindow::Tab {
    C_OBJECT_ABSTRACT(FooterWidget)
public:
    virtual void apply_settings() override;
    virtual void cancel_settings() override;

    static ErrorOr<NonnullRefPtr<FooterWidget>> create();

private:
    static ErrorOr<NonnullRefPtr<FooterWidget>> try_create();
    FooterWidget();

    void on_footer_settings_override_change();

    RefPtr<GUI::TextEditor> m_footer_text;
    RefPtr<GUI::CheckBox> m_enable_footer;
    RefPtr<GUI::CheckBox> m_override_footer;
};

class PerformanceWidget : public GUI::SettingsWindow::Tab {
    C_OBJECT_ABSTRACT(PerformanceWidget)
public:
    virtual void apply_settings() override;
    virtual void cancel_settings() override;

    static ErrorOr<NonnullRefPtr<PerformanceWidget>> create(NonnullRefPtr<PresenterWidget> presenter_widget);

private:
    static ErrorOr<NonnullRefPtr<PerformanceWidget>> try_create();
    PerformanceWidget();

    RefPtr<GUI::SpinBox> m_cache_size;
    RefPtr<GUI::SpinBox> m_prerender_count;
    RefPtr<PresenterWidget> m_presenter_widget;
};

}
