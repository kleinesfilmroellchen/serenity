/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/HashMap.h>
#include <LibDSP/Processor.h>
#include <LibDSP/Synthesizers.h>

namespace LibDSP::Synthesizers {

Classic::Classic(NonnullRefPtr<Transport> transport)
    : LibDSP::SynthesizerProcessor(transport)
    , m_waveform("Waveform"sv, Waveform::Sine)
{
    m_parameters.append(m_waveform);
}

Signal Classic::process_impl(Signal const& input_signal)
{
    auto& in = input_signal.get<OrderedHashMap<u16, RollNote>>();

    Sample out;

    // "Press" the necessary notes in the internal representation,
    // and "release" all of the others
    for (size_t i = 0; i < note_count; ++i) {
        if (in.has(i))
    }

    // Pressed notes

    return Sample {};
}

}
