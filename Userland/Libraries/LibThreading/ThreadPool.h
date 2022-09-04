/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/AtomicRefCounted.h>
#include <AK/FixedArray.h>
#include <AK/Function.h>
#include <AK/NonnullRefPtr.h>
#include <AK/Optional.h>
#include <AK/Queue.h>
#include <AK/RefPtr.h>
#include <AK/WeakPtr.h>
#include <LibThreading/ConditionVariable.h>
#include <LibThreading/Mutex.h>
#include <LibThreading/MutexProtected.h>
#include <LibThreading/Thread.h>

namespace Threading {

template<typename ResultT>
class ThreadPool;

template<typename ResultT>
class Future final : public AtomicRefCounted<Future<ResultT>> {
    friend class ThreadPool<ResultT>;

public:
    bool is_completed() { return m_result.has_value(); }
    void wait_for_completion() { }
    Optional<ResultT> result() { return m_result; }

    operator ResultT() const { return m_result.release_value(); }

private:
    Function<ResultT(void)> m_function;
    Optional<ResultT> m_result;
};

template<typename ResultT>
class ThreadPool : public AtomicRefCounted<ThreadPool<ResultT>>
    , public Weakable<ThreadPool<ResultT>> {
public:
    static ErrorOr<ThreadPool<ResultT>> try_create(unsigned pool_size)
    {
        auto threads = MUST(FixedArray<RefPtr<Thread>>::try_create(pool_size));
        // We need to first create the pool itself, as the thread's main method depends on the pool's existence
        auto pool = TRY(AK::adopt_nonnull_ref_or_enomem(new (nothrow) ThreadPool(threads)));
        unsigned worker_index = 0;
        for (auto& thread : pool->m_threads) {
            thread = TRY(Thread::try_create(pool_thread_main(WeakPtr(pool)), String::formatted("ThreadPool worker {}", worker_index++)));
            thread->start();
        }

        return pool;
    }

    ErrorOr<NonnullRefPtr<Future<ResultT>>> execute(Function<ResultT(void)> function)
    {
        auto future = TRY(try_make_ref_counted<Future<ResultT>>(function));
        m_queue.with_locked([&](auto& queue) {
            queue.enqueue(future);
        });
        {
            MutexLocker locker(m_worker_mutex);
            m_worker_wake.signal();
        }

        return future;
    }

    ~ThreadPool()
    {
        // When we arrive, here, our refcount is 0, so under no circumstances must anyone create a refptr to us.
        // However, the weak pointers that the worker threads have are normally only invalidated once the Weakable destructor runs, which happens after this destructor.
        // So we need to invalidate the weak pointers immediately.
        Weakable<ThreadPool>::revoke_weak_ptrs();
        // Signal all threads to exit as soon as possible.
        m_exit_requested.store(true);
        {
            MutexLocker locker(m_worker_mutex);
            m_worker_wake.broadcast();
        }

        for (auto& thread : m_threads) {
            auto result = thread->join();
            VERIFY(!result.is_error() || result.error() == ESRCH);
        }
    }

private:
    ThreadPool(FixedArray<RefPtr<Thread>> threads)
        : m_threads(move(threads))
    {
    }

    static intptr_t pool_thread_main(WeakPtr<ThreadPool<ResultT>> parent)
    {
        // If the parent dies faster than we
        auto worker_mutex = parent->m_worker_mutex;
        auto worker_wake = parent->m_worker_wake;
        // We must NEVER EVER create a RefPtr here (with strong_ref), that causes a TOCTOU crash when interleaved with ThreadPool destruction;
        // making us strongly reference a ThreadPool with ref count 0 because we believe the weak ptr is still valid.
        // See also the notes for the ThreadPool destructor above.
        while (auto parent_pointer = parent.ptr()) {
            if (parent_pointer == nullptr || parent_pointer->ref_count() == 0 || parent_pointer->m_exit_requested)
                return 0;

            // If exit was requested while we did work, we MUST NOT wait on the condition variable anymore, as we would not wake up and everything would deadlock.
            if (self.m_exit_requested)
                return 0;

            // At this point, we release the strong reference to the pool while we sleep.
            delete self;
            // This allows others to destruct the pool, which will prompt us to exit.
            {
                MutexLocker locker(worker_mutex);
                // NOTE: Destruction cannot happen while the condition body runs.
                //       The pool only gets destructed after all the threads have exited, and we're clearly still running.
                //       For this reason, it is safe to assume that the parent remains existent during the callback.
                worker_wake.wait_while([&]() {
                    return !parent.is_null() && !parent->m_exit_requested && parent->m_queue.with_locked([&](auto& queue) { queue.is_empty(); });
                });
            }
        }
        return 0;
    }

    // Never take this lock...
    MutexProtected<Queue<NonnullRefPtr<Future<ResultT>>>> m_queue;

    // before this lock. The workers need to briefly lock the queue within the condvar check.
    SharedMutex m_worker_mutex;
    SharedConditionVariable m_worker_wake;

    Atomic<bool> m_exit_requested { false };

    FixedArray<RefPtr<Thread>> m_threads;
};

}
