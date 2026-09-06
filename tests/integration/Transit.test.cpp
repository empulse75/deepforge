#include "TestSupport.hpp"

#include "app/Autopilot.hpp"
#include "app/GameSession.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace df;

namespace {

/// Finds a transit node in the world and marks it activated, the way reaching
/// and using one in play would.
bool activate_a_transit_node(GameSession& session, i64 searchDepth) {
    const auto& db = session.content();
    for (i64 y = 300; y < searchDepth; y += 32) {
        for (i64 x = -400; x <= 400; x += 32) {
            for (auto* lm : session.world().landmarks_near(TileCoord{x, y}, 40)) {
                const auto& def = db.landmarks()[static_cast<usize>(lm->definition)];
                if (def.kind != LandmarkKind::TransitNode) {
                    continue;
                }
                lm->activated = true;
                session.world().set_landmark_state(chunk_of(lm->origin), lm->stableId, true, lm->looted);
                session.progress().add_transit_node(lm->stableId, lm->origin.x + lm->width / 2,
                                                    lm->origin.y + lm->height / 2);
                return true;
            }
        }
    }
    return false;
}

} // namespace

TEST_CASE("transit nodes exist and become permanent launch points", "[integration][transit]") {
    auto session = dft::make_session("integration_transit", 20260906);
    REQUIRE(activate_a_transit_node(session, 1600));
    REQUIRE_FALSE(session.progress().transitNodes.empty());

    const auto node = session.progress().transitNodes.front();
    CHECK(node.y > 250);

    // Launching from the node starts the run at that depth, not at the surface.
    session.start_expedition(0);
    CHECK(session.state() == GameStateId::Expedition);
    CHECK(session.rig().depth_tiles() >= node.y - 8);
    CHECK(session.rig().depth_tiles() <= node.y + 8);
    // And the rig is never dropped inside rock.
    CHECK_FALSE(session.world().is_solid(session.rig().tile()));
}

TEST_CASE("an expedition launched from a node extracts through it", "[integration][transit]") {
    auto session = dft::make_session("integration_transit_return", 20260906);
    REQUIRE(activate_a_transit_node(session, 1600));
    const auto node = session.progress().transitNodes.front();

    session.start_expedition(0);
    const i64 entry = session.rig().depth_tiles();

    // Dig down a little, then climb back to the entry depth. The turn is
    // latched: without it the pilot would oscillate around the turn depth.
    bool returning = false;
    for (int i = 0; i < 8000 && session.state() == GameStateId::Expedition; ++i) {
        const auto here = session.rig().tile();
        if (here.y >= entry + 20) {
            returning = true;
        }
        TickCommands cmd;
        if (!returning) {
            if (session.world().is_solid(TileCoord{here.x, here.y + 1})) {
                cmd.mine = MineDirection::Down;
            }
        } else {
            const bool blocked = session.world().is_solid(TileCoord{here.x, here.y - 1});
            cmd.mine = blocked ? MineDirection::Up : MineDirection::None;
            cmd.thrust = !blocked;
        }
        session.tick(cmd);
    }

    // Reaching the entry depth again ends the run, without a climb to daylight.
    INFO("entry " << entry << " node " << node.y << " state "
                  << std::string(game_state_name(session.state())));
    CHECK(session.state() != GameStateId::Expedition);
    CHECK(session.progress().stats.deepestDepth > entry);
}

TEST_CASE("transit nodes survive a save round trip", "[integration][transit][save]") {
    auto session = dft::make_session("integration_transit_save", 20260906);
    REQUIRE(activate_a_transit_node(session, 1600));
    const auto before = session.progress().transitNodes;
    REQUIRE(session.save_game(2).has_value());

    GameSession reloaded(dft::shipped_content(), session.saves().directory());
    REQUIRE(reloaded.load_game(2).has_value());
    REQUIRE(reloaded.progress().transitNodes.size() == before.size());
    for (usize i = 0; i < before.size(); ++i) {
        CHECK(reloaded.progress().transitNodes[i].stableId == before[i].stableId);
        CHECK(reloaded.progress().transitNodes[i].x == before[i].x);
        CHECK(reloaded.progress().transitNodes[i].y == before[i].y);
    }
    CHECK(reloaded.progress().activatedTransitNodes == session.progress().activatedTransitNodes);
}
