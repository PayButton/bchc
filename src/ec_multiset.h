// Copyright (c) 2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <pubkey.h>
#include <serialize.h>
#include <span.h>
#include <uint256.h>
#include <util/saltedhashers.h>

#include <array>
#include <initializer_list>
#include <memory>
#include <utility>

/// A C++ wrapper for secp256k1_multiset
class ECMultiSet {
    struct Priv;
    std::unique_ptr<Priv> p;  ///< pointer to implementation (hides the secp256k1_muliset * type)

public:
    /// Construct the empty set (containing no elements)
    ECMultiSet();

    using ByteSpan = Span<const uint8_t>;
    /// Construct a multi-set containing 1 element, byte-blob `item`
    explicit ECMultiSet(ByteSpan item);

    /// Construct this multi-set from a compressed pubkey. If the pubkey is !.IsFullyValid() or is !.IsCompressed(),
    /// then the empty set will be constructed.
    explicit ECMultiSet(const CPubKey &pubKey);

    using PubKeyBytes = std::array<uint8_t, 33>;
    /// Construct this multi-set from a serialized compressed pubkey (33 bytes) as obtained from the network or from
    /// this class's .GetPubKeyBytes() method. The first byte must be 0x2 or 0x3, or 0x0 (for the empty set).
    /// If the bytes are invalid, the empty set is constructed.
    explicit ECMultiSet(const PubKeyBytes &bytes);

    /// Construct a multi-set containing the elements of the provided initializer_list
    ECMultiSet(std::initializer_list<ByteSpan> items);

    ECMultiSet(const ECMultiSet &);
    ECMultiSet(ECMultiSet &&) noexcept;

    ~ECMultiSet() noexcept;

    /// Returns true iff this is the empty set
    bool IsEmpty() const noexcept;

    /// Adds the hash of the bytes of `item` to the set.
    ECMultiSet & Add(ByteSpan item) noexcept;

    /// Removes the hash of the bytes of `item` from the set.
    ///
    /// Note that if item was not in this set, or if this set is empty, the set will now be at some unspecified EC
    /// point, and likely can never become IsEmpty() ever again.
    ECMultiSet & Remove(ByteSpan item) noexcept;

    /// Adds the full contents of another set to this set. Conceptually, any duplicates are "added twice".
    ECMultiSet & Combine(const ECMultiSet &o) noexcept;

    /// Clears this set, making it empty (as if it were default constructed)
    void Clear() noexcept;

    /// Returns the set's state hash. This uniquely identifies a particular set and is suitable for comparing set
    /// equality.  Empty sets always have the returned hash equal to 32 zeroed bytes.
    uint256 GetHash() const noexcept;

    /// If the set is not empty, returns the set's pubkey in compressed form (33 bytes).
    /// The first byte is 0x3 or 0x2.  If the set is empty, will return a CPubKey that is !pubkey.IsValid()
    CPubKey GetPubKey() const noexcept;

    /// If the set is not empty, returns the set's pubkey in compressed form (33 bytes).
    /// The first byte is 0x3 or 0x2.  If the set is empty, will return an array filled with zeroes (33 zeroed bytes).
    PubKeyBytes GetPubKeyBytes() const noexcept;

    /// If the pubkey is !.IsFullyValid() or is !.IsCompressed(), then the set will be cleared (made empty).
    ECMultiSet & SetPubKey(const CPubKey &pubKey) noexcept;

    /// Set this multi-set from a serialized compressed pubkey (33 bytes) as obtained from the network or from
    /// this class's .GetPubKeyBytes() method. The first byte must be 0x2 or 0x3, or 0x0 (for the empty set).
    /// If the bytes are invalid, the set is made empty.
    ECMultiSet & SetPubKeyBytes(const PubKeyBytes &bytes) noexcept;

    ECMultiSet & operator=(const ECMultiSet &) noexcept;
    ECMultiSet & operator=(ECMultiSet &&) noexcept;

    ECMultiSet & operator+=(const ECMultiSet &o) noexcept { return Combine(o); }
    ECMultiSet & operator+=(ByteSpan item) noexcept { return Add(item); }
    ECMultiSet & operator-=(ByteSpan item) noexcept { return Remove(item); }

    bool operator==(const ECMultiSet &o) const noexcept { return GetHash() == o.GetHash(); }
    bool operator!=(const ECMultiSet &o) const noexcept { return GetHash() != o.GetHash(); }

    /// Compares the hashes of the two sets to each other. This is so that these objects support being placed inside a
    /// std::set or as a key value for a set::map.
    bool operator<(const ECMultiSet &o) const noexcept { return GetHash() < o.GetHash(); }

    friend inline ECMultiSet operator+(const ECMultiSet &a, const ECMultiSet &b) {
        return std::move(ECMultiSet(a).Combine(b));
    }
    friend inline ECMultiSet operator+(const ECMultiSet &a, ByteSpan b) { return std::move(ECMultiSet(a).Add(b)); }
    friend inline ECMultiSet operator-(const ECMultiSet &a, ByteSpan b) { return std::move(ECMultiSet(a).Remove(b)); }

    // Serializes to 33-bytes, which is either a valid compressed pubkey (beginning with 0x2 or 0x3) for a non-empty
    // set, or 33 bytes of all-zeroes for an empty set.
    SERIALIZE_METHODS(ECMultiSet, obj) {
        PubKeyBytes bytes;
        SER_WRITE(obj, bytes = obj.GetPubKeyBytes());
        READWRITE(bytes);
        if constexpr (ser_action.ForRead()) {
            obj.SetPubKeyBytes(bytes);
            // validate that the pubkey is valid on reading, or throw
            if (constexpr const PubKeyBytes allZeroes = {{0u}}; obj.IsEmpty() && bytes != allZeroes) {
                throw std::ios_base::failure("Invalid pubkey on read: expected either a valid compressed pubkey or"
                                             " all zeroes");
            }
        }
    }
};

/// Salted hasher for ECMultiSet, so it may be used in a std::unordered_set or as a std::unordered_map key
struct SaltedECMultiSetHasher : protected SaltedUint256Hasher {
    SaltedECMultiSetHasher() noexcept {} // exclicitly define to circumvent some libstdc++-11 bugs on Debian unstable
    size_t operator()(const ECMultiSet &ecm) const noexcept { return SaltedUint256Hasher::operator()(ecm.GetHash()); }
};
