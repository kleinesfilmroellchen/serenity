/*
 * Copyright (c) 2022, kleines Filmröllchen <malu.bertsch@gmail.com>.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/FixedArray.h>
#include <AK/Types.h>
#include <LibDSP/FFT.h>

namespace LibDSP {

// For good references on Overlap-Save see
// https://blog.robertelder.org/overlap-add-overlap-save/
// https://en.wikipedia.org/wiki/Overlap%E2%80%93save_method
template<typename ValueT, size_t L = 16>
class OverlapSaveConvolution {
public:
    using ContainedType = ValueT;
    constexpr OverlapSaveConvolution(FixedArray<ValueT>&& H)
        : m_H(H)
    {
    }

    // Adapted from https://de.wikipedia.org/wiki/Overlap-Save-Verfahren#Pseudocode
    virtual constexpr void convolute(FixedArray<ValueT> const& x, FixedArray<ValueT>& y)
    {
        // H = FFT(h,N)
        // i = 1
        size_t i = 1;
        // Nx = length(x)
        size_t N = x.size();
        // while i <= Nx
        while (i <= N) {
            //     il = min(i+N-1,Nx)
            size_t il = min(i + M() - 1, N);
            //     yt = IFFT( FFT(x(i:il),N) * H, N)
            auto yt = fft(elementwise_multiply(fft(x.span().slice(i, il), false), m_H), true);
            //     y(i : i+N-M) = yt(M : N)
            y.span(i, i + N - M()) = yt.span(M(), N);
            //     i = i+L
            i += L;
            // end
        }
    }

    static constexpr FixedArray<ValueT> elementwise_multiply(FixedArray<ValueT> const& first, FixedArray<ValueT> const& second)
    {
        FixedArray<ValueT> output(first.size());
        for (size_t i = 0; i < first.size(); ++i) {
            output[i] = first[i] * second[i];
        }
        return output;
    }

    // Is this correct?
    constexpr size_t M() const { return m_H.size(); }

protected:
    virtual ~OverlapSaveConvolution() = default;

private:
    FixedArray<ValueT> const m_H;
};

}
