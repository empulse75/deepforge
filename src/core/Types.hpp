// Project Deepforge - core fundamental types.
#pragma once

#include <cstddef>
#include <cstdint>

namespace df {

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = std::size_t;

/// World tile coordinate. Positive Y is downward (depth).
struct TileCoord {
    i64 x = 0;
    i64 y = 0;

    friend constexpr bool operator==(const TileCoord&, const TileCoord&) = default;
};

/// Chunk coordinate; a chunk spans kChunkSize tiles on each axis.
struct ChunkCoord {
    i64 x = 0;
    i64 y = 0;

    friend constexpr bool operator==(const ChunkCoord&, const ChunkCoord&) = default;
};

/// Centralized world geometry constants (Section 7.1 of the design brief).
inline constexpr i32 kChunkSize = 32;
inline constexpr i32 kChunkTileCount = kChunkSize * kChunkSize;

/// Simulation runs at a fixed 60 Hz.
inline constexpr i32 kSimulationHz = 60;
inline constexpr double kFixedDeltaSeconds = 1.0 / static_cast<double>(kSimulationHz);

/// Metres represented by one tile edge. Used for depth reporting only.
inline constexpr float kMetresPerTile = 1.0f;

/// Pixel size of a tile at 1x zoom for presentation.
inline constexpr int kTilePixels = 32;

constexpr i64 floor_div(i64 a, i64 b) noexcept {
    const i64 q = a / b;
    const i64 r = a % b;
    return (r != 0 && ((r < 0) != (b < 0))) ? q - 1 : q;
}

constexpr i64 floor_mod(i64 a, i64 b) noexcept {
    const i64 r = a % b;
    return (r != 0 && ((r < 0) != (b < 0))) ? r + b : r;
}

constexpr ChunkCoord chunk_of(TileCoord t) noexcept {
    return ChunkCoord{floor_div(t.x, kChunkSize), floor_div(t.y, kChunkSize)};
}

/// Index of a tile within its chunk, row-major.
constexpr i32 local_index_of(TileCoord t) noexcept {
    const i64 lx = floor_mod(t.x, kChunkSize);
    const i64 ly = floor_mod(t.y, kChunkSize);
    return static_cast<i32>(ly * kChunkSize + lx);
}

constexpr TileCoord tile_of(ChunkCoord c, i32 localIndex) noexcept {
    return TileCoord{c.x * kChunkSize + (localIndex % kChunkSize),
                     c.y * kChunkSize + (localIndex / kChunkSize)};
}

} // namespace df
