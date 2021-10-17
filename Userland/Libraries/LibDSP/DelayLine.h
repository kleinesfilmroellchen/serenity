/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Noncopyable.h>
#include <AK/Platform.h>
#include <AK/StdLibExtras.h>
#include <AK/Vector.h>
#include <LibDSP/Music.h>

namespace LibDSP {

// Delays signals in time.
// Usage: Read and copy the sample at index 0 (output of the delay line),
// then overwrite it (input to the delay line). Then advance the delay line.
// The simplest use case is a straight-up delay effect.
class DelayLine {
    AK_MAKE_NONCOPYABLE(DelayLine);

public:
    DelayLine(size_t capacity);
    DelayLine()
        : DelayLine(0)
    {
    }

    void resize(size_t new_capacity);

    ALWAYS_INLINE size_t size() const { return max(m_buffer.size(), 1); }

    // The delay line is always indexed relative to its current location.
    template<Integral SizeT>
    Sample& operator[](SizeT index)
    {
        if (m_buffer.size() < 1) [[unlikely]]
            return m_default_sample;
        SizeT real_index = m_offset + index;
        real_index %= size();
        return m_buffer[static_cast<size_t>(real_index)];
    }
    template<Integral SizeT>
    Sample const& operator[](SizeT index) const
    {
        if (m_buffer.size() < 1) [[unlikely]]
            return Sample::empty();
        SizeT real_index = m_offset + index;
        real_index %= size();
        return m_buffer[static_cast<size_t>(real_index)];
    }

    // Advance the delay line
    DelayLine& operator++();

private:
    Vector<Sample> m_buffer;
    // For some exceptional situations, we just need a dummy sample reference.
    Sample m_default_sample;
    size_t m_offset { 0 };
};

}
