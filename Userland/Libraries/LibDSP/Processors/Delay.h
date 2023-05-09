/*
 * Copyright (c) 2021-2022, kleines Filmröllchen <filmroellchen@serenityos.org>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibDSP/Processor.h>

namespace DSP::Effects {

// A simple digital delay effect using a delay buffer.
// This is based on Piano's old built-in delay.
class Delay : public EffectProcessor {
public:
    Delay(NonnullRefPtr<Transport>);

private:
    virtual void process_impl(Signal const&, Signal&) override;
    void handle_delay_time_change();

    ProcessorRangeParameter m_delay_decay;
    ProcessorRangeParameter m_delay_time;
    ProcessorRangeParameter m_dry_gain;

    Vector<Sample> m_delay_buffer;
    size_t m_delay_index { 0 };
};

}
