/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>
 * Copyright (c) 2021, David Isaksson <davidisaksson93@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Math.h>
#include <math.h>

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
// FIXME: This is basically constexpr but clang doesn't believe us.
double const DYNAMIC_RANGE_DB = 20.0 * log10(AK::pow(2.0, BIT_DEPTH));
double const DYNAMIC_RANGE = AK::pow(10.0, DYNAMIC_RANGE_DB * 0.05);
double const VOLUME_A = 1 / DYNAMIC_RANGE;
double const VOLUME_B = log(DYNAMIC_RANGE);

// Format ranges:
// - Linear:          0.0 to 1.0
// - Amplitude:       0.0 to 1.0
// - dB(linear):   ~-96.3 to 0.0 (At max)
// - dB(amplitude):  -inf to 0.0
// - Panning:        -1.0 to 1.0 (Left to Right)

ALWAYS_INLINE double linear_to_amplitude_impl(double const value)
{
    return VOLUME_A * exp(VOLUME_B * value);
}

ALWAYS_INLINE double amplitude_to_linear_impl(double const amplitude)
{
    return log(amplitude / VOLUME_A) / VOLUME_B;
}

// Since the functions to calculate linear values to amplitude and vice versa are logarithmic
// we add a linear slope at the lower values to avoid asymptotic behavior.
// These constants define where we start that slope.
double const LINEAR_FALLOFF_THRESHOLD = 0.1;
double const LINEAR_FALLOFF_SLOPE = linear_to_amplitude_impl(LINEAR_FALLOFF_THRESHOLD) / LINEAR_FALLOFF_THRESHOLD;

ALWAYS_INLINE double linear_to_amplitude(double const value)
{
    if (value < LINEAR_FALLOFF_THRESHOLD)
        return value * LINEAR_FALLOFF_SLOPE;

    return linear_to_amplitude_impl(value);
}

ALWAYS_INLINE double amplitude_to_linear(double const amplitude)
{
    if (amplitude < LINEAR_FALLOFF_THRESHOLD)
        return amplitude / LINEAR_FALLOFF_SLOPE;

    return amplitude_to_linear_impl(amplitude);
}

ALWAYS_INLINE double db_to_linear(double const dB, double const dB_headroom = 0.0, double const dB_range = DYNAMIC_RANGE_DB)
{
    if (dB < -dB_range)
        return 0;

    return (dB + dB_range - dB_headroom) / dB_range;
}

ALWAYS_INLINE double linear_to_db(double const value, double const dB_headroom = 0.0, double const dB_range = DYNAMIC_RANGE_DB)
{
    if (value == 0)
        return -static_cast<double>(INFINITY);

    return value * dB_range - dB_range + dB_headroom;
}

// db <-> amplitude can be used for audio visualizations
ALWAYS_INLINE double db_to_amplitude(double const dB)
{
    return AK::pow(10.0, 0.05 * dB);
}

ALWAYS_INLINE double amplitude_to_db(double const amplitude)
{
    return 20.0 * log10(amplitude);
}

// A single sample in an audio buffer.
// Values are floating point, and should range from -1.0 to +1.0
struct Sample {
    constexpr Sample() = default;

    static Sample const& empty()
    {
        static Sample const the_empty_sample {};
        return the_empty_sample;
    }

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
        double factor = linear_to_amplitude(change);
        left *= factor;
        right *= factor;
        return *this;
    }

    ALWAYS_INLINE Sample log_multiplied(double const volume_change) const
    {
        double factor = linear_to_amplitude(volume_change);
        return { left * factor, right * factor };
    }

    // Constant power panning
    ALWAYS_INLINE Sample& pan(double const position)
    {
        double const pi_over_2 = AK::Pi<double> * 0.5;
        double const root_over_2 = AK::sqrt(2.0) * 0.5;
        double const angle = position * pi_over_2 * 0.5;
        left *= root_over_2 * (AK::cos(angle) - AK::sin(angle));
        right *= root_over_2 * (AK::cos(angle) + AK::sin(angle));
        return *this;
    }

    ALWAYS_INLINE Sample panned(double const position) const
    {
        Sample new_sample { left, right };
        new_sample.pan(position);
        return new_sample;
    }

    // Constant power fading between two samples (0 = only this, 1 = only other), compare panning
    ALWAYS_INLINE Sample fade(Sample const other, double position) const
    {
        // We get in 0..1, but the constant power algorithm needs -1..1
        position = position * 2 - 1;
        double const pi_over_2 = AK::Pi<double> * 0.5;
        double const root_over_2 = AK::sqrt(2.0) * 0.5;
        double const angle = position * pi_over_2 * 0.5;
        double self_gain = (root_over_2 * (AK::cos(angle) - AK::sin(angle)));
        double other_gain = (root_over_2 * (AK::cos(angle) + AK::sin(angle)));
        Sample out = *this * self_gain;
        out += other * other_gain;
        return out;
    }

    static ALWAYS_INLINE Sample fade(Sample const first, Sample const second, double position)
    {
        return first.fade(second, position);
    }

    constexpr Sample& operator*=(double const mult)
    {
        left *= mult;
        right *= mult;
        return *this;
    }

    constexpr Sample operator*(double const mult) const
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

    constexpr Sample operator+(Sample const& other) const
    {
        return { left + other.left, right + other.right };
    }

    double left { 0 };
    double right { 0 };
};

}
