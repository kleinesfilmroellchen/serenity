/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibDSP/Processor.h>
#include <LibDSP/Processors/BandAdjustment.h>

namespace DSP::Effects {

// A parametric equalizer consisting of 8 band adjustment filters.
// FIXME: Implement offset and LP/HP filters and allow changing the filter types.
class ParametricEQ : public EffectProcessor {
public:
    static constexpr size_t filter_count = 8;

    ParametricEQ(NonnullRefPtr<Transport>);
    virtual ~ParametricEQ() = default;

    virtual ErrorOr<void> resize_internal_buffers_to(size_t) override;

private:
    virtual void process_impl(Signal const&, Signal&) override;

    Array<NonnullRefPtr<BandAdjustment>, filter_count> m_filters;
    // Second buffer to use as input/output while we pass the signal through the filters.
    Signal m_filter_buffer { FixedArray<Sample> {} };
};

}
