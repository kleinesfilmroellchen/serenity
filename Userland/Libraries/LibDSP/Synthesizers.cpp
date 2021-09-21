/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/HashMap.h>
#include <AK/Math.h>
#include <LibDSP/Processor.h>
#include <LibDSP/Synthesizers.h>
#include <math.h>

namespace LibDSP::Synthesizers {

Classic::Classic(NonnullRefPtr<Transport> transport)
    : LibDSP::SynthesizerProcessor(transport)
    , m_waveform("Waveform"sv, Waveform::Sine)
    , m_attack("Attack"sv, 0, 2000, 5)
    , m_decay("Decay"sv, 0, 20'000, 80)
    , m_sustain("Sustain"sv, 0, 1, 0.725)
    , m_release("Release", 0, 6'000, 120)
{
    m_parameters.append(m_waveform);
}

Signal Classic::process_impl(Signal const& input_signal)
{
    auto& in = input_signal.get<RollNotes>();

    Sample out;

    SinglyLinkedList<PitchedEnvelope> playing_envelopes;

    // "Press" the necessary notes in the internal representation,
    // and "release" all of the others
    for (u8 i = 0; i < note_count; ++i) {
        if (auto maybe_note = in.get(i); maybe_note.has_value())
            m_playing_notes.set(i, maybe_note.value());
        // Don't append an envelope for non-playing notes
        else if (!(m_playing_notes.contains(i) && m_playing_notes.get(i)->is_playing(m_transport->time())))
            continue;

        playing_envelopes.append(PitchedEnvelope { m_playing_notes.get(i)->to_envelope(m_transport->time(), m_attack * m_transport->ms_sample_rate(), m_decay * m_transport->ms_sample_rate(), m_release * m_transport->ms_sample_rate()), i });
    }

    for (auto envelope : playing_envelopes) {
        double volume = volume_from_envelope(envelope);
        double wave = wave_position(envelope.note);
        out += volume * wave;
    }

    return out;
}

double Classic::volume_from_envelope(Envelope envelope)
{
    switch (static_cast<EnvelopeState>(envelope)) {
        using enum EnvelopeState;
    case Off:
        return 0;
    case Attack:
        return envelope.attack();
    case Decay:
        return (1. - m_sustain) * envelope.decay() + m_sustain;
    case Sustain:
        return m_sustain;
    case Release:
        return (1 - envelope.release()) * m_sustain;
    }
    VERIFY_NOT_REACHED();
}

double Classic::wave_position(u8 note)
{
    switch (m_waveform) {
    case Sine:
        return sin_position(note);
    case Triangle:
        return triangle_position(note);
    case Square:
        return square_position(note);
    case Saw:
        return saw_position(note);
    case Noise:
        return noise_position(note);
    }
    VERIFY_NOT_REACHED();
}

double Classic::samples_per_cycle(u8 note)
{
    return note_frequencies[note] / m_transport->sample_rate();
}

// Just a sin() function
double Classic::sin_position(u8 note)
{
    double cycle_pos = m_transport->time() / samples_per_cycle(note);
    return AK::sin(cycle_pos * 2 * AK::Pi<double>);
}

// Absolute value of the saw wave "flips" the negative portion into the positive, creating a ramp up and down.
double Classic::triangle_position(u8 note)
{
    double saw = saw_position(note);
    return AK::fabs(saw) * 2 - 1;
}

// The first half of the cycle period is 1, the other half -1.
double Classic::square_position(u8 note)
{
    double progress = AK::fmod(static_cast<double>(m_transport->time()), samples_per_cycle(note)) / samples_per_cycle(note);
    return progress >= 0.5 ? -1 : 1;
}

// Modulus creates inverse saw, which we need to flip and scale.
double Classic::saw_position(u8 note)
{
    double unscaled = samples_per_cycle(note) - AK::fmod(static_cast<double>(m_transport->time()), samples_per_cycle(note));
    return unscaled / (samples_per_cycle(note) / 2.) - 1;
}

// We resample the noise twice per cycle.
double Classic::noise_position(u8 note)
{
    return (get_random_from_seed(round(m_transport->time() / (samples_per_cycle(note) / 2)) * (samples_per_cycle(note) / 2)) - .5) * 2;
}

double Classic::get_random_from_seed(u64 seed)
{
    u64 const a = 6364136223846793005;
    u64 const b = 2531011;
    return (a * seed + b) / static_cast<double>(NumericLimits<u64>().max());
}

}
