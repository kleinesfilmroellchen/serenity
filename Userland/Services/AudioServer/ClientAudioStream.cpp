/*
 * Copyright (c) 2018-2022, the SerenityOS developers.
 * Copyright (c) 2021-2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ClientAudioStream.h"
#include <LibDSP/Resampler.h>

namespace AudioServer {

ClientAudioStream::ClientAudioStream(ConnectionFromClient& client)
    : m_client(client)
{
}

Optional<ConnectionFromClient&> ClientAudioStream::client()
{
    return m_client.has_value() ? *m_client : Optional<ConnectionFromClient&> {};
}

bool ClientAudioStream::is_connected() const
{
    return m_client && m_client->is_open();
}

ErrorOr<void> ClientAudioStream::ensure_resampler(u32 audiodevice_sample_rate)
{
    if (m_resampler.has_value() && m_resampler->ratio() == m_sample_rate / static_cast<double>(audiodevice_sample_rate))
        return {};

    auto sinc_function = TRY(DSP::InterpolatedSinc::create(13, 512));
    m_resampler = TRY((DSP::SincResampler<Audio::Sample>::create(m_sample_rate, audiodevice_sample_rate, Audio::AUDIO_BUFFER_SIZE, move(sinc_function), 13, 2000)));

    return {};
}

ErrorOr<Audio::Sample> ClientAudioStream::get_next_sample(u32 audiodevice_sample_rate)
{
    // If the sample rate changes underneath us, we will still play the existing buffer unchanged until we're done.
    // This is not a significant problem since the buffers are very small (~100 samples or less).
    TRY(ensure_resampler(audiodevice_sample_rate));

    // Note: Even though we only check client state here, we will probably close the client much earlier.
    if (!is_connected())
        return Error::from_string_view("Audio client disconnected"sv);

    if (m_paused)
        return Error::from_string_view("Audio client paused"sv);

    if (m_in_chunk_location >= m_current_audio_chunk.size()) {
        auto result = m_buffer->dequeue();
        if (result.is_error()) {
            if (result.error() == Audio::AudioQueue::QueueStatus::Empty) {
                dbgln_if(AUDIO_DEBUG, "Audio client {} can't keep up!", m_client->client_id());
            }

            return Error::from_string_view("Audio client can't keep up"sv);
        }
        TRY(m_current_audio_chunk.try_resize_and_keep_capacity(static_cast<size_t>(ceil(m_resampler->ratio() * Audio::AUDIO_BUFFER_SIZE))));
        auto actual_size = m_resampler->process(result.value().span(), m_current_audio_chunk);
        TRY(m_current_audio_chunk.try_resize_and_keep_capacity(actual_size));

        m_in_chunk_location = 0;
    }

    return m_current_audio_chunk[m_in_chunk_location++];
}

void ClientAudioStream::set_buffer(NonnullOwnPtr<Audio::AudioQueue> buffer)
{
    m_buffer = move(buffer);
}

void ClientAudioStream::clear()
{
    ErrorOr<Array<Audio::Sample, Audio::AUDIO_BUFFER_SIZE>, Audio::AudioQueue::QueueStatus> result = Audio::AudioQueue::QueueStatus::Invalid;
    do {
        result = m_buffer->dequeue();
    } while (!result.is_error() || result.error() != Audio::AudioQueue::QueueStatus::Empty);
}

void ClientAudioStream::set_paused(bool paused)
{
    m_paused = paused;
}

FadingProperty<double>& ClientAudioStream::volume()
{
    return m_volume;
}

double ClientAudioStream::volume() const
{
    return m_volume;
}

void ClientAudioStream::set_volume(double const volume)
{
    m_volume = volume;
}

bool ClientAudioStream::is_muted() const
{
    return m_muted;
}

void ClientAudioStream::set_muted(bool muted)
{
    m_muted = muted;
}

u32 ClientAudioStream::sample_rate() const
{
    return m_sample_rate;
}

void ClientAudioStream::set_sample_rate(u32 sample_rate)
{
    dbgln_if(AUDIO_DEBUG, "queue {} got sample rate {} Hz", m_client->client_id(), sample_rate);
    m_sample_rate = sample_rate;
}

}
