/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "BandAdjustmentFilter.h"

namespace DSP {

float BandAdjustmentFilter::center_frequency() const { return m_center_frequency; }
float BandAdjustmentFilter::gain() const { return m_gain; }
float BandAdjustmentFilter::gain_db() const { return Sample::linear_to_db(m_gain); }
float BandAdjustmentFilter::q() const { return m_q; }

void BandAdjustmentFilter::set_center_frequency(float frequency)
{
    m_center_frequency = frequency;
    recompute_coefficients();
}

void BandAdjustmentFilter::set_gain(float gain)
{
    m_gain = gain;
    recompute_coefficients();
}

void BandAdjustmentFilter::set_gain_db(float gain_db)
{
    set_gain(Sample::db_to_linear(gain_db));
}

void BandAdjustmentFilter::set_q(float q_factor)
{
    m_q = q_factor;
    recompute_coefficients();
}

void BandAdjustmentFilter::recompute_coefficients()
{
    // The filter only works with boost gain; see the filter inversion below.
    auto const gain = m_gain >= 1.f ? (1.f / m_gain) : m_gain;

    auto const digital_bandwidth = m_q * m_center_frequency;

    // Our frequencies are in cycles/s, but the formula expects rads/sample.
    auto const radians_conversion_factor = 2 * AK::Pi<float> / static_cast<float>(m_transport->sample_rate());
    auto const digital_bandwidth_radians = digital_bandwidth * radians_conversion_factor;
    auto const digital_center_frequency_radians = m_center_frequency * radians_conversion_factor;

    /// These computations mostly translate from digital to analog, reversing discretization.

    // GB, gain at which the bandwidth is measured.
    auto const bandwidth_gain = 0.95f * gain;
    // G0 and G1, gain at zero and Nyquist frequencies.
    auto constexpr offset_gain = 1.f;

    // Ω0, eq. 19
    auto const analog_center_frequency = AK::tan(digital_center_frequency_radians / 2);
    // W², eq. 18
    auto const w_squared = AK::sqrt((gain * gain - offset_gain * offset_gain) / (gain * gain - offset_gain * offset_gain)) * analog_center_frequency * analog_center_frequency;
    // ΔΩ, eq. 19
    auto const analog_bandwidth = (1 + AK::sqrt((bandwidth_gain * bandwidth_gain - offset_gain * offset_gain) / (bandwidth_gain * bandwidth_gain - offset_gain * offset_gain)) * w_squared) * AK::tan(digital_bandwidth_radians / 2.0f);
    // eq. 17
    auto const c = analog_bandwidth * analog_bandwidth * AK::abs(bandwidth_gain * bandwidth_gain - offset_gain * offset_gain)
        - 2 * w_squared * (AK::abs(bandwidth_gain * bandwidth_gain - offset_gain * offset_gain) - AK::sqrt((bandwidth_gain * bandwidth_gain - offset_gain * offset_gain) * (bandwidth_gain * bandwidth_gain - offset_gain * offset_gain)));
    auto const d = 2 * w_squared * (AK::abs(gain * gain - offset_gain * offset_gain) - AK::sqrt((gain * gain - offset_gain * offset_gain) * (gain * gain - offset_gain * offset_gain)));
    // eq. 16
    auto const a = AK::sqrt((c + d) / (gain * gain - bandwidth_gain * bandwidth_gain));
    auto const b = AK::sqrt((gain * gain * c + bandwidth_gain * bandwidth_gain * d) / (gain * gain - bandwidth_gain * bandwidth_gain));

    // Finally compute the coefficients (eq. 20)
    // "a", for previous output, denominator
    Array<Sample, 3> feedback;
    // "b", for previous input, numerator
    Array<Sample, 3> feedforward;

    auto const normalizer = 1 + w_squared + a;

    feedback[0] = Sample(1.0f);
    feedback[1] = Sample(-2 * ((1 - w_squared) / normalizer));
    feedback[2] = Sample((1 + w_squared - a) / normalizer);

    feedforward[0] = Sample((offset_gain + offset_gain * w_squared + b) / normalizer);
    feedforward[1] = Sample(-2 * (offset_gain - offset_gain * w_squared) / normalizer);
    feedforward[2] = Sample((offset_gain + offset_gain * w_squared - b) / normalizer);

    // When normalizer is very small (gain ≈ 1), we might get NaN for some coefficients. Simply set them to zero then.
    for (auto& coefficient : feedback) {
        if (coefficient.any_channel_is_nan())
            coefficient = SAMPLE_OFF;
    }
    for (auto& coefficient : feedforward) {
        if (coefficient.any_channel_is_nan())
            coefficient = SAMPLE_OFF;
    }

    // The band adjustment filter from the paper only works for boosting, i.e. positive gain.
    // To allow for both boost and cut, inverting the given transfer function (G(s) = Z(s) / N(s)) is all we need.
    // Simply setting G*(s) = G(s)⁻¹ = N(s) / Z(s) ) means that we need to exchange feedback and feedforward.
    if (m_gain < 1.f) {
        for (auto i = 0u; i < 3; ++i)
            swap(feedback[i], feedforward[i]);
    }

    // dbgln("# gain = {} ({})\nZ = {}\nN = {}", m_gain, gain, feedback.span(), feedforward.span());

    set_feedforward_coefficients(move(feedforward));
    set_feedback_coefficients(move(feedback));
}

}
