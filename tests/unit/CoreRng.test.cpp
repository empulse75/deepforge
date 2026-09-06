#include "core/Hash.hpp"
#include "core/Noise.hpp"
#include "core/Rng.hpp"
#include "core/Types.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <vector>

using namespace df;

TEST_CASE("splitmix64 produces known vectors", "[core][rng]") {
    // Locked-in reference values; a change here means every existing world seed
    // would generate different terrain.
    CHECK(splitmix64(0) == 0xE220A8397B1DCDAFull);
    CHECK(splitmix64(1) == 0x910A2DEC89025CC1ull);
    CHECK(splitmix64(0xDEADBEEFull) == splitmix64(0xDEADBEEFull));
}

TEST_CASE("rng is reproducible from a seed", "[core][rng]") {
    Rng a{12345};
    Rng b{12345};
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(a.next_u64() == b.next_u64());
    }
}

TEST_CASE("rng streams are independent", "[core][rng]") {
    Rng ore = make_rng(777, RngStream::Ores, 3, 4);
    Rng cosmetic = make_rng(777, RngStream::Cosmetic, 3, 4);
    bool differs = false;
    for (int i = 0; i < 16; ++i) {
        if (ore.next_u64() != cosmetic.next_u64()) {
            differs = true;
        }
    }
    CHECK(differs);

    // Draining the cosmetic stream must not perturb the ore stream.
    Rng oreAgain = make_rng(777, RngStream::Ores, 3, 4);
    Rng cosmetic2 = make_rng(777, RngStream::Cosmetic, 3, 4);
    for (int i = 0; i < 100; ++i) {
        (void)cosmetic2.next_u64();
    }
    Rng oreControl = make_rng(777, RngStream::Ores, 3, 4);
    for (int i = 0; i < 16; ++i) {
        REQUIRE(oreAgain.next_u64() == oreControl.next_u64());
    }
}

TEST_CASE("below() is unbiased over its range", "[core][rng]") {
    Rng rng{42};
    std::vector<int> counts(7, 0);
    constexpr int kSamples = 70000;
    for (int i = 0; i < kSamples; ++i) {
        const u32 v = rng.below(7);
        REQUIRE(v < 7);
        ++counts[v];
    }
    for (int c : counts) {
        // Expect 10000 each; allow a wide but meaningful band.
        CHECK(c > 9000);
        CHECK(c < 11000);
    }
}

TEST_CASE("unit_float stays in range", "[core][rng]") {
    Rng rng{9001};
    for (int i = 0; i < 20000; ++i) {
        const float f = rng.unit_float();
        REQUIRE(f >= 0.0f);
        REQUIRE(f < 1.0f);
    }
}

TEST_CASE("rng state round trips", "[core][rng]") {
    Rng a{555};
    for (int i = 0; i < 10; ++i) {
        (void)a.next_u64();
    }
    u64 saved[4];
    for (int i = 0; i < 4; ++i) {
        saved[i] = a.state()[i];
    }
    Rng b;
    b.set_state(saved);
    for (int i = 0; i < 50; ++i) {
        REQUIRE(a.next_u64() == b.next_u64());
    }
}

TEST_CASE("noise is integer-deterministic and bounded", "[core][noise]") {
    for (i64 x = -40; x < 40; ++x) {
        for (i64 y = -40; y < 40; ++y) {
            const i32 n = fbm_2d(1234, x, y, 32, 4);
            REQUIRE(n >= -kNoiseOne - 2);
            REQUIRE(n <= kNoiseOne + 2);
            REQUIRE(n == fbm_2d(1234, x, y, 32, 4));
        }
    }
}

TEST_CASE("noise varies across space", "[core][noise]") {
    std::set<i32> seen;
    for (i64 x = 0; x < 200; ++x) {
        seen.insert(value_noise_2d(99, x, 17, 16));
    }
    CHECK(seen.size() > 100);
}

TEST_CASE("chunk coordinate maths handle negatives", "[core][types]") {
    CHECK(floor_div(-1, 32) == -1);
    CHECK(floor_mod(-1, 32) == 31);
    CHECK(chunk_of(TileCoord{-1, -1}) == ChunkCoord{-1, -1});
    CHECK(chunk_of(TileCoord{0, 0}) == ChunkCoord{0, 0});
    CHECK(chunk_of(TileCoord{31, 32}) == ChunkCoord{0, 1});
    CHECK(local_index_of(TileCoord{-1, -1}) == 31 * 32 + 31);
    for (i64 x = -70; x < 70; ++x) {
        for (i64 y = -70; y < 70; ++y) {
            const TileCoord t{x, y};
            REQUIRE(tile_of(chunk_of(t), local_index_of(t)) == t);
        }
    }
}
