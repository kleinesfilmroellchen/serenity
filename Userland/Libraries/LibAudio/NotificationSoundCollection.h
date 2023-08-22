/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/FixedStringBuffer.h>
#include <AK/NonnullOwnPtr.h>
#include <LibAudio/FlacLoader.h>
#include <LibAudio/FlacTypes.h>
#include <LibAudio/FlacWriter.h>
#include <LibAudio/LoaderError.h>

namespace Audio::Notifications {

using SoundID = FixedStringBuffer<16>;

// “SerenityOS Sound Notifications”
constexpr StringView application_block_id = "SOSN"sv;

// All recognized standard sound IDs.
enum class StandardSoundID {
    Error,
    Startup,
    Notification,
    PlugIn,
    PlugOut,
    VolumeCheck,
};

SoundID id_for(StandardSoundID standard_id);

// A single notification sound.
struct Sound {
    SoundID id;
    FixedArray<Sample> audio;

    static ErrorOr<Sound> create(StandardSoundID standard_id, FixedArray<Sample> audio);
    static ErrorOr<Sound> create(Utf8View custom_id, FixedArray<Sample> audio);
    static ErrorOr<Sound> create(SoundID custom_id, FixedArray<Sample> audio);
};

// A collection of notification sounds, stored within a FLAC file.
class SoundCollection {
public:
    SoundCollection() = default;

    static ErrorOr<SoundCollection, LoaderError> load(NonnullOwnPtr<SeekableStream> stream);
    ErrorOr<void> write(NonnullOwnPtr<SeekableStream> stream);

    // Overwrites any existing sound with that ID.
    ErrorOr<void> add_sound(SoundID id, ReadonlySpan<Sample> audio);
    // Returns the removed sound.
    Optional<Sound> remove_sound(SoundID id);

    Optional<Sound&> sound_by_id(SoundID id) { return m_sounds.get(id); }
    Optional<Sound const&> sound_by_id(SoundID id) const { return m_sounds.get(id); }

    OrderedHashMap<SoundID, Sound>::ConstIteratorType begin() const { return m_sounds.begin(); }
    OrderedHashMap<SoundID, Sound>::ConstIteratorType end() const { return m_sounds.end(); }
    OrderedHashMap<SoundID, Sound>::IteratorType begin() { return m_sounds.begin(); }
    OrderedHashMap<SoundID, Sound>::IteratorType end() { return m_sounds.end(); }

    StringView name() const { return m_name; }
    void set_name(String name) { m_name = move(name); }

    u32 sample_rate() const { return m_sample_rate; }
    void set_sample_rate(u32 sample_rate) { m_sample_rate = sample_rate; }

private:
    ErrorOr<void> serialize_sosn_block(Stream& stream);

    OrderedHashMap<SoundID, Sound> m_sounds;
    String m_name;
    u32 m_sample_rate { 44100 };
};

}
