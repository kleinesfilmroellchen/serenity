/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/NonnullRefPtr.h>
#include <AK/Types.h>
#include <LibDSP/DelayLine.h>
#include <LibDSP/Processor.h>
#include <LibDSP/ProcessorParameter.h>
#include <LibDSP/Transport.h>

namespace LibDSP::Effects {

// A simple digital delay effect using a delay buffer.
// This is based on Piano's old built-in delay.
class Delay : public EffectProcessor {
public:
    Delay(NonnullRefPtr<Transport>);

private:
    virtual Signal process_impl(Signal const&) override;
    void handle_delay_time_change();

    ProcessorRangeParameter m_delay_decay;
    ProcessorRangeParameter m_delay_time;
    ProcessorRangeParameter m_input_gain;
    ProcessorRangeParameter m_wet_dry;

    DelayLine m_delay_line;
};

// Adapted from:
// https://ccrma.stanford.edu/~jos/pasp/Artificial_Reverberation.html
// (and the sub pages)
// http://www.earlevel.com/main/1997/01/19/a-bit-about-reverb/
class Reverb : public EffectProcessor {
public:
    Reverb(NonnullRefPtr<Transport>);

private:
    virtual Signal process_impl(Signal const&) override;
    void handle_early_time_change();

    ProcessorRangeParameter m_early_reflection_gain;
    ProcessorRangeParameter m_early_reflection_time;
    ProcessorRangeParameter m_early_reflection_density;
    // Schroeder allpass
    ProcessorRangeParameter m_reverb_decay;
    ProcessorRangeParameter m_shape;
    ProcessorRangeParameter m_wet_dry;

    // Tapped Delay Line (TDL) for the early reflections
    DelayLine m_early_reflector_tdl;

    // Allpass filter (3 stages) for late reverb through feedback and feedforward comb filters
    // Length 1051
    DelayLine m_allpass_line_1;
    // Length 337
    DelayLine m_allpass_line_2;
    // Length 113
    DelayLine m_allpass_line_3;

    void generate_prime_database();
    Vector<unsigned> m_primes;
};

// A simple effect that applies volume, mute and pan to its input signal.
// Convenient for attenuating signals in the middle of long chains.
class Mastering : public EffectProcessor {
public:
    Mastering(NonnullRefPtr<Transport>);

private:
    virtual Signal process_impl(Signal const&) override;

    ProcessorRangeParameter m_master_volume;
    ProcessorRangeParameter m_pan;
    ProcessorBooleanParameter m_mute;
};

}
