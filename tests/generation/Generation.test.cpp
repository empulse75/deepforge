#include "TestSupport.hpp"

#include "world/WorldMap.hpp"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>

using namespace df;

TEST_CASE("the same seed generates identical chunks", "[generation][determinism]") {
    const auto& db = dft::shipped_content();
    WorldGenerator a(db, 987654321ull);
    WorldGenerator b(db, 987654321ull);
    Chunk ca;
    Chunk cb;
    const ChunkCoord coords[] = {{0, 0}, {0, 5}, {-3, 12}, {7, 40}, {-9, 91}, {2, 130}};
    for (const auto& c : coords) {
        a.generate(c, ca);
        b.generate(c, cb);
        REQUIRE(ca.content_hash() == cb.content_hash());
    }
}

TEST_CASE("chunk hashes are stable regardless of generation order", "[generation][determinism]") {
    const auto& db = dft::shipped_content();
    WorldGenerator gen(db, 4242);
    std::map<u64, u64> forward;
    Chunk chunk;
    for (i64 y = 0; y < 6; ++y) {
        for (i64 x = -3; x <= 3; ++x) {
            gen.generate(ChunkCoord{x, y}, chunk);
            forward[chunk_key(ChunkCoord{x, y})] = chunk.content_hash();
        }
    }
    // Reverse order must produce the same result: no cross-chunk state leaks.
    for (i64 y = 5; y >= 0; --y) {
        for (i64 x = 3; x >= -3; --x) {
            gen.generate(ChunkCoord{x, y}, chunk);
            REQUIRE(forward[chunk_key(ChunkCoord{x, y})] == chunk.content_hash());
        }
    }
}

TEST_CASE("a vein looks the same from either side of a chunk border", "[generation][determinism]") {
    const auto& db = dft::shipped_content();
    WorldMap map(db, WorldGenerator(db, 55555));
    // Sample a strip crossing chunk borders, evict everything, sample again.
    std::vector<i32> first;
    for (i64 x = -80; x < 80; ++x) {
        first.push_back(map.material_index(TileCoord{x, 200}));
    }
    map.evict_beyond(ChunkCoord{1000, 1000}, 0);
    for (i64 x = -80; x < 80; ++x) {
        REQUIRE(map.material_index(TileCoord{x, 200}) == first[static_cast<usize>(x + 80)]);
    }
}

TEST_CASE("world boundaries are indestructible and enclosing", "[generation][safety]") {
    const auto& db = dft::shipped_content();
    WorldGenerator gen(db, 31337);
    WorldMap map(db, gen);
    const auto& rules = gen.rules();
    for (i64 y = 0; y < 400; y += 37) {
        REQUIRE_FALSE(map.is_mineable(TileCoord{rules.halfWidth + 1, y}));
        REQUIRE_FALSE(map.is_mineable(TileCoord{-rules.halfWidth - 1, y}));
    }
    REQUIRE_FALSE(map.is_mineable(TileCoord{0, rules.bottomDepth + 1}));
}

TEST_CASE("the entry shaft is open on every seed", "[generation][safety]") {
    const auto& db = dft::shipped_content();
    for (u64 seed = 1; seed <= 60; ++seed) {
        WorldGenerator gen(db, seed * 7919ull);
        WorldMap map(db, gen);
        const auto& rules = gen.rules();
        for (i64 y = gen.surface_row(rules.shaftX); y <= rules.shaftDepth; ++y) {
            INFO("seed " << seed << " depth " << y);
            REQUIRE_FALSE(map.is_solid(TileCoord{rules.shaftX, y}));
        }
    }
}

TEST_CASE("no lethal hazard hides in the tutorial layer", "[generation][safety]") {
    const auto& db = dft::shipped_content();
    for (u64 seed = 1; seed <= 40; ++seed) {
        WorldGenerator gen(db, seed * 104729ull);
        WorldMap map(db, gen);
        const auto& rules = gen.rules();
        const ChunkCoord c0 = chunk_of(TileCoord{-rules.tutorialSafeHalfWidth, 0});
        const ChunkCoord c1 = chunk_of(TileCoord{rules.tutorialSafeHalfWidth, rules.tutorialSafeDepth});
        for (i64 cy = c0.y; cy <= c1.y; ++cy) {
            for (i64 cx = c0.x; cx <= c1.x; ++cx) {
                const Chunk& chunk = map.chunk(ChunkCoord{cx, cy});
                for (const auto& hz : chunk.hazards()) {
                    INFO("seed " << seed << " hazard at " << hz.tile.x << "," << hz.tile.y);
                    REQUIRE_FALSE(gen.in_tutorial_zone(hz.tile));
                }
            }
        }
    }
}

TEST_CASE("generation sweep over 1000 seeds", "[generation][sweep][.slow]") {
    const auto& db = dft::shipped_content();
    const auto ferrite = db.material("material.ferrite");
    const auto cuprite = db.material("material.cuprite");
    REQUIRE(ferrite.valid());
    REQUIRE(cuprite.valid());

    int seedsWithoutStarterOre = 0;
    int seedsWithBadTile = 0;
    int seedsTooDense = 0;
    int seedsTooEmpty = 0;

    for (u64 seed = 1; seed <= 1000; ++seed) {
        WorldGenerator gen(db, seed * 2654435761ull);
        WorldMap map(db, gen);
        const auto& rules = gen.rules();

        int starterOre = 0;
        int solid = 0;
        int air = 0;
        bool badTile = false;
        // The band a first expedition can actually reach.
        for (i64 y = 4; y < 120; ++y) {
            for (i64 x = -40; x <= 40; ++x) {
                const i32 mi = map.material_index(TileCoord{x, y});
                if (mi < 0 || static_cast<usize>(mi) >= db.materials().size()) {
                    badTile = true;
                    continue;
                }
                const auto& m = db.materials()[static_cast<usize>(mi)];
                if (m.category == MaterialCategory::Empty) {
                    ++air;
                } else {
                    ++solid;
                }
                if (mi == ferrite.index || mi == cuprite.index) {
                    ++starterOre;
                }
            }
        }
        if (badTile) {
            ++seedsWithBadTile;
        }
        // Enough ore to fill a starting hold at least twice.
        if (starterOre < 60) {
            ++seedsWithoutStarterOre;
        }
        const double airFraction = static_cast<double>(air) / static_cast<double>(air + solid);
        if (airFraction > 0.75) {
            ++seedsTooEmpty;
        }
        if (airFraction < 0.005) {
            ++seedsTooDense;
        }
        // The shaft must always be open.
        REQUIRE_FALSE(map.is_solid(TileCoord{rules.shaftX, 10}));
    }

    INFO("seeds without starter ore: " << seedsWithoutStarterOre);
    INFO("seeds with invalid tiles: " << seedsWithBadTile);
    INFO("seeds too empty: " << seedsTooEmpty << ", too dense: " << seedsTooDense);
    CHECK(seedsWithBadTile == 0);
    CHECK(seedsWithoutStarterOre == 0);
    CHECK(seedsTooEmpty == 0);
    CHECK(seedsTooDense == 0);
}

TEST_CASE("every biome and mandatory material is reachable", "[generation][progression]") {
    const auto& db = dft::shipped_content();
    for (u64 seed : {1ull, 42ull, 20260905ull, 999983ull}) {
        WorldGenerator gen(db, seed);
        WorldMap map(db, gen);
        std::set<i32> found;
        // Sample each biome band; the pity pass guarantees at least one vein of
        // every guaranteed material per region.
        for (const auto& biome : db.biomes()) {
            const i64 lo = biome.minDepth + 10;
            const i64 hi = std::min<i64>(biome.maxDepth - 10, lo + 500);
            for (i64 y = lo; y < hi; y += 1) {
                for (i64 x = -220; x <= 220; x += 1) {
                    found.insert(map.material_index(TileCoord{x, y}));
                }
            }
            map.evict_beyond(ChunkCoord{0, chunk_of(TileCoord{0, hi}).y}, 6);
        }
        for (const auto& m : db.materials()) {
            if (m.rarityWeightBp <= 0) {
                continue;
            }
            const auto h = db.material(m.id.text());
            INFO("seed " << seed << " material " << m.id.text());
            CHECK(found.count(h.index) > 0);
        }
    }
}
