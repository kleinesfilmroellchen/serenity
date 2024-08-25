/*
 * Copyright (c) 2024, sdomi <ja@sdomi.pl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/MACAddress.h>
#include <Kernel/Net/IPv4/IPv6.h>

// https://www.rfc-editor.org/rfc/rfc4443#section-2.1

struct ICMPv6Type {
    enum {
        DestinationUnreachable = 1,
        PacketTooBig = 2,
        TimeExceeded = 3,
        ParameterProblem = 4,
        EchoRequest = 128,
        EchoReply = 129
    };
};

class [[gnu::packed]] ICMPHeader {
public:
    ICMPHeader() = default;
    ~ICMPHeader() = default;

    u8 type() const { return m_type; }
    void set_type(u8 b) { m_type = b; }

    u8 code() const { return m_code; }
    void set_code(u8 b) { m_code = b; }

    u16 checksum() const { return m_checksum; }
    void set_checksum(u16 w) { m_checksum = w; }

    void const* payload() const { return this + 1; }
    void* payload() { return this + 1; }

private:
    u8 m_type { 0 };
    u8 m_code { 0 };
    NetworkOrdered<u16> m_checksum { 0 };
    // NOTE: The rest of the header is 4 bytes
};

static_assert(AssertSize<ICMPHeader, 4>());

struct [[gnu::packed]] ICMPEchoPacket {
    ICMPHeader header;
    NetworkOrdered<u16> identifier;
    NetworkOrdered<u16> sequence_number;
    void* payload() { return this + 1; }
    void const* payload() const { return this + 1; }
};
