/*
 * Copyright (c) 2021, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "BandAdjustment.h"
#include <AK/FixedArray.h>
#include <math.h>

namespace DSP::Effects {

BandAdjustment::BandAdjustment(NonnullRefPtr<Transport> transport)
    : EffectProcessor(transport)
    , m_filter(move(transport))
    , m_frequency("Frequency"_string, 20, 20'000, 300, Logarithmic::Yes)
    , m_gain_db("Gain (dB)"_string, -60, 20, 0, Logarithmic::No)
    , m_q("Q"_short_string, 0.02, 7, 1, Logarithmic::No)
{
    m_parameters.append(m_frequency);
    m_parameters.append(m_gain_db);
    m_parameters.append(m_q);

    m_frequency.register_change_listener([this](auto const& frequency) {
        m_filter.set_center_frequency(static_cast<float>(frequency));
    });
    m_gain_db.register_change_listener([this](auto const& gain_db) {
        m_filter.set_gain_db(static_cast<float>(gain_db));
    });
    m_q.register_change_listener([this](auto const& q) {
        m_filter.set_q(static_cast<float>(q));
    });

    m_filter.set_center_frequency(static_cast<float>(m_frequency));
    m_filter.set_gain_db(static_cast<float>(m_gain_db));
    m_filter.set_q(static_cast<float>(m_q));
}

void BandAdjustment::process_impl(Signal const& input_signal, Signal& output_signal)
{
    auto const input_samples = input_signal.get<FixedArray<Sample>>().span();
    auto output_samples = output_signal.get<FixedArray<Sample>>().span();
    m_filter.filter(input_samples, output_samples);
}

}
