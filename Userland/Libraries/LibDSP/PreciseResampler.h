/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Array.h>
#include <AK/TypedTransfer.h>
#include <AK/Vector.h>
#include <LibAudio/Queue.h>
#include <LibDSP/Window.h>

namespace DSP {

// Number of samples left and right of the current one that are used.
static constexpr size_t resampler_filter_size = 32;
// Most windows are only accurate for powers of 2.
static_assert(AK::popcount(resampler_filter_size) == 1);

// 0-th order modified Bessel function of first kind.
constexpr double Izero(double x)
{
    double u, temp;
    int n;

    double sum = u = n = 1;
    double halfx = x / 2.0;
    do {
        temp = halfx / (double)n;
        n += 1;
        temp *= temp;
        u *= temp;
        sum += u;
    } while (u >= NumericLimits<double>::epsilon() * sum);

    return sum;
}

template<size_t FilterSize>
Array<double, FilterSize> generate_sinc_filter()
requires(AK::popcount(FilterSize) == 1)
{
    double IBeta, temp, temp1, inm1;

    Array<double, FilterSize> c;
    // Filter rolloff speed within the pass band.
    constexpr double rolloff = 0.9;
    //
    constexpr double frequency = 0.5 * rolloff;

    // Calculate ideal lowpass filter impulse response coefficients.
    c[0] = rolloff;
    for (size_t i = 1; i < c.size(); i++) {
        temp = AK::Pi<double> * (double)i / (double)resampler_filter_size;
        c[i] = sin<double>(2.0 * temp * frequency) / temp; /* Analog sinc function, cutoff = frq */
    }

    /*
     * Calculate and Apply Kaiser window to ideal lowpass filter.
     * Note: last window value is IBeta which is NOT zero.
     * You're supposed to really truncate the window here, not ramp
     * it to zero. This helps reduce the first sidelobe.
     */
    IBeta = 1.0 / Izero(Beta);
    inm1 = 1.0 / ((double)(N - 1));
    for (i = 1; i < N; i++) {
        temp = (double)i * inm1;
        temp1 = 1.0 - temp * temp;
        temp1 = (temp1 < 0 ? 0 : temp1); /* make sure it's not negative since
                                            we're taking the square root - this
                                            happens on Pentium 4's due to tiny
                                            roundoff errors */
        c[i] *= Izero(Beta * sqrt(temp1)) * IBeta;
    }
}

// Window used for band-limiting data after resampling.
// TODO: Make this constexpr once Clang has constexpr cos().
static Array<double, resampler_filter_size> resampler_window { Window<double>::blackman_harris<resampler_filter_size>() };
// Resampling sinc() filter, band-limited with Kaiser window.
// TODO: Make this constexpr once Clang has constexpr sin().
static Array<double, resampler_filter_size> resampling_sinc_filter {
    generate_sinc_filter<resampler_filter_size>()
};

enum class ConsumedInput : bool {
    No,
    Yes,
};

// Resampler using sinc(x) = sin(x)/x interpolation.
// This is the mathematically accurate way of interpolating between sample rates,
// but since the sinc function has infinite extents, we limit it to a certain number of zero-order crossings.
// This makes the band-limiting less precise by introducing noise beyond the Nyquist limit (stop band),
// which (as usual) gets aliased into lower frequencies.
// The band limiting filter is a Kaiser window.
// The current sample window and sinc size in use are listed in the constants above.
template<typename SampleType>
class PreciseResampler {
    AK_MAKE_NONCOPYABLE(PreciseResampler);

public:
    PreciseResampler(size_t input_sample_rate, size_t output_sample_rate)
        : m_ratio(static_cast<double>(input_sample_rate) / static_cast<double>(output_sample_rate))
    {
    }
    ~PreciseResampler() = default;

    // The resampling ratio was derived from the input and output sample rate.
    double ratio() const { return m_ratio; }
    void set_ratio(double ratio) { m_ratio = ratio; }

    // Resamples the input data into the output data. Previous resampling state is reused.
    // After a successful return, the output span indicates the range of data that couldn't be filled due to missing input.
    // Algorithm ported from libresample, https://github.com/minorninth/libresample/blob/master/src/resample.c
    // licensed under BSD 2-clause.
    ErrorOr<ConsumedInput> resample(ReadonlySpan<SampleType> input, Span<SampleType>& output)
    {
        if (m_ratio <= 0)
            return Error::from_string_literal("Input sample ratio invalid");

        // Move remaining buffered output data into the output.
        if (!m_output_buffer.is_empty()) {
            auto const count = min(m_output_buffer.size(), output.size());
            AK::TypedTransfer<SampleType>::move(output.data(), m_output_buffer.span().trim(count).data(), count);
            m_output_buffer.remove(0, count);
            output = output.offset(count);
        }

        if (output.size() == 0)
            return ConsumedInput::No;

        // FIXME: We don't account for filter scale when downsampling.
        // FIXME: There is no way of padding the input with zeroes for processing the final data.

        // Move input to internal input buffer.
        TRY(m_input_buffer.try_extend(input));

        if (m_ratio >= 1) {
            TRY(resample_up());
        } else if (m_ratio < 1) {
            TRY(resample_down());
        }

        return ConsumedInput::Yes;
    }

private:
    // The "up" and "down" functions are for upsampling and downsampling, respectively.

    ErrorOr<void> resample_up() { }
    ErrorOr<void> resample_down() { }

    // Filter an input for upsampling.
    // Note that the input span is indexed in the middle first, and depending on the direction,
    // we will walk to the start or end of the span.
    SampleType filter_up(Array<float, resampler_filter_size / 2> filter_coeffients, Span<SampleType> input, double phase, int direction)
    {
        double a = 0;
        float v, t;

        phase *= resampler_filter_size; /* Npc is number of values per 1/delta in impulse response */

        v = 0.0; /* The output value */
        float* current_filter = &filter_coeffients[(int)phase];
        size_t filter_count = filter_coeffients.size();
        float* filter_end = &filter_coeffients.last();

        if (direction == 1)                              /* If doing right wing...              */
        {                                                /* ...drop extra coeff, so when phase is  */
            filter_end--;                                /*    0.5, we don't do too many mult's */
            if (phase == 0)                              /* If the phase is zero...           */
            {                                            /* ...then we've already skipped the */
                current_filter += resampler_filter_size; /*    first sample, so we must also  */
                                                         /*    skip ahead in filter_coeffients[] */
            }
        }

        while (current_filter < filter_end) {
            t = *current_filter;                     /* Get filter coeff */
            t *= *Xp;                                /* Mult coeff by input sample */
            v += t;                                  /* The filter output */
            current_filter += resampler_filter_size; /* Filter coeff step */
            Xp += direction;                         /* Input signal step. NO CHECK ON BOUNDS */
        }

        return v;
    }

    double m_ratio;
    double m_time;

    // Buffers to store superflous data since the user can give us inputs and outputs of any size.
    Vector<SampleType, max(Audio::AUDIO_BUFFER_SIZE, resampler_filter_size)> m_input_buffer {};
    Vector<SampleType, max(Audio::AUDIO_BUFFER_SIZE, resampler_filter_size)> m_output_buffer {};
};

}
