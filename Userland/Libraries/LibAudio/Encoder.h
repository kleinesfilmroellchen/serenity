/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Span.h>
#include <LibAudio/Sample.h>

namespace Audio {

class Encoder {
public:
    virtual ~Encoder() = default;

    virtual ErrorOr<void> write_samples(Span<Sample> samples) = 0;

    virtual ErrorOr<void> finalize() = 0;
};

}
