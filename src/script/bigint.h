// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#ifdef HAVE_CONFIG_H
#include <config/bitcoin-config.h>
#endif

#include <serialize.h>
#include <span.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

/*
 * Arbitrary precision integer class. Supports most common arithmetic ops. This is implemented using the
 * pimpl idiom and is backed by libgmp.
 *
 * Serialization is compatible with the `CScriptNum` (script number) format but unlike `CScriptNum`, serialized
 * numbers may be arbitrarily long.
 *
 * Default constructed instances do no allocations and occupy no extra space besides a nullptr.
 * Assigning values to this class may do some allocations, however.
 */
class BigInt {
    struct Impl;
    std::unique_ptr<Impl> m_p; ///< We use the pimpl idiom for this class to hide implementation details
    Impl &p(); // will construct m_p if one doesn't exist, and return it, or return existing m_p
    const Impl &p() const noexcept; // returns a reference to m_p (if m_p is not null), or a ref to the static sharedZero
    static const Impl sharedZero;

public:
    /// Default-construct with value 0. Does no allocations and occupies no extra space besides a nullptr.
    BigInt() noexcept;

    /// Destructor needs to be defined in .cpp file due to pimpl idiom
    ~BigInt();

    static_assert(std::numeric_limits<long long>::max() == std::numeric_limits<int64_t>::max()
                  && std::numeric_limits<unsigned long long>::max() == std::numeric_limits<uint64_t>::max(),
                  "Integer overloads below assume this");

    /// Construct from all C++ integer types except `char`; note that these are intentionally not made `explicit` so
    /// that this class may support implcit conversions from C++ integers.
    BigInt(signed char x) : BigInt() { setInt(x); }
    BigInt(short x) : BigInt() { setInt(x); }
    BigInt(int x) : BigInt() { setInt(x); }
    BigInt(long x) : BigInt() { setInt(x); }
    BigInt(long long x) : BigInt() { setInt(x); }
    BigInt(unsigned char x) : BigInt() { setInt(x); }
    BigInt(unsigned short x) : BigInt() { setInt(x); }
    BigInt(unsigned int x) : BigInt() { setInt(x); }
    BigInt(unsigned long x) : BigInt() { setInt(x); }
    BigInt(unsigned long long x) : BigInt() { setInt(x); }

#if HAVE_INT128
    // If compiler supports it: Interop with 128-bit integers
    using int128_t = __int128;
    using uint128_t = unsigned __int128;

    BigInt(int128_t x) : BigInt() { setInt(x); }
    BigInt(uint128_t x) : BigInt() { setInt(x); }
#endif

    /// Construct from a string. Warning: No indication of error is given if the string fails to parse,
    /// and the class will act as if it were default-constructed (will contain value 0) on parse errors.
    /// For detecting errors, use the FromString() static factory method instead.
    explicit BigInt(const std::string &str, unsigned base = 0) : BigInt(str.c_str(), base) {}
    explicit BigInt(const char *str, unsigned base = 0);  // overload for above, `str` must be nul-terminated


    /* Copy & move operations */

    BigInt(BigInt &&) noexcept;
    BigInt(const BigInt &);
    BigInt &operator=(BigInt &&) noexcept;
    BigInt &operator=(const BigInt &);

    void swap(BigInt &other) noexcept;


    /* Misc ops */

    /// Sign-negates this instance (x -> -x, or -x -> x)
    void negate() noexcept;
    /// Retuns -1 if this value is negative, 0 if it is 0, and 1 if it is positive
    int sign() const noexcept;
    /// Retruns true iff this instance's value is < 0
    bool isNegative() const noexcept { return sign() < 0; }
    /// Returns the number of bits that are needed to represent this BigInt, disregarding the sign (abs value).
    /// Note that if this BigInt stores a 0, then a size of 1 is returned here.
    size_t absValNumBits() const noexcept;
    size_t absValNumBytes() const noexcept { return (absValNumBits() + 7u) / 8u; }
    /// Absolute value
    BigInt abs() const;
    /// Returns the truncated square root of this instance's stored value. If this instance stores a negative value,
    /// then std::domain_error will be thrown.
    BigInt sqrt() const;
    /// Return this instance's stored value raised to `power`; WARNING: this operation may exhaust memory for large operands.
    BigInt pow(unsigned long power) const;
    /// Return this instance's stored value rasied to `exp` modulo `mod`. Throws if `mod` is zero or `exp` is negative.
    BigInt powMod(const BigInt &exp, const BigInt &mod) const;
    /// Mathematical modulo operator; WARNING: this is different than regular C++ `operator%` for negative operands.
    /// Note: the sign of the operands are ignored; the result here is always non-negative.
    BigInt mathModulo(const BigInt &o) const;


    /* Set/Get an int */

    void setInt(long long x);
    void setInt(long x) { setInt(static_cast<long long>(x)); }
    void setInt(int x) { setInt(static_cast<long long>(x)); }
    void setInt(short x) { setInt(static_cast<long long>(x)); }
    void setInt(signed char x) { setInt(static_cast<long long>(x)); }
    void setInt(unsigned long long x);
    void setInt(unsigned long x) { setInt(static_cast<unsigned long long>(x)); }
    void setInt(unsigned int x) { setInt(static_cast<unsigned long long>(x)); }
    void setInt(unsigned short x) { setInt(static_cast<unsigned long long>(x)); }
    void setInt(unsigned char x) { setInt(static_cast<unsigned long long>(x)); }
    void setInt(bool b) { setInt(static_cast<unsigned long long>(b)); }

    /// Retrieve value as a signed 64-bit int. If the value doesn't fit, then a std::nullopt is returned.
    std::optional<int64_t> getInt() const noexcept;
    /// Retrieve value as an unsigned 64-bit int. If the value doesn't fit, or is negative, std::nullopt is returned.
    std::optional<uint64_t> getUInt() const noexcept;

#if HAVE_INT128
    // Interop with 128-bit integers
    void setInt(int128_t);
    void setInt(uint128_t);

    std::optional<int128_t> getInt128() const noexcept;
    std::optional<uint128_t> getUInt128() const noexcept;
#endif


    /* To/From CScriptNum format */

    /// Returns a "minimally encoded" VM format representation (e.g. CScriptNum format).
    std::vector<uint8_t> serialize() const;

    /// Inverse of above, assign to this instance from VM representation.
    void unserialize(Span<const uint8_t> bytes);


    /* Comparison */

    /// Compares *this to `o`. Returns 0 if *this == `o`, -1 if *this < o, or 1 if *this > o
    int compare(const BigInt &o) const;
    /// Like above but faster.
    int compare(long long x) const;
    int compare(long x) const { return compare(static_cast<long long>(x)); }
    int compare(short x) const { return compare(static_cast<long long>(x)); }
    int compare(int x) const { return compare(static_cast<long long>(x)); }
    int compare(signed char x) const { return compare(static_cast<long long>(x)); }
    int compare(unsigned long long x) const;
    int compare(unsigned long x) const { return compare(static_cast<unsigned long long>(x)); }
    int compare(unsigned int x) const { return compare(static_cast<unsigned long long>(x)); }
    int compare(unsigned short x) const { return compare(static_cast<unsigned long long>(x)); }
    int compare(unsigned char x) const { return compare(static_cast<unsigned long long>(x)); }

#if HAVE_INT128
    int compare(int128_t x) const;
    int compare(uint128_t x) const;
#endif


    /* Operator overloads */

    explicit operator bool() const { return sign() != 0; }

    // Assign from an int, long, unsigned int, int64_t, int128_t, uint128_t, etc.
    template<typename I>
    std::enable_if_t<std::is_integral_v<I>, BigInt &>
    /* BigInt & */ operator=(I x) {
        setInt(x);
        return *this;
    }

    BigInt &operator+=(const BigInt &o);
    BigInt &operator-=(const BigInt &o);
    BigInt &operator*=(const BigInt &o);
    BigInt &operator/=(const BigInt &o);
    BigInt &operator%=(const BigInt &o);

    friend BigInt operator+(const BigInt &a, const BigInt &b) { BigInt r(a); return r += b; }
    friend BigInt operator-(const BigInt &a, const BigInt &b) { BigInt r(a); return r -= b; }
    friend BigInt operator*(const BigInt &a, const BigInt &b) { BigInt r(a); return r *= b; }
    friend BigInt operator/(const BigInt &a, const BigInt &b) { BigInt r(a); return r /= b; }
    friend BigInt operator%(const BigInt &a, const BigInt &b) { BigInt r(a); return r %= b; }

    BigInt &operator|=(const BigInt &o); // bitwise or
    BigInt &operator&=(const BigInt &o); // bitwise and
    BigInt &operator^=(const BigInt &o); // bitwise xor

    friend BigInt operator|(const BigInt &a, const BigInt &b) { BigInt r(a); return r |= b; }
    friend BigInt operator&(const BigInt &a, const BigInt &b) { BigInt r(a); return r &= b; }
    friend BigInt operator^(const BigInt &a, const BigInt &b) { BigInt r(a); return r ^= b; }

    BigInt &operator<<=(int); // left-shift
    BigInt &operator>>=(int); // right-shift
    BigInt  operator<<(int n) const { BigInt r(*this); return r <<= n; }
    BigInt  operator>>(int n) const { BigInt r(*this); return r >>= n; }

    BigInt operator-() const { BigInt ret(*this); ret.negate(); return ret; } // sign negate

    BigInt &operator++();
    BigInt  operator++(int) { BigInt ret(*this); ++*this; return ret; }
    BigInt &operator--();
    BigInt  operator--(int) { BigInt ret(*this); --*this; return ret; }

#define DECLARE_CMP_OPS(T) \
    bool operator< (T o) const { return compare(o)  < 0; } \
    bool operator<=(T o) const { return compare(o) <= 0; } \
    bool operator==(T o) const { return compare(o) == 0; } \
    bool operator!=(T o) const { return compare(o) != 0; } \
    bool operator>=(T o) const { return compare(o) >= 0; } \
    bool operator> (T o) const { return compare(o)  > 0; }

    // BigInt comparison
    DECLARE_CMP_OPS(const BigInt &)

    // signed int* comparison
    DECLARE_CMP_OPS(long long)
    DECLARE_CMP_OPS(long)
    DECLARE_CMP_OPS(int)
    DECLARE_CMP_OPS(short)
    DECLARE_CMP_OPS(signed char)

    // unsigned int* comparison
    DECLARE_CMP_OPS(unsigned long long)
    DECLARE_CMP_OPS(unsigned long)
    DECLARE_CMP_OPS(unsigned int)
    DECLARE_CMP_OPS(unsigned short)
    DECLARE_CMP_OPS(unsigned char)

#if HAVE_INT128
    // 128-bit int comparison
    DECLARE_CMP_OPS(int128_t)
    DECLARE_CMP_OPS(uint128_t)
#endif

#undef DECLARE_CMP_OPS


    /* String ops */

    /// Returns the string representation of this integer in `base`.
    /// @param base - may range from 2 to 62, or -36 to -2, with the negative versions using upper-case letters for
    ///               bases >10.
    /// @throw std::invalid_argument  - if `base` is outside the range [-36, -2] or [2, 62].
    std::string ToString(int base = 10) const;

    /// Parse a string into a `BigInt`.
    /// @param str - the string to parse.
    /// @param base - 0 for autodetect based on prefix (e.g. 0x / 0X for hex, 0b / 0B for binary, 0 for octal,
    ///               or decimal as default if no prefix), or a value in the range [2, 62].
    /// @return a valid optional on success, or an empty optional on failure to parse or if `base` is nonzero and
    ///         outside the range [2, 62].
    static std::optional<BigInt> FromString(const std::string &str, unsigned base = 0) { return FromString(str.c_str(), base); }
    static std::optional<BigInt> FromString(const char *str, unsigned base = 0); // convenient overload for above

    /* Bitcoin serialization */
    SERIALIZE_METHODS(BigInt, obj) {
        std::vector<uint8_t> tmp;
        SER_WRITE(obj, tmp = obj.serialize());
        READWRITE(tmp);
        SER_READ(obj, obj.unserialize(tmp));
    }

    // C++ ostream support; respects stream modifiers e.g. std::hex, etc
    friend std::ostream &operator<<(std::ostream &, const BigInt &);

private:
    /// Like serialize() above but does't have the quirky CScriptNum sign bit/byte. Returns just the raw little-endian
    /// absolute value, optionally setting `*neg` to true/false to indicate sign.
    std::vector<uint8_t> serializeAbsVal(bool *neg = nullptr) const;

    // Internal templated helpers
    template<typename I> std::optional<I> getIntImpl() const noexcept;
    template<typename I> void setIntImpl(I);
    template<typename I> int compareImpl(I) const;

public:
    // Random number generation (wrapper around gmp_randclass & FastRandomContext). This random generator is for tests
    // and is not cryptographically secure.
    class InsecureRand {
        struct Impl;
        std::unique_ptr<Impl> p;
    public:
        // Construct this random generator with an optional deterministic seed.
        // If no seed is specified, the current time as returned by GetTimeMicros() is used.
        InsecureRand(std::optional<unsigned long> seed = std::nullopt);
        InsecureRand(InsecureRand &&o); // move c'tor
        ~InsecureRand();

        // Seed this random generator with deterministic seed `s`
        void reseed(unsigned long s);
        // Returns a uniformly distributed BigInt in the range [0, max-1], inclusive
        BigInt randRange(const BigInt &max);
        // Returns a uniformly distributed BigInt in the range [0, 2^n - 1], inclusive
        BigInt randBitCount(unsigned long n);
        // Returns a randomly generated, non-negative BigInt whose serialized size is exactly nBytes.
        BigInt randLength(size_t nBytes);
    };

    friend class BigInt::InsecureRand;
};

/// Operator overload allowing for string literals such as "12345"_bi to yield BigInt instances (useful for tests).
inline BigInt operator""_bi(const char *str, size_t) { return BigInt(str); }
/// Operator overload allowing for integer literals such as 12345_bi to yield BigInt instances (useful for tests).
inline BigInt operator""_bi(const char *x) { return BigInt(x); }
