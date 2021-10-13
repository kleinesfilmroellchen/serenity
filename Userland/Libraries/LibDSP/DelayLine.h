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

    // The delay line is always indexed relative to its current location
    Sample const& operator[](ssize_t index) const;
    Sample& operator[](ssize_t index);
    Sample const& operator[](size_t index) const;
    Sample& operator[](size_t index);

    // Advance the delay line
    DelayLine& operator++();

private:
    Vector<Sample> m_buffer;
    size_t m_offset { 0 };
};

}
