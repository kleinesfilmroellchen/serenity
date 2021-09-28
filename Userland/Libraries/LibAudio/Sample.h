/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>
 * Copyright (c) 2021, David Isaksson <davidisaksson93@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Math.h>

namespace Audio {
using namespace AK::Exponentials;

// Logarithmic scaling, as audio should ALWAYS do.
// Reference: https://www.dr-lex.be/info-stuff/volumecontrols.html
// We use the curve `factor = a * exp(b * change)`,
// where change is the input fraction we want to change by,
// The dynamic range is based on the bit depth which is currently set to
// 16 bit. This might change in the future and can be changed later.
// This dynamic range gives us ~96.3 dB of dB range.
// Reference: https://en.wikipedia.org/wiki/DBFS

constexpr double BIT_DEPTH = 16;
constexpr double DYNAMIC_RANGE_DB = 20.0 * log10(AK::pow(2.0, BIT_DEPTH));
constexpr double DYNAMIC_RANGE = AK::pow(10.0, DYNAMIC_RANGE_DB * 0.05);
constexpr double VOLUME_A = 1 / DYNAMIC_RANGE;
double const VOLUME_B = log(DYNAMIC_RANGE);

// Format ranges:
// - Linear:        0.0 to 1.0
// - Logarithmic:   0.0 to 1.0
// - dB:         ~-96.3 to 0.0

ALWAYS_INLINE double linear_to_log(double const change)
{
    // Linear slope the first 3 dB to avoid asymptotic behaviour
    // TODO: Can this be done more effectively?
    return change < 0.05 ? change * 0.028 : VOLUME_A * exp(VOLUME_B * change);
}

ALWAYS_INLINE double log_to_linear(double const val)
{
    // Linear slope at val < 0.1 to avoid asymptotic behaviour
    // TODO: Can this be done more effectively?
    return val < 0.1 ? val * 50 : log(val / VOLUME_A) / VOLUME_B;
}

ALWAYS_INLINE double db_to_linear(double const dB)
{
    return (dB + DYNAMIC_RANGE_DB) / DYNAMIC_RANGE_DB;
}

ALWAYS_INLINE double linear_to_db(double const val)
{
    return val * DYNAMIC_RANGE_DB - DYNAMIC_RANGE_DB;
}

// A single sample in an audio buffer.
// Values are floating point, and should range from -1.0 to +1.0
struct Sample {
    constexpr Sample() = default;

    // For mono
    constexpr Sample(double left)
        : left(left)
        , right(left)
    {
    }

    // For stereo
    constexpr Sample(double left, double right)
        : left(left)
        , right(right)
    {
    }

    void clip()
    {
        if (left > 1)
            left = 1;
        else if (left < -1)
            left = -1;

        if (right > 1)
            right = 1;
        else if (right < -1)
            right = -1;
    }

    ALWAYS_INLINE Sample& log_multiply(double const change)
    {
        double factor = linear_to_log(change);
        left *= factor;
        right *= factor;
        return *this;
    }

    ALWAYS_INLINE Sample log_multiplied(double const volume_change) const
    {
        Sample new_frame { left, right };
        new_frame.log_multiply(volume_change);
        return new_frame;
    }

    ALWAYS_INLINE Sample& log_pan(double const pan)
    {
        left *= linear_to_log(min(pan * -1 + 1.0, 1.0));
        right *= linear_to_log(min(pan + 1.0, 1.0));
        return *this;
    }

    ALWAYS_INLINE Sample log_pan(double const pan) const
    {
        Sample new_frame { left, right };
        new_frame.log_pan(pan);
        return new_frame;
    }

    constexpr Sample& operator*=(double const mult)
    {
        left *= mult;
        right *= mult;
        return *this;
    }

    constexpr Sample operator*(double const mult)
    {
        return { left * mult, right * mult };
    }

    constexpr Sample& operator+=(Sample const& other)
    {
        left += other.left;
        right += other.right;
        return *this;
    }
    constexpr Sample& operator+=(double other)
    {
        left += other;
        right += other;
        return *this;
    }

    constexpr Sample operator+(Sample const& other)
    {
        return { left + other.left, right + other.right };
    }

    double left { 0 };
    double right { 0 };
};

}
