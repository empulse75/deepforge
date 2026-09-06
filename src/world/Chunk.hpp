#pragma once

#include "core/Types.hpp"
#include "world/Tile.hpp"

#include <array>
#include <vector>

namespace df {

/// A landmark instance placed by generation. Its mutable state (activated,
/// looted) is persisted; its position and kind are regenerated.
struct LandmarkInstance {
    i32 definition = -1;
    TileCoord origin{};
    i32 width = 0;
    i32 height = 0;
    bool activated = false;
    bool looted = false;
    u64 stableId = 0; ///< Deterministic id used by save files and the map.
};

/// A hazard instance. Hazards are simulated as small actors, not tiles.
struct HazardInstance {
    i32 definition = -1;
    TileCoord tile{};
    float phase = 0.0f; ///< Deterministic cycle position.
    float cooldown = 0.0f;
    bool triggered = false;
    u64 stableId = 0;
};

/// Sparse mining progress for tiles that are partially drilled.
struct MiningProgress {
    i32 localIndex = 0;
    float progress = 0.0f; ///< 0..1
    float sinceTouched = 0.0f;
};

class Chunk {
public:
    Chunk() = default;

    explicit Chunk(ChunkCoord coord) : mCoord(coord) {}

    ChunkCoord coord() const noexcept { return mCoord; }

    void set_coord(ChunkCoord c) noexcept { mCoord = c; }

    i32 material_at(i32 localIndex) const noexcept { return mMaterial[static_cast<usize>(localIndex)]; }

    u8 variant_at(i32 localIndex) const noexcept { return mVariant[static_cast<usize>(localIndex)]; }

    u8 flags_at(i32 localIndex) const noexcept { return mFlags[static_cast<usize>(localIndex)]; }

    Tile tile_at(i32 localIndex) const noexcept {
        const auto i = static_cast<usize>(localIndex);
        return Tile{mMaterial[i], mVariant[i], mFlags[i]};
    }

    void set_material(i32 localIndex, i32 material) noexcept {
        mMaterial[static_cast<usize>(localIndex)] = material;
    }

    void set_variant(i32 localIndex, u8 v) noexcept { mVariant[static_cast<usize>(localIndex)] = v; }

    void set_flags(i32 localIndex, u8 f) noexcept { mFlags[static_cast<usize>(localIndex)] = f; }

    void add_flag(i32 localIndex, TileFlag f) noexcept {
        set_flag(mFlags[static_cast<usize>(localIndex)], f);
    }

    bool dirty() const noexcept { return mDirty; }

    void mark_dirty() noexcept { mDirty = true; }

    void clear_dirty() noexcept { mDirty = false; }

    std::vector<LandmarkInstance>& landmarks() noexcept { return mLandmarks; }

    const std::vector<LandmarkInstance>& landmarks() const noexcept { return mLandmarks; }

    std::vector<HazardInstance>& hazards() noexcept { return mHazards; }

    const std::vector<HazardInstance>& hazards() const noexcept { return mHazards; }

    std::vector<MiningProgress>& mining() noexcept { return mMining; }

    const std::vector<MiningProgress>& mining() const noexcept { return mMining; }

    float mining_progress(i32 localIndex) const noexcept {
        for (const auto& m : mMining) {
            if (m.localIndex == localIndex) {
                return m.progress;
            }
        }
        return 0.0f;
    }

    void set_mining_progress(i32 localIndex, float progress) {
        for (auto& m : mMining) {
            if (m.localIndex == localIndex) {
                m.progress = progress;
                m.sinceTouched = 0.0f;
                return;
            }
        }
        mMining.push_back(MiningProgress{localIndex, progress, 0.0f});
    }

    void clear_mining_progress(i32 localIndex) {
        for (usize i = 0; i < mMining.size(); ++i) {
            if (mMining[i].localIndex == localIndex) {
                mMining[i] = mMining.back();
                mMining.pop_back();
                return;
            }
        }
    }

    /// Deterministic content hash used by generation stability tests.
    u64 content_hash() const;

private:
    ChunkCoord mCoord{};
    std::array<i32, static_cast<usize>(kChunkTileCount)> mMaterial{};
    std::array<u8, static_cast<usize>(kChunkTileCount)> mVariant{};
    std::array<u8, static_cast<usize>(kChunkTileCount)> mFlags{};
    std::vector<LandmarkInstance> mLandmarks;
    std::vector<HazardInstance> mHazards;
    std::vector<MiningProgress> mMining;
    bool mDirty = false;
};

/// Persisted difference between the generated chunk and the played chunk.
struct ChunkDelta {
    struct TileChange {
        i32 localIndex = 0;
        i32 material = 0;
        u8 flags = 0;
    };

    struct LandmarkState {
        u64 stableId = 0;
        bool activated = false;
        bool looted = false;
    };

    ChunkCoord coord{};
    std::vector<TileChange> tiles;
    std::vector<LandmarkState> landmarks;
    std::vector<MiningProgress> mining;
    /// Fog state is a bitset rather than per-tile changes so that walking
    /// through a chunk does not create a thousand delta records.
    std::array<u8, static_cast<usize>(kChunkTileCount) / 8> revealed{};
    bool hasRevealed = false;

    bool empty() const noexcept {
        return tiles.empty() && landmarks.empty() && mining.empty() && !hasRevealed;
    }
};

} // namespace df
