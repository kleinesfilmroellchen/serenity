/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "PresenterSettings.h"
#include <Applications/Fidewepre/PresenterWidget.h>
#include <LibConfig/Client.h>
#include <LibGUI/SpinBox.h>
#include <LibGUI/TextBox.h>

namespace Fidewepre {

FooterWidget::FooterWidget() = default;
PerformanceWidget::PerformanceWidget() = default;

ErrorOr<NonnullRefPtr<FooterWidget>> FooterWidget::create()
{
    auto widget = TRY(try_create());

    widget->m_override_footer = widget->find_descendant_of_type_named<GUI::CheckBox>("override_footer");
    widget->m_enable_footer = widget->find_descendant_of_type_named<GUI::CheckBox>("enable_footer");
    widget->m_footer_text = widget->find_descendant_of_type_named<GUI::TextEditor>("footer_text");
    widget->m_override_footer->on_checked = [=](auto) { widget->on_footer_settings_override_change(); widget->set_modified(true); };
    widget->m_enable_footer->on_checked = [=](auto) { widget->set_modified(true); };
    widget->m_footer_text->on_change = [=] { widget->set_modified(true); };

    // Our implementation resets the UI settings input widgets to the stored config values.
    widget->cancel_settings();
    widget->on_footer_settings_override_change();
    return widget;
}

void FooterWidget::apply_settings()
{
    Config::write_bool("Presenter"sv, "Footer"sv, "OverrideFooter"sv, m_override_footer->is_checked());
    Config::write_bool("Presenter"sv, "Footer"sv, "EnableFooter"sv, m_enable_footer->is_checked());
    Config::write_string("Presenter"sv, "Footer"sv, "FooterText"sv, m_footer_text->text());
}

// Enables or disables footer override settings input to make it clear when they have effect and when not.
void FooterWidget::on_footer_settings_override_change()
{
    auto is_overridden = m_override_footer->is_checked();
    m_enable_footer->set_enabled(is_overridden);
    m_footer_text->set_enabled(is_overridden);
}

void FooterWidget::cancel_settings()
{
    auto override_state = Config::read_bool("Presenter"sv, "Footer"sv, "OverrideFooter"sv, false);
    auto enable_state = Config::read_bool("Presenter"sv, "Footer"sv, "EnableFooter"sv, true);
    auto footer_text_state = Config::read_string("Presenter"sv, "Footer"sv, "FooterText"sv, "{presentation_title} - {slide_title}"sv);

    m_override_footer->set_checked(override_state);
    m_enable_footer->set_checked(enable_state);
    m_footer_text->set_text(footer_text_state);
}

ErrorOr<NonnullRefPtr<PerformanceWidget>> PerformanceWidget::create(NonnullRefPtr<PresenterWidget> presenter_widget)
{
    auto widget = TRY(try_create());
    widget->m_presenter_widget = move(presenter_widget);
    widget->m_prerender_count = widget->find_descendant_of_type_named<GUI::SpinBox>("prerender_count");
    widget->m_cache_size = widget->find_descendant_of_type_named<GUI::SpinBox>("cache_size");

    widget->m_cache_size->on_change = [=](auto) { widget->set_modified(true); };
    widget->m_prerender_count->on_change = [=](auto) { widget->set_modified(true); };

    widget->cancel_settings();
    return widget;
}

void PerformanceWidget::apply_settings()
{
    Config::write_u32("Presenter"sv, "Performance"sv, "PrerenderCount"sv, m_prerender_count->value());
    Config::write_u32("Presenter"sv, "Performance"sv, "CacheSize"sv, m_cache_size->value());

    if (auto presentation = m_presenter_widget->current_presentation(); presentation.has_value()) {
        presentation->config_u32_did_change("Presenter"sv, "Performance"sv, "PrerenderCount"sv, m_prerender_count->value());
        presentation->config_u32_did_change("Presenter"sv, "Performance"sv, "CacheSize"sv, m_cache_size->value());
    }
}

void PerformanceWidget::cancel_settings()
{
    auto prerender_count = Config::read_u32("Presenter"sv, "Performance"sv, "PrerenderCount"sv, 1);
    auto cache_size = Config::read_u32("Presenter"sv, "Performance"sv, "CacheSize"sv, DEFAULT_CACHE_SIZE);

    m_prerender_count->set_value(prerender_count);
    m_cache_size->set_value(cache_size);

    if (auto presentation = m_presenter_widget->current_presentation(); presentation.has_value()) {
        presentation->config_u32_did_change("Presenter"sv, "Performance"sv, "PrerenderCount"sv, m_prerender_count->value());
        presentation->config_u32_did_change("Presenter"sv, "Performance"sv, "CacheSize"sv, m_cache_size->value());
    }
}

}
