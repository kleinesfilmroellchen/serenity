/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>
#include <LibDSP/BandAdjustmentFilter.h>
#include <LibDSP/Processor.h>
#include <LibDSP/ProcessorParameter.h>
#include <LibDSP/Transport.h>

namespace DSP::Effects {

class BandAdjustment : public EffectProcessor {
public:
    BandAdjustment(NonnullRefPtr<Transport>);
    ~BandAdjustment() = default;

private:
    virtual void process_impl(Signal const&, Signal&) override;

    BandAdjustmentFilter m_filter;
    ProcessorRangeParameter m_frequency;
    ProcessorRangeParameter m_gain_db;
    ProcessorRangeParameter m_q;
};

}
