// Copyright (c) 2026 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHRONIK_CPP_UTIL_SPAN_H
#define BITCOIN_CHRONIK_CPP_UTIL_SPAN_H

#include <cstdint>
#include <span>

// Helper functions to safely cast basic byte pointers to uint8_t pointers.
inline uint8_t *UCharCast(char *c) {
    return (uint8_t *)c;
}
inline uint8_t *UCharCast(uint8_t *c) {
    return c;
}
inline uint8_t *UCharCast(signed char *c) {
    return reinterpret_cast<uint8_t *>(c);
}
inline uint8_t *UCharCast(std::byte *c) {
    return (uint8_t *)c;
}
inline const uint8_t *UCharCast(const char *c) {
    return (uint8_t *)c;
}
inline const uint8_t *UCharCast(const uint8_t *c) {
    return c;
}
inline const uint8_t *UCharCast(const signed char *c) {
    return reinterpret_cast<const uint8_t *>(c);
}
inline const uint8_t *UCharCast(const std::byte *c) {
    return reinterpret_cast<const uint8_t *>(c);
}
// Helper concept for the basic byte types.
template <typename B>
concept BasicByte = requires { UCharCast(std::span<B>{}.data()); };

// Helper function to safely convert a Span to a Span<[const] uint8_t>.
template <typename T>
constexpr auto UCharSpanCast(Span<T> s)
    -> Span<typename std::remove_pointer<decltype(UCharCast(s.data()))>::type> {
    return {UCharCast(s.data()), s.size()};
}

/**
 * Like the Span constructor, but for (const) uint8_t member types only. Only
 * works for (un)signed char containers.
 */
template <typename V>
constexpr auto MakeUCharSpan(V &&v)
    -> decltype(UCharSpanCast(Span{std::forward<V>(v)})) {
    return UCharSpanCast(Span{std::forward<V>(v)});
}


#endif // BITCOIN_CHRONIK_CPP_UTIL_SPAN_H
