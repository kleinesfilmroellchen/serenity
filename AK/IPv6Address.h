/*
 * Copyright (c) 2022, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Endian.h>
#include <AK/Format.h>
#include <AK/Optional.h>
#include <AK/StringView.h>
#include <AK/Vector.h>

#ifdef KERNEL
#    include <AK/Error.h>
#    include <Kernel/Library/KString.h>
#else
#    include <AK/String.h>
#endif
#include <AK/IPv4Address.h>
#include <AK/StringBuilder.h>

namespace AK {

class [[gnu::packed]] IPv6Address {
public:
    using in6_addr_t = u8[16];

    constexpr IPv6Address() = default;

    constexpr IPv6Address(in6_addr_t const& data)
    {
        for (size_t i = 0; i < 16; i++)
            m_data[i] = data[i];
    }

    constexpr IPv6Address(IPv4Address const& ipv4_address)
    {
        // IPv4 mapped IPv6 address
        m_data[10] = 0xff;
        m_data[11] = 0xff;
        m_data[12] = ipv4_address[0];
        m_data[13] = ipv4_address[1];
        m_data[14] = ipv4_address[2];
        m_data[15] = ipv4_address[3];
    }

    constexpr u16 operator[](int i) const { return group(i); }

#ifdef KERNEL
    ErrorOr<NonnullOwnPtr<Kernel::KString>> to_string() const
#else
    ErrorOr<String> to_string() const
#endif
    {
        if (is_zero()) {
#ifdef KERNEL
            return Kernel::KString::try_create("::"sv);
#else
            return "::"_string;
#endif
        }

        StringBuilder builder;

        if (is_ipv4_mapped()) {
#ifdef KERNEL
            return Kernel::KString::formatted("::ffff:{}.{}.{}.{}", m_data[12], m_data[13], m_data[14], m_data[15]);
#else
            return String::formatted("::ffff:{}.{}.{}.{}", m_data[12], m_data[13], m_data[14], m_data[15]);
#endif
        }

        // Find the start of the longest span of 0 values
        Optional<int> longest_zero_span_start;
        int zero_span_length = 0;
        for (int i = 0; i < 8;) {
            if (group(i) != 0) {
                i++;
                continue;
            }
            int contiguous_zeros = 1;
            for (int j = i + 1; j < 8; j++) {
                if (group(j) != 0)
                    break;
                contiguous_zeros++;
            }

            if (!longest_zero_span_start.has_value() || longest_zero_span_start.value() < contiguous_zeros) {
                longest_zero_span_start = i;
                zero_span_length = contiguous_zeros;
            }

            i += contiguous_zeros;
        }

        for (int i = 0; i < 8;) {
            if (longest_zero_span_start.has_value() && longest_zero_span_start.value() == i) {
                if (longest_zero_span_start.value() + zero_span_length >= 8)
                    TRY(builder.try_append("::"sv));
                else
                    TRY(builder.try_append(':'));
                i += zero_span_length;
                continue;
            }

            if (i == 0)
                TRY(builder.try_appendff("{:x}", group(i)));
            else
                TRY(builder.try_appendff(":{:x}", group(i)));

            i++;
        }
#ifdef KERNEL
        return Kernel::KString::try_create(builder.string_view());
#else
        return builder.to_string();
#endif
    }

    static Optional<IPv6Address> from_string(StringView string)
    {
        if (string.is_null())
            return {};

        auto const parts = string.split_view(':', SplitBehavior::KeepEmpty);
        if (parts.is_empty())
            return {};
        if (parts.size() > 9) {
            // We may have 9 parts if the address is compressed
            // at the beginning or end, e.g. by substituting the
            // leading or trailing 0 with a : character. Otherwise,
            // the maximum number of parts is 8, which we validate
            // when expanding the compression.
            return {};
        }
        if (parts.size() >= 4 && parts[parts.size() - 1].contains('.')) {
            // Check if this may be an ipv4 mapped address
            auto is_ipv4_prefix = [&]() {
                auto separator_part = parts[parts.size() - 2].trim_whitespace();
                if (separator_part.is_empty())
                    return false;
                auto separator_value = StringUtils::convert_to_uint_from_hex(separator_part);
                if (!separator_value.has_value() || separator_value.value() != 0xffff)
                    return false;
                // TODO: this allows multiple compression tags "::" in the prefix, which is technically not legal
                for (size_t i = 0; i < parts.size() - 2; i++) {
                    auto part = parts[i].trim_whitespace();
                    if (part.is_empty())
                        continue;
                    auto value = StringUtils::convert_to_uint_from_hex(part);
                    if (!value.has_value() || value.value() != 0)
                        return false;
                }
                return true;
            };

            if (is_ipv4_prefix()) {
                auto ipv4_address = IPv4Address::from_string(parts[parts.size() - 1]);
                if (ipv4_address.has_value())
                    return IPv6Address(ipv4_address.value());
                return {};
            }
        }

        in6_addr_t addr {};
        int group = 0;
        int have_groups = 0;
        bool found_compressed = false;
        for (size_t i = 0; i < parts.size();) {
            auto trimmed_part = parts[i].trim_whitespace();
            if (trimmed_part.is_empty()) {
                if (found_compressed)
                    return {};
                int empty_parts = 1;
                bool is_leading = (i == 0);
                bool is_trailing = false;
                for (size_t j = i + 1; j < parts.size(); j++) {
                    if (!parts[j].trim_whitespace().is_empty())
                        break;
                    empty_parts++;
                    if (j == parts.size() - 1)
                        is_trailing = true;
                }
                if (is_leading && is_trailing) {
                    if (empty_parts > 3)
                        return {};
                    return IPv6Address();
                }
                if (is_leading || is_trailing) {
                    if (empty_parts > 2)
                        return {};
                } else if (empty_parts > 1) {
                    return {};
                }

                int remaining_parts = parts.size() - empty_parts - have_groups;
                found_compressed = true;
                group = 8 - remaining_parts;
                VERIFY(group >= 0);
                i += empty_parts;
                continue;
            } else {
                i++;
            }
            auto part = StringUtils::convert_to_uint_from_hex(trimmed_part);
            if (!part.has_value() || part.value() > 0xffff)
                return {};

            if (++have_groups > 8)
                return {};

            VERIFY(group < 8);
            addr[group * sizeof(u16)] = (u8)(part.value() >> 8);
            addr[group * sizeof(u16) + 1] = (u8)part.value();
            group++;
        }

        return IPv6Address(addr);
    }

    constexpr in6_addr_t const& to_in6_addr_t() const { return m_data; }

    constexpr bool operator==(IPv6Address const& other) const = default;
    constexpr bool operator!=(IPv6Address const& other) const = default;

    constexpr bool is_zero() const
    {
        for (auto& d : m_data) {
            if (d != 0)
                return false;
        }
        return true;
    }

    constexpr bool is_ipv4_mapped() const
    {
        if (m_data[0] || m_data[1] || m_data[2] || m_data[3] || m_data[4] || m_data[5] || m_data[6] || m_data[7] || m_data[8] || m_data[9])
            return false;
        if (m_data[10] != 0xff || m_data[11] != 0xff)
            return false;
        return true;
    }

    Optional<IPv4Address> ipv4_mapped_address() const
    {
        if (is_ipv4_mapped())
            return IPv4Address(m_data[12], m_data[13], m_data[14], m_data[15]);
        return {};
    }

    // https://datatracker.ietf.org/doc/html/rfc4291#section-2.5.3
    [[nodiscard]] static IPv6Address loopback()
    {
        return IPv6Address({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 });
    }

    [[nodiscard]] constexpr bool is_loopback() const
    {
        return *this == loopback();
    }

    [[nodiscard]] constexpr bool is_in_subnet(IPv6Address subnet, u16 network_size) const
    {
        VERIFY(network_size <= 128);
        return this->network(network_size) == subnet;
    }

    [[nodiscard]] constexpr IPv6Address network(u16 network_size) const
    {
        VERIFY(network_size <= 128);
        u64 const low_word_size = min(network_size, 64);
        u64 const high_word_size = clamp(static_cast<i16>(network_size) - 64, 0, 64);
        // FIXME: somehow avoid branching for special case *without* introducing UB?
        // Bitmagic works as follows:
        //  - Invert the network size since we can more easily create many mask bits towards the "high" side of the address (i.e. low bits)
        //  - For instance, size = 16 => mask off top 48 bits, so first create a mask with 48 1 bits at the bottom.
        //  - Use inverted size to create corresponding bitmask ((1 << size) - 1). This bitmask is exactly inverse to what we want.
        //    In the example, inverting gives us all 1 bits in the top 16 positions.
        u64 const low_word_mask = low_word_size == 0 ? 0uLL : ~((1uLL << (64 - low_word_size)) - 1uLL);
        u64 const high_word_mask = high_word_size == 0 ? 0uLL : ~((1uLL << (64 - high_word_size)) - 1uLL);
        u64 const masked_low_word = low_word_mask & low_word();
        u64 const masked_high_word = high_word_mask & high_word();
        return { masked_low_word, masked_high_word };
    }

    // https://datatracker.ietf.org/doc/html/rfc4291#section-2.5.6
    [[nodiscard]] constexpr bool is_link_local() const
    {
        return is_in_subnet({ { 0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }, 10);
    }

    // https://datatracker.ietf.org/doc/html/rfc4193
    [[nodiscard]] constexpr bool is_unique_local() const
    {
        return is_in_subnet({ { 0xfc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }, 7);
    }

    // https://datatracker.ietf.org/doc/html/rfc2373#section-2.7
    [[nodiscard]] constexpr bool is_multicast() const
    {
        return is_in_subnet({ { 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }, 8);
    }
    [[nodiscard]] constexpr bool is_unicast() const { return !is_multicast(); }

private:
    constexpr u16 group(unsigned i) const
    {
        VERIFY(i < 8);
        return ((u16)m_data[i * sizeof(u16)] << 8) | m_data[i * sizeof(u16) + 1];
    }

    constexpr u64 low_word() const
    {
        // NOTE: since IPv6Address is packed, its alignment is that of a byte (most certainly 1).
        //       Therefore, we cannot simply bit_cast half of the data array (or use __builtin_bswap64),
        //       as the u64 load would violate alignment requirements of 64-bit integers on most platforms.
        return (static_cast<u64>(m_data[0]) << (8 * 7))
            | (static_cast<u64>(m_data[1]) << (8 * 6))
            | (static_cast<u64>(m_data[2]) << (8 * 5))
            | (static_cast<u64>(m_data[3]) << (8 * 4))
            | (static_cast<u64>(m_data[4]) << (8 * 3))
            | (static_cast<u64>(m_data[5]) << (8 * 2))
            | (static_cast<u64>(m_data[6]) << (8 * 1))
            | (static_cast<u64>(m_data[7]) << (8 * 0));
    }

    constexpr u64 high_word() const
    {
        return (static_cast<u64>(m_data[8]) << (8 * 7))
            | (static_cast<u64>(m_data[9]) << (8 * 6))
            | (static_cast<u64>(m_data[10]) << (8 * 5))
            | (static_cast<u64>(m_data[11]) << (8 * 4))
            | (static_cast<u64>(m_data[12]) << (8 * 3))
            | (static_cast<u64>(m_data[13]) << (8 * 2))
            | (static_cast<u64>(m_data[14]) << (8 * 1))
            | (static_cast<u64>(m_data[15]) << (8 * 0));
    }

    // Internal use only since it requires the big endian words from the functions above.
    constexpr IPv6Address(u64 const low_word, u64 const high_word)
    {
        u64 const le_low_word = __builtin_bswap64(low_word);
        u64 const le_high_word = __builtin_bswap64(high_word);
        for (size_t i = 0; i < 8; ++i) {
            u8 const low_bits = (le_low_word >> (i * 8)) & 0xff;
            u8 const high_bits = (le_high_word >> (i * 8)) & 0xff;
            m_data[i] = low_bits;
            m_data[i + 8] = high_bits;
        }
    }

    in6_addr_t m_data {};
};

static_assert(sizeof(IPv6Address) == 16);

template<>
struct Traits<IPv6Address> : public DefaultTraits<IPv6Address> {
    // SipHash-4-8 is considered conservatively secure, even if not cryptographically secure.
    static unsigned hash(IPv6Address const& address) { return sip_hash_bytes<4, 8>({ &address.to_in6_addr_t(), sizeof(address.to_in6_addr_t()) }); }
};

#ifdef KERNEL
template<>
struct Formatter<IPv6Address> : Formatter<StringView> {
    ErrorOr<void> format(FormatBuilder& builder, IPv6Address const& value)
    {
        return Formatter<StringView>::format(builder, TRY(value.to_string())->view());
    }
};
#else
template<>
struct Formatter<IPv6Address> : Formatter<StringView> {
    ErrorOr<void> format(FormatBuilder& builder, IPv6Address const& value)
    {
        return Formatter<StringView>::format(builder, TRY(value.to_string()));
    }
};
#endif

}

#if USING_AK_GLOBALLY
using AK::IPv6Address;
#endif
