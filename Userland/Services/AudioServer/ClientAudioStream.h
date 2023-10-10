/*
 * Copyright (c) 2018-2022, the SerenityOS developers.
 * Copyright (c) 2021-2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "ConnectionFromClient.h"
#include "FadingProperty.h"
#include <AK/Atomic.h>
#include <AK/Debug.h>
#include <AK/RefCounted.h>
#include <AK/WeakPtr.h>
#include <LibAudio/Queue.h>
#include <LibAudio/Sample.h>
#include <LibDSP/Resampler.h>

namespace AudioServer {

class ClientAudioStream : public RefCounted<ClientAudioStream> {
public:
    enum class ErrorState {
        ClientDisconnected,
        ClientPaused,
        ClientUnderrun,
        ResamplingError,
    };

    explicit ClientAudioStream(ConnectionFromClient&);
    ~ClientAudioStream() = default;

    ErrorOr<Audio::Sample, ErrorState> get_next_sample(u32 audiodevice_sample_rate);
    void clear();

    bool is_connected() const;

    Optional<ConnectionFromClient&> client();

    void set_buffer(NonnullOwnPtr<Audio::AudioQueue> buffer);

    void set_paused(bool paused);
    FadingProperty<double>& volume();
    double volume() const;
    void set_volume(double volume);
    bool is_muted() const;
    void set_muted(bool muted);
    u32 sample_rate() const;
    void set_sample_rate(u32 sample_rate);

private:
    // Use less than the recommended taps so that we have less taps than the input size.
    using Resampler = DSP::InterpolatedSincResampler<Audio::Sample, DSP::recommended_float_sinc_taps, DSP::recommended_float_oversample>;

    ErrorOr<void> ensure_resampler(u32 audiodevice_sample_rate);
    ErrorOr<void, ClientAudioStream::ErrorState> resample_into_current_chunk(ReadonlySpan<Audio::Sample> new_buffer, u32 audiodevice_sample_rate);

    OwnPtr<Audio::AudioQueue> m_buffer;
    Vector<Audio::Sample, Audio::AUDIO_BUFFER_SIZE> m_current_audio_chunk;
    size_t m_in_chunk_location;
    Optional<Resampler> m_resampler {};

    bool m_paused { true };
    bool m_muted { false };
    u32 m_sample_rate;

    WeakPtr<ConnectionFromClient> m_client;
    FadingProperty<double> m_volume { 1 };
};

}
