/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/DeprecatedString.h>
#include <AK/Types.h>

namespace Disassembly {

class SymbolProvider {
public:
    virtual DeprecatedString symbolicate(FlatPtr, u32* offset = nullptr) const = 0;

protected:
    virtual ~SymbolProvider() = default;
};

}
