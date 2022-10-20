/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "LibAudio/LoaderError.h"
#include <AK/ByteBuffer.h>
#include <AK/FixedArray.h>
#include <LibAudio/Loader.h>
#include <LibAudio/SampleFormats.h>
#include <LibAudio/StreamDecoder.h>
#include <LibCore/Stream.h>

namespace Audio {

// A decoder for a stream of raw PCM samples of various formats. In files, this kind of data is usually found in WAV.
class PCMStreamDecoder : public StreamDecoder {
public:
    PCMStreamDecoder(PcmSampleFormat sample_format, unsigned channels)
        : m_channels(channels)
        , m_sample_format(sample_format)
        , m_bytes_per_sample(pcm_bits_per_sample(sample_format))
    {
    }
    virtual ~PCMStreamDecoder() = default;

    virtual LoaderSamples decode(Core::Stream::Stream& encoded_stream) const override
    {
        Vector<Sample> samples;
        auto const bytes_per_interchannel_sample = m_channels * m_bytes_per_sample;
        auto interchannel_sample_buffer = LOADER_TRY(ByteBuffer::create_uninitialized(bytes_per_interchannel_sample));

        while (!encoded_stream.is_eof()) {
            auto read_size = LOADER_TRY(encoded_stream.read(interchannel_sample_buffer)).size();
            if (read_size == 0)
                break;
            if (read_size < bytes_per_interchannel_sample)
                return LoaderError { LoaderError::Category::Format, "Cut-off interchannel sample at end of chunk" };
        }

        return LOADER_TRY(FixedArray<Sample>::try_create(samples.span()));
    }

private:
    unsigned m_channels;
    PcmSampleFormat m_sample_format;
    size_t m_bytes_per_sample;
};

}
