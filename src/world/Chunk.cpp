#include "world/Chunk.hpp"

#include "core/Hash.hpp"

namespace df {

u64 Chunk::content_hash() const {
    u64 h = hash_coords(0x5EEDF0F6E1234567ull, mCoord.x, mCoord.y);
    for (usize i = 0; i < static_cast<usize>(kChunkTileCount); ++i) {
        h = hash_combine(h, static_cast<u64>(static_cast<u32>(mMaterial[i])));
        h = hash_combine(h, mVariant[i]);
    }
    for (const auto& l : mLandmarks) {
        h = hash_combine(h, l.stableId);
        h = hash_combine(h, static_cast<u64>(static_cast<u32>(l.definition)));
        h = hash_coords(h, l.origin.x, l.origin.y);
    }
    for (const auto& z : mHazards) {
        h = hash_combine(h, z.stableId);
        h = hash_combine(h, static_cast<u64>(static_cast<u32>(z.definition)));
    }
    return h;
}

} // namespace df
