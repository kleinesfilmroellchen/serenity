/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Assertions.h>
#include <AK/Endian.h>
#include <AK/IPv6Address.h>
#include <AK/Types.h>

namespace Kernel {

// Header extensions and special values only.
// For transport protocol numbers, see Kernel::TransportProtocol
enum class IPv6NextHeader : u8 {
    HopByHopOption = 0,
    Routing = 43,
    Fragment = 44,
    NoNextHeader = 59,
    DestinationOptions = 60,
};

// https://datatracker.ietf.org/doc/html/rfc8200#section-3
class [[gnu::packed]] IPv6PacketHeader {
public:
    u8 version() const { return (m_version >> 4) & 0xf; }
    void set_version(u8 version) { m_version = (m_version & 0x0f) | (version << 4); }

    u16 length() const { return m_length; }
    void set_length(u16 length) { m_length = length; }

    u16 flow_label() const { return m_flow_label; }
    void set_flow_label(u16 flow_label) { m_flow_label = flow_label; }

    // Aka. TTL
    u8 hop_limit() const { return m_hop_limit; }
    void set_hop_limit(u8 hop_limit) { m_hop_limit = hop_limit; }

    u8 next_header() const { return m_next_header; }
    void set_next_header(u8 next_header) { m_next_header = next_header; }

    IPv6Address const& source() const { return m_source; }
    void set_source(IPv6Address const& address) { m_source = address; }

    IPv6Address const& destination() const { return m_destination; }
    void set_destination(IPv6Address const& address) { m_destination = address; }

    void* payload() { return this + 1; }
    void const* payload() const { return this + 1; }

    u16 payload_size() const { return m_length - sizeof(IPv6PacketHeader); }

private:
    union {
        struct [[gnu::packed]] {
            u8 m_version : 4;
            u8 m_traffic_class : 8;
            u32 m_flow_label : 20;
        };
        u32 m_version_and_traffic;
    };
    NetworkOrdered<u16> m_length;
    u8 m_next_header { static_cast<u8>(IPv6NextHeader::NoNextHeader) };
    u8 m_hop_limit { 0 };
    IPv6Address m_source;
    IPv6Address m_destination;
};

static_assert(AssertSize<IPv6PacketHeader, 10 * 32 / 8>());

}
