/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibAudio/Loader.h>
#include <LibCore/Stream.h>

namespace Audio {

// An interface for decoding audio data from a stateless stream. This is the main difference to the loaders, which are stateful.
// Note that many loaders are in fact implemented in terms of a modified streaming decoder.
class StreamDecoder {
public:
    virtual ~StreamDecoder() = default;

    // Decode audio data from all of the given encoded audio stream.
    virtual LoaderSamples decode(Core::Stream::Stream& encoded_stream) const;
};

}
