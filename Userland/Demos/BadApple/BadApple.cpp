/*
 * Copyright (c) 2018-2021, kleines Filmröllchen
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Format.h>
#include <AK/NonnullRefPtr.h>
#include <AK/Queue.h>
#include <AK/RefPtr.h>
#include <LibAudio/Loader.h>
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
#include <LibGfx/Forward.h>
#include <LibGfx/Painter.h>
#include <LibThreading/BackgroundAction.h>
#include <LibThreading/Mutex.h>
#include <LibThreading/Thread.h>
#include <sched.h>
#include <serenity.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
extern char** environ;

constexpr int FRAME_RATE = 5;
String const BASE_PATH = "/res/badapple";
String const FRAMES_FOLDER = "frames";
String const AUDIO_FILE = "bad_apple.flac";
constexpr int FRAME_BUFFER = 60;

class BadApple : public GUI::Frame {
    C_OBJECT(BadApple);

public:
    virtual ~BadApple() override { }

private:
    BadApple();
    virtual void paint_event(GUI::PaintEvent&) override;
    virtual void timer_event(Core::TimerEvent&) override;

    void start_audio();

    RefPtr<Gfx::Bitmap> m_frame;
    int m_framenum = { 0 };

    Queue<RefPtr<Gfx::Bitmap>> m_frame_queue;
    Threading::Mutex m_queue_mutex;
    RefPtr<Audio::Loader> m_loader;

    bool m_waiting_for_audio { false };
};

BadApple::BadApple()
    : GUI::Frame()
{
    Threading::BackgroundAction<bool>::create([this](auto&) {
        int last_loaded_frame { 0 };
        while (true) {
            while (m_framenum <= last_loaded_frame - FRAME_BUFFER)
                // dbgln("frame {} playing, loaded to {}, buffer {}", m_framenum, last_loaded_frame, last_loaded_frame - FRAME_BUFFER);
                sched_yield();
            ++last_loaded_frame;
            auto new_frame = Gfx::Bitmap::try_load_from_file(String::formatted("{}/{}/{:04d}.png", BASE_PATH, FRAMES_FOLDER, last_loaded_frame));
            if (new_frame.is_null()) {
                warnln("Couldn't load frame {}", last_loaded_frame);
                return false;
            }
            Threading::MutexLocker lock(m_queue_mutex);
            m_frame_queue.enqueue(new_frame);
            lock.unlock();
        }
        return true;
    });

    stop_timer();
    start_timer(1000. / static_cast<double>(FRAME_RATE));
}

void BadApple::paint_event(GUI::PaintEvent& event)
{
    if (m_frame.is_null())
        return;
    GUI::Painter painter(*this);
    painter.add_clip_rect(event.rect());
    painter.blit(Gfx::IntPoint(0, 0), *m_frame, m_frame->rect());
}

void BadApple::timer_event(Core::TimerEvent& evt)
{
    if (m_waiting_for_audio) {
        stop_timer();
        start_timer(1000. / static_cast<double>(FRAME_RATE));
    }
    (void)evt;

    Threading::MutexLocker lock(m_queue_mutex);
    if (!m_frame_queue.is_empty()) {
        ++m_framenum;
        m_frame = m_frame_queue.dequeue();
    }
    lock.unlock();

    if (m_framenum == 1)
        start_audio();

    update();
}

void BadApple::start_audio()
{
    dbgln("Starting audio playback...");
    String filename = String::formatted("{}/{}", BASE_PATH, AUDIO_FILE);
    __pid_t pid;
    char const* argv[] = { "/bin/aplay", filename.characters(), nullptr };
    posix_spawn(&pid, "/bin/aplay", nullptr, nullptr, const_cast<char**>(argv), environ);
    dbgln("spawned aplay with pid {}", pid);

    // Restart drawing stuff in 5s
    m_waiting_for_audio = true;
    stop_timer();
    start_timer(800);

    /*
    m_loader = Audio::Loader::create();
    if (m_loader->has_error()) {
        warnln("Couldn't load audio file: {}", m_loader->error_string());
        return;
    }

    Threading::BackgroundAction<bool>::create([this](auto&) {
        auto audio_connection = Audio::ClientConnection::construct();
        auto resampler = Audio::ResampleHelper<double>(m_loader->sample_rate(), audio_connection->get_sample_rate());
        while (m_loader->loaded_samples() < m_loader->total_samples()) {
            auto samples = m_loader->get_more_samples();
            if (m_loader->has_error()) {
                warnln("Error while loading audio: {}", m_loader->error_string());
                return false;
            }
            samples = Audio::resample_buffer(resampler, *samples);
            audio_connection->enqueue(*samples);
        }

        return true;
    });*/
}

int main(int argc, char** argv)
{
    auto app = GUI::Application::construct(argc, argv);

    if (pledge("stdio recvfd sendfd rpath thread proc exec", nullptr) < 0) {
        perror("pledge");
        return 1;
    }

    if (unveil("/res", "r") < 0) {
        perror("unveil");
        return 1;
    }

    if (unveil("/bin/aplay", "rwx") < 0) {
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
