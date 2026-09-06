// Integer value/gradient noise.
//
// World generation must be bit-identical on every supported platform, so every
// operation here is integer-only. Results are returned in fixed point where
// kNoiseOne represents 1.0.
#pragma once

#include "core/Hash.hpp"
#include "core/Types.hpp"

namespace df {

/// Fixed point scale used by all noise outputs (Q16.16).
inline constexpr i32 kNoiseShift = 16;
inline constexpr i32 kNoiseOne = 1 << kNoiseShift;

/// Deterministic lattice value in [-kNoiseOne, kNoiseOne].
constexpr i32 lattice_value(u64 seed, i64 x, i64 y) noexcept {
    const u64 h = hash_coords(seed, x, y);
    // Take 17 bits and centre them: range [-65536, 65535].
    return static_cast<i32>(h >> 30 & 0x1FFFFu) - kNoiseOne;
}

/// Integer smoothstep on Q16.16 t in [0, kNoiseOne]: 3t^2 - 2t^3.
constexpr i32 smoothstep_fx(i32 t) noexcept {
    const i64 tt = (static_cast<i64>(t) * t) >> kNoiseShift; // t^2
    const i64 ttt = (tt * t) >> kNoiseShift;                 // t^3
    return static_cast<i32>(3 * tt - 2 * ttt);
}

constexpr i32 lerp_fx(i32 a, i32 b, i32 t) noexcept {
    return a + static_cast<i32>((static_cast<i64>(b - a) * t) >> kNoiseShift);
}

/// 2D value noise sampled at (x, y) where `period` tiles span one lattice cell.
/// Returns Q16.16 in [-1, 1].
constexpr i32 value_noise_2d(u64 seed, i64 x, i64 y, i32 period) noexcept {
    if (period < 1) {
        period = 1;
    }
    const i64 cx = floor_div(x, period);
    const i64 cy = floor_div(y, period);
    const i64 fx = floor_mod(x, period);
    const i64 fy = floor_mod(y, period);

    const i32 tx = smoothstep_fx(static_cast<i32>((fx << kNoiseShift) / period));
    const i32 ty = smoothstep_fx(static_cast<i32>((fy << kNoiseShift) / period));

    const i32 v00 = lattice_value(seed, cx, cy);
    const i32 v10 = lattice_value(seed, cx + 1, cy);
    const i32 v01 = lattice_value(seed, cx, cy + 1);
    const i32 v11 = lattice_value(seed, cx + 1, cy + 1);

    const i32 top = lerp_fx(v00, v10, tx);
    const i32 bottom = lerp_fx(v01, v11, tx);
    return lerp_fx(top, bottom, ty);
}

/// Fractal Brownian motion over value noise. Amplitudes halve per octave and
/// the result is normalised back to roughly [-1, 1] in Q16.16.
constexpr i32 fbm_2d(u64 seed, i64 x, i64 y, i32 basePeriod, int octaves) noexcept {
    if (octaves < 1) {
        octaves = 1;
    }
    i64 sum = 0;
    i64 norm = 0;
    i32 period = basePeriod;
    i32 amp = kNoiseOne;
    for (int o = 0; o < octaves; ++o) {
        const i32 n = value_noise_2d(hash_combine(seed, static_cast<u64>(o) + 1), x, y, period);
        sum += (static_cast<i64>(n) * amp) >> kNoiseShift;
        norm += amp;
        amp >>= 1;
        period = period > 1 ? period / 2 : 1;
        if (amp == 0) {
            break;
        }
    }
    if (norm == 0) {
        return 0;
    }
    return static_cast<i32>((sum << kNoiseShift) / norm);
}

/// Ridged noise: 1 - |fbm|, useful for vein spines and cave walls.
constexpr i32 ridge_2d(u64 seed, i64 x, i64 y, i32 basePeriod, int octaves) noexcept {
    const i32 n = fbm_2d(seed, x, y, basePeriod, octaves);
    const i32 a = n < 0 ? -n : n;
    return kNoiseOne - a;
}

/// Convert Q16.16 noise to a float in [-1, 1] for presentation only.
constexpr float noise_to_float(i32 fx) noexcept {
    return static_cast<float>(fx) / static_cast<float>(kNoiseOne);
}

} // namespace df

namespace df {
/// Q16.16 noise mapped to basis points in [0, 10000]. Shared by generation and
/// tuning tools so both agree on the scale.
constexpr i32 noise_to_bp_pub(i32 n) noexcept {
    const i64 shifted = static_cast<i64>(n) + kNoiseOne;
    return static_cast<i32>((shifted * 10000) / (2 * kNoiseOne));
}
} // namespace df
