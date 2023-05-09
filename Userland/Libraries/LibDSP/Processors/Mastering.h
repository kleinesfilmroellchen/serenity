/*
 * Copyright (c) 2021-2023, kleines Filmröllchen <filmroellchen@serenityos.org>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibDSP/Processor.h>

namespace DSP::Effects {

// A simple effect that applies volume, mute and pan to its input signal.
// Convenient for attenuating signals in the middle of long chains.
class Mastering : public EffectProcessor {
public:
    Mastering(NonnullRefPtr<Transport>);

    // The mastering processor can be used by the track and therefore needs to be able to write to a fixed array directly.
    // Otherwise, Track needs to do more unnecessary sample data copies.
    void process_to_fixed_array(Signal const&, FixedArray<Sample>&);

private:
    virtual void process_impl(Signal const&, Signal&) override;

    ProcessorRangeParameter m_pan;
    ProcessorRangeParameter m_volume;
    ProcessorBooleanParameter m_muted;
};

}
