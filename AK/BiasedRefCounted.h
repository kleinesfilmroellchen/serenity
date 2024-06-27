/*
 * Copyright (c) 2024, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Assertions.h>
#include <AK/Checked.h>
#include <AK/HashMap.h>
#include <AK/Noncopyable.h>
#include <AK/Platform.h>
#include <AK/ScopeGuard.h>
#include <AK/Singleton.h>
#include <AK/StdLibExtraDetails.h>
#include <AK/Types.h>
#include <unistd.h>

#ifdef KERNEL
#    include <Kernel/Locking/Spinlock.h>
#else
#    include <LibThreading/Mutex.h>
#endif

namespace AK {

class BiasedRefCountedBase;

namespace Detail {

enum class ShouldDestroy : bool {
    Yes,
    No,
};

// A queue for in-risk-of-leaking objects.
// Accessing this queue is relatively expensive, but it is rarely needed.
class QueueManagement {
public:
    static QueueManagement& the();

    ShouldDestroy queue(BiasedRefCountedBase const& object);
    ShouldDestroy explicit_merge();

#ifdef KERNEL
    // None (or the lowest lock rank) is actually correct to use here!
    // While taking this lock we never take any other locks.
    using Lock = Kernel::RecursiveSpinLock<LockRank::None>;
#else
    using Lock = Threading::Mutex;
#endif

    // MutexProtected and SpinlockProtected have differing APIs, so we don't use them here directly.
    using Queue = HashMap<int, Vector<BiasedRefCountedBase const&, 8>>;

private:
    Queue m_queued_objects;
    Lock m_lock;
};

}

// A biased reference counting (BRC) implementation for fast, thread-safe reference counting.
// BRC was introduced by Choi, Shull, and Torrellas in https://dl.acm.org/doi/pdf/10.1145/3243176.3243195
// It is used in CPython for reducing the need for a GIL; see PEP 703.
// BRC implements the reference count in two parts: a "biased" counter that only the owner thread uses,
// and a "shared" counter that all other threads access.
// The biased counter is non-atomic and fast to access, while the shared counter needs slow CAS accesses.
// Since most accesses come from an owner thread, and actually sharing a RefPtr across threads is relatively rare,
// this provides an immense speedup over AtomicRefCounted without slowing down single-threaded use cases.
// This implementation of BRC deviates from the paper in that it doesn't implement weak references.
class BiasedRefCountedBase {
    AK_MAKE_NONCOPYABLE(BiasedRefCountedBase);
    AK_MAKE_NONMOVABLE(BiasedRefCountedBase);

public:
    using RefCountType = i32;

    // Algorithm 2 Increment operation (Page 6)
    // procedure Increment(obj)
    ALWAYS_INLINE void ref() const
    {
        // owner_tid := obj.rcword.biased.tid
        auto owner_tid = m_biased.owner_tid;
        // my_tid := GetThreadID()
        auto my_tid = get_internal_tid();
        // if owner_tid == my_tid then
        if (owner_tid == my_tid)
            // FastIncrement (obj)
            fast_increment();
        // else
        else
            // SlowIncrement (obj)
            slow_increment();
        // end if
        // end procedure
    }

    [[nodiscard]] bool try_ref() const
    {
        TODO();
    }

protected:
    friend class Detail::QueueManagement;

    BiasedRefCountedBase() = default;
    ~BiasedRefCountedBase() { TODO(); }

    // We fold the real TID into itself to get a TID that fits in the relatively small amount of space available.
    // Within a single program, this aliasing of distinct TIDs should not become a problem.
    static ALWAYS_INLINE int get_internal_tid()
    {
        auto real_tid = gettid();
        return static_cast<int>(((real_tid >> tid_bits) ^ real_tid) & tid_max);
    }

    // Algorithm 2 Increment operation (Page 6)
    // procedure FastIncrement(obj)
    ALWAYS_INLINE void fast_increment() const
    {
        // NOTE: Non-standard checks.
        VERIFY(static_cast<size_t>(m_biased.biased_count) + 1 <= biased_max);
        // obj.rcword.biased.counter += 1
        m_biased.biased_count += 1;
        // end procedure
    }

    // Algorithm 2 Increment operation (Page 6)
    // procedure SlowIncrement(obj)
    ALWAYS_INLINE void slow_increment() const
    {
        // (old := obj.rcword.shared)
        Shared old_shared = bit_cast<Shared>(m_shared);
        Shared new_shared;
        // do
        do {
            // old := obj.rcword.shared
            // NOTE: Provided to us by the CAS operation.
            // new := old
            new_shared = old_shared;
            // NOTE: Non-standard checks.
            VERIFY(static_cast<size_t>(new_shared.shared_count) + 1 <= shared_max);
            // new.counter += 1
            new_shared.shared_count += 1;
            // while !CAS(&obj.rcword.shared, old, new)
        } while (!m_shared.compare_exchange_strong(*bit_cast<u32*>(&old_shared), bit_cast<u32>(new_shared), AK::memory_order_acquire));
        // end procedure
    }

    // Algorithm 3 Decrement operation (Page 7)
    // procedure Decrement(obj)
    ALWAYS_INLINE Detail::ShouldDestroy deref_base() const
    {
        // owner_tid := obj.rcword.biased.tid
        auto owner_tid = m_biased.owner_tid;
        // my_tid := GetThreadID()
        auto my_tid = get_internal_tid();
        // if owner_tid == my_tid then
        if (owner_tid == my_tid)
            // FastDecrement(obj)
            return fast_decrement();
        // else
        else
            // SlowDecrement(obj)
            return slow_decrement();
        // end if
        // end procedure
    }

    // Algorithm 3 Decrement operation (Page 7)
    // procedure FastDecrement(obj)
    ALWAYS_INLINE Detail::ShouldDestroy fast_decrement() const
    {
        // obj.rcword.biased.counter −= 1
        m_biased.biased_count -= 1;
        // if obj.rcword.biased.counter > 0 then
        if (m_biased.biased_count > 0)
            // return
            return Detail::ShouldDestroy::No;
        // end if
        // (old := obj.rcword.shared)
        Shared old_shared = bit_cast<Shared>(m_shared);
        Shared new_shared;
        // do ▷ biased counter is zero
        do {
            // old := obj.rcword.shared
            // NOTE: Provided to us by the CAS operation.
            // new := old
            new_shared = old_shared;
            // new.merged :=True
            new_shared.is_merged = true;
            // while !CAS (&obj.rcword.shared, old, new)
        } while (!m_shared.compare_exchange_strong(*bit_cast<u32*>(&old_shared), bit_cast<u32>(new_shared), AK::memory_order_acquire));
        // if new.counter == 0 then
        if (new_shared.shared_count == 0)
            // Deallocate(obj)
            return Detail::ShouldDestroy::Yes;
        // else
        else
            // obj.rcword.biased.tid := 0
            m_biased.owner_tid = 0;
        // end if
        // end procedure
        return Detail::ShouldDestroy::No;
    }

    // Algorithm 3 Decrement operation (Page 7)
    // procedure SlowDecrement(obj)
    ALWAYS_INLINE Detail::ShouldDestroy slow_decrement() const
    {
        // (old := obj.rcword.shared)
        Shared old_shared = bit_cast<Shared>(m_shared);
        Shared new_shared;
        // do
        do {
            // old := obj.rcword.shared
            // NOTE: Provided to us by the CAS operation.
            // new := old
            new_shared = old_shared;
            // new.counter −= 1
            new_shared.shared_count -= 1;
            // if new.counter < 0 then
            if (new_shared.shared_count < 0)
                // new.queued :=True
                new_shared.is_queued = true;
            // end if
            // while !CAS (&obj.rcword.shared, old, new)
        } while (!m_shared.compare_exchange_strong(*bit_cast<u32*>(&old_shared), bit_cast<u32>(new_shared), AK::memory_order_acquire));

        // if old.queued != new.queued then ▷ queued has been *first* set in this invocation
        if (old_shared.is_queued != new_shared.is_queued)
            // Queue(obj)
            Detail::QueueManagement::the().queue(*this);
        // else if new.merged == True and new.counter == 0 then
        else if (new_shared.is_merged && new_shared.shared_count == 0)
            // Deallocate(obj)
            return Detail::ShouldDestroy::Yes;
        // end if
        // end procedure
        return Detail::ShouldDestroy::No;
    }

    // Number of bits allocated for biased refcount.
    static constexpr size_t bias_bits = 16;
    static constexpr size_t biased_max = (1 << (bias_bits - 1)) - 1;
    // Number of bits allocated for thread ID.
    // See get_internal_tid().
    static constexpr size_t tid_bits = sizeof(RefCountType) * 8 - bias_bits;
    static constexpr size_t tid_max = (1 << (tid_bits - 1)) - 1;
    union Biased {
        struct {
            int owner_tid : tid_bits;
            RefCountType biased_count : bias_bits;
        };
        u32 complete;
    };
    static_assert(AssertSize<Biased, sizeof(RefCountType)>());

    static constexpr size_t shared_bits = sizeof(RefCountType) * 8 - 2;
    static constexpr size_t shared_max = (1 << (shared_bits - 1)) - 1;
    struct Shared {
        RefCountType shared_count : shared_bits;
        bool is_queued : 1;
        bool is_merged : 1;
    };
    static_assert(AssertSize<Shared, sizeof(RefCountType)>());

    mutable Biased m_biased;
    // Need to use u32 since our concepts prevent Shared from being used directly.
    // With our assertion above and bit_cast, we access the data in a structured way.
    mutable Atomic<u32> m_shared;
};

template<typename T>
class BiasedRefCounted : public BiasedRefCountedBase {
public:
    bool unref() const
    {
        auto* that = const_cast<T*>(static_cast<T const*>(this));

        auto should_destroy = deref_base();
        if (should_destroy == Detail::ShouldDestroy::Yes) {
            if constexpr (requires { that->will_be_destroyed(); })
                that->will_be_destroyed();
            delete static_cast<T const*>(this);
            return true;
        }
        return false;
    }
};

}

#if USING_AK_GLOBALLY
using AK::BiasedRefCounted;
using AK::BiasedRefCountedBase;
#endif
