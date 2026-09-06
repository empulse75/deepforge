// Project-owned deterministic PRNG (Section 18 of the design brief).
//
// We never use <random> distributions for anything that affects world layout,
// because their results are implementation-defined across standard libraries.
#pragma once

#include "core/Hash.hpp"
#include "core/Types.hpp"

#include <string_view>

namespace df {

/// Named random streams. Each stream is independent, so adding a cosmetic
/// consumer can never shift ore placement for an existing seed.
enum class RngStream : u32 {
    Terrain = 1,
    Caves = 2,
    Ores = 3,
    Hazards = 4,
    Landmarks = 5,
    Loot = 6,
    Contracts = 7,
    Research = 8,
    Cosmetic = 9,
    Audio = 10,
    Rescue = 11,
    Balance = 12,
};

/// xoshiro256** - fast, high quality, entirely integer, trivially serializable.
class Rng {
public:
    constexpr Rng() noexcept { seed(0x123456789ABCDEFull); }

    explicit constexpr Rng(u64 s) noexcept { seed(s); }

    constexpr void seed(u64 s) noexcept {
        u64 x = s;
        for (auto& v : mState) {
            x = splitmix64(x);
            v = x;
        }
        if (mState[0] == 0 && mState[1] == 0 && mState[2] == 0 && mState[3] == 0) {
            mState[0] = 0x9E3779B97F4A7C15ull;
        }
    }

    constexpr u64 next_u64() noexcept {
        const u64 result = rotl(mState[1] * 5, 7) * 9;
        const u64 t = mState[1] << 17;
        mState[2] ^= mState[0];
        mState[3] ^= mState[1];
        mState[1] ^= mState[2];
        mState[0] ^= mState[3];
        mState[2] ^= t;
        mState[3] = rotl(mState[3], 45);
        return result;
    }

    constexpr u32 next_u32() noexcept { return static_cast<u32>(next_u64() >> 32); }

    /// Uniform in [0, bound). Rejection sampled so the result is unbiased and
    /// identical on all platforms. bound == 0 yields 0.
    constexpr u32 below(u32 bound) noexcept {
        if (bound == 0) {
            return 0;
        }
        const u32 threshold = static_cast<u32>(-static_cast<i32>(bound)) % bound;
        for (;;) {
            const u32 r = next_u32();
            if (r >= threshold) {
                return r % bound;
            }
        }
    }

    /// Uniform integer in [lo, hi] inclusive.
    constexpr i32 range(i32 lo, i32 hi) noexcept {
        if (hi <= lo) {
            return lo;
        }
        return lo + static_cast<i32>(below(static_cast<u32>(hi - lo + 1)));
    }

    /// Uniform float in [0,1). Built from a 24-bit mantissa so the mapping is
    /// exact and reproducible.
    constexpr float unit_float() noexcept {
        return static_cast<float>(next_u64() >> 40) * (1.0f / 16777216.0f);
    }

    constexpr double unit_double() noexcept {
        return static_cast<double>(next_u64() >> 11) * (1.0 / 9007199254740992.0);
    }

    constexpr float range_float(float lo, float hi) noexcept { return lo + (hi - lo) * unit_float(); }

    /// True with the given probability expressed in parts-per-10000, so that
    /// content data can express chances without float comparisons.
    constexpr bool chance_bp(u32 basisPoints) noexcept { return below(10000) < basisPoints; }

    constexpr bool chance(float p) noexcept { return unit_float() < p; }

    constexpr const u64* state() const noexcept { return mState; }

    constexpr void set_state(const u64 s[4]) noexcept {
        for (int i = 0; i < 4; ++i) {
            mState[i] = s[i];
        }
    }

private:
    static constexpr u64 rotl(u64 x, int k) noexcept { return (x << k) | (x >> (64 - k)); }

    u64 mState[4]{};
};

/// Derive an independent stream from the world seed, a named stream and a
/// coordinate pair. The same inputs always produce the same sequence.
constexpr u64 derive_seed(u64 worldSeed, RngStream stream, i64 x = 0, i64 y = 0) noexcept {
    u64 h = hash_combine(worldSeed, static_cast<u64>(stream) * 0x27D4EB2F165667C5ull);
    return hash_coords(h, x, y);
}

constexpr Rng make_rng(u64 worldSeed, RngStream stream, i64 x = 0, i64 y = 0) noexcept {
    return Rng{derive_seed(worldSeed, stream, x, y)};
}

/// Derive a stream from an arbitrary label; used by content-driven passes.
inline Rng make_labeled_rng(u64 worldSeed, std::string_view label, i64 x = 0, i64 y = 0) noexcept {
    return Rng{hash_coords(hash_combine(worldSeed, fnv1a64(label)), x, y)};
}

} // namespace df
