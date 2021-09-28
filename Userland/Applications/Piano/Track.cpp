/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2019-2020, William McPherson <willmcpherson2@gmail.com>
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Track.h"
#include "Music.h"
#include <AK/Math.h>
#include <AK/NonnullRefPtr.h>
#include <AK/NumericLimits.h>
#include <LibAudio/Loader.h>
#include <LibDSP/Music.h>
#include <math.h>

Track::Track(const u32& time)
    : m_time(time)
    , m_temporary_transport(make_ref_counted<LibDSP::Transport>(120, 4))
    , m_delay(make_ref_counted<LibDSP::Effects::Delay>(m_temporary_transport))
    , m_synth(make_ref_counted<LibDSP::Synthesizers::Classic>(m_temporary_transport))
{
    set_volume(volume_max);
}

Track::~Track()
{
}

void Track::fill_sample(Sample& sample)
{
    m_temporary_transport->time() = m_time;

    Audio::Sample new_sample;
    auto playing_notes = LibDSP::RollNotes {};

    for (size_t i = 0; i < note_count; ++i) {
        auto& notes_at_pitch = m_roll_notes[i];
        for (auto& note : notes_at_pitch) {
            if (note.is_playing(m_time))
                playing_notes.set(i, note);
        }
        auto& key_at_pitch = m_keyboard_notes[i];
        if (key_at_pitch.has_value() && key_at_pitch.value().is_playing(m_time))
            playing_notes.set(i, key_at_pitch.value());
        // No need to keep non-playing keyboard notes around.
        else
            m_keyboard_notes[i] = {};
    }

    auto synthesized_sample = m_synth->process(playing_notes).get<LibDSP::Sample>();
    auto delayed_sample = m_delay->process(synthesized_sample).get<LibDSP::Sample>();

    // HACK: Convert to old Piano datastructures
    new_sample.left = delayed_sample.left * NumericLimits<i16>::max();
    new_sample.right = delayed_sample.right * NumericLimits<i16>::max();

    new_sample.left = clamp(new_sample.left, NumericLimits<i16>::min(), NumericLimits<i16>::max());
    new_sample.right = clamp(new_sample.right, NumericLimits<i16>::min(), NumericLimits<i16>::max());

    sample.left += new_sample.left;
    sample.right += new_sample.right;

    // TODO: Master processor
    sample.left *= m_volume / static_cast<double>(volume_max) * volume_factor;
    sample.right *= m_volume / static_cast<double>(volume_max) * volume_factor;
}

void Track::reset()
{
    for (size_t note = 0; note < note_count; ++note)
        m_roll_iterators[note] = m_roll_notes[note].begin();
}

// FIXME: Dead code for now
String Track::set_recorded_sample(const StringView& path)
{
    NonnullRefPtr<Audio::Loader> loader = Audio::Loader::create(path);
    if (loader->has_error())
        return String(loader->error_string());
    auto buffer = loader->get_more_samples(60 * loader->sample_rate()); // 1 minute maximum
    if (loader->has_error())
        return String(loader->error_string());
    // Resample to Piano's internal sample rate
    auto resampler = Audio::ResampleHelper<double>(loader->sample_rate(), sample_rate);
    buffer = Audio::resample_buffer(resampler, *buffer);

    if (!m_recorded_sample.is_empty())
        m_recorded_sample.clear();
    m_recorded_sample.resize(buffer->sample_count());

    double peak = 0;
    for (int i = 0; i < buffer->sample_count(); ++i) {
        double left_abs = fabs(buffer->samples()[i].left);
        double right_abs = fabs(buffer->samples()[i].right);
        if (left_abs > peak)
            peak = left_abs;
        if (right_abs > peak)
            peak = right_abs;
    }

    if (peak) {
        for (int i = 0; i < buffer->sample_count(); ++i) {
            m_recorded_sample[i].left = buffer->samples()[i].left / peak;
            m_recorded_sample[i].right = buffer->samples()[i].right / peak;
        }
    }

    return String::empty();
}

void Track::sync_roll(int note)
{
    auto it = m_roll_notes[note].find_if([&](auto& roll_note) { return roll_note.off_sample > m_time; });
    if (it.is_end())
        m_roll_iterators[note] = m_roll_notes[note].begin();
    else
        m_roll_iterators[note] = it;
}

void Track::set_roll_note(int note, u32 on_sample, u32 off_sample)
{
    RollNote new_roll_note = { on_sample, off_sample, (u8)note, 0 };

    VERIFY(note >= 0 && note < note_count);
    VERIFY(new_roll_note.off_sample < roll_length);
    VERIFY(new_roll_note.length() >= 2);

    for (auto it = m_roll_notes[note].begin(); !it.is_end();) {
        if (it->on_sample > new_roll_note.off_sample) {
            m_roll_notes[note].insert_before(it, new_roll_note);
            sync_roll(note);
            return;
        }
        if (it->on_sample <= new_roll_note.on_sample && it->off_sample >= new_roll_note.on_sample) {
            it.remove(m_roll_notes[note]);
            sync_roll(note);
            return;
        }
        if ((new_roll_note.on_sample == 0 || it->on_sample >= new_roll_note.on_sample - 1) && it->on_sample <= new_roll_note.off_sample) {
            it.remove(m_roll_notes[note]);
            it = m_roll_notes[note].begin();
            continue;
        }
        ++it;
    }

    m_roll_notes[note].append(new_roll_note);
    sync_roll(note);
}

void Track::set_keyboard_note(int note, Switch state)
{
    VERIFY(note >= 0 && note < note_count);
    if (state == Switch::Off) {
        // If the note is playing, we need to start releasing it, otherwise just delete
        if (auto& maybe_roll_note = m_keyboard_notes[note]; maybe_roll_note.has_value()) {
            auto& roll_note = maybe_roll_note.value();
            if (roll_note.is_playing(m_time))
                roll_note.off_sample = m_time;
            else
                m_keyboard_notes[note] = {};
        }
    } else
        // FIXME: The end time needs to be far in the future.
        m_keyboard_notes[note]
            = RollNote { m_time, m_time + static_cast<u32>(sample_rate) * 10'000, static_cast<u8>(note), 0 };
}

void Track::set_volume(int volume)
{
    VERIFY(volume >= 0);
    m_volume = volume;
}
