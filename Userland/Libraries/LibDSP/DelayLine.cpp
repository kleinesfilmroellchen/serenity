/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Concepts.h>
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

DelayLine& DelayLine::operator++()
{
    ++m_offset;
    m_offset %= size();
    return *this;
}

}
