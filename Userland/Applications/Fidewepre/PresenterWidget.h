/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "Presentation.h"
#include <Kernel/API/KeyCode.h>
#include <LibGUI/Action.h>
#include <LibGUI/Event.h>
#include <LibGUI/Progressbar.h>
#include <LibGUI/SettingsWindow.h>
#include <LibGUI/UIDimensions.h>
#include <LibGUI/Widget.h>
#include <LibThreading/BackgroundAction.h>
#include <LibThreading/ConditionVariable.h>

// Title, Author
constexpr StringView const title_template = "{} ({}) — Presenter"sv;

enum class SlideProgress {
    Starting,
    Rendering,
    Writing,
    Error,
};

struct ExportState {
    unsigned current_slide { 0 };
    SlideProgress progress { SlideProgress::Starting };
};

class PresenterWidget : public GUI::Widget {
    C_OBJECT(PresenterWidget);

public:
    PresenterWidget();
    ErrorOr<void> initialize_menubar();

    virtual ~PresenterWidget() override = default;

    // Errors that happen here are directly displayed to the user.
    void set_file(StringView file_name);

    Optional<Presentation&> current_presentation() { return m_current_presentation ? *m_current_presentation.ptr() : Optional<Presentation&> {}; }

protected:
    virtual void paint_event(GUI::PaintEvent&) override;
    virtual void keydown_event(GUI::KeyEvent&) override;
    virtual void drag_enter_event(GUI::DragEvent&) override;
    virtual void drop_event(GUI::DropEvent&) override;

private:
    void go_to_slide_from_key_sequence();
    void update_slides_actions();
    void on_export_slides_action();
    void update_export_status();

    OwnPtr<Presentation> m_current_presentation;
    RefPtr<GUI::Action> m_next_slide_action;
    RefPtr<GUI::Action> m_previous_slide_action;
    RefPtr<GUI::Action> m_full_screen_action;
    RefPtr<GUI::Action> m_present_from_first_slide_action;
    RefPtr<GUI::SettingsWindow> m_settings_window;

    RefPtr<Core::Timer> m_fullscreen_hide_cursor_timer;
    Vector<KeyCode, 3> m_current_key_sequence;

    RefPtr<Threading::BackgroundAction<int>> m_exporter;
    Threading::MutexProtected<ExportState> m_export_state {};
    Threading::ConditionVariable m_export_state_updated { m_export_state };
    Atomic<bool> m_export_done { false };
    RefPtr<GUI::Window> m_progress_window {};
    RefPtr<GUI::Progressbar> m_progress_bar;

    Threading::Mutex m_presentation_state;
    Threading::ConditionVariable m_presentation_state_updated { m_presentation_state };
    RefPtr<Threading::Thread> m_slide_predrawer {};
};
