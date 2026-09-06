// Deterministic, chunked world generation.
//
// Every pass takes an explicitly derived random stream so that adding a new
// pass (or a cosmetic consumer) can never move ore for an existing seed
// (Section 7.2). All generation arithmetic is integer-only, so output is
// bit-identical across platforms and standard libraries.
#pragma once

#include "content/ContentDatabase.hpp"
#include "core/Rng.hpp"
#include "world/Chunk.hpp"

#include <vector>

namespace df {

/// Fixed world geometry. Values are tiles.
struct WorldRules {
    i64 halfWidth = 420;    ///< Playable half-width; beyond this is bedrock.
    i64 bottomDepth = 4200; ///< Deepest playable row.
    i64 skyHeight = 30;     ///< Air rows above the surface.
    i64 shaftX = 0;         ///< Entry shaft centre.
    i32 shaftHalfWidth = 1;
    i64 shaftDepth = 26;        ///< Pre-carved shaft depth from the surface.
    i64 tutorialSafeDepth = 96; ///< No lethal hazards above this depth near spawn.
    i64 tutorialSafeHalfWidth = 30;
    i64 veinRegionSize = 224; ///< Pity/guarantee region for critical resources.
    i32 surfaceRoughness = 3; ///< Max surface height variation in tiles.
};

/// Resolved per-material generation rule, precomputed once at construction.
struct MaterialGenRule {
    i32 materialIndex = -1;
    std::vector<i32> biomeIndices; ///< Empty means every biome.
    i32 minDepth = 0;
    i32 maxDepth = 0;
    i32 veinSizeMin = 1;
    i32 veinSizeMax = 1;
    i32 rarityWeightBp = 0;
    i32 fieldPeriod = 24;
    i32 fieldThresholdBp = 0;
    bool guaranteed = false; ///< Participates in the pity placement pass.
};

class WorldGenerator {
public:
    WorldGenerator(const ContentDatabase& db, u64 worldSeed, WorldRules rules = {});

    /// Generates a chunk from scratch. Deterministic for (seed, coord, content).
    void generate(ChunkCoord coord, Chunk& out) const;

    u64 seed() const noexcept { return mSeed; }

    const WorldRules& rules() const noexcept { return mRules; }

    const ContentDatabase& content() const noexcept { return *mDb; }

    /// Biome index covering a depth, honouring the transition band.
    i32 biome_index_at(i64 depth) const;

    /// Surface row for a column; the top solid tile.
    i64 surface_row(i64 x) const;

    /// True when the tile lies inside the tutorial-safe template near spawn.
    bool in_tutorial_zone(TileCoord t) const;

private:
    void pass_base_geology(ChunkCoord coord, Chunk& out) const;
    void pass_caves(ChunkCoord coord, Chunk& out) const;
    void pass_structural(ChunkCoord coord, Chunk& out) const;
    void pass_veins(ChunkCoord coord, Chunk& out) const;
    void pass_guaranteed_veins(ChunkCoord coord, Chunk& out) const;
    void pass_landmarks(ChunkCoord coord, Chunk& out) const;
    void pass_hazards(ChunkCoord coord, Chunk& out) const;
    void pass_entry_shaft(ChunkCoord coord, Chunk& out) const;
    void pass_safety(ChunkCoord coord, Chunk& out) const;

    /// Places a vein blob; only overwrites plain rock of a matching biome.
    void stamp_vein(ChunkCoord coord, Chunk& out, const MaterialGenRule& rule, TileCoord anchor, i32 size,
                    Rng& rng) const;

    bool rule_allows(const MaterialGenRule& rule, i64 depth, i32 biomeIndex) const;

    bool is_plain_rock(const Chunk& c, i32 localIndex) const;

    const ContentDatabase* mDb = nullptr;
    u64 mSeed = 0;
    WorldRules mRules;
    std::vector<MaterialGenRule> mVeinRules;
    i32 mAir = -1;
    i32 mBedrock = -1;
    std::vector<i32> mBiomeBase; ///< material index per biome
    std::vector<i32> mBiomeSoft;
    std::vector<i32> mBiomeStructural;
    std::vector<bool> mIsPlainRock; ///< indexed by material
    i32 mMaxVeinMargin = 8;
    i32 mMaxLandmarkMargin = 24;
};

} // namespace df
