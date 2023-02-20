/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Assertions.h>
#include <AK/Atomic.h>
#include <AK/Noncopyable.h>
#include <AK/Types.h>
#include <pthread.h>

namespace Threading {

class Mutex {
    AK_MAKE_NONCOPYABLE(Mutex);
    AK_MAKE_NONMOVABLE(Mutex);
    friend class ConditionVariable;

public:
    Mutex()
        : m_lock_count(0)
    {
#ifndef AK_OS_SERENITY
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&m_mutex, &attr);
#endif
    }
    ~Mutex()
    {
        VERIFY(m_lock_count == 0);
        // FIXME: pthread_mutex_destroy() is not implemented.
    }

    void lock();
    void unlock();

    bool is_locked() const { return m_lock_count.load() > 0; }

private:
#ifdef AK_OS_SERENITY
    pthread_mutex_t m_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#else
    pthread_mutex_t m_mutex;
#endif
    Atomic<unsigned, AK::memory_order_acq_rel> m_lock_count { 0 };
};

class MutexLocker {
    AK_MAKE_NONCOPYABLE(MutexLocker);
    AK_MAKE_NONMOVABLE(MutexLocker);

public:
    ALWAYS_INLINE explicit MutexLocker(Mutex& mutex)
        : m_mutex(mutex)
    {
        lock();
    }
    ALWAYS_INLINE ~MutexLocker()
    {
        unlock();
    }
    ALWAYS_INLINE void unlock() { m_mutex.unlock(); }
    ALWAYS_INLINE void lock() { m_mutex.lock(); }

private:
    Mutex& m_mutex;
};

ALWAYS_INLINE void Mutex::lock()
{
    pthread_mutex_lock(&m_mutex);
    m_lock_count.fetch_add(1, AK::memory_order_acquire);
}

ALWAYS_INLINE void Mutex::unlock()
{
    VERIFY(m_lock_count > 0);
    m_lock_count.fetch_sub(1, AK::memory_order_release);
    pthread_mutex_unlock(&m_mutex);
}

}
