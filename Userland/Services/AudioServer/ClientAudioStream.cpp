/*
 * Copyright (c) 2018-2022, the SerenityOS developers.
 * Copyright (c) 2021-2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ClientAudioStream.h"

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
    if (m_resampler.has_value() && m_resampler->ratio() == m_sample_rate / static_cast<float>(audiodevice_sample_rate))
        return {};

    dbgln_if(AUDIO_DEBUG, "Constructing new resampler from {} Hz to {} Hz for client {}", m_sample_rate, audiodevice_sample_rate, m_client->client_id());
    m_resampler = TRY((Resampler::create(m_sample_rate, audiodevice_sample_rate, Audio::AUDIO_BUFFER_SIZE, Resampler::SincFunction::sinc_taps, 2000)));

    // The old chunk data is at a wrong sample rate, discard it.
    // This reduces glitches in running playbacks when the sample rate is changed.
    m_current_audio_chunk.clear_with_capacity();
    return {};
}

ErrorOr<void, ClientAudioStream::ErrorState> ClientAudioStream::resample_into_current_chunk(ReadonlySpan<Audio::Sample> new_buffer, u32 audiodevice_sample_rate)
{
    // Skip the slow resampler if possible.
    if (audiodevice_sample_rate == m_sample_rate) {
        auto resize_result = m_current_audio_chunk.try_resize_and_keep_capacity(new_buffer.size());
        if (resize_result.is_error())
            return ErrorState::ResamplingError;

        new_buffer.copy_to(m_current_audio_chunk.span());
    } else {
        auto resize_result = m_current_audio_chunk.try_resize_and_keep_capacity(static_cast<size_t>(AK::ceil(Audio::AUDIO_BUFFER_SIZE / m_resampler->ratio())));
        if (resize_result.is_error())
            return ErrorState::ResamplingError;

        auto const actual_size = m_resampler->process(new_buffer, m_current_audio_chunk);

        resize_result = m_current_audio_chunk.try_resize_and_keep_capacity(actual_size);
        if (resize_result.is_error())
            return ErrorState::ResamplingError;
    }

    m_in_chunk_location = 0;
    return {};
}

ErrorOr<Audio::Sample, ClientAudioStream::ErrorState> ClientAudioStream::get_next_sample(u32 audiodevice_sample_rate)
{
    // Note: Even though we only check client state here, we will probably close the client much earlier.
    if (!is_connected())
        return ErrorState::ClientDisconnected;

    if (m_paused)
        return ErrorState::ClientUnderrun;

    // If the sample rate changes underneath us, we will still play the existing buffer unchanged until we're done.
    // This is not a significant problem since the buffers are very small (~100 samples or less).
    auto ensure_result = ensure_resampler(audiodevice_sample_rate);
    if (ensure_result.is_error())
        return ErrorState::ResamplingError;

    if (m_in_chunk_location >= m_current_audio_chunk.size()) {
        auto result = m_buffer->dequeue();
        if (result.is_error()) {
            dbgln_if(AUDIO_DEBUG, "Audio client {} can't keep up!", m_client->client_id());
            return ErrorState::ClientUnderrun;
        }
        TRY(resample_into_current_chunk(result.value().span(), audiodevice_sample_rate));
    }

    return m_current_audio_chunk[m_in_chunk_location++];
}

void ClientAudioStream::set_buffer(NonnullOwnPtr<Audio::AudioQueue> buffer)
{
    m_buffer = move(buffer);
}

void ClientAudioStream::clear()
{
    ErrorOr<Array<Audio::Sample, Audio::AUDIO_BUFFER_SIZE>, Audio::AudioQueue::QueueStatus> result = Audio::AudioQueue::QueueStatus::Empty;
    do {
        result = m_buffer->dequeue();
    } while (!result.is_error());
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
