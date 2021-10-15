/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/NonnullRefPtr.h>
#include <AK/Types.h>
#include <LibDSP/Effects.h>
#include <LibDSP/Processor.h>
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

    m_transport->add_client(*this);
    m_delay_time.add_client(*this);
}

Delay::~Delay()
{
    m_transport->remove_client(*this);
    m_delay_time.remove_client(*this);
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
    Sample const& in = input_signal.get<Sample>();
    Sample delayed = m_delay_line[0_z] * m_delay_decay;

    m_delay_line[0_z] = delayed + in * m_input_gain;
    ++m_delay_line;

    return Signal(Sample::fade(delayed, in, m_wet_dry));
}

Reverb::Reverb(NonnullRefPtr<Transport> transport)
    : EffectProcessor(move(transport))
    , m_early_reflection_gain("Early reflections"sv, 0, 1, 1)
    , m_early_reflection_time("Early reflection duration"sv, 0.01, 300, 100)
    , m_early_reflection_density("Early reflection density"sv, 3, 20, 5)
    , m_reverb_decay("Decay"sv, 0, 1, 0.7)
    , m_shape("Shape"sv, 0, 1, 0.5)
    , m_wet_dry("Wet/Dry"sv, 0, 1, 0.6)
{
    m_parameters.append(m_early_reflection_gain);
    m_parameters.append(m_early_reflection_time);
    m_parameters.append(m_reverb_decay);
    m_parameters.append(m_shape);
    m_parameters.append(m_wet_dry);

    generate_prime_database();
    dbgln("Primes: {}", m_primes);
}

// Essentially a flipped boolean: make yes the default
enum PrimeState : u8 {
    Yes = 0,
    No = 1,
};

void Reverb::generate_prime_database()
{
    double seconds = static_cast<double>(m_early_reflection_time.max_value()) / 1000.0;
    size_t max_sample_count = ceil(seconds * m_transport->sample_rate());

    Vector<PrimeState> is_prime;
    is_prime.resize(max_sample_count);
    is_prime[0] = is_prime[1] = No;
    // Prime sieve
    for (size_t i = 2; i < max_sample_count / 2; ++i) {
        if (is_prime[i] == No)
            continue;
        for (size_t multiple = i * 2; multiple < max_sample_count; multiple += i)
            is_prime[multiple] = No;
    }
    // Let's worst-case assume that every third number is a prime; that's a reasonable overestimate into the low 1000's.
    m_primes.ensure_capacity(max_sample_count / 3);
    for (size_t i = 0; i < max_sample_count; ++i) {
        if (is_prime[i] == Yes)
            m_primes.unchecked_append(static_cast<unsigned>(i));
    }
}

void Reverb::generate_tapoff_indices()
{
    TODO();
}

// Process a Schroeder allpass section.
// Conventions adopted from digital signal processing, see
// https://ccrma.stanford.edu/~jos/pasp/Schroeder_Allpass_Sections.html
static void process_allpass(DelayLine& delay, double g, Sample& x)
{
    Sample delay_out = delay[0_z];
    // v is the signal going into the delay line and forward-fed to the output
    Sample v = delay_out * g + x;

    delay[0_z] = v;
    ++delay;

    // y
    x = v * -g + delay_out;
}

void Reverb::handle_early_time_change()
{
    double seconds = static_cast<double>(m_early_reflection_time) / 1000.0;
    size_t sample_count = ceil(seconds * m_transport->sample_rate());
    m_early_reflector_tdl.resize(sample_count);
}

Signal Reverb::process_impl(Signal const& input_signal)
{
    handle_early_time_change();

    Sample const& in = input_signal.get<Sample>();

    // Early reflections
    Sample early;

    // More taps = more echo
    for (size_t i = 0; i < ceil(m_early_reflection_density); ++i) {
        early += m_early_reflector_tdl[0_z];
        //m_early_reflector_tdl[0_z] = early;
        ++m_early_reflector_tdl;
    }

    // Late reverb
    Sample late = m_early_reflector_tdl[0_z];
    process_allpass(m_allpass_line_1, m_reverb_decay, late);
    process_allpass(m_allpass_line_2, m_reverb_decay, late);
    process_allpass(m_allpass_line_3, m_reverb_decay, late);

    return Signal(Sample::fade(early + late, in, m_wet_dry));
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
        return Signal(in);
    }

    out += in.panned(static_cast<double>(m_pan));
    out += in.log_multiplied(static_cast<double>(m_master_volume));
    return Signal(out);
}

}
