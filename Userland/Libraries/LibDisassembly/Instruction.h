/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/DeprecatedString.h>
#include <AK/Types.h>
#include <LibDisassembly/SymbolProvider.h>

namespace Disassembly {

class Instruction {
public:
    virtual ~Instruction() = default;

    virtual DeprecatedString to_deprecated_string(u32 origin, Optional<SymbolProvider const&> = {}) const = 0;
    virtual DeprecatedString mnemonic() const = 0;
    virtual size_t length() const = 0;
};

}
