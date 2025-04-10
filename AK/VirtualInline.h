/*
 * Copyright (c) 2025, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Assertions.h>
#include <AK/Format.h>
#include <AK/Platform.h>
#include <AK/RefCounted.h>
#include <AK/StdLibExtras.h>
#include <AK/Traits.h>
#include <AK/Types.h>

namespace AK {

// A class that stores a type one of its subtypes inline.
// Effectively a NonnullOwnPtr except without the pointer.
// Since it uses inline storage, the space has to be specified beforehand,
// and all subclasses that are to be used with this storage must fit the space.
// DO NOT use this type if you do not control EVERY subclass you want to put in here!
// Ideally, use it only with `final`-based sealed hierarchies within one application or library.
template<typename T, size_t Space>
class [[nodiscard]] VirtualInline {
public:
    using ElementType = T;
    static_assert(Space >= sizeof(T), "This VirtualInline can’t even fit the base class.");

    VirtualInline()
    requires(IsConstructible<T>)
    {
        new (m_storage.data()) T;
    }
    explicit VirtualInline(T const& value)
    requires(IsCopyConstructible<T>)
    {
        new (m_storage.data()) T(value);
    }
    explicit VirtualInline(T&& value)
    requires(IsMoveConstructible<T>)
    {
        new (m_storage.data()) T(forward<T>(value));
    }
    VirtualInline(VirtualInline&& other)
    {
        new (m_storage.data()) T(forward<T>(other.move_from()));
    }
    VirtualInline(VirtualInline const& other)
    {
        new (m_storage.data()) T(other.ref());
    }
    template<typename U>
    VirtualInline(VirtualInline<U, Space>&& other)
    requires(IsBaseOf<T, U> || IsBaseOf<U, T>)
        : VirtualInline(static_cast<T&&>(other.move_from()))
    {
    }
    template<typename U, size_t OtherSpace>
    VirtualInline(VirtualInline<U, OtherSpace> const& other)
    requires(IsBaseOf<T, U> || IsBaseOf<U, T>)
        : VirtualInline(static_cast<T const&>(other.ref()))
    {
    }
    ~VirtualInline() { clear(); }

    VirtualInline& operator=(VirtualInline const& other)
    {
        VirtualInline other_inline(other);
        swap(other_inline);
        return *this;
    }
    VirtualInline& operator=(VirtualInline&& other)
    {
        VirtualInline other_inline(forward<VirtualInline>(other));
        swap(other_inline);
        return *this;
    }
    template<typename U>
    VirtualInline& operator=(VirtualInline<U, Space> const& other)
    requires(IsBaseOf<T, U> || IsBaseOf<U, T>)
    {
        VirtualInline other_inline(other);
        swap(other_inline);
        return *this;
    }
    template<typename U>
    VirtualInline& operator=(VirtualInline<U, Space>&& other)
    requires(IsBaseOf<T, U> || IsBaseOf<U, T>)
    {
        VirtualInline other_inline(forward<VirtualInline<U, Space>>(other));
        swap(other_inline);
        return *this;
    }
    template<typename U>
    VirtualInline& operator=(U&& other)
    requires(IsMoveConstructible<U> && (IsBaseOf<T, U> || IsBaseOf<U, T>))
    {
        clear();
        new (m_storage.data()) U(forward<U>(other));
        return *this;
    }

    ALWAYS_INLINE RETURNS_NONNULL T* ptr() const { return bit_cast<T*>(m_storage.data()); }
    ALWAYS_INLINE T& ref() { return *bit_cast<T*>(m_storage.data()); }
    ALWAYS_INLINE T const& ref() const { return *bit_cast<T const*>(m_storage.data()); }
    ALWAYS_INLINE T&& move_from() { return move(*bit_cast<T*>(m_storage.data())); }

    ALWAYS_INLINE RETURNS_NONNULL T* operator->() const { return ptr(); }
    ALWAYS_INLINE T& operator*() const { return *ptr(); }
    ALWAYS_INLINE RETURNS_NONNULL operator T*() const { return ptr(); }

    void swap(VirtualInline& other)
    {
        AK::swap(m_storage, other.m_storage);
    }

    template<typename U>
    void swap(VirtualInline<U, Space>& other)
    {
        AK::swap(m_storage, other.storage(Badge<VirtualInline> {}));
    }

    template<typename U>
    Array<u8, Space>& storage(Badge<VirtualInline<U, Space>>) { return m_storage; }

private:
    void clear()
    {
        reinterpret_cast<T*>(m_storage.data())->~T();
        m_storage.fill(0);
    }

    alignas(T) Array<u8, Space> m_storage {};
};

template<typename T, size_t Space>
struct Traits<VirtualInline<T, Space>> : public DefaultTraits<VirtualInline<T, Space>> {
    using PeekType = T*;
    using ConstPeekType = T const*;
    static unsigned hash(VirtualInline<T, Space> const& p) { return ptr_hash(p.ptr()); }
    static bool equals(VirtualInline<T, Space> const& a, VirtualInline<T, Space> const& b) { return a.ptr() == b.ptr(); }
};

template<typename T, typename U, size_t Space>
inline void swap(VirtualInline<T, Space>& a, VirtualInline<U, Space>& b)
{
    a.swap(b);
}

template<Formattable T, size_t Space>
struct Formatter<VirtualInline<T, Space>> : Formatter<T> {
    ErrorOr<void> format(FormatBuilder& builder, VirtualInline<T, Space> const& value)
    {
        return Formatter<T>::format(builder, *value);
    }
};

template<typename T, size_t Space>
requires(!HasFormatter<T>)
struct Formatter<VirtualInline<T, Space>> : Formatter<T const*> {
    ErrorOr<void> format(FormatBuilder& builder, VirtualInline<T, Space> const& value)
    {
        return Formatter<T const*>::format(builder, value.ptr());
    }
};

}

#if USING_AK_GLOBALLY
using AK::VirtualInline;
#endif
