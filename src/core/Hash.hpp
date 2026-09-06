// Deterministic, platform-independent hashing primitives.
#pragma once

#include "core/Types.hpp"

#include <string_view>

namespace df {

/// SplitMix64 finalizer. Bit-exact on every platform (integer only).
constexpr u64 splitmix64(u64 x) noexcept {
    x += 0x9E3779B97F4A7C15ull;
    u64 z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

/// FNV-1a over bytes; used for stable string identifiers.
constexpr u64 fnv1a64(std::string_view s) noexcept {
    u64 h = 0xCBF29CE484222325ull;
    for (const char c : s) {
        h ^= static_cast<u64>(static_cast<unsigned char>(c));
        h *= 0x100000001B3ull;
    }
    return h;
}

/// Combine an accumulator with another value deterministically.
constexpr u64 hash_combine(u64 seed, u64 value) noexcept {
    return splitmix64(seed ^ (value + 0x9E3779B97F4A7C15ull + (seed << 6) + (seed >> 2)));
}

constexpr u64 hash_coords(u64 seed, i64 x, i64 y) noexcept {
    u64 h = hash_combine(seed, static_cast<u64>(x) * 0xD6E8FEB86659FD93ull);
    h = hash_combine(h, static_cast<u64>(y) * 0xA24BAED4963EE407ull);
    return h;
}

} // namespace df
