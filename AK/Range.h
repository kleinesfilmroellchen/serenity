/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace AK {

template<typename T>
class Range {
private:
    static T one = static_cast<T>(1);
    static T zero = static_cast<T>(0);

    class RangeIterator {
    public:
        RangeIterator(T iterator_begin)
            : m_current(iterator_begin)
        {
        }

        RangeIterator operator++()
        {
            m_current += m_step;
        }

    private:
        T m_current;
    };

public:
    Range(T end)
        : Range(0, end)
    {
    }
    Range(T begin, T end)
        : Range(begin, end, 1)
    {
    }
    Range(T begin, T end, T step)
        : m_begin(begin)
        , m_end(end)
        , m_step(step)
    {
    }

    RangeIterator begin() const { return RangeIterator(begin); }
    RangeIterator end() const { return RangeIterator(m_end >= m_begin ? m_end + one : m_begin - one); }

private:
    T const m_begin {};
    T const m_end {};
    T const m_step {};
};

}

using AK::Range;
