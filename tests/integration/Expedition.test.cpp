#include "TestSupport.hpp"

#include "app/Autopilot.hpp"
#include "app/GameSession.hpp"
#include "domain/Loadout.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace df;

TEST_CASE("a new game reaches the first purchase headlessly", "[integration][loop]") {
    auto session = dft::make_session("integration_first_purchase", 20260905);
    REQUIRE(session.state() == GameStateId::SurfaceHub);
    const i64 startingCredits = session.progress().credits;

    // The cheapest first upgrade must not already be affordable at spawn: the
    // player has to earn it.
    i64 cheapest = 1 << 30;
    for (usize t = 0; t < session.content().equipment().size(); ++t) {
        const auto& track = session.content().equipment()[t];
        if (track.tiers.size() > 1) {
            cheapest = std::min<i64>(cheapest, track.tiers[1].costCredits);
        }
    }
    CHECK(startingCredits < cheapest);

    AutopilotConfig config;
    config.targetDepth = 140;
    double minutes = 0.0;
    bool purchased = false;
    for (int run = 0; run < 6 && !purchased; ++run) {
        const auto report = run_autopilot_expedition(session, config);
        INFO("run " << run << " depth " << report.depthReached << " credits " << session.progress().credits);
        CHECK_FALSE(report.stalled);
        minutes += static_cast<double>(report.seconds) / 60.0;
        for (usize t = 0; t < session.content().equipment().size(); ++t) {
            if (session.economy().can_buy_equipment(static_cast<i32>(t))) {
                REQUIRE(session.economy().buy_equipment(static_cast<i32>(t)).has_value());
                purchased = true;
                break;
            }
        }
    }
    CHECK(purchased);
    // Pacing target: the first meaningful upgrade lands inside the first few
    // short runs, not after a grind (Section 10.4).
    CHECK(minutes < 12.0);
    CHECK(session.progress().stats.tilesMined > 50);
}

TEST_CASE("save, quit, reload keeps the world the player changed", "[integration][persistence]") {
    auto session = dft::make_session("integration_persistence", 31415);
    AutopilotConfig config;
    config.targetDepth = 90;
    const auto report = run_autopilot_expedition(session, config);
    REQUIRE(report.tilesMined > 20);

    // Record the tunnel the run carved.
    std::vector<TileCoord> mined;
    for (i64 y = 6; y < report.depthReached; ++y) {
        for (i64 x = -12; x <= 12; ++x) {
            const TileCoord t{x, y};
            if (!session.world().is_solid(t)) {
                mined.push_back(t);
            }
        }
    }
    REQUIRE(mined.size() > 20);
    const i64 credits = session.progress().credits;
    const i64 deepest = session.progress().stats.deepestDepth;
    const i64 tiles = session.progress().stats.tilesMined;

    REQUIRE(session.save_game(1).has_value());

    // A brand new session loading that slot must see the same world.
    GameSession reloaded(dft::shipped_content(), session.saves().directory());
    REQUIRE(reloaded.load_game(1).has_value());
    CHECK(reloaded.world_seed() == session.world_seed());
    CHECK(reloaded.progress().credits == credits);
    CHECK(reloaded.progress().stats.deepestDepth == deepest);
    CHECK(reloaded.progress().stats.tilesMined == tiles);
    int stillOpen = 0;
    for (const auto& t : mined) {
        if (!reloaded.world().is_solid(t)) {
            ++stillOpen;
        }
    }
    CHECK(stillOpen == static_cast<int>(mined.size()));
}

TEST_CASE("failure preserves permanent progression", "[integration][failure]") {
    auto session = dft::make_session("integration_failure", 8675309);
    // Buy something permanent first.
    session.progress().credits = 5000;
    const auto track = session.content().equipment_track("equipment.fuel_tank");
    REQUIRE(session.economy().buy_equipment(track.index).has_value());
    const i32 tier = session.progress().equipmentTier[static_cast<usize>(track.index)];
    session.refresh();

    // Strand the rig deep with no fuel and no hull.
    session.start_expedition();
    session.rig().position = Vec2{0.0f, 300.0f};
    session.rig().hull = 0.5f;
    for (int i = 0; i < 200 && session.state() == GameStateId::Expedition; ++i) {
        TickCommands cmd;
        cmd.mine = MineDirection::Down;
        session.tick(cmd);
    }
    session.rig().hull = 0.0f;
    session.tick(TickCommands{});
    REQUIRE(session.state() == GameStateId::FailureRecovery);

    const auto preview = session.pending_rescue();
    const i64 creditsBefore = session.progress().credits;
    session.resolve_failure();

    CHECK(session.state() == GameStateId::SurfaceHub);
    // Everything permanent survives a failure.
    CHECK(session.progress().equipmentTier[static_cast<usize>(track.index)] == tier);
    CHECK(session.progress().stats.rescues == 1);
    // Credits pay the capped fee and then whatever service the player can
    // afford, but never go negative.
    CHECK(session.progress().credits >= 0);
    CHECK(session.progress().credits <= creditsBefore - preview.fee);
    // The rig comes back usable.
    CHECK(session.rig().hull >= session.sim().stats().hullMax * 0.2f);
    CHECK(session.rig().fuel >= session.sim().stats().fuelCapacity * 0.3f);
}

TEST_CASE("repeated failures cannot strand a player with nothing", "[integration][failure]") {
    auto session = dft::make_session("integration_no_spiral", 5551212);
    session.progress().credits = 0;
    session.progress().freeRescueUsed = true;
    for (int i = 0; i < 8; ++i) {
        session.start_expedition();
        session.rig().position = Vec2{0.0f, 250.0f};
        session.tick(TickCommands{});
        session.rig().hull = 0.0f;
        session.tick(TickCommands{});
        if (session.state() == GameStateId::FailureRecovery) {
            session.resolve_failure();
        }
        INFO("iteration " << i);
        CHECK(session.progress().credits >= 0);
        // The free service floor guarantees a usable rig every time.
        CHECK(session.rig().fuel >= session.sim().stats().fuelCapacity * 0.3f);
        CHECK(session.rig().hull >= session.sim().stats().hullMax * 0.2f);
    }
}

TEST_CASE("game state transitions validate their prerequisites", "[integration][state]") {
    auto session = dft::make_session("integration_states", 4242);
    CHECK(session.state() == GameStateId::SurfaceHub);
    // Cannot go straight back to the hub from an expedition without ending it.
    session.start_expedition();
    CHECK(session.state() == GameStateId::Expedition);
    CHECK_FALSE(session.transition(GameStateId::SurfaceHub).has_value());
    const auto summary = session.return_to_hub();
    CHECK(summary.depthReached >= 0);
    CHECK(session.state() == GameStateId::ExpeditionResults);
}

TEST_CASE("an autosave is written at every save boundary", "[integration][save]") {
    auto session = dft::make_session("integration_autosave", 90210);
    const auto slot = session.saves().slot_path(0);
    std::error_code ec;
    std::filesystem::remove(slot, ec);

    AutopilotConfig config;
    config.targetDepth = 60;
    (void)run_autopilot_expedition(session, config);
    CHECK(std::filesystem::exists(slot));

    auto info = session.saves().inspect(dft::shipped_content(), 0);
    CHECK(info.exists);
    CHECK_FALSE(info.damaged);
    CHECK(info.schemaVersion > 0);
}

TEST_CASE("twenty simulated minutes run without a crash or soft lock", "[integration][smoke]") {
    auto session = dft::make_session("integration_smoke", 777777);
    double minutes = 0.0;
    int expeditions = 0;
    while (minutes < 20.0 && expeditions < 25) {
        AutopilotConfig config;
        config.targetDepth = 80 + 40 * expeditions;
        const auto report = run_autopilot_expedition(session, config);
        INFO("expedition " << expeditions << " depth " << report.depthReached);
        CHECK_FALSE(report.stalled);
        minutes += static_cast<double>(report.seconds) / 60.0;
        ++expeditions;
        // Spend anything affordable so progression actually advances.
        for (usize t = 0; t < session.content().equipment().size(); ++t) {
            (void)session.economy().buy_equipment(static_cast<i32>(t));
        }
        session.refresh();
        CHECK(session.progress().credits >= 0);
        CHECK(session.state() != GameStateId::ErrorRecovery);
    }
    CHECK(expeditions > 1);
    CHECK(session.progress().stats.deepestDepth > 100);
}

TEST_CASE("slice 1: the whole loop chains end to end in one flow", "[integration][loop][slice1]") {
    // One save, one continuous progression: descend and dig (autopilot),
    // manage the pressures (fuel/cargo force the return), extract and sell,
    // buy an upgrade with the proceeds, then spend insight plus a delivered
    // sample on a research node. Every stage's output feeds the next.
    auto session = dft::make_session("integration_slice1_loop", 3407);
    REQUIRE(session.state() == GameStateId::SurfaceHub);

    // Descend, dig, manage pressures, extract: a full autopilot expedition.
    AutopilotConfig config;
    config.targetDepth = 140;
    const auto report = run_autopilot_expedition(session, config);
    CHECK(report.returned);
    CHECK_FALSE(report.stalled);
    CHECK(report.tilesMined > 20);
    // Selling happened on return (the session sells what the hold carried).
    CHECK(session.progress().credits > 0);

    // Buy an upgrade with the proceeds: the fuel tank tier 1 the pacing gate
    // promises is reachable from a clean save within a few such runs.
    const auto tank = session.content().equipment_track("equipment.fuel_tank");
    REQUIRE(tank.valid());
    for (int run = 0; run < 6 && !session.economy().can_buy_equipment(tank.index); ++run) {
        const auto r = run_autopilot_expedition(session, config);
        CHECK_FALSE(r.stalled);
    }
    REQUIRE(session.economy().buy_equipment(tank.index).has_value());
    CHECK(session.progress().equipmentTier[static_cast<usize>(tank.index)] == 1);

    // Advance one research node on the other curve: insight plus a delivered
    // sample, neither of which credits can buy.
    const auto& db = session.content();
    const auto node = db.research_node("research.field_metallurgy");
    REQUIRE(node.valid());
    session.economy().award_insight(10, compute_stats(db, session.progress()));
    const auto sample = db.material("material.echo_shard");
    session.progress().samples.add(db, sample.index, 1);
    REQUIRE(session.economy().complete_research(node.index).has_value());
    CHECK(session.progress().completedResearch.count(node.index) == 1);

    // The changed rig descends again: the loop closes on itself.
    const auto second = run_autopilot_expedition(session, config);
    CHECK_FALSE(second.stalled);
    CHECK(second.tilesMined > 0);
}
