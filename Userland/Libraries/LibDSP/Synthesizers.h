/*
 * Copyright (c) 2021, kleines Filmröllchen <malu.bertsch@gmail.com>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/SinglyLinkedList.h>
#include <LibDSP/Processor.h>
#include <LibDSP/ProcessorParameter.h>
#include <LibDSP/Transport.h>

namespace LibDSP::Synthesizers {

enum Waveform : u8 {
    Sine,
    Triangle,
    Square,
    Saw,
    Noise,
};

class Classic : public SynthesizerProcessor {
public:
    Classic(NonnullRefPtr<Transport>);

private:
    virtual Signal process_impl(Signal const&) override;

    ProcessorEnumParameter<Waveform> m_waveform;
    ProcessorRangeParameter m_attack;
    ProcessorRangeParameter m_decay;
    ProcessorRangeParameter m_sustain;
    ProcessorRangeParameter m_release;

    Array<Envelope, note_count> m_envelopes;
};

}
