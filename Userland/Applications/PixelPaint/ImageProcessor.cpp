/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ImageProcessor.h"
#include "LibThreading/BackgroundAction.h"
#include "LibThreading/Mutex.h"
#include "LibThreading/Thread.h"

namespace PixelPaint {

void ImageProcessingCommand::cancel()
{
    m_cancelled.store(true);
    auto processor = ImageProcessor::the();
    {
        Threading::MutexLocker locker(processor->m_wakeup_mutex);
        processor->m_wakeup_variable.signal();
    }
}

bool ImageProcessingCommand::is_cancelled() const
{
    return m_cancelled.load();
}

FilterApplicationCommand::FilterApplicationCommand(NonnullRefPtr<Filter> filter, NonnullRefPtr<Layer> target_layer)
    : m_filter(move(filter))
    , m_target_layer(move(target_layer))
{
}

void FilterApplicationCommand::execute()
{
    m_filter->cancellation_requested = &m_cancelled;
    m_filter->apply(m_target_layer->content_bitmap(), m_target_layer->content_bitmap());
    m_filter->m_editor->gui_event_loop().deferred_invoke([strong_this = NonnullRefPtr(*this)]() {
        // HACK: we can't tell strong_this to not be const
        (*const_cast<NonnullRefPtr<Layer>*>(&strong_this->m_target_layer))->did_modify_bitmap(strong_this->m_target_layer->rect());
        strong_this->m_filter->m_editor->did_complete_action(String::formatted("Filter {}", strong_this->m_filter->filter_name()));
    });
}

static Singleton<ImageProcessor> s_image_processor;

ImageProcessor::ImageProcessor()
    : m_command_queue(MUST(Queue::try_create()))
    , m_processor_thread(Threading::Thread::construct([this]() {
        processor_main();
        return 0;
    },
          "Image Processor Scheduler"sv))
    , m_wakeup_variable(m_wakeup_mutex)
{
}

ImageProcessor* ImageProcessor::the()
{
    return s_image_processor;
}

void ImageProcessor::processor_main()
{
    int worker_number = 0;
    while (true) {
        if (auto next_command = m_command_queue.try_dequeue(); !next_command.is_error()) {
            auto command = next_command.value();
            if (command->is_cancelled())
                continue;

            bool volatile worker_done = false;
            auto worker = Threading::Thread::construct([&]() {
                command->execute();

                worker_done = true;
                {
                    Threading::MutexLocker locker(m_wakeup_mutex);
                    m_wakeup_variable.signal();
                }
                return 0;
            },
                String::formatted("Image Processor Worker {}", worker_number++));
            worker->start();

            {
                Threading::MutexLocker locker { m_wakeup_mutex };
                m_wakeup_variable.wait_while([&]() { return !worker_done && !command->is_cancelled(); });
            }
            if (command->is_cancelled()) {
                auto result = worker->cancel();
                // Since we know the tid is valid, ESRCH can only happen if the thread already exited.
                VERIFY(!result.is_error() || result.error() == ESRCH);
            } else {
                auto result = worker->join();
                // Same as above, here it is actually very likely.
                VERIFY(!result.is_error() || result.error() == ESRCH);
            }
        } else {
            Threading::MutexLocker locker { m_wakeup_mutex };
            m_wakeup_variable.wait_while([this]() { return m_command_queue.weak_used() == 0; });
        }
    }
}

ErrorOr<void> ImageProcessor::enqueue_command(NonnullRefPtr<ImageProcessingCommand> command)
{
    if (auto queue_status = m_command_queue.try_enqueue(move(command)); queue_status.is_error())
        return ENOSPC;

    if (!m_processor_thread->is_started()) {
        m_processor_thread->start();
        m_processor_thread->detach();
    }

    m_wakeup_mutex.lock();
    m_wakeup_variable.signal();
    m_wakeup_mutex.unlock();

    return {};
}

}
