#include "world/WorldGenerator.hpp"

#include "core/Log.hpp"
#include "core/Math.hpp"
#include "core/Noise.hpp"

#include <algorithm>

namespace df {
namespace {

/// Converts Q16.16 noise in [-1,1] to basis points in [0,10000].
constexpr i32 noise_to_bp(i32 n) {
    const i64 shifted = static_cast<i64>(n) + kNoiseOne; // [0, 2*one]
    return static_cast<i32>((shifted * 10000) / (2 * kNoiseOne));
}

constexpr i32 kVeinStreamSalt = 0x5645494E; // "VEIN"
constexpr i32 kPityStreamSalt = 0x50495459; // "PITY"

} // namespace

WorldGenerator::WorldGenerator(const ContentDatabase& db, u64 worldSeed, WorldRules rules)
    : mDb(&db), mSeed(worldSeed), mRules(rules) {
    mAir = db.airMaterial.index;
    mBedrock = db.boundaryMaterial.index;

    mIsPlainRock.assign(db.materials().size(), false);
    for (const auto& b : db.biomes()) {
        const auto base = db.material(b.baseMaterial.text());
        const auto soft = db.material(b.softMaterial.text());
        const auto structural = db.material(b.structuralMaterial.text());
        mBiomeBase.push_back(base.index);
        mBiomeSoft.push_back(soft.index);
        mBiomeStructural.push_back(structural.index);
        if (base.valid()) {
            mIsPlainRock[static_cast<usize>(base.index)] = true;
        }
        if (soft.valid()) {
            mIsPlainRock[static_cast<usize>(soft.index)] = true;
        }
    }

    for (usize i = 0; i < db.materials().size(); ++i) {
        const auto& m = db.materials()[i];
        if (m.rarityWeightBp <= 0) {
            continue;
        }
        MaterialGenRule rule;
        rule.materialIndex = static_cast<i32>(i);
        rule.minDepth = m.minDepth;
        rule.maxDepth = m.maxDepth;
        rule.veinSizeMin = std::max(1, m.veinSizeMin);
        rule.veinSizeMax = std::max(rule.veinSizeMin, m.veinSizeMax);
        rule.rarityWeightBp = m.rarityWeightBp;
        rule.fieldPeriod = std::max(4, m.fieldPeriod);
        rule.fieldThresholdBp = m.fieldThresholdBp;
        // Artifacts, strategic materials and volatiles get pity placement so no
        // seed can lock a player out of mandatory progression (Section 7.4).
        rule.guaranteed =
            m.category == MaterialCategory::Artifact || m.category == MaterialCategory::Strategic ||
            m.category == MaterialCategory::Volatile || m.category == MaterialCategory::Commodity;
        for (const auto& bid : m.biomes) {
            const auto h = db.biome(bid.text());
            if (h.valid()) {
                rule.biomeIndices.push_back(h.index);
            }
        }
        mMaxVeinMargin = std::max(mMaxVeinMargin, rule.veinSizeMax + 2);
        mVeinRules.push_back(std::move(rule));
    }

    for (const auto& l : db.landmarks()) {
        mMaxLandmarkMargin = std::max(mMaxLandmarkMargin, std::max(l.width, l.height) + 2);
    }
}

i32 WorldGenerator::biome_index_at(i64 depth) const {
    const auto& biomes = mDb->biomes();
    if (biomes.empty()) {
        return 0;
    }
    for (usize i = 0; i < biomes.size(); ++i) {
        if (depth <= biomes[i].maxDepth) {
            return static_cast<i32>(i);
        }
    }
    return static_cast<i32>(biomes.size() - 1);
}

i64 WorldGenerator::surface_row(i64 x) const {
    if (mRules.surfaceRoughness <= 0) {
        return 0;
    }
    const i32 n = value_noise_2d(derive_seed(mSeed, RngStream::Terrain), x, 0, 40);
    const i64 offset = (static_cast<i64>(n) * mRules.surfaceRoughness) / kNoiseOne;
    return offset;
}

bool WorldGenerator::in_tutorial_zone(TileCoord t) const {
    return t.y <= mRules.tutorialSafeDepth && std::llabs(t.x - mRules.shaftX) <= mRules.tutorialSafeHalfWidth;
}

bool WorldGenerator::is_plain_rock(const Chunk& c, i32 localIndex) const {
    const i32 m = c.material_at(localIndex);
    if (m < 0 || static_cast<usize>(m) >= mIsPlainRock.size()) {
        return false;
    }
    if (has_flag(c.flags_at(localIndex), TileFlag::Landmark)) {
        return false;
    }
    return mIsPlainRock[static_cast<usize>(m)];
}

bool WorldGenerator::rule_allows(const MaterialGenRule& rule, i64 depth, i32 biomeIndex) const {
    if (depth < rule.minDepth || depth > rule.maxDepth) {
        return false;
    }
    if (rule.biomeIndices.empty()) {
        return true;
    }
    return std::find(rule.biomeIndices.begin(), rule.biomeIndices.end(), biomeIndex) !=
           rule.biomeIndices.end();
}

// ---------------------------------------------------------------------------

void WorldGenerator::generate(ChunkCoord coord, Chunk& out) const {
    out = Chunk{coord};
    pass_base_geology(coord, out);
    pass_caves(coord, out);
    pass_structural(coord, out);
    pass_veins(coord, out);
    pass_guaranteed_veins(coord, out);
    pass_landmarks(coord, out);
    pass_hazards(coord, out);
    pass_entry_shaft(coord, out);
    pass_safety(coord, out);
}

void WorldGenerator::pass_base_geology(ChunkCoord coord, Chunk& out) const {
    const u64 variantSeed = derive_seed(mSeed, RngStream::Cosmetic, coord.x, coord.y);
    for (i32 i = 0; i < kChunkTileCount; ++i) {
        const TileCoord t = tile_of(coord, i);
        if (std::llabs(t.x) > mRules.halfWidth || t.y > mRules.bottomDepth) {
            out.set_material(i, mBedrock);
            continue;
        }
        const i64 surface = surface_row(t.x);
        if (t.y < surface) {
            out.set_material(i, mAir);
            continue;
        }
        const i32 biomeIndex = biome_index_at(t.y);
        const auto& biome = mDb->biomes()[static_cast<usize>(biomeIndex)];
        i32 material = mBiomeBase[static_cast<usize>(biomeIndex)];

        // Gradual transition: near the top of a biome, blend in the previous
        // biome's base so the boundary is visible but never a hard line.
        if (biomeIndex > 0) {
            const i64 into = t.y - biome.minDepth;
            if (into >= 0 && into < biome.transitionBand) {
                const i32 mixBp = static_cast<i32>((into * 10000) / std::max<i64>(1, biome.transitionBand));
                const u64 h = hash_coords(derive_seed(mSeed, RngStream::Terrain, 1, 0), t.x, t.y);
                if (static_cast<i32>(h % 10000) > mixBp) {
                    material = mBiomeBase[static_cast<usize>(biomeIndex - 1)];
                }
            }
        }
        // Soft pockets give the drill something to bite into and vary pacing.
        const i32 soft = fbm_2d(derive_seed(mSeed, RngStream::Terrain, 2, 0), t.x, t.y, 26, 2);
        if (noise_to_bp(soft) > 7400) {
            material = mBiomeSoft[static_cast<usize>(biomeIndex)];
        }
        out.set_material(i, material);
        out.set_variant(i, static_cast<u8>(hash_coords(variantSeed, t.x, t.y) & 0x3u));
    }
}

void WorldGenerator::pass_caves(ChunkCoord coord, Chunk& out) const {
    const u64 caveSeed = derive_seed(mSeed, RngStream::Caves);
    for (i32 i = 0; i < kChunkTileCount; ++i) {
        const TileCoord t = tile_of(coord, i);
        if (out.material_at(i) == mBedrock || out.material_at(i) == mAir) {
            continue;
        }
        // Keep a solid crust so the surface reads as ground.
        if (t.y < surface_row(t.x) + 4) {
            continue;
        }
        const i32 biomeIndex = biome_index_at(t.y);
        const auto& biome = mDb->biomes()[static_cast<usize>(biomeIndex)];
        const i32 n = fbm_2d(caveSeed, t.x, t.y, biome.cavePeriod, 3);
        const i32 openness = noise_to_bp(n);
        if (openness > 10000 - biome.caveThresholdBp) {
            out.set_material(i, mAir);
        }
    }
}

void WorldGenerator::pass_structural(ChunkCoord coord, Chunk& out) const {
    const u64 sSeed = derive_seed(mSeed, RngStream::Terrain, 7, 7);
    for (i32 i = 0; i < kChunkTileCount; ++i) {
        const TileCoord t = tile_of(coord, i);
        if (t.y < 48 || !is_plain_rock(out, i)) {
            continue;
        }
        const i32 biomeIndex = biome_index_at(t.y);
        const i32 r = ridge_2d(sSeed, t.x, t.y, 34, 3);
        if (noise_to_bp(r) > 9900) {
            out.set_material(i, mBiomeStructural[static_cast<usize>(biomeIndex)]);
            out.add_flag(i, TileFlag::Support);
        }
    }
}

void WorldGenerator::stamp_vein(ChunkCoord coord, Chunk& out, const MaterialGenRule& rule, TileCoord anchor,
                                i32 size, Rng& rng) const {
    TileCoord p = anchor;
    for (i32 step = 0; step < size; ++step) {
        if (chunk_of(p) == coord) {
            const i32 li = local_index_of(p);
            const i32 biomeIndex = biome_index_at(p.y);
            if (is_plain_rock(out, li) && rule_allows(rule, p.y, biomeIndex)) {
                out.set_material(li, rule.materialIndex);
            }
        }
        // Bias the walk slightly horizontally so veins read as seams.
        switch (rng.below(6)) {
            case 0:
            case 1:
                p.x += 1;
                break;
            case 2:
            case 3:
                p.x -= 1;
                break;
            case 4:
                p.y += 1;
                break;
            default:
                p.y -= 1;
                break;
        }
    }
}

void WorldGenerator::pass_veins(ChunkCoord coord, Chunk& out) const {
    const i64 minX = coord.x * kChunkSize - mMaxVeinMargin;
    const i64 maxX = coord.x * kChunkSize + kChunkSize + mMaxVeinMargin;
    const i64 minY = coord.y * kChunkSize - mMaxVeinMargin;
    const i64 maxY = coord.y * kChunkSize + kChunkSize + mMaxVeinMargin;

    for (const auto& rule : mVeinRules) {
        const i32 period = rule.fieldPeriod;
        const i64 c0x = floor_div(minX, period);
        const i64 c1x = floor_div(maxX, period);
        const i64 c0y = floor_div(minY, period);
        const i64 c1y = floor_div(maxY, period);
        for (i64 cy = c0y; cy <= c1y; ++cy) {
            for (i64 cx = c0x; cx <= c1x; ++cx) {
                const i64 anchorY = cy * period + period / 2;
                if (anchorY < rule.minDepth - rule.veinSizeMax ||
                    anchorY > rule.maxDepth + rule.veinSizeMax) {
                    continue;
                }
                Rng rng{hash_coords(hash_combine(derive_seed(mSeed, RngStream::Ores),
                                                 static_cast<u64>(rule.materialIndex) * 0x9E37u +
                                                     static_cast<u64>(kVeinStreamSalt)),
                                    cx, cy)};
                // Field noise makes veins clump into believable ore fields.
                const i32 field = fbm_2d(derive_seed(mSeed, RngStream::Ores, rule.materialIndex + 1, 0),
                                         cx * period, cy * period, 6 * period, 2);
                if (noise_to_bp(field) < rule.fieldThresholdBp) {
                    continue;
                }
                if (!rng.chance_bp(static_cast<u32>(rule.rarityWeightBp))) {
                    continue;
                }
                const TileCoord anchor{cx * period + rng.range(0, period - 1),
                                       cy * period + rng.range(0, period - 1)};
                const i32 size = rng.range(rule.veinSizeMin, rule.veinSizeMax);
                stamp_vein(coord, out, rule, anchor, size, rng);
            }
        }
    }
}

void WorldGenerator::pass_guaranteed_veins(ChunkCoord coord, Chunk& out) const {
    // Pity rule: inside every region of the world, each guaranteed material is
    // placed at least once, so no seed can withhold mandatory progression.
    const i64 region = mRules.veinRegionSize;
    const i64 minX = coord.x * kChunkSize - mMaxVeinMargin;
    const i64 maxX = coord.x * kChunkSize + kChunkSize + mMaxVeinMargin;
    const i64 minY = coord.y * kChunkSize - mMaxVeinMargin;
    const i64 maxY = coord.y * kChunkSize + kChunkSize + mMaxVeinMargin;

    for (const auto& rule : mVeinRules) {
        if (!rule.guaranteed) {
            continue;
        }
        for (i64 ry = floor_div(minY, region); ry <= floor_div(maxY, region); ++ry) {
            for (i64 rx = floor_div(minX, region); rx <= floor_div(maxX, region); ++rx) {
                const i64 regionTop = ry * region;
                const i64 regionBottom = regionTop + region - 1;
                if (regionBottom < rule.minDepth || regionTop > rule.maxDepth) {
                    continue;
                }
                Rng rng{hash_coords(hash_combine(derive_seed(mSeed, RngStream::Ores),
                                                 static_cast<u64>(rule.materialIndex) * 0x1F1Fu +
                                                     static_cast<u64>(kPityStreamSalt)),
                                    rx, ry)};
                const i64 lo = std::max(regionTop, static_cast<i64>(rule.minDepth));
                const i64 hi = std::min(regionBottom, static_cast<i64>(rule.maxDepth));
                if (hi < lo) {
                    continue;
                }
                const i64 ax = rx * region + rng.range(2, static_cast<i32>(region) - 3);
                const i64 ay = lo + rng.below(static_cast<u32>(hi - lo + 1));
                if (std::llabs(ax) > mRules.halfWidth - 2) {
                    continue;
                }
                const i32 size = rng.range(rule.veinSizeMin, rule.veinSizeMax);
                stamp_vein(coord, out, rule, TileCoord{ax, ay}, size, rng);
            }
        }
    }
}

void WorldGenerator::pass_landmarks(ChunkCoord coord, Chunk& out) const {
    const i64 minX = coord.x * kChunkSize - mMaxLandmarkMargin;
    const i64 maxX = coord.x * kChunkSize + kChunkSize + mMaxLandmarkMargin;
    const i64 minY = coord.y * kChunkSize - mMaxLandmarkMargin;
    const i64 maxY = coord.y * kChunkSize + kChunkSize + mMaxLandmarkMargin;

    const auto& defs = mDb->landmarks();
    for (usize li = 0; li < defs.size(); ++li) {
        const auto& def = defs[li];
        const i64 region = def.regionHeight;
        for (i64 ry = floor_div(minY, region); ry <= floor_div(maxY, region); ++ry) {
            for (i64 rx = floor_div(minX, region); rx <= floor_div(maxX, region); ++rx) {
                const i64 regionTop = ry * region;
                if (regionTop + region < def.minDepth || regionTop > def.maxDepth) {
                    continue;
                }
                Rng rng{hash_coords(
                    hash_combine(derive_seed(mSeed, RngStream::Landmarks), fnv1a64(def.id.text())), rx, ry)};
                const i32 count = rng.range(def.perRegionMin, def.perRegionMax);
                std::vector<TileCoord> placed;
                for (i32 n = 0; n < count; ++n) {
                    TileCoord origin{};
                    bool ok = false;
                    for (int attempt = 0; attempt < 6 && !ok; ++attempt) {
                        origin.x = rx * region + rng.range(1, static_cast<i32>(region) - def.width - 2);
                        origin.y = ry * region + rng.range(1, static_cast<i32>(region) - def.height - 2);
                        if (origin.y < def.minDepth || origin.y + def.height > def.maxDepth) {
                            continue;
                        }
                        if (std::llabs(origin.x) > mRules.halfWidth - def.width - 2) {
                            continue;
                        }
                        // A landmark must never overwrite the tutorial zone.
                        if (origin.y < 40) {
                            continue;
                        }
                        ok = true;
                        for (const auto& p : placed) {
                            const i64 dx = p.x - origin.x;
                            const i64 dy = p.y - origin.y;
                            if (dx * dx + dy * dy < static_cast<i64>(def.minSpacing) * def.minSpacing) {
                                ok = false;
                                break;
                            }
                        }
                    }
                    if (!ok) {
                        continue;
                    }
                    placed.push_back(origin);

                    const i32 biomeIndex = biome_index_at(origin.y);
                    if (!def.biomes.empty()) {
                        bool biomeOk = false;
                        for (const auto& bid : def.biomes) {
                            const auto h = mDb->biome(bid.text());
                            if (h.valid() && h.index == biomeIndex) {
                                biomeOk = true;
                                break;
                            }
                        }
                        if (!biomeOk) {
                            continue;
                        }
                    }

                    const u64 stableId =
                        hash_combine(hash_coords(fnv1a64(def.id.text()), origin.x, origin.y), mSeed);

                    // Stamp: hollow the room, keep a one-tile shell of support.
                    bool touchesChunk = false;
                    Rng lootRng{hash_combine(stableId, 0x1007ull)};
                    for (i32 dy = 0; dy < def.height; ++dy) {
                        for (i32 dx = 0; dx < def.width; ++dx) {
                            const TileCoord t{origin.x + dx, origin.y + dy};
                            if (chunk_of(t) != coord) {
                                continue;
                            }
                            touchesChunk = true;
                            const i32 idx = local_index_of(t);
                            if (out.material_at(idx) == mBedrock) {
                                continue;
                            }
                            const bool edge =
                                dx == 0 || dy == 0 || dx == def.width - 1 || dy == def.height - 1;
                            if (edge && def.kind != LandmarkKind::Cavern &&
                                def.kind != LandmarkKind::CrumblyLayer) {
                                out.set_material(idx, mBiomeStructural[static_cast<usize>(biomeIndex)]);
                                out.add_flag(idx, TileFlag::Landmark);
                            } else {
                                out.set_material(idx, mAir);
                                out.add_flag(idx, TileFlag::Landmark);
                                out.add_flag(idx, TileFlag::NoHazard);
                            }
                        }
                    }
                    // Loot: seed ore tiles on the floor of the room.
                    if (!def.lootMaterials.empty() && def.lootAmountMax > 0) {
                        const i32 amount = lootRng.range(def.lootAmountMin, def.lootAmountMax);
                        for (i32 k = 0; k < amount; ++k) {
                            const auto& mid =
                                def.lootMaterials[lootRng.below(static_cast<u32>(def.lootMaterials.size()))];
                            const auto mh = mDb->material(mid.text());
                            if (!mh.valid()) {
                                continue;
                            }
                            const TileCoord t{origin.x + 1 + lootRng.range(0, std::max(0, def.width - 3)),
                                              origin.y + def.height - 2};
                            if (chunk_of(t) != coord) {
                                continue;
                            }
                            const i32 idx = local_index_of(t);
                            if (out.material_at(idx) == mAir) {
                                out.set_material(idx, mh.index);
                            }
                        }
                    }
                    if (touchesChunk) {
                        LandmarkInstance inst;
                        inst.definition = static_cast<i32>(li);
                        inst.origin = origin;
                        inst.width = def.width;
                        inst.height = def.height;
                        inst.stableId = stableId;
                        // Only the chunk containing the origin owns the instance,
                        // so it is never duplicated across chunk boundaries.
                        if (chunk_of(origin) == coord) {
                            out.landmarks().push_back(inst);
                        }
                    }
                }
            }
        }
    }
}

void WorldGenerator::pass_hazards(ChunkCoord coord, Chunk& out) const {
    const auto& biomes = mDb->biomes();
    const i64 chunkTopDepth = coord.y * kChunkSize;
    if (chunkTopDepth < 0) {
        return;
    }
    const i32 biomeIndex = biome_index_at(chunkTopDepth + kChunkSize / 2);
    const auto& biome = biomes[static_cast<usize>(biomeIndex)];
    if (biome.hazards.empty()) {
        return;
    }
    Rng rng = make_rng(mSeed, RngStream::Hazards, coord.x, coord.y);
    i32 totalWeight = 0;
    for (const auto& h : biome.hazards) {
        totalWeight += h.weightBp;
    }
    if (totalWeight <= 0) {
        return;
    }
    // Density rises slowly with depth but stays bounded.
    const i32 attempts = 1 + static_cast<i32>(std::min<i64>(3, chunkTopDepth / 900));
    for (i32 a = 0; a < attempts; ++a) {
        if (!rng.chance_bp(5200)) {
            continue;
        }
        i32 pick = static_cast<i32>(rng.below(static_cast<u32>(totalWeight)));
        const BiomeHazardEntry* chosen = nullptr;
        for (const auto& h : biome.hazards) {
            pick -= h.weightBp;
            if (pick < 0) {
                chosen = &h;
                break;
            }
        }
        if (chosen == nullptr) {
            continue;
        }
        const auto hh = mDb->hazard(chosen->hazardId.text());
        if (!hh.valid()) {
            continue;
        }
        const auto& hdef = mDb->hazard_def(hh);
        const i32 localIndex = static_cast<i32>(rng.below(static_cast<u32>(kChunkTileCount)));
        const TileCoord t = tile_of(coord, localIndex);
        if (t.y < hdef.minDepth) {
            continue;
        }
        if (in_tutorial_zone(t)) {
            continue; // Section 7.4: no lethal surprises in the first layer.
        }
        if (out.material_at(localIndex) != mAir) {
            continue;
        }
        if (has_flag(out.flags_at(localIndex), TileFlag::NoHazard)) {
            continue;
        }
        HazardInstance inst;
        inst.definition = hh.index;
        inst.tile = t;
        inst.phase = static_cast<float>(rng.below(1000)) / 1000.0f;
        inst.stableId = hash_coords(derive_seed(mSeed, RngStream::Hazards), t.x, t.y);
        out.hazards().push_back(inst);
    }
}

void WorldGenerator::pass_entry_shaft(ChunkCoord coord, Chunk& out) const {
    for (i32 i = 0; i < kChunkTileCount; ++i) {
        const TileCoord t = tile_of(coord, i);
        if (t.y < surface_row(t.x) || t.y > mRules.shaftDepth) {
            continue;
        }
        if (std::llabs(t.x - mRules.shaftX) <= mRules.shaftHalfWidth) {
            out.set_material(i, mAir);
            out.add_flag(i, TileFlag::NoHazard);
        }
    }
}

void WorldGenerator::pass_safety(ChunkCoord coord, Chunk& out) const {
    // Tutorial-safe template: guarantee a friendly starting layer.
    for (i32 i = 0; i < kChunkTileCount; ++i) {
        const TileCoord t = tile_of(coord, i);
        if (!in_tutorial_zone(t)) {
            continue;
        }
        out.add_flag(i, TileFlag::NoHazard);
        // Nothing above the tutorial depth may be structural or indestructible
        // unless it is the world boundary itself.
        const i32 m = out.material_at(i);
        if (m == mBedrock && std::llabs(t.x) <= mRules.halfWidth && t.y <= mRules.bottomDepth) {
            out.set_material(i, mBiomeBase[0]);
        }
        if (m >= 0 && static_cast<usize>(m) < mDb->materials().size()) {
            const auto& def = mDb->materials()[static_cast<usize>(m)];
            if (def.category == MaterialCategory::Structural && t.y < 40) {
                out.set_material(i, mBiomeBase[0]);
            }
            if (def.category == MaterialCategory::Volatile) {
                out.set_material(i, mBiomeBase[0]);
            }
        }
    }
    // Starter guarantee: several readable veins flank the entry shaft so the
    // very first expedition always produces a sale, on every seed
    // (Section 7.4 tutorial-safe template).
    {
        const auto ferrite = mDb->material("material.ferrite");
        const auto cuprite = mDb->material("material.cuprite");
        if (ferrite.valid()) {
            Rng rng = make_rng(mSeed, RngStream::Ores, 0xFEED, 0);
            for (i32 cluster = 0; cluster < 8; ++cluster) {
                const i32 side = (cluster % 2 == 0) ? 1 : -1;
                const TileCoord anchor{mRules.shaftX + side * rng.range(2, 12),
                                       10 + cluster * 8 + rng.range(0, 6)};
                const i32 material = (cluster >= 5 && cuprite.valid()) ? cuprite.index : ferrite.index;
                const i32 size = rng.range(6, 11);
                TileCoord p = anchor;
                for (i32 step = 0; step < size; ++step) {
                    if (chunk_of(p) == coord) {
                        const i32 idx = local_index_of(p);
                        if (is_plain_rock(out, idx)) {
                            out.set_material(idx, material);
                        }
                    }
                    switch (rng.below(4)) {
                        case 0:
                            p.x += 1;
                            break;
                        case 1:
                            p.x -= 1;
                            break;
                        case 2:
                            p.y += 1;
                            break;
                        default:
                            p.y -= 1;
                            break;
                    }
                }
            }
        }
    }
}

} // namespace df
