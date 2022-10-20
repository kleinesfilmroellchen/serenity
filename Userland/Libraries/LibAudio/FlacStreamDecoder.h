/*
 * Copyright (c) 2022, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibAudio/StreamDecoder.h>
namespace Audio {

// A stream decoder capable of decoding FLAC streams. This decoder supports the streamable subset of FLAC as defined by the specification.
class FlacStreamDecoder : public StreamDecoder {
};

}
