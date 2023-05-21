/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ParametricEQ.h"

namespace DSP::Effects {

ParametricEQ::ParametricEQ(NonnullRefPtr<Transport> transport)
    : EffectProcessor(transport)
    // FIXME: Find a waỳ to make this OOM-safe and prettier.
    , m_filters({
          make_ref_counted<BandAdjustment>(transport),
          make_ref_counted<BandAdjustment>(transport),
          make_ref_counted<BandAdjustment>(transport),
          make_ref_counted<BandAdjustment>(transport),
          make_ref_counted<BandAdjustment>(transport),
          make_ref_counted<BandAdjustment>(transport),
          make_ref_counted<BandAdjustment>(transport),
          make_ref_counted<BandAdjustment>(transport),
      })
{
    for (auto const& filter : m_filters)
        parameters().extend(filter->parameters());
}

void ParametricEQ::process_impl(Signal const& input_signal, Signal& output_signal)
{
    auto is_first_filter = false;
    for (auto& filter : m_filters) {
        filter->process(is_first_filter ? input_signal : m_filter_buffer, output_signal);
        swap(output_signal, m_filter_buffer);
    }
    // We just swapped the output into the buffer, swap it back. (This is an inexpensive pointer copy.)
    swap(output_signal, m_filter_buffer);
}

ErrorOr<void> ParametricEQ::resize_internal_buffers_to(size_t buffer_size)
{
    m_filter_buffer = TRY(FixedArray<Sample>::create(buffer_size));
    for (auto& filter : m_filters)
        TRY(filter->resize_internal_buffers_to(buffer_size));
    return {};
}

}
