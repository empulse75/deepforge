#include "TestSupport.hpp"

#include "core/Hash.hpp"
#include "domain/Expedition.hpp"
#include "domain/Loadout.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstring>
#include <vector>

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

TEST_CASE("the climb decision: anchored is cheaper per tile, flying is faster", "[simulation][fuel][climb]") {
    // The return-plan decision (SPEC: drill-anchored ~0.15 fuel/tile vs flying
    // ~0.28/tile at ~6x speed). Measured, not asserted from prose.

    // Flying: hold thrust up an open shaft and measure fuel per tile climbed.
    Harness fly(3407);
    for (i64 y = -80; y <= 40; ++y) {
        fly.map.set_material(TileCoord{0, y}, fly.db.airMaterial.index);
    }
    fly.teleport(0.0f, 40.0f);
    const float flyStart = fly.rig.fuel;
    const float flyStartY = fly.rig.position.y;
    TickCommands up;
    up.thrust = true;
    fly.step(up, 600); // 10 s of climb
    const float flyTiles = flyStartY - fly.rig.position.y;
    REQUIRE(flyTiles > 20.0f);
    const float flyPerTile = (flyStart - fly.rig.fuel) / flyTiles;

    // Anchored: drill through identical rock and measure fuel per tile broken.
    // The rig braces on the tile it cuts, so only the drill draws fuel.
    Harness anchor(3407);
    anchor.teleport(0.0f, 40.0f);
    const float anchorStart = anchor.rig.fuel;
    TickCommands drill;
    drill.mine = MineDirection::Down;
    anchor.step(drill, 600);
    const float anchorTiles = static_cast<float>(anchor.progress.stats.tilesMined);
    REQUIRE(anchorTiles > 1.0f);
    const float anchorPerTile = (anchorStart - anchor.rig.fuel) / anchorTiles;

    INFO("flying " << flyPerTile << " fuel/tile");
    INFO("anchored " << anchorPerTile << " fuel/tile");
    // The decision must be real in both directions: cheaper per tile anchored,
    // much faster flying.
    CHECK(anchorPerTile < flyPerTile);
    // Both runs lasted 10 s, so tiles covered is speed: flying must be several
    // times faster per second even though it costs more per tile.
    CHECK(flyTiles / anchorTiles > 3.0f);
    // And the flying figure must match the documented model (thrust 2.5/s at a
    // ~9 tiles/s climb ceiling): a silent change to either constant moves this.
    CHECK(flyPerTile == Catch::Approx(0.28f).epsilon(0.15f));
}

TEST_CASE("seed 3407: the same run replays bit for bit", "[simulation][determinism][seed3407]") {
    // Host policy seed. One scripted command stream expanded tick by tick, run
    // twice through identical worlds; the final state must hash identical, and
    // ragged batch delivery of the same stream must match too.
    struct Script {
        float moveX;
        bool thrust;
        MineDirection mine;
        int ticks;
    };
    const Script script[] = {
        {0.0f, false, MineDirection::Down, 240},  {0.6f, false, MineDirection::Right, 120},
        {-0.4f, true, MineDirection::None, 180},  {0.0f, true, MineDirection::Up, 300},
        {0.8f, false, MineDirection::Down, 200},  {-0.7f, false, MineDirection::Left, 150},
        {0.2f, true, MineDirection::None, 260},   {0.0f, false, MineDirection::Down, 320},
        {-0.5f, true, MineDirection::Up, 200},    {0.0f, false, MineDirection::None, 120},
    };
    std::vector<TickCommands> stream;
    for (const auto& s : script) {
        for (int i = 0; i < s.ticks; ++i) {
            TickCommands cmd;
            cmd.moveX = s.moveX;
            cmd.thrust = s.thrust;
            cmd.mine = s.mine;
            stream.push_back(cmd);
        }
    }

    auto run = [&stream](const std::vector<int>& batches) {
        Harness h(3407);
        usize next = 0;
        for (int batch : batches) {
            for (int i = 0; i < batch && next < stream.size(); ++i, ++next) {
                h.step(stream[next], 1);
            }
        }
        REQUIRE(next == stream.size());
        const auto bits = [](float f) {
            u32 u = 0;
            std::memcpy(&u, &f, sizeof(u));
            return static_cast<u64>(u);
        };
        u64 hash = 0;
        hash = hash_combine(hash, bits(h.rig.position.x));
        hash = hash_combine(hash, bits(h.rig.position.y));
        hash = hash_combine(hash, bits(h.rig.velocity.x));
        hash = hash_combine(hash, bits(h.rig.velocity.y));
        hash = hash_combine(hash, bits(h.rig.fuel));
        hash = hash_combine(hash, bits(h.rig.hull));
        hash = hash_combine(hash, bits(h.rig.heat));
        hash = hash_combine(hash, static_cast<u64>(h.progress.stats.tilesMined));
        hash = hash_combine(hash, static_cast<u64>(h.cargo.used_units(h.db) * 1000.0f));
        return hash;
    };

    const u64 a = run({2090});
    const u64 b = run({2090});
    CHECK(a == b);
    // Ragged delivery of the identical stream: same hash.
    const u64 c = run({7, 13, 60, 1, 91, 200, 33, 500, 17, 60, 40, 3, 250, 120, 60, 100, 60, 60, 60, 355});
    CHECK(a == c);
}
