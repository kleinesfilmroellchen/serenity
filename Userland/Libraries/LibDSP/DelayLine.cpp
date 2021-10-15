/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibDSP/DelayLine.h>

namespace LibDSP {

DelayLine::DelayLine(size_t capacity)
    : m_offset(0)
{
    m_buffer.resize(capacity, true);
}

void DelayLine::resize(size_t new_capacity)
{
    if (new_capacity == size())
        return;

    m_buffer.resize(new_capacity, true);
    m_offset %= size();
}

// There's a separate signed implementation because it may be slower than the unsigned implementation.
Sample const& DelayLine::operator[](ssize_t index) const
{
    ssize_t real_index = m_offset + index;
    while (real_index < 0)
        real_index += size();
    real_index %= size();
    return m_buffer[real_index];
}
Sample& DelayLine::operator[](ssize_t index)
{
    ssize_t real_index = m_offset + index;
    while (real_index < 0)
        real_index += size();
    real_index %= size();
    return m_buffer[real_index];
}

Sample const& DelayLine::operator[](size_t index) const
{
    // Unlikely special-case for zero-size delay lines
    if (m_buffer.size() < 1) [[unlikely]]
        return Sample::empty();
    size_t real_index = (index + m_offset) % size();
    return m_buffer[real_index];
}
Sample& DelayLine::operator[](size_t index)
{
    // Unlikely special-case for zero-size delay lines
    if (m_buffer.size() < 1) [[unlikely]]
        return m_default_sample;
    size_t real_index = (index + m_offset) % size();
    return m_buffer[real_index];
}

DelayLine& DelayLine::operator++()
{
    ++m_offset;
    m_offset %= size();
    return *this;
}

}
