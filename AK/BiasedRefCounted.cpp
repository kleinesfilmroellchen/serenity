/*
 * Copyright (c) 2024, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "BiasedRefCounted.h"

namespace AK::Detail {

static Singleton<QueueManagement> s_thread_queue_management;

QueueManagement& QueueManagement::the()
{
    return s_thread_queue_management;
}

static ShouldDestroy perform_merge(BiasedRefCountedBase const& object, int tid)
{
}

// Algorithm 4 Extra operations (Page 7)
// procedure Queue(obj)
ShouldDestroy QueueManagement::queue(BiasedRefCountedBase const& object)
{
    // owner_tid := obj.rcword.biased.tid
    auto owner_tid = object.m_biased.owner_tid;
    // Non-zero; otherwise we wouldn't even be calling this function.
    VERIFY(owner_tid != 0);

#ifdef KERNEL
    Kernel::SpinlockLocker locker(m_lock);
#else
    Threading::MutexLocker locker(m_lock);
#endif

    // "When a non-owner
    //  thread makes the shared counter of an object negative, it first
    //  checks whether the object’s owner thread is alive by looking-up
    //  the QueuedObjects structure — which implicitly records the live
    //  threads. If the owner thread is not alive, the non-owner thread
    //  merges the counters instead of queuing the object, and either deal-
    //  locates the object or unbiases it."
    if (!m_queued_objects.contains(owner_tid)) [[unlikely]]
        return perform_merge(object, owner_tid);

    // QueuedObjects[owner_tid].append(obj)
    auto& tid_objects = m_queued_objects.ensure(owner_tid);
    tid_objects.append(object);
    // end procedure
}

// Algorithm 4 Extra operations (Page 7)
// procedure ExplicitMerge
ShouldDestroy QueueManagement::explicit_merge()
{
#ifdef KERNEL
    Kernel::SpinlockLocker locker(m_lock);
#else
    Threading::MutexLocker locker(m_lock);
#endif

    // my_tid := GetThreadID()
    auto my_tid = BiasedRefCountedBase::get_internal_tid();

    // for all obj ∈ QueuedObjects[my_tid] do
    for (auto const& object : *m_queued_objects.get(my_tid)) {
        auto result = perform_merge(object, my_tid);
        if (result == ShouldDestroy::Yes) {
            // Must be done in the templated class since we can't call the destructor correctly here.
            TODO();
        }
        // end for
    }
    // end procedure
}

}
