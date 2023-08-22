/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "NotificationSoundCollection.h"
#include <AK/MemoryStream.h>
#include <AK/QuickSort.h>

namespace Audio::Notifications {

SoundID id_for(StandardSoundID standard_id)
{
    static HashMap<StandardSoundID, SoundID> standard_id_names {
        { StandardSoundID::Error, "error" },
        { StandardSoundID::Startup, "startup" },
        { StandardSoundID::Notification, "notification" },
        { StandardSoundID::PlugIn, "plug-in" },
        { StandardSoundID::PlugOut, "plug-out" },
    };
    return standard_id_names.get(standard_id).value();
}

ErrorOr<Sound> Sound::create(Utf8View custom_id, FixedArray<Sample> audio)
{
    if (custom_id.byte_length() > SoundID::fixed_length())
        return Error::from_string_view("Too long sound ID"sv);

    SoundID id { custom_id.as_string() };
    return Sound::create(id, move(audio));
}

ErrorOr<Sound> Sound::create(StandardSoundID standard_id, FixedArray<Sample> audio)
{
    auto id = id_for(standard_id);
    return Sound::create(id, move(audio));
}

ErrorOr<Sound> Sound::create(SoundID id, FixedArray<Sample> audio)
{
    return Sound {
        .id = id,
        .audio = move(audio),
    };
}

Optional<Sound> SoundCollection::remove_sound(SoundID id)
{
    auto old_sound = m_sounds.take(id);
    return old_sound;
}

ErrorOr<void> SoundCollection::add_sound(SoundID id, ReadonlySpan<Sample> audio)
{
    auto fixed_array_audio = TRY(FixedArray<Sample>::create(audio));
    TRY(m_sounds.try_set(id, TRY(Sound::create(id, move(fixed_array_audio)))));
    return {};
}

ErrorOr<void> SoundCollection::serialize_sosn_block(Stream& stream)
{
    u64 last_audio_end = 0;

    for (auto const& sound : m_sounds) {
        auto const& id = sound.value.id;
        auto audio_size = sound.value.audio.size();
        TRY(stream.write_value<BigEndian<u64>>(last_audio_end));
        TRY(stream.write_until_depleted(id.storage()));

        last_audio_end += audio_size;
    }

    return {};
}

ErrorOr<SoundCollection, LoaderError> SoundCollection::load(NonnullOwnPtr<SeekableStream> stream)
{
    auto loader = TRY(Audio::Loader::create(move(stream)));
    auto maybe_flac_plugin = loader->plugin<FlacLoaderPlugin>();
    if (!maybe_flac_plugin.has_value())
        return LoaderError { LoaderError::Category::Format, "Only FLAC-based collections are supported" };

    auto const& flac_plugin = maybe_flac_plugin.release_value();
    auto maybe_sosn_block = flac_plugin.data_for_application(application_block_id);
    if (!maybe_sosn_block.has_value())
        return LoaderError { LoaderError::Category::Format, "FLAC file is missing the sound collection information block (SOSN block)" };
    auto sosn_block = maybe_sosn_block.release_value();
    SoundCollection collection;

    FixedMemoryStream sosn_stream { sosn_block };

    // Since entries can be in any order, we first read in the descriptors
    // and then sort them by their sample start index to more easily read in the audio data.
    struct SOSNEntry {
        u64 audio_start;
        SoundID id;
    };
    Vector<SOSNEntry> entries;

    while (!sosn_stream.is_eof()) {
        auto audio_start = TRY(sosn_stream.read_value<BigEndian<u64>>());
        SoundID id;
        TRY(sosn_stream.read_until_filled(id.storage()));
        TRY(entries.try_append({
            .audio_start = audio_start,
            .id = id,
        }));
    }

    AK::quick_sort(entries, [](auto const& a, auto const& b) {
        return static_cast<i64>(a.audio_start) - static_cast<i64>(b.audio_start);
    });

    for (size_t i = 0; i < entries.size(); ++i) {
        auto const& entry = entries[i];
        size_t sample_count;
        if (i < entries.size() - 1) {
            auto audio_end = entries[i + 1].audio_start;
            sample_count = audio_end - entry.audio_start;
        } else {
            sample_count = loader->total_samples() - entry.audio_start;
        }
        TRY(loader->seek(entry.audio_start));
        auto audio_data = TRY(loader->get_more_samples(sample_count));
        TRY(collection.m_sounds.try_set(entry.id,
            Sound {
                .id = entry.id,
                .audio = move(audio_data),
            }));
    }

    auto metadata = loader->metadata();
    collection.set_name(metadata.title.value_or({}));

    return collection;
}

ErrorOr<void> SoundCollection::write(NonnullOwnPtr<SeekableStream> stream)
{
    auto writer = TRY(FlacWriter::create(move(stream)));

    size_t total_samples = 0;
    for (auto const& sound : m_sounds)
        total_samples += sound.value.audio.size();
    TRY(writer->set_sample_rate(m_sample_rate));
    // FIXME: Figure out why an extra factor 2 is necessary here for our “flush” seekpoints.
    writer->sample_count_hint(total_samples + static_cast<size_t>(m_sounds.size() * FlacWriter::seekpoint_period_seconds * m_sample_rate * 2));

    // SOSN application metadata block
    AllocatingMemoryStream sosn_stream;
    TRY(serialize_sosn_block(sosn_stream));
    auto sosn_block = TRY(sosn_stream.read_until_eof());
    TRY(writer->add_application_block(application_block_id, move(sosn_block)));

    Metadata metadata;
    metadata.replace_encoder_with_serenity();
    metadata.title = m_name;
    TRY(writer->set_bits_per_sample(16));
    TRY(writer->set_num_channels(2));
    TRY(writer->set_metadata(metadata));
    TRY(writer->finalize_header_format());

    for ([[maybe_unused]] auto const& sound : m_sounds) {
        TRY(writer->write_samples(sound.value.audio.span()));
        TRY(writer->flush_samples_with_padding());
    }

    TRY(writer->finalize());

    return {};
}

}
