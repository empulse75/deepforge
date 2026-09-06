// Terrain storage primitives. Terrain is never represented as entities
// (Section 15.5): a chunk is a set of compact parallel arrays.
#pragma once

#include "core/Types.hpp"

namespace df {

/// Per-tile bit flags. Fog and scan state live here; anything derivable for
/// rendering does not.
enum class TileFlag : u8 {
    Revealed = 1 << 0, ///< Player has seen this tile.
    Scanned = 1 << 1,  ///< Revealed by the scanner rather than by sight.
    Modified = 1 << 2, ///< Differs from generated output; must be persisted.
    Landmark = 1 << 3, ///< Part of a landmark stamp.
    NoHazard = 1 << 4, ///< Generation guaranteed this tile is hazard-free.
    Support = 1 << 5,  ///< Structural: removing it can trigger a collapse.
};

constexpr u8 operator|(TileFlag a, TileFlag b) noexcept {
    return static_cast<u8>(static_cast<u8>(a) | static_cast<u8>(b));
}

constexpr bool has_flag(u8 bits, TileFlag f) noexcept {
    return (bits & static_cast<u8>(f)) != 0;
}

constexpr void set_flag(u8& bits, TileFlag f) noexcept {
    bits = static_cast<u8>(bits | static_cast<u8>(f));
}

constexpr void clear_flag(u8& bits, TileFlag f) noexcept {
    bits = static_cast<u8>(bits & static_cast<u8>(~static_cast<u8>(f)));
}

/// One tile as seen by gameplay code.
struct Tile {
    i32 material = -1; ///< Index into ContentDatabase::materials().
    u8 variant = 0;    ///< Cosmetic variation; never affects simulation.
    u8 flags = 0;

    constexpr bool valid() const noexcept { return material >= 0; }
};

} // namespace df
