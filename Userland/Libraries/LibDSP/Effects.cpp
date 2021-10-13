/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Effects.h"
#include <AK/Types.h>
#include <math.h>

namespace LibDSP::Effects {

Delay::Delay(NonnullRefPtr<Transport> transport)
    : EffectProcessor(move(transport))
    , m_delay_decay("Decay"sv, 0.01, 0.99, 0.33)
    , m_delay_time("Delay Time"sv, 3, 2000, 900)
    , m_input_gain("Input Gain"sv, 0, 1, 0.9)
    , m_wet_dry("Wet/Dry"sv, 0, 1, 0.8)
{

    m_parameters.append(m_input_gain);
    m_parameters.append(m_delay_decay);
    m_parameters.append(m_delay_time);
    m_parameters.append(m_wet_dry);
}

void Delay::handle_delay_time_change()
{
    // We want a delay buffer that can hold samples filling the specified number of milliseconds.
    double seconds = static_cast<double>(m_delay_time) / 1000.0;
    size_t sample_count = ceil(seconds * m_transport->sample_rate());
    m_delay_line.resize(sample_count);
}

Signal Delay::process_impl(Signal const& input_signal)
{
    handle_delay_time_change();

    Sample const& in = input_signal.get<Sample>();
    Sample out;
    out += in.log_multiplied(m_input_gain);
    out += m_delay_line[0_z] * m_delay_decay;

    m_delay_line[0_z] = out;
    ++m_delay_line;

    return Signal(Sample::fade(out, in, m_wet_dry));
}

Mastering::Mastering(NonnullRefPtr<Transport> transport)
    : EffectProcessor(move(transport))
    , m_master_volume("Master"sv, 0, 1, 1)
    , m_pan("Pan"sv, -1, 1, 0)
    , m_mute("Mute"sv, false)
{
    m_parameters.append(m_master_volume);
    m_parameters.append(m_pan);
    m_parameters.append(m_mute);
}

Signal Mastering::process_impl(Signal const& input_signal)
{
    Sample const& in = input_signal.get<Sample>();
    Sample out;

    if (m_mute.value()) {
        return Signal(out);
    }

    out += in.panned(static_cast<double>(m_pan));
    out += in.log_multiplied(static_cast<double>(m_master_volume));
    return Signal(out);
}

}
