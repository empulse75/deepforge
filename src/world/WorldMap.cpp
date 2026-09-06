#include "world/WorldMap.hpp"

#include "core/Log.hpp"
#include "core/Math.hpp"

#include <algorithm>

namespace df {

WorldMap::WorldMap(const ContentDatabase& db, WorldGenerator generator, usize cacheCapacity)
    : mDb(&db), mGenerator(std::move(generator)), mCacheCapacity(std::max<usize>(16, cacheCapacity)) {}

Chunk& WorldMap::ensure_chunk(ChunkCoord c) const {
    const u64 key = chunk_key(c);
    const auto it = mChunks.find(key);
    if (it != mChunks.end()) {
        return *it->second;
    }
    auto chunkPtr = std::make_unique<Chunk>();
    mGenerator.generate(c, *chunkPtr);
    ++mStats.chunksGenerated;

    // Re-apply persisted differences on top of freshly generated terrain.
    const auto dit = mDeltas.find(key);
    if (dit != mDeltas.end()) {
        const ChunkDelta& d = dit->second;
        for (const auto& tc : d.tiles) {
            chunkPtr->set_material(tc.localIndex, tc.material);
            chunkPtr->set_flags(tc.localIndex, static_cast<u8>(chunkPtr->flags_at(tc.localIndex) | tc.flags |
                                                               static_cast<u8>(TileFlag::Modified)));
        }
        if (d.hasRevealed) {
            for (i32 i = 0; i < kChunkTileCount; ++i) {
                if ((d.revealed[static_cast<usize>(i) / 8] & (1u << (i % 8))) != 0) {
                    chunkPtr->add_flag(i, TileFlag::Revealed);
                }
            }
        }
        for (const auto& ls : d.landmarks) {
            for (auto& inst : chunkPtr->landmarks()) {
                if (inst.stableId == ls.stableId) {
                    inst.activated = ls.activated;
                    inst.looted = ls.looted;
                }
            }
        }
        for (const auto& mp : d.mining) {
            chunkPtr->set_mining_progress(mp.localIndex, mp.progress);
        }
    }

    Chunk& ref = *chunkPtr;
    mChunks.emplace(key, std::move(chunkPtr));
    mLru.push_back(key);

    while (mChunks.size() > mCacheCapacity && !mLru.empty()) {
        const u64 oldest = mLru.front();
        mLru.pop_front();
        if (oldest == key) {
            mLru.push_back(oldest);
            break;
        }
        if (mChunks.erase(oldest) > 0) {
            ++mStats.chunksEvicted;
        }
    }
    mStats.residentChunks = mChunks.size();
    return ref;
}

Chunk& WorldMap::chunk(ChunkCoord c) const {
    return ensure_chunk(c);
}

bool WorldMap::chunk_resident(ChunkCoord c) const {
    return mChunks.find(chunk_key(c)) != mChunks.end();
}

ChunkDelta& WorldMap::delta_for(ChunkCoord c) const {
    const u64 key = chunk_key(c);
    auto it = mDeltas.find(key);
    if (it == mDeltas.end()) {
        ChunkDelta d;
        d.coord = c;
        it = mDeltas.emplace(key, std::move(d)).first;
    }
    mStats.deltaChunks = mDeltas.size();
    return it->second;
}

Tile WorldMap::tile(TileCoord t) const {
    ++mStats.tileQueries;
    return ensure_chunk(chunk_of(t)).tile_at(local_index_of(t));
}

i32 WorldMap::material_index(TileCoord t) const {
    return ensure_chunk(chunk_of(t)).material_at(local_index_of(t));
}

const MaterialDefinition& WorldMap::material(TileCoord t) const {
    const i32 idx = material_index(t);
    return mDb->materials()[static_cast<usize>(std::max(0, idx))];
}

bool WorldMap::is_solid(TileCoord t) const {
    return material(t).solid;
}

bool WorldMap::is_mineable(TileCoord t) const {
    const auto& m = material(t);
    return m.solid && m.mineable;
}

float WorldMap::hardness(TileCoord t) const {
    const auto& m = material(t);
    if (!m.mineable) {
        return 1e9f;
    }
    const i32 bi = mGenerator.biome_index_at(t.y);
    const float scale = mDb->biomes()[static_cast<usize>(bi)].hardnessScale;
    // Structural rock keeps its own hardness rather than scaling further; it is
    // already meant to be a route decision, not a grind.
    return m.category == MaterialCategory::Structural ? m.hardness : m.hardness * scale;
}

void WorldMap::record_tile_change(ChunkCoord c, i32 localIndex, i32 materialIndex) const {
    ChunkDelta& d = delta_for(c);
    for (auto& tc : d.tiles) {
        if (tc.localIndex == localIndex) {
            tc.material = materialIndex;
            return;
        }
    }
    d.tiles.push_back(ChunkDelta::TileChange{localIndex, materialIndex, static_cast<u8>(TileFlag::Modified)});
}

void WorldMap::set_material(TileCoord t, i32 materialIndex) {
    const ChunkCoord c = chunk_of(t);
    const i32 li = local_index_of(t);
    Chunk& ch = ensure_chunk(c);
    ch.set_material(li, materialIndex);
    ch.add_flag(li, TileFlag::Modified);
    ch.mark_dirty();
    record_tile_change(c, li, materialIndex);
}

float WorldMap::mining_progress(TileCoord t) const {
    return ensure_chunk(chunk_of(t)).mining_progress(local_index_of(t));
}

void WorldMap::set_mining_progress(TileCoord t, float progress) {
    const ChunkCoord c = chunk_of(t);
    const i32 li = local_index_of(t);
    ensure_chunk(c).set_mining_progress(li, progress);
    ChunkDelta& d = delta_for(c);
    for (auto& mp : d.mining) {
        if (mp.localIndex == li) {
            mp.progress = progress;
            return;
        }
    }
    d.mining.push_back(MiningProgress{li, progress, 0.0f});
}

void WorldMap::clear_mining_progress(TileCoord t) {
    const ChunkCoord c = chunk_of(t);
    const i32 li = local_index_of(t);
    ensure_chunk(c).clear_mining_progress(li);
    const auto it = mDeltas.find(chunk_key(c));
    if (it != mDeltas.end()) {
        auto& v = it->second.mining;
        v.erase(std::remove_if(v.begin(), v.end(),
                               [li](const MiningProgress& mp) { return mp.localIndex == li; }),
                v.end());
    }
}

void WorldMap::tick_mining_decay(float dt, TileCoord centre, i32 radius) {
    const ChunkCoord c0 = chunk_of(TileCoord{centre.x - radius, centre.y - radius});
    const ChunkCoord c1 = chunk_of(TileCoord{centre.x + radius, centre.y + radius});
    const float delay = mDb->economy().mineProgressDecayDelay;
    const float rate = mDb->economy().mineProgressDecayRate;
    for (i64 cy = c0.y; cy <= c1.y; ++cy) {
        for (i64 cx = c0.x; cx <= c1.x; ++cx) {
            const ChunkCoord cc{cx, cy};
            if (!chunk_resident(cc)) {
                continue;
            }
            Chunk& ch = chunk(cc);
            auto& list = ch.mining();
            for (usize i = 0; i < list.size();) {
                auto& mp = list[i];
                mp.sinceTouched += dt;
                if (mp.sinceTouched > delay) {
                    mp.progress -= rate * dt;
                }
                if (mp.progress <= 0.0f) {
                    const TileCoord t = tile_of(cc, mp.localIndex);
                    list[i] = list.back();
                    list.pop_back();
                    const auto it = mDeltas.find(chunk_key(cc));
                    if (it != mDeltas.end()) {
                        auto& v = it->second.mining;
                        const i32 li = local_index_of(t);
                        v.erase(std::remove_if(v.begin(), v.end(),
                                               [li](const MiningProgress& m) { return m.localIndex == li; }),
                                v.end());
                    }
                    continue;
                }
                ++i;
            }
        }
    }
}

void WorldMap::reveal(TileCoord t) {
    const ChunkCoord c = chunk_of(t);
    const i32 li = local_index_of(t);
    Chunk& ch = ensure_chunk(c);
    if (has_flag(ch.flags_at(li), TileFlag::Revealed)) {
        return;
    }
    ch.add_flag(li, TileFlag::Revealed);
    ChunkDelta& d = delta_for(c);
    d.revealed[static_cast<usize>(li) / 8] |= static_cast<u8>(1u << (li % 8));
    d.hasRevealed = true;
}

bool WorldMap::revealed(TileCoord t) const {
    return has_flag(ensure_chunk(chunk_of(t)).flags_at(local_index_of(t)), TileFlag::Revealed);
}

void WorldMap::reveal_radius(TileCoord centre, i32 radius, bool scanned) {
    const i64 r2 = static_cast<i64>(radius) * radius;
    for (i64 dy = -radius; dy <= radius; ++dy) {
        for (i64 dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > r2) {
                continue;
            }
            const TileCoord t{centre.x + dx, centre.y + dy};
            reveal(t);
            if (scanned) {
                ensure_chunk(chunk_of(t)).add_flag(local_index_of(t), TileFlag::Scanned);
            }
        }
    }
}

void WorldMap::evict_beyond(ChunkCoord centre, i32 radius) {
    std::vector<u64> toErase;
    for (const auto& [key, ch] : mChunks) {
        const i64 dx = ch->coord().x - centre.x;
        const i64 dy = ch->coord().y - centre.y;
        if (std::llabs(dx) > radius || std::llabs(dy) > radius) {
            toErase.push_back(key);
        }
    }
    for (const u64 key : toErase) {
        mChunks.erase(key);
        ++mStats.chunksEvicted;
    }
    mLru.erase(
        std::remove_if(mLru.begin(), mLru.end(), [this](u64 k) { return mChunks.find(k) == mChunks.end(); }),
        mLru.end());
    mStats.residentChunks = mChunks.size();
}

std::vector<ChunkDelta> WorldMap::collect_deltas() const {
    // Landmark state lives on resident chunks; fold it into the delta store
    // before handing the list to the save system.
    for (const auto& [key, ch] : mChunks) {
        for (const auto& inst : ch->landmarks()) {
            if (!inst.activated && !inst.looted) {
                continue;
            }
            ChunkDelta& d = delta_for(ch->coord());
            bool found = false;
            for (auto& ls : d.landmarks) {
                if (ls.stableId == inst.stableId) {
                    ls.activated = inst.activated;
                    ls.looted = inst.looted;
                    found = true;
                    break;
                }
            }
            if (!found) {
                d.landmarks.push_back(ChunkDelta::LandmarkState{inst.stableId, inst.activated, inst.looted});
            }
        }
    }
    std::vector<ChunkDelta> out;
    out.reserve(mDeltas.size());
    for (const auto& [key, d] : mDeltas) {
        if (!d.empty()) {
            out.push_back(d);
        }
    }
    std::sort(out.begin(), out.end(), [](const ChunkDelta& a, const ChunkDelta& b) {
        return a.coord.y != b.coord.y ? a.coord.y < b.coord.y : a.coord.x < b.coord.x;
    });
    return out;
}

void WorldMap::apply_deltas(const std::vector<ChunkDelta>& deltas) {
    mChunks.clear();
    mLru.clear();
    mDeltas.clear();
    for (const auto& d : deltas) {
        mDeltas[chunk_key(d.coord)] = d;
    }
    mStats.deltaChunks = mDeltas.size();
    mStats.residentChunks = 0;
}

std::vector<LandmarkInstance*> WorldMap::landmarks_near(TileCoord centre, i32 radius) const {
    std::vector<LandmarkInstance*> out;
    const ChunkCoord c0 = chunk_of(TileCoord{centre.x - radius, centre.y - radius});
    const ChunkCoord c1 = chunk_of(TileCoord{centre.x + radius, centre.y + radius});
    for (i64 cy = c0.y; cy <= c1.y; ++cy) {
        for (i64 cx = c0.x; cx <= c1.x; ++cx) {
            Chunk& ch = ensure_chunk(ChunkCoord{cx, cy});
            for (auto& inst : ch.landmarks()) {
                const i64 dx = inst.origin.x + inst.width / 2 - centre.x;
                const i64 dy = inst.origin.y + inst.height / 2 - centre.y;
                if (std::llabs(dx) <= radius && std::llabs(dy) <= radius) {
                    out.push_back(&inst);
                }
            }
        }
    }
    return out;
}

void WorldMap::set_landmark_state(ChunkCoord c, u64 stableId, bool activated, bool looted) {
    Chunk& ch = ensure_chunk(c);
    for (auto& inst : ch.landmarks()) {
        if (inst.stableId == stableId) {
            inst.activated = activated;
            inst.looted = looted;
        }
    }
    ChunkDelta& d = delta_for(c);
    for (auto& ls : d.landmarks) {
        if (ls.stableId == stableId) {
            ls.activated = activated;
            ls.looted = looted;
            return;
        }
    }
    d.landmarks.push_back(ChunkDelta::LandmarkState{stableId, activated, looted});
}

} // namespace df
