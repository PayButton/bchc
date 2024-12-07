// Copyright (c) 2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/bigint.h>
#include <script/script_num_encoding.h>

#include <compat/endian.h>
#include <random.h>
#include <span.h>
#include <tinyformat.h>
#include <util/time.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new> // for std::launder
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <gmpxx.h>

namespace {
// TODO: Replace with std::in_range when C++20 is supported
template <typename Target, typename Int>
bool NumFits(Int x) {
    static_assert(std::is_integral_v<Int> && std::is_integral_v<Target> && std::is_signed_v<Int> == std::is_signed_v<Target>);
    return x >= std::numeric_limits<Target>::min() && x <= std::numeric_limits<Target>::max();
}

// TODO: Replace with std::endian when C++20 is supported
[[maybe_unused]]
bool IsBigEndianHost() {
    const uint32_t tst = 1u;
    return *reinterpret_cast<const unsigned char *>(&tst) != 1u;
}

template <typename I>
constexpr void EnsureIntAtLeast64Bits() {
    static_assert(std::is_integral_v<I>);
    using Cmp64 = std::conditional_t<std::is_signed_v<I>, int64_t, uint64_t>;
    static_assert(std::numeric_limits<I>::min() <= std::numeric_limits<Cmp64>::min()
                  && std::numeric_limits<I>::max() >= std::numeric_limits<Cmp64>::max());
}

template <typename UInt>
std::enable_if_t<std::is_integral_v<UInt> && std::is_unsigned_v<UInt>, UInt>
/* UInt */ SwapIfBigEndianHost(UInt ux, bool htole [[maybe_unused]]) {
    EnsureIntAtLeast64Bits<UInt>();
    if constexpr (sizeof(UInt) == sizeof(uint64_t)) {
        ux = htole ? htole64(ux) : le64toh(ux);
    } else {
        // runtime endian detect
        if (IsBigEndianHost()) {
            // native order is big endian; do reversal
            auto *begin = reinterpret_cast<std::byte *>(&ux);
            auto *end = begin + sizeof(ux);
            std::reverse(begin, end);
        }
    }
    return ux;
}

// Functions and typedefs related to import/export of data to/from libgmp
static constexpr size_t ULSz = sizeof(unsigned long); // 8 or 4 bytes depending on platform
using ULWord = std::array<std::byte, ULSz>; // Words used for import/export to libgmp, encapsulated as an array of bytes
static_assert(alignof(ULWord) == 1u); // Words should support unaligned access (libgmp supports this)

// Convert any Span of bytes to a Span of ULWord.
// Precondition: The input byte span *must* be sized such that it ends on a ULWord-boundary.
template<typename ByteLike>
auto BytesToULWordSpan(Span<ByteLike> bytes) /* -> Span<[const] ULWord> */ {
    using BareByteLike = std::remove_cv_t<ByteLike>;
    static_assert(std::is_same_v<BareByteLike, uint8_t> || std::is_same_v<BareByteLike, std::byte>);
    constexpr bool isconst = std::is_const_v<ByteLike>;
    using WordType = std::conditional_t<isconst, const ULWord, ULWord>;
    assert(bytes.size() % ULSz == 0);
    return Span<WordType>(std::launder(reinterpret_cast<WordType *>(bytes.data())), bytes.size() / ULSz);
}

// Convert any unsigned integral pointer to a Span of 1 or more ULWords
// Precondition: sizeof(UInt) must be >= ULSz and must be evenly divisible by ULSz
template<typename UInt>
auto UIntToULWordSpan(UInt *u, size_t count = 1u) /* -> Span<[const] ULWord> */ {
    using Bare = std::remove_cv_t<UInt>;
    static_assert(std::is_integral_v<Bare> && std::is_unsigned_v<Bare>);
    EnsureIntAtLeast64Bits<Bare>();
    static_assert(sizeof(UInt) % ULSz == 0 && sizeof(UInt) >= ULSz);
    constexpr bool isconst = std::is_const_v<UInt>;
    using Byte = std::conditional_t<isconst, const std::byte, std::byte>;
    // and convert to Span<[cosnt] ULWord>; paranoia: cast through std::byte to avoid UB no matter what.
    return BytesToULWordSpan(Span<Byte>{reinterpret_cast<Byte *>(u), sizeof(UInt) * count});
}

} // namespace

struct BigInt::Impl : mpz_class {
    using mpz_class::mpz_class;

    mpz_class &base() noexcept { return *this; }
    const mpz_class &base() const noexcept { return *this; }

    /**
     * Export stored value to a buffer quickly, as little endian ULWord-sized words.
     * @pre - outbuf must be sized to at least the amount of space needed to store the exported data.
     * @post - outbuf will contain `this`'s stored value encoded as unsigned little endian (omitting sign information).
     * @returns - the number of words actually written.
     */
    size_t exportWords(Span<ULWord> outbuf) const;

    /**
     *  Import data from a buffer quickly (little endian ULWord-sized words).
     *  @pre - inbuf must be the abs value little endian encoding of the value we wish to store.
     *  @post - this instance will store the value represented by inbuf.
     */
    void importWords(Span<const ULWord> inbuf);
};

size_t BigInt::Impl::exportWords(Span<ULWord> outbuf) const {
    size_t count{};
    void *buf = mpz_export(outbuf.data(), &count, -1, ULSz, -1, 0, get_mpz_t());
    assert(count <= outbuf.size()); // enforce precondition
    assert(buf == outbuf.data()); // this should never happen, but ensure libgmp didn't return some error nullptr
    return count;
}

void BigInt::Impl::importWords(Span<const ULWord> inbuf) {
    if (inbuf.empty()) {
        *this = 0;
    } else {
        mpz_import(get_mpz_t(), inbuf.size(), -1, ULSz, -1, 0, inbuf.data());
    }
}

// Implicitly creates an instance on first use
BigInt::Impl &BigInt::p() {
    if (!m_p) m_p = std::make_unique<Impl>();
    return *m_p;
}

/* static */ const BigInt::Impl BigInt::sharedZero;

const BigInt::Impl &BigInt::p() const noexcept {
    if (!m_p) return sharedZero;
    return *m_p;
}

BigInt::BigInt() noexcept {}
BigInt::~BigInt() {} // we need to define this here due to pimpl idiom

/* -- Move and copy -- */
BigInt::BigInt(BigInt &&o) noexcept : m_p(std::move(o.m_p)) {}

BigInt::BigInt(const BigInt &o) {
    if (o.m_p) m_p = std::make_unique<Impl>(*o.m_p);
}

BigInt &BigInt::operator=(BigInt &&o) noexcept {
    if (this != &o) {
        // swap pointers, then re-initialize `o` to empty
        swap(o);
        if (o.m_p) o.m_p.reset();
    }
    return *this;
}

BigInt &BigInt::operator=(const BigInt &o) {
    if (this != &o) {
        if (o.m_p) p().base() = o.m_p->base();
        else m_p.reset();
    }
    return *this;
}

/* static */
void BigInt::swap(BigInt &o) noexcept {
    // swap pointers
    m_p.swap(o.m_p);
}

/* -- End move and Copy */

// handles both signed and insigned I; I must be at least 64-bits.
template <typename I>
void BigInt::setIntImpl(I x) {
    static_assert(std::is_integral_v<I>);
    EnsureIntAtLeast64Bits<I>();
    constexpr bool issigned = std::is_signed_v<I>;
    using Int = std::conditional_t<issigned, I, std::make_signed_t<I>>;
    using UInt = std::make_unsigned_t<Int>;
    using TargetType = std::conditional_t<issigned, long, unsigned long>;
    if (NumFits<TargetType>(x)) {
        // This branch is normally taken unless `Int` is `int128_t` or we are on on LLP64 platforms such as Windows
        if (x) p().base() = static_cast<TargetType>(x);
        else m_p.reset(); // clear (logically equivalent to 0)
        return;
    }
    // This code-path may be taken sometimes on LLP64 platforms such as Windows, or if `Int` is int128_t
    // 1. Convert `x` to `UInt` (dropping any sign if `issigned == true`)
    // 2. Ensure little endian (if applicable)
    // 3. Assign to self using the `mpz_import` function which can read raw little endian data
    // 4. Apply sign (if `x` was negative and `issigned == true`)
    UInt le_ux;
    bool neg [[maybe_unused]];
    if constexpr (issigned) {
        // `x` is signed, convert to unsigned absolute value
        neg = x < Int{0};
        UInt ux;
        if (!neg) {
            ux = x;
        } else {
            if (x == std::numeric_limits<Int>::min()) {
                // `-x` is undefined is in this case; handle specially
                ux = static_cast<UInt>(-(x + Int{1})) + UInt{1u};
            } else {
                // Otherwise `-x` is defined
                ux = -x;
            }
        }
        le_ux = SwapIfBigEndianHost(ux, true);
    } else {
        // `x` is unsigned, ensure little endian
        le_ux = SwapIfBigEndianHost(x, true);
    }

    // import in word-sized chunks
    p().importWords(UIntToULWordSpan(&std::as_const(le_ux)));

    // lastly, negate if `x` was negative
    if constexpr (issigned) {
        if (neg) negate();
    }
}

void BigInt::setInt(long long x) { setIntImpl(x); }
void BigInt::setInt(unsigned long long x) { setIntImpl(x); }

#if HAVE_INT128
void BigInt::setInt(int128_t x) { setIntImpl(x); }
void BigInt::setInt(uint128_t x) { setIntImpl(x); }
#endif

template <typename I>
std::optional<I> BigInt::getIntImpl() const noexcept {
    static_assert(std::is_integral_v<I>);
    EnsureIntAtLeast64Bits<I>();
    if (!m_p) {
        // we are a default-constructed instance, return 0
        return static_cast<I>(0);
    }
    constexpr bool issigned = std::is_signed_v<I>;
    if constexpr (issigned) {
        if (m_p->fits_slong_p()) {
            // fast path -- taken on LP64 platforms if the stored value is small enough
            static_assert(   std::numeric_limits<long>::min() >= std::numeric_limits<I>::min()
                          && std::numeric_limits<long>::max() <= std::numeric_limits<I>::max());
            return m_p->get_si(); // despite the name `get_si`, this function returns a long
        }
    } else {
        if (isNegative()) {
            // negative values unsupported in the unsigned case
            return std::nullopt;
        }
        if (m_p->fits_ulong_p()) {
            // fast path -- taken on LP64 platforms if the stored value is small enough
            static_assert(   std::numeric_limits<unsigned long>::min() >= std::numeric_limits<I>::min()
                          && std::numeric_limits<unsigned long>::max() <= std::numeric_limits<I>::max());
            return m_p->get_ui(); // despite the name `get_ui`, this function returns an unsigned long
        }
    }
    // requiredBits is either 64 or 128; we support INT64_MIN and INT128_MIN so we need full bits for that edge case.
    constexpr size_t requiredBits = sizeof(I) * 8u;
    if (absValNumBits() > requiredBits) {
        // definitely won't fit, return nullopt
        return std::nullopt;
    }

    using UInt = std::conditional_t<issigned, std::make_unsigned_t<I>, I>;

    // 1. "export" into the memory of `absval` as little endian bytes
    UInt absval = 0u;

    m_p->exportWords(UIntToULWordSpan(&absval));

    // 2. ensure result is in host endian order
    absval = SwapIfBigEndianHost(absval, false);

    if constexpr (issigned) {
        // signed case, apply sign, handling INTxx_MIN edge case
        using Int = I;
        const bool neg = isNegative();
        constexpr UInt onePastMax = static_cast<UInt>(std::numeric_limits<Int>::max()) + UInt{1u};
        if (neg && absval == onePastMax) {
            // support INT64_MIN and/or INT128_MIN edge cases
            return std::numeric_limits<Int>::min();
        } else if (absval >= onePastMax) {
            // uses more bits than we support in a signed Int, bail.
            return std::nullopt;
        }
        assert(absval <= static_cast<UInt>(std::numeric_limits<Int>::max())); // this failing indicates a programming error
        // 3. apply sign
        Int ret = absval;
        if (neg) ret = -ret;
        return ret;
    } else {
        // unsigned case, no more work needs to be done
        return absval;
    }
}

std::optional<int64_t> BigInt::getInt() const noexcept { return getIntImpl<int64_t>(); }
std::optional<uint64_t> BigInt::getUInt() const noexcept { return getIntImpl<uint64_t>(); }
#if HAVE_INT128
auto BigInt::getInt128() const noexcept -> std::optional<int128_t> { return getIntImpl<int128_t>(); }
auto BigInt::getUInt128() const noexcept -> std::optional<uint128_t> { return getIntImpl<uint128_t>(); }
#endif

size_t BigInt::absValNumBits() const noexcept {
    if (!m_p) return 1u; // empty/0 has 1 bit as per our API docs (which matches libgmp)
    return mpz_sizeinbase(m_p->get_mpz_t(), 2);
}

int BigInt::sign() const noexcept {
    const int val = !m_p ? 0 : mpz_sgn(m_p->get_mpz_t());
    return std::clamp(val, -1, 1);
}

BigInt BigInt::abs() const {
    BigInt ret;
    if (m_p) {
        mpz_abs(ret.p().get_mpz_t(), m_p->get_mpz_t());
    }
    return ret;
}

BigInt BigInt::sqrt() const {
    BigInt ret;
    if (const int sgn = sign(); sgn < 0) {
        // We must guard against sqrt of negative values because not only is it not mathematically defined, depending on
        // how gmp is compiled, it may also cause gmp to make the process raise unix signal SIGFPE. So we prefer to
        // throw a C++ exception here instead.
        throw std::domain_error("Attempted to take the square root of a negative value");
    } else if (sgn > 0) {
        // Positive, nonzero, actually do some work.
        mpz_sqrt(ret.p().get_mpz_t(), p().get_mpz_t());
    } // else: For 0 we return a default-constructed BigInt (== 0).
    return ret;
}

BigInt BigInt::pow(unsigned long power) const {
    BigInt ret;
    if (m_p) {
        mpz_pow_ui(ret.p().get_mpz_t(), m_p->get_mpz_t(), power);
    } else if (!power) {
        // anything to the 0 power is 1, including 0^0
        ret = 1;
    } // else: 0 if !m_p && power != 0
    return ret;
}

BigInt BigInt::powMod(const BigInt &exp, const BigInt &mod) const {
    BigInt ret;
    if (!mod.m_p || mod.sign() == 0) {
        throw std::invalid_argument("A zero `mod` argument was provided to BigInt::powMod");
    }
    if (exp.sign() < 0) {
        // Even though it's possible to use a negative exponent with mpz_powm in some cases, we won't support it.
        throw std::invalid_argument("A negative `exp` argument was provided to BigInt::powMod");
    }
    mpz_powm(ret.p().get_mpz_t(), // result
             p().get_mpz_t(), // base
             exp.p().get_mpz_t(), // exp
             mod.m_p->get_mpz_t()); // mod
    return ret;
}

BigInt BigInt::mathModulo(const BigInt &o) const {
    if (!o.m_p || o.sign() == 0) throw std::invalid_argument("A zero `mod` argument was provided to BigInt::mathModulo");
    BigInt ret;
    if (m_p) {
        mpz_mod(ret.p().get_mpz_t(), m_p->get_mpz_t(), o.m_p->get_mpz_t());
    }
    return ret;
}

std::vector<uint8_t> BigInt::serializeAbsVal(bool *neg) const {
    std::vector<uint8_t> ret;
    const int sgn = sign();
    if (sgn != 0) { // sign of 0 means value is 0, so if 0, we do nothing and return empty vector, otherwise do export
        assert(m_p != nullptr);
        const size_t nbytes = absValNumBytes();
        const size_t expectedCount = (nbytes + (ULSz-1u)) / ULSz;
        ret.reserve(std::max(expectedCount * ULSz, nbytes + 1u)); // reserve 1 extra in case caller needs to push 0x00 or 0x80
        ret.resize(expectedCount * ULSz); // make space now
        // finally, export the data to little endian as ULSz sized "words"
        const size_t count = m_p->exportWords(BytesToULWordSpan(Span{ret}));
        assert(count == expectedCount);
        ret.resize(nbytes); // shrink to exact byte size (possibly trims trailing high order zeroes)
    }
    if (neg) *neg = sgn < 0;
    return ret;
}

std::vector<uint8_t> BigInt::serialize() const {
    bool neg;
    auto ret = serializeAbsVal(&neg);
    if (!ret.empty()) {
        if (ret.back() & 0x80u) {
            ret.push_back(neg ? 0x80u : 0x00u);
        } else if (neg) {
            ret.back() |= 0x80u;
        }
    }
    return ret;
}

void BigInt::unserialize(Span<const uint8_t> b) {
    if (b.empty() || (b.size() == 1 && (b.back() == 0x00u || b.back() == 0x80u))) {
        // empty vector, or zero or "negative zero" all map to 0.
        m_p.reset(); // null Impl is logcally equivalent to 0
        return;
    }

    const bool neg = b.back() & 0x80u; // save sign bit
    std::vector<uint8_t> tmp;
    const size_t extraBytes = b.size() % ULSz ? ULSz - (b.size() % ULSz) : 0u;
    Span<const uint8_t> data; // may point to either `tmp` or `b`
    if (neg || extraBytes) {
        // We copy the buffer to pad it to a ULSz word boundary (faster libgmp import), and/or maybe undo the sign bit
        tmp.reserve(b.size() + extraBytes);
        tmp.assign(b.begin(), b.end());
        if (neg) {
            // need to undo the sign bit
            tmp.back() &= 0x7fu;
        }
        tmp.resize(tmp.size() + extraBytes); // pad to ULSz boundary
        data = tmp;
    } else {
        // Fast path: no need to pad or undo sign bit, we have everything we need already
        data = b;
    }

    const size_t sz = data.size();
    assert(sz > 0u && 0 == sz % ULSz); // The above code block ensured this predicate

    // Import in terms of unsigned-long sized words (this is faster than doing it byte-wise)
    p().importWords(BytesToULWordSpan(data));

    // Apply sign
    if (neg) {
        negate();
    }
}

void BigInt::negate() noexcept {
    if (m_p && sign() != 0) {
        // negate by assigning the -mpz back to self. gmp supports input and output args being the same reference.
        mpz_neg(m_p->get_mpz_t(), m_p->get_mpz_t());
    }
}

template <typename IntType>
int BigInt::compareImpl(IntType x) const {
    constexpr bool issigned = std::is_signed_v<IntType>;
    using TargetType = std::conditional_t<issigned, long, unsigned long>;
    if (!m_p) {
        // short-circuit for a default-constructed or moved-from instance
        if (constexpr IntType zero{}; x == zero) return 0;
        else if (x > zero) return -1; // we are zero and x is positive, so we are smaller
        return 1; // we are null (zero) and x is negative, so we are larger
    }
    if (NumFits<TargetType>(x)) {
        int val;
        if constexpr (issigned) {
            val = mpz_cmp_si(m_p->get_mpz_t(), static_cast<TargetType>(x));
        } else {
            val = mpz_cmp_ui(m_p->get_mpz_t(), static_cast<TargetType>(x));
        }
        return std::clamp(val, -1, 1); // grr, mpz_cmp returns random values <0, etc
    }
    return compare(BigInt(x));
}

int BigInt::compare(const BigInt &o) const {
    if (!m_p) return -o.sign();
    else if (!o.m_p) return sign();
    const int val = mpz_cmp(m_p->get_mpz_t(), o.m_p->get_mpz_t());
    return std::clamp(val, -1, 1); // grr, mpz_cmp returns random values <0, etc
}

int BigInt::compare(long long x) const { return compareImpl(x); }
int BigInt::compare(unsigned long long x) const { return compareImpl(x); }

#if HAVE_INT128
int BigInt::compare(int128_t x) const { return compareImpl(x); }
int BigInt::compare(uint128_t x) const { return compareImpl(x); }
#endif

BigInt &BigInt::operator+=(const BigInt &o) {
    if (o.m_p) {
        p().base() += o.m_p->base();
    }
    return *this;
}

BigInt &BigInt::operator-=(const BigInt &o) {
    if (o.m_p) {
        p().base() -= o.m_p->base();
    }
    return *this;
}

BigInt &BigInt::operator*=(const BigInt &o) {
    if (o.m_p) {
        p().base() *= o.m_p->base();
    } else {
        // set to 0
        m_p.reset();
    }
    return *this;
}

BigInt &BigInt::operator/=(const BigInt &o) {
    if (!o.m_p || o == 0) throw std::invalid_argument("Attempted division by 0 in BigInt::operator/=");
    // libgmpxx operator/= is the same as C++ normal division, so we just use that
    p().base() /= o.m_p->base();
    return *this;
}

BigInt &BigInt::operator%=(const BigInt &o) {
    if (!o.m_p || o == 0) throw std::invalid_argument("Attempted modulo by 0 in BigInt::operator%=");
    // libgmpxx operator%= is the same as C++ normal modulus, so we just use that
    p().base() %= o.m_p->base();
    return *this;
}

BigInt &BigInt::operator|=(const BigInt &o) {
    if (o.m_p) {
        p().base() |= o.m_p->base();
    }
    return *this;
}

BigInt &BigInt::operator&=(const BigInt &o) {
    if (o.m_p) {
        p().base() &= o.m_p->base();
    } else {
        // o is 0, so we become 0.
        m_p.reset();
    }
    return *this;
}

BigInt &BigInt::operator^=(const BigInt &o) {
    if (o.m_p) {
        p().base() ^= o.m_p->base();
    }
    return *this;
}

BigInt &BigInt::operator++() { ++p().base(); return *this; }
BigInt &BigInt::operator--() { --p().base(); return *this; }

BigInt &BigInt::operator<<=(int x) {
    p().base() <<= x;
    return *this;
}

BigInt &BigInt::operator>>=(int x) {
    p().base() >>= x;
    return *this;
}

/* --- To/From String --- */

std::string BigInt::ToString(const int base) const {
    const int abase = std::abs(base);
    if (abase < 2 || abase > 62 || base < -36) {
        throw std::invalid_argument(strprintf("Unsupported `base` argument to BigInt::ToString: %i", base));
    }
    std::string ret;
    if (!m_p) {
        // short-circuit return 0, which is the same in all bases
        ret.assign(1u, '0');
        return ret;
    }
    const size_t nbytes = mpz_sizeinbase(m_p->get_mpz_t(), abase) + 2u; // from libgmp: +1 for possible sign and +1 for nul byte
    ret.resize(nbytes, '\0');
    const char *const r = mpz_get_str(ret.data(), base, m_p->get_mpz_t());
    if (!r) {
        // This should never happen; gmp returns nullptr to indicate argument errors. Throw to indicate failure in case
        // different versions of libgmp behave differently w.r.t. the `base` arg.
        throw std::runtime_error(strprintf("mpz_get_str returned a nullptr in BigInt::ToString for base: %i", base));
    }
    assert(r == ret.data()); // the gmp API guarantees this. If this fails something is seriously wrong.
    while (!ret.empty() && ret.back() == '\0') {
        // we overshot the size we needed, trim the string
        ret.pop_back();
    }
    return ret;
}

/* static */
std::optional<BigInt> BigInt::FromString(const char *const str, const unsigned base /* = 0 */) {
    std::optional<BigInt> ret;
    if (str && (!base || (base >= 2u && base <= 62u))) {
        ret.emplace();
        if (ret->p().set_str(str, base) != 0) {
            // an error occurred, reset the optional
            ret.reset();
        }
    }
    return ret;
}

BigInt::BigInt(const char *const str, const unsigned base /* = 0 */) {
    if (auto opt = FromString(str, base)) {
        // steal the `m_p` from *opt
        m_p = std::move(opt->m_p);
    } else {
        // oops, parse failure. Do nothing. We have no `m_p` but that's ok, we are 0 without an `m_p`.
    }
}

// ostream support
std::ostream &operator<<(std::ostream &s, const BigInt &bi) {
    return s << bi.p().base();
}


/* --- class BigInt::Rand --- */

struct BigInt::InsecureRand::Impl {
    gmp_randclass gmpRand{gmp_randinit_default};
    FastRandomContext fastRand{false};
    Impl() = default;
};

BigInt::InsecureRand::InsecureRand(std::optional<unsigned long> seed)
    : p(std::make_unique<InsecureRand::Impl>()) {
    reseed(seed ? *seed : GetTimeMicros());
}

BigInt::InsecureRand::InsecureRand(InsecureRand &&o) : InsecureRand(std::nullopt) {
    p.swap(o.p);
}

BigInt::InsecureRand::~InsecureRand() {}

void BigInt::InsecureRand::reseed(unsigned long s) {
    p->gmpRand.seed(s);
    // now reseed the FastRandomContext by filling u256 with `s`.
    uint256 u256{uint256::Uninitialized};
    const uint64_t le_s = htole64(s);
    for (size_t i = 0; i < u256.size(); /* */) {
        const size_t nBytes = std::min<size_t>(sizeof(le_s), u256.size() - i);
        std::memcpy(u256.data() + i, &le_s, nBytes);
        i += nBytes;
    }
    p->fastRand = FastRandomContext(u256); // seed with u256
}

BigInt BigInt::InsecureRand::randRange(const BigInt &max) {
    BigInt ret;
    ret.p().base() = p->gmpRand.get_z_range(max.p());
    return ret;
}

BigInt BigInt::InsecureRand::randBitCount(unsigned long n) {
    BigInt ret;
    ret.p().base() = p->gmpRand.get_z_bits(n);
    return ret;
}

#if HAVE_W_STRINGOP_OVERFLOW && defined(__GNUG__)
/* Suppress false GCC warning for the below code. See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=109569  */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow=0"
#endif
BigInt BigInt::InsecureRand::randLength(size_t nBytes) {
    BigInt ret;
    std::vector<uint8_t> bytes;
    while (bytes.size() != nBytes) { // keep looping until predicate is satisfied
        if (bytes.size() < nBytes) {
            // Append shortfall
            auto newbytes = p->fastRand.randbytes(nBytes - bytes.size());
            if (bytes.empty()) {
                bytes.swap(newbytes); // fast pointer swap to reduce copying
            } else {
                bytes.insert(bytes.end(), newbytes.begin(), newbytes.end()); // slower copy
            }
        } else {
            // Truncate any excess that may have been added by a MinimallyEncode() pass
            bytes.resize(nBytes);
        }
        // Ensure positive and minimally encode
        if (!bytes.empty()) bytes.back() &= 0x7fu; // strip any possible sign bits to ensure a positive number
        ScriptNumEncoding::MinimallyEncode(bytes); // may compactify or extend based on ending
    }
    ret.unserialize(bytes); // populate BigInt
    return ret;
}
#if HAVE_W_STRINGOP_OVERFLOW && defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
