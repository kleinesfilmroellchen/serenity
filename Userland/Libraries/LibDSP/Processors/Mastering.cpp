/*
 * Copyright (c) 2021-2022, kleines Filmröllchen <filmroellchen@serenityos.org>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Mastering.h"

namespace DSP::Effects {

Mastering::Mastering(NonnullRefPtr<Transport> transport)
    : EffectProcessor(move(transport))
    , m_pan("Pan"_short_string, -1, 1, 0, Logarithmic::No)
    , m_volume("Volume"_short_string, 0, 1, 1, Logarithmic::No)
    , m_muted("Mute"_short_string, false)
{
    m_parameters.append(m_muted);
    m_parameters.append(m_volume);
    m_parameters.append(m_pan);
}

void Mastering::process_impl(Signal const& input_signal, Signal& output)
{
    process_to_fixed_array(input_signal, output.get<FixedArray<Sample>>());
}

void Mastering::process_to_fixed_array(Signal const& input_signal, FixedArray<Sample>& output)
{
    if (m_muted) {
        output.fill_with({});
        return;
    }

    auto const& input = input_signal.get<FixedArray<Sample>>();
    for (size_t i = 0; i < input.size(); ++i) {
        auto sample = input[i];
        sample.log_multiply(static_cast<float>(m_volume));
        sample.pan(static_cast<float>(m_pan));
        output[i] = sample;
    }
}

}
