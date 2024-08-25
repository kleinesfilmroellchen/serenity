/*
 * Copyright (c) 2024, kleines Filmröllchen <filmroellchen@serenityos.org>
 * Copyright (c) 2024, sdomi <ja@sdomi.pl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/MACAddress.h>
#include <Kernel/Net/IPv4/IPv6.h>

// https://www.rfc-editor.org/rfc/rfc4443

// Section 2.1
enum class ICMPv6Type {
    DestinationUnreachable = 1,
    PacketTooBig = 2,
    TimeExceeded = 3,
    ParameterProblem = 4,
    EchoRequest = 128,
    EchoReply = 129,
    NeighborSolicitation = 135,
};

class [[gnu::packed]] ICMPv6Header {
public:
    ICMPv6Header() = default;
    ~ICMPv6Header() = default;

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
};

static_assert(AssertSize<ICMPv6Header, 4>());

struct [[gnu::packed]] ICMPv6EchoPacket {
    ICMPv6Header header;
    NetworkOrdered<u16> identifier;
    NetworkOrdered<u16> sequence_number;

    void* payload() { return this + sizeof(ICMPv6EchoPacket); }
    void const* payload() const { return this + sizeof(ICMPv6EchoPacket); }
};

static_assert(AssertSize<ICMPv6EchoPacket, 8>());

struct [[gnu::packed]] IPv6NeighborSolicitation {
    ICMPv6Header header;
    u32 reserved;
    IPv6Address target_address;

    MACAddress* source_link_layer_address() { return bit_cast<MACAddress*>(this + sizeof(IPv6NeighborSolicitation)); }
    MACAddress const* source_link_layer_address() const { return bit_cast<MACAddress const*>(this + sizeof(IPv6NeighborSolicitation)); }
};

static_assert(AssertSize<IPv6NeighborSolicitation, 6 * 32 / 8>());
