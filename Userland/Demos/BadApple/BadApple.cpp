/*
 * Copyright (c) 2018-2021, kleines Filmröllchen
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "AK/NonnullRefPtr.h"
#include "LibGfx/Forward.h"
#include "LibGfx/Painter.h"
#include <AK/Format.h>
#include <AK/RefPtr.h>
#include <LibCore/ElapsedTimer.h>
#include <LibCore/Event.h>
#include <LibCore/Object.h>
#include <LibGUI/Action.h>
#include <LibGUI/Application.h>
#include <LibGUI/Event.h>
#include <LibGUI/Frame.h>
#include <LibGUI/Icon.h>
#include <LibGUI/Label.h>
#include <LibGUI/Menu.h>
#include <LibGUI/Menubar.h>
#include <LibGUI/Painter.h>
#include <LibGUI/Widget.h>
#include <LibGUI/Window.h>
#include <LibGfx/Bitmap.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

constexpr int FRAME_RATE = 20;
String const BASE_PATH = "/res/badapple";
String const FRAMES_FOLDER = "frames";

class BadApple : public GUI::Frame {
    C_OBJECT(BadApple);

public:
    virtual ~BadApple() override { }

private:
    BadApple();
    virtual void paint_event(GUI::PaintEvent&) override;
    virtual void timer_event(Core::TimerEvent&) override;

    RefPtr<Gfx::Bitmap> frame;
    unsigned framenum { 1 };
};

BadApple::BadApple()
    : GUI::Frame()
{
    stop_timer();
    start_timer(1000. / static_cast<double>(FRAME_RATE));
}

void BadApple::paint_event(GUI::PaintEvent& event)
{
    if (frame.is_null())
        return;
    GUI::Painter painter(*this);
    painter.add_clip_rect(event.rect());
    painter.blit(Gfx::IntPoint(0, 0), *frame, frame->rect());
}

void BadApple::timer_event(Core::TimerEvent& evt)
{
    (void)evt;

    ++framenum;
    frame = Gfx::Bitmap::try_load_from_file(String::formatted("{}/{}/{:04d}.png", BASE_PATH, FRAMES_FOLDER, framenum));
    if (frame.is_null()) {
        warnln("Couldn't load frame {}", framenum);
        return;
    }
    update();
}

int main(int argc, char** argv)
{
    auto app = GUI::Application::construct(argc, argv);

    if (pledge("stdio recvfd sendfd rpath", nullptr) < 0) {
        perror("pledge");
        return 1;
    }

    if (unveil("/res", "r") < 0) {
        perror("unveil");
        return 1;
    }

    if (unveil(nullptr, nullptr) < 0) {
        perror("unveil");
        return 1;
    }

    NonnullRefPtr<GUI::Window> window = GUI::Window::construct();

    auto app_icon = GUI::Icon::default_icon("app-badapple");
    window->set_icon(app_icon.bitmap_for_size(16));

    // this order is important
    window->set_double_buffering_enabled(false);
    window->set_title("Bad Apple!!");
    window->set_resizable(false);
    window->resize(960, 720);
    window->show();
    window->set_minimum_size(960, 720);
    window->set_maximized(false);
    window->center_on_screen();

    BadApple& badapple = window->set_main_widget<BadApple>();
    (void)badapple;

    return app->exec();
}
