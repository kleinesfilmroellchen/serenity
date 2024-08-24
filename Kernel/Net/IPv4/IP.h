/*
 * Copyright (c) 2024, kleines Filmröllchen <filmroellchen@serenityos.org>
 * Copyright (c) 2024, sdomi <ja@sdomi.pl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Kernel {

enum class TransportProtocol : u16 {
    ICMP = 1,
    TCP = 6,
    UDP = 17,
};

}
