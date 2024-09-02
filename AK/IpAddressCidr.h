/*
 * Copyright (c) 2024, famfo <famfo@famfo.xyz>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/IPv4Address.h>
#include <AK/IPv6Address.h>
#include <AK/UFixedBigInt.h>

#ifdef KERNEL
#    include <Kernel/Library/MiniStdLib.h>
#endif

namespace AK {

class IPv4AddressCidr;
class IPv6AddressCidr;

namespace Details {

template<typename Address>
class AddressTraits;

template<OneOf<IPv4AddressCidr, IPv6AddressCidr> AddressFamily>
class IPAddressCidr {
public:
    enum class IPAddressCidrError {
        CidrTooLong,
        StringParsingFailed,
    };

    using IPAddress = Details::AddressTraits<AddressFamily>::IPAddress;

    static constexpr ErrorOr<AddressFamily, IPAddressCidrError> create(IPAddress address, u8 length)
    {
        if (length > AddressFamily::MAX_LENGTH)
            return IPAddressCidrError::CidrTooLong;

        return AddressFamily(address, length);
    }

    constexpr IPAddress const& ip_address() const& { return m_address; }
    constexpr u32 length() const { return m_length; }

    constexpr void set_ip_address(IPAddress address) { m_address = address; }
    constexpr ErrorOr<void, IPAddressCidrError> set_length(u32 length)
    {
        if (length > AddressFamily::MAX_LENGTH)
            return IPAddressCidrError::CidrTooLong;

        m_length = length;
        return ErrorOr<void, IPAddressCidrError>();
    }

    constexpr static ErrorOr<AddressFamily, IPAddressCidrError> from_string(StringView string)
    {
        Vector<StringView> const parts = string.split_view('/');

        if (parts.size() != 2)
            return IPAddressCidrError::StringParsingFailed;

        Optional ip_address = IPAddress::from_string(parts[0]);
        if (!ip_address.has_value())
            return IPAddressCidrError::StringParsingFailed;

        Optional<u8> length = parts[1].to_number<u8>();
        if (!length.has_value())
            return IPAddressCidrError::StringParsingFailed;

        return IPAddressCidr::create(ip_address.value(), length.value());
    }

#ifdef KERNEL
    ErrorOr<NonnullOwnPtr<Kernel::KString>> to_string() const
#else
    ErrorOr<String> to_string() const
#endif
    {
        StringBuilder builder;

        auto address_string = TRY(m_address.to_string());

#ifdef KERNEL
        TRY(builder.try_append(address_string->view()));
#else
        TRY(builder.try_append(address_string));
#endif

        TRY(builder.try_append('/'));
        TRY(builder.try_appendff("{}", m_length));

#ifdef KERNEL
        return Kernel::KString::try_create(builder.string_view());
#else
        return builder.to_string();
#endif
    }

    constexpr bool operator==(IPAddressCidr const& other) const = default;
    constexpr bool operator!=(IPAddressCidr const& other) const = default;

protected:
    constexpr IPAddressCidr(IPAddress address, u8 length)
    {
        m_address = address;
        m_length = length;
    }

private:
    IPAddress m_address;
    u8 m_length;
};

template<>
class AddressTraits<IPv4AddressCidr> {
public:
    using IPAddress = IPv4Address;
};

template<>
class AddressTraits<IPv6AddressCidr> {
public:
    using IPAddress = IPv6Address;
};

}

class IPv4AddressCidr : public Details::IPAddressCidr<IPv4AddressCidr> {
public:
    constexpr IPv4AddressCidr(IPv4Address address, u8 length)
        : IPAddressCidr(address, length)
    {
    }

    constexpr IPv4Address netmask() const
    {
        IPv4Address netmask;
        u8 free_bits = MAX_LENGTH - length();

        if (free_bits == 32) {
            netmask = IPv4Address(0, 0, 0, 0);
        } else {
            u8 address[4];
            NetworkOrdered<u32> mask = NumericLimits<u32>::max() << free_bits;

            memcpy(address, &mask, sizeof(address));
            netmask = IPv4Address(address);
        }

        return netmask;
    }

    constexpr IPv4Address first_address_of_subnet() const
    {
        u32 mask = netmask().to_u32();
        return IPv4Address(ip_address().to_u32() & mask);
    }

    constexpr IPv4Address last_address_of_subnet() const
    {
        u32 mask = netmask().to_u32();
        u32 first = ip_address().to_u32() & mask;
        return IPv4Address(first | ~mask);
    }

    bool contains(IPv4Address other) const
    {
        IPv4AddressCidr other_cidr = IPv4AddressCidr::create(other, length()).value();
        return first_address_of_subnet() == other_cidr.first_address_of_subnet();
    }

    static u8 const MAX_LENGTH = 32;
};

template<>
struct Traits<IPv4AddressCidr> : public DefaultTraits<IPv4AddressCidr> {
    static unsigned hash(IPv4AddressCidr const& address)
    {
        IPv4Address ip_address = address.ip_address();
        return sip_hash_bytes<4, 8>({ &ip_address, address.length() });
    }
};

#ifdef KERNEL
template<>
struct Formatter<IPv4AddressCidr> : Formatter<StringView> {
    ErrorOr<void> format(FormatBuilder& builder, IPv4AddressCidr value)
    {
        return Formatter<StringView>::format(builder, TRY(value.to_string())->view());
    }
};
#else
template<>
struct Formatter<IPv4AddressCidr> : Formatter<StringView> {
    ErrorOr<void> format(FormatBuilder& builder, IPv4AddressCidr value)
    {
        return Formatter<StringView>::format(builder, TRY(value.to_string()));
    }
};
#endif

class IPv6AddressCidr : public Details::IPAddressCidr<IPv6AddressCidr> {
public:
    constexpr IPv6AddressCidr(IPv6Address address, u8 length)
        : IPAddressCidr(address, length)
    {
    }

    constexpr IPv6Address first_address_of_subnet() const
    {
        u8 address[16];
        u8 free_bits = MAX_LENGTH - length();

        if (free_bits != 128) {
            NetworkOrdered<u128> mask = NumericLimits<u128>::max() << free_bits;
            u8 address_mask[16];

            memcpy(address_mask, &mask, sizeof(address_mask));

            auto const* original_address = ip_address().to_in6_addr_t();
            for (int i = 0; i < 16; i++) {
                address[i] = original_address[i] & address_mask[i];
            }
        }

        return address;
    }

    constexpr IPv6Address last_address_of_subnet() const
    {
        u8 address[16];
        u8 free_bits = MAX_LENGTH - length();

        u8 inverse_address_mask[16];
        NetworkOrdered<u128> inverse_mask = NumericLimits<u128>::max() >> (128 - free_bits);

        if (free_bits != 128) {
            NetworkOrdered<u128> mask = NumericLimits<u128>::max() << free_bits;
            u8 address_mask[16];

            memcpy(address_mask, &mask, sizeof(address_mask));

            auto const* original_address = ip_address().to_in6_addr_t();
            for (int i = 0; i < 16; i++) {
                address[i] = original_address[i] & address_mask[i];
            }
        }

        memcpy(inverse_address_mask, &inverse_mask, sizeof(inverse_address_mask));

        for (int i = 0; i < 16; i++) {
            address[i] = address[i] | inverse_address_mask[i];
        }

        return address;
    }

    bool contains(IPv6Address other) const
    {
        IPv6AddressCidr other_cidr = IPv6AddressCidr::create(other, length()).value();
        return first_address_of_subnet() == other_cidr.first_address_of_subnet();
    }

    static u8 const MAX_LENGTH = 128;
};

template<>
struct Traits<IPv6AddressCidr> : public DefaultTraits<IPv6AddressCidr> {
    static unsigned hash(IPv6AddressCidr const& address)
    {
        IPv6Address ip_address = address.ip_address();
        return sip_hash_bytes<4, 8>({ &ip_address, address.length() });
    }
};

#ifdef KERNEL
template<>
struct Formatter<IPv6AddressCidr> : Formatter<StringView> {
    ErrorOr<void> format(FormatBuilder& builder, IPv6AddressCidr value)
    {
        return Formatter<StringView>::format(builder, TRY(value.to_string())->view());
    }
};
#else
template<>
struct Formatter<IPv6AddressCidr> : Formatter<StringView> {
    ErrorOr<void> format(FormatBuilder& builder, IPv6AddressCidr value)
    {
        return Formatter<StringView>::format(builder, TRY(value.to_string()));
    }
};
#endif

}

#if USING_AK_GLOBALLY
using AK::IPv4AddressCidr;
using AK::IPv6AddressCidr;
#endif
