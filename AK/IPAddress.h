/*
 * Copyright (c) 2025, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/IPv4Address.h>
#include <AK/IPv6Address.h>

namespace AK {

// The enum values here are not strictly required, but make this type easily decipherable in debugging output.
enum class IPVersion : u8 {
    IPv6 = 6,
    IPv4 = 4,
};

// A combined IPv4/IPv6 address type. Use this if you want to support both legacy and modern IP addresses.
// Implementation detail: IPv4 addresses are stored in the IPv4-mapped range ::ffff:0.0.0.0/96 within IPv6.
// This is a range specifically designed for the purpose we’re using it for here.
// See RFC 4291, Section 2.5.5.2: https://datatracker.ietf.org/doc/html/rfc4291#section-2.5.5.2
// This class is a thin wrapper around IPv6Address with different semantics and expanded multi-version awareness.
class [[gnu::packed]] IPAddress : public IPv6Address {
public:
    constexpr IPAddress() = default;

    constexpr IPAddress(in6_addr_t const& data)
        : IPv6Address(data)
    {
    }

    constexpr IPAddress(IPv4Address const& ipv4_address)
        : IPv6Address(ipv4_address)
    {
    }

    constexpr IPAddress(IPv6Address const& ipv6_address)
        : IPv6Address(ipv6_address)
    {
    }

#ifdef KERNEL
    ErrorOr<NonnullOwnPtr<Kernel::KString>> to_string() const
#else
    ErrorOr<String> to_string() const
#endif
    {
        if (version() == IPVersion::IPv4)
            return to_ipv4().to_string();
        return IPv6Address::to_string();
    }

    static Optional<IPAddress> from_string(StringView string)
    {
        // Every valid IPv6 address contains at least one (actually two) colons “:”, while no IPv4 address ever does.
        if (string.contains(':'))
            return IPv6Address::from_string(string).map([](auto value) { return IPAddress(value); });
        return IPv4Address::from_string(string).map([](auto value) { return IPAddress(value); });
    }

    constexpr bool operator==(IPAddress const& other) const = default;
    constexpr bool operator!=(IPAddress const& other) const = default;
    constexpr bool operator==(IPv4Address const& other) const { return to_ipv4() == other; }
    constexpr bool operator!=(IPv4Address const& other) const { return to_ipv4() != other; }
    // IPv6 comparison operators inherited from parent

    constexpr IPv4Address to_ipv4() const
    {
        VERIFY(version() == IPVersion::IPv4);
        return IPv4Address(m_data[12], m_data[13], m_data[14], m_data[15]);
    }

    constexpr IPVersion version() const
    {
        if (is_ipv4_mapped())
            return IPVersion::IPv4;
        return IPVersion::IPv6;
    }
};

static_assert(sizeof(IPAddress) == 16);

template<>
struct Traits<IPAddress> : public Traits<IPv6Address> {
    // SipHash-4-8 is considered conservatively secure, even if not cryptographically secure.
    static unsigned hash(IPv6Address const& address) { return sip_hash_bytes<4, 8>({ &address.to_in6_addr_t(), sizeof(address.to_in6_addr_t()) }); }
};

#ifdef KERNEL
template<>
struct Formatter<IPAddress> : Formatter<StringView> {
    ErrorOr<void> format(FormatBuilder& builder, IPv6Address const& value)
    {
        return Formatter<StringView>::format(builder, TRY(value.to_string())->view());
    }
};
#else
template<>
struct Formatter<IPAddress> : Formatter<StringView> {
    ErrorOr<void> format(FormatBuilder& builder, IPv6Address const& value)
    {
        return Formatter<StringView>::format(builder, TRY(value.to_string()));
    }
};
#endif

}

#if USING_AK_GLOBALLY
using AK::IPAddress;
using AK::IPVersion;
#endif
