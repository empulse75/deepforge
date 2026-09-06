// Chunk cache, tile queries and the persistent delta store.
//
// The map owns the only mutable view of terrain. Generated content is
// reproducible from the seed, so saves persist only what the player changed
// (Section 7.1).
#pragma once

#include "world/Chunk.hpp"
#include "world/WorldGenerator.hpp"

#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

namespace df {

inline u64 chunk_key(ChunkCoord c) noexcept {
    return (static_cast<u64>(static_cast<u32>(static_cast<i32>(c.x))) << 32) |
           static_cast<u64>(static_cast<u32>(static_cast<i32>(c.y)));
}

/// Counters exposed to the debug overlay (Section 20).
struct WorldStats {
    usize residentChunks = 0;
    usize deltaChunks = 0;
    u64 chunksGenerated = 0;
    u64 chunksEvicted = 0;
    u64 tileQueries = 0;
};

class WorldMap {
public:
    WorldMap(const ContentDatabase& db, WorldGenerator generator, usize cacheCapacity = 512);

    const ContentDatabase& content() const noexcept { return *mDb; }

    const WorldGenerator& generator() const noexcept { return mGenerator; }

    /// Returns the tile, generating or loading its chunk if required.
    Tile tile(TileCoord t) const;

    i32 material_index(TileCoord t) const;

    const MaterialDefinition& material(TileCoord t) const;

    bool is_solid(TileCoord t) const;

    bool is_mineable(TileCoord t) const;

    /// Effective hardness including the biome scaling factor.
    float hardness(TileCoord t) const;

    /// Replaces a tile and records the change for persistence.
    void set_material(TileCoord t, i32 materialIndex);

    float mining_progress(TileCoord t) const;
    void set_mining_progress(TileCoord t, float progress);
    void clear_mining_progress(TileCoord t);

    /// Advances progress decay for partially mined tiles near the player.
    void tick_mining_decay(float dt, TileCoord centre, i32 radius);

    void reveal(TileCoord t);
    bool revealed(TileCoord t) const;
    void reveal_radius(TileCoord centre, i32 radius, bool scanned = false);

    Chunk& chunk(ChunkCoord c) const;

    bool chunk_resident(ChunkCoord c) const;

    /// Removes chunks far from the centre, keeping their deltas.
    void evict_beyond(ChunkCoord centre, i32 radius);

    /// All persisted differences, ready for the save file.
    std::vector<ChunkDelta> collect_deltas() const;

    void apply_deltas(const std::vector<ChunkDelta>& deltas);

    std::vector<LandmarkInstance*> landmarks_near(TileCoord centre, i32 radius) const;

    /// Marks a landmark instance's persistent state.
    void set_landmark_state(ChunkCoord c, u64 stableId, bool activated, bool looted);

    const WorldStats& stats() const noexcept { return mStats; }

    void reset_stats() noexcept {
        mStats.chunksGenerated = 0;
        mStats.chunksEvicted = 0;
    }

private:
    Chunk& ensure_chunk(ChunkCoord c) const;
    ChunkDelta& delta_for(ChunkCoord c) const;
    void record_tile_change(ChunkCoord c, i32 localIndex, i32 material) const;

    const ContentDatabase* mDb = nullptr;
    WorldGenerator mGenerator;
    usize mCacheCapacity = 512;

    mutable std::unordered_map<u64, std::unique_ptr<Chunk>> mChunks;
    mutable std::deque<u64> mLru;
    mutable std::unordered_map<u64, ChunkDelta> mDeltas;
    mutable WorldStats mStats;
};

} // namespace df
