#include "TestSupport.hpp"

#include "domain/Expedition.hpp"
#include "domain/Loadout.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace df;

namespace {

/// Builds a headless simulation over a scratch world.
struct Harness {
    explicit Harness(u64 seed = 1234) : db(dft::shipped_content()), map(db, WorldGenerator(db, seed)) {
        progress.initialise(db);
        sim = std::make_unique<ExpeditionSim>(db, map, progress);
        rig.position = Vec2{0.0f, -2.0f};
        cargo.set_capacity(sim->stats().cargoCapacity);
        samples.set_capacity(4.0f);
        rig.fuel = sim->stats().fuelCapacity;
        rig.hull = sim->stats().hullMax;
        sim->begin_expedition(rig, cargo, samples);
    }

    /// Moves the rig and clears the tile it occupies, so collision resolution
    /// does not relocate it before the test runs.
    void teleport(float x, float y) {
        rig.position = Vec2{x, y};
        rig.velocity = Vec2{};
        const TileCoord t = rig.tile();
        for (i64 dy = -1; dy <= 0; ++dy) {
            for (i64 dx = -1; dx <= 1; ++dx) {
                map.set_material(TileCoord{t.x + dx, t.y + dy}, db.airMaterial.index);
            }
        }
    }

    void step(const TickCommands& cmd, int ticks = 1) {
        for (int i = 0; i < ticks; ++i) {
            sim->tick(rig, cargo, samples, cmd, static_cast<float>(kFixedDeltaSeconds), events);
        }
    }

    const ContentDatabase& db;
    WorldMap map;
    PlayerProgress progress;
    std::unique_ptr<ExpeditionSim> sim;
    RigState rig;
    Inventory cargo;
    Inventory samples;
    EventQueue events;
};

} // namespace

TEST_CASE("mine time follows the design formula and its clamps", "[simulation][mining]") {
    Harness h;
    const auto& econ = h.db.economy();
    // Place a known material under the rig and quote it.
    const auto ferrite = h.db.material("material.ferrite");
    const TileCoord target{0, 10};
    h.map.set_material(target, ferrite.index);

    const MineQuote q = h.sim->quote(h.rig, target);
    REQUIRE(q.mineable);
    const float expected = clamp(
        econ.baseMineSeconds * std::pow(q.hardness / h.sim->stats().drillPower, econ.mineHardnessExponent),
        econ.minMineSeconds, econ.maxMineSeconds);
    CHECK_THAT(q.seconds, Catch::Matchers::WithinRel(expected, 0.02f));
    CHECK(q.fuelPerTile > 0.0f);

    // Harder rock takes longer; a stronger drill takes less.
    const auto basalt = h.db.material("material.basalt");
    h.map.set_material(TileCoord{1, 10}, basalt.index);
    const MineQuote hard = h.sim->quote(h.rig, TileCoord{1, 10});
    CHECK(hard.seconds > q.seconds);

    h.progress.equipmentTier[static_cast<usize>(h.db.equipment_track("equipment.drill_power").index)] = 4;
    h.sim->refresh_stats();
    const MineQuote upgraded = h.sim->quote(h.rig, TileCoord{1, 10});
    CHECK(upgraded.seconds < hard.seconds);
}

TEST_CASE("unmineable tiles are never breakable", "[simulation][mining]") {
    Harness h;
    const auto bedrock = h.db.boundaryMaterial;
    h.map.set_material(TileCoord{0, 1}, bedrock.index);
    h.rig.position = Vec2{0.0f, 0.0f};
    TickCommands cmd;
    cmd.mine = MineDirection::Down;
    h.step(cmd, 600);
    CHECK(h.map.material_index(TileCoord{0, 1}) == bedrock.index);
}

TEST_CASE("mining a tile yields cargo and clears the tile", "[simulation][mining]") {
    Harness h;
    const auto ferrite = h.db.material("material.ferrite");
    h.teleport(0.0f, 9.0f);
    h.map.set_material(TileCoord{0, 10}, ferrite.index);
    TickCommands cmd;
    cmd.mine = MineDirection::Down;
    h.step(cmd, 400);
    CHECK(h.map.material_index(TileCoord{0, 10}) == h.db.airMaterial.index);
    CHECK(h.cargo.count(ferrite.index) > 0);
    CHECK(h.progress.stats.tilesMined > 0);
}

TEST_CASE("interrupted mining keeps its progress", "[simulation][mining]") {
    Harness h;
    const auto basalt = h.db.material("material.basalt");
    h.teleport(0.0f, 9.0f);
    h.map.set_material(TileCoord{0, 10}, basalt.index);

    TickCommands mine;
    mine.mine = MineDirection::Down;
    h.step(mine, 10);
    const float partial = h.map.mining_progress(TileCoord{0, 10});
    CHECK(partial > 0.0f);
    CHECK(partial < 1.0f);

    // Stop briefly; the stored progress must survive the interruption.
    h.step(TickCommands{}, 5);
    CHECK(h.map.mining_progress(TileCoord{0, 10}) == partial);

    // After the configured delay it decays rather than persisting forever.
    h.step(TickCommands{}, static_cast<int>((h.db.economy().mineProgressDecayDelay + 1.0f) * 60.0f));
    CHECK(h.map.mining_progress(TileCoord{0, 10}) < partial);
}

TEST_CASE("collision never traps the rig inside terrain", "[simulation][collision]") {
    Harness h;
    // Bury the rig deliberately, then let the simulation resolve it.
    for (i64 dy = -2; dy <= 2; ++dy) {
        for (i64 dx = -2; dx <= 2; ++dx) {
            h.map.set_material(TileCoord{dx, 40 + dy}, h.db.material("material.loam").index);
        }
    }
    h.rig.position = Vec2{0.0f, 40.0f};
    TickCommands cmd;
    cmd.unstuck = true;
    h.step(cmd, 2);
    // The rig must end up somewhere it can legally exist.
    const TileCoord t = h.rig.tile();
    CHECK_FALSE(h.map.is_solid(t));
}

TEST_CASE("the rig lands on solid ground and does not sink", "[simulation][collision]") {
    Harness h;
    h.teleport(0.0f, -6.0f);
    h.step(TickCommands{}, 300);
    CHECK(h.rig.onGround);
    CHECK_FALSE(h.map.is_solid(h.rig.tile()));
    const float restingY = h.rig.position.y;
    h.step(TickCommands{}, 120);
    CHECK_THAT(h.rig.position.y, Catch::Matchers::WithinAbs(restingY, 0.05f));
}

TEST_CASE("collision holds at chunk borders", "[simulation][collision]") {
    Harness h;
    // Walk along a solid floor that crosses a chunk boundary.
    for (i64 x = 28; x < 40; ++x) {
        h.map.set_material(TileCoord{x, 51}, h.db.material("material.loam").index);
        h.map.set_material(TileCoord{x, 50}, h.db.airMaterial.index);
    }
    h.teleport(28.0f, 50.0f);
    TickCommands cmd;
    cmd.moveX = 1.0f;
    for (int i = 0; i < 400; ++i) {
        h.step(cmd, 1);
        CHECK_FALSE(h.map.is_solid(h.rig.tile()));
    }
}

TEST_CASE("thrust burns fuel and drilling does not pay hover cost", "[simulation][fuel]") {
    Harness h;
    h.teleport(0.0f, 30.0f);
    const float start = h.rig.fuel;
    TickCommands thrust;
    thrust.thrust = true;
    h.step(thrust, 60);
    const float afterThrust = h.rig.fuel;
    CHECK(afterThrust < start);

    Harness h2;
    h2.teleport(0.0f, 30.0f);
    h2.map.set_material(TileCoord{0, 29}, h2.db.material("material.loam").index);
    TickCommands drill;
    drill.mine = MineDirection::Up;
    drill.thrust = true; // held, but the anchor must make it free
    const float before = h2.rig.fuel;
    h2.step(drill, 5);
    CHECK(h2.rig.anchored);
    const float drillCost = before - h2.rig.fuel;
    CHECK(drillCost < (start - afterThrust));
}

TEST_CASE("heat rises with depth and cooling holds it down", "[simulation][heat]") {
    Harness hot(777);
    hot.sim->begin_expedition(hot.rig, hot.cargo, hot.samples);
    hot.teleport(0.0f, 1400.0f); // Ember Mantle
    hot.step(TickCommands{}, 240);
    const float withoutCooling = hot.rig.heat;

    Harness cooled(777);
    cooled.progress.equipmentTier[static_cast<usize>(cooled.db.equipment_track("equipment.cooling").index)] =
        5;
    cooled.sim->refresh_stats();
    cooled.sim->begin_expedition(cooled.rig, cooled.cargo, cooled.samples);
    cooled.teleport(0.0f, 1400.0f);
    cooled.step(TickCommands{}, 240);
    CHECK(cooled.rig.heat <= withoutCooling);
}

TEST_CASE("falling far enough damages the hull", "[simulation][damage]") {
    Harness h;
    // Carve a clear shaft, drop the rig down it and check the landing.
    for (i64 y = 60; y < 110; ++y) {
        for (i64 x = -1; x <= 1; ++x) {
            h.map.set_material(TileCoord{x, y}, h.db.airMaterial.index);
        }
    }
    for (i64 x = -1; x <= 1; ++x) {
        h.map.set_material(TileCoord{x, 110}, h.db.material("material.granite_spur").index);
    }
    h.teleport(0.0f, 61.0f);
    const float hull = h.rig.hull;
    h.step(TickCommands{}, 240);
    CHECK(h.rig.hull < hull);
}

TEST_CASE("cargo mass slows acceleration but never stops the rig", "[simulation][cargo]") {
    // Both rigs run down an identical open corridor so the only difference is
    // the load they carry.
    auto corridor = [](Harness& h) {
        h.teleport(0.0f, 30.0f);
        for (i64 x = -4; x < 60; ++x) {
            h.map.set_material(TileCoord{x, 30}, h.db.airMaterial.index);
            h.map.set_material(TileCoord{x, 31}, h.db.material("material.granite_spur").index);
        }
    };

    Harness light;
    corridor(light);
    TickCommands move;
    move.moveX = 1.0f;
    light.step(move, 20);
    const float lightSpeed = std::fabs(light.rig.velocity.x);

    Harness heavy;
    corridor(heavy);
    heavy.cargo.add(heavy.db, heavy.db.material("material.ferrite").index,
                    static_cast<i32>(heavy.sim->stats().cargoCapacity));
    heavy.step(move, 20);
    const float heavySpeed = std::fabs(heavy.rig.velocity.x);
    INFO("light " << lightSpeed << " heavy " << heavySpeed);
    CHECK(heavySpeed < lightSpeed);
    // A full hold slows the rig; it must never immobilise it.
    CHECK(heavySpeed > lightSpeed * 0.5f);
}

TEST_CASE("the simulation is identical regardless of render frame timing", "[simulation][determinism]") {
    // Same command stream, same number of fixed steps: the result must match
    // bit for bit no matter how the frames were grouped.
    auto run = [](const std::vector<int>& batches) {
        Harness h(24680);
        h.rig.position = Vec2{0.0f, 5.0f};
        TickCommands cmd;
        cmd.mine = MineDirection::Down;
        cmd.moveX = 0.3f;
        int done = 0;
        for (int batch : batches) {
            for (int i = 0; i < batch; ++i) {
                h.step(cmd, 1);
                ++done;
            }
        }
        return std::tuple{
            h.rig.position.x, h.rig.position.y, h.rig.fuel, h.rig.hull, h.progress.stats.tilesMined, done};
    };
    const auto a = run({600});
    const auto b = run({1, 3, 17, 200, 60, 60, 59, 100, 100});
    CHECK(std::get<5>(a) == std::get<5>(b));
    CHECK(std::get<0>(a) == std::get<0>(b));
    CHECK(std::get<1>(a) == std::get<1>(b));
    CHECK(std::get<2>(a) == std::get<2>(b));
    CHECK(std::get<3>(a) == std::get<3>(b));
    CHECK(std::get<4>(a) == std::get<4>(b));
}

TEST_CASE("mined tiles survive chunk eviction and reload", "[simulation][persistence]") {
    Harness h;
    h.teleport(0.0f, 9.0f);
    TickCommands cmd;
    cmd.mine = MineDirection::Down;
    h.step(cmd, 1200);
    const i64 depth = h.rig.tile().y;
    REQUIRE(depth > 12);

    std::vector<TileCoord> mined;
    for (i64 y = 10; y < depth; ++y) {
        if (!h.map.is_solid(TileCoord{0, y})) {
            mined.push_back(TileCoord{0, y});
        }
    }
    REQUIRE_FALSE(mined.empty());

    // Evict everything, then confirm the tunnel is still there.
    h.map.evict_beyond(ChunkCoord{9999, 9999}, 0);
    for (const auto& t : mined) {
        INFO("tile " << t.x << "," << t.y);
        CHECK_FALSE(h.map.is_solid(t));
    }
}

TEST_CASE("volatile material punishes careless extraction", "[simulation][volatile]") {
    Harness h;
    const auto fulmenite = h.db.material("material.fulmenite");
    REQUIRE(fulmenite.valid());
    h.sim->begin_expedition(h.rig, h.cargo, h.samples);
    h.teleport(0.0f, 1200.0f);
    h.map.set_material(TileCoord{0, 1201}, fulmenite.index);
    const float hull = h.rig.hull;
    TickCommands cmd;
    cmd.mine = MineDirection::Down;
    h.step(cmd, 600);
    CHECK(h.rig.hull < hull);
}
