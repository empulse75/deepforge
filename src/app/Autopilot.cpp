#include "app/Autopilot.hpp"

#include "core/Log.hpp"

#include <algorithm>
#include <cmath>

namespace df {
namespace {

bool worth_mining(const WorldMap& map, TileCoord t) {
    if (!map.is_mineable(t)) {
        return false;
    }
    const auto& m = map.material(t);
    return m.baseValue > 0 || m.protectedSample || m.category == MaterialCategory::Strategic;
}

float fuel_per_tile_up(const ContentDatabase& db, const StatBlock& stats) {
    const float riseSpeed = std::max(1.0f, 9.0f * stats.thrust);
    return db.economy().thrustFuelPerSecond * stats.hoverFuel / riseSpeed;
}

/// Cost of climbing one tile by drilling: no thrust, only the drill draw.
float fuel_per_tile_drill_up(float cutCost) {
    return cutCost;
}

} // namespace

AutopilotReport run_autopilot_expedition(GameSession& session, const AutopilotConfig& config) {
    AutopilotReport report;
    const ContentDatabase& db = session.content();
    session.start_expedition();

    const i64 startDepth = session.rig().depth_tiles();
    bool returning = false;
    bool haveTarget = false;
    i64 targetTicks = 0;
    TileCoord targetTile{};
    i64 stuckTicks = 0;
    Vec2 lastPos = session.rig().position;

    for (i64 tick = 0; tick < config.maxTicks; ++tick) {
        if (session.state() != GameStateId::Expedition) {
            break;
        }
        const RigState& rig = session.rig();
        const StatBlock& stats = session.sim().stats();
        const TileCoord here = rig.tile();

        const float cargoFraction =
            stats.cargoCapacity > 0.0f ? session.cargo().used_units(db) / stats.cargoCapacity : 0.0f;
        // Budget for the climb plus the tiles that must be cut on the way out,
        // priced from a real mining quote rather than a guess.
        const MineQuote upQuote = session.sim().quote(rig, TileCoord{here.x, here.y - 1});
        const float cutCost = upQuote.mineable ? upQuote.fuelPerTile : 0.0f;
        // Climbing a cut shaft costs the drill draw; open shafts cost thrust.
        // Budget for the more expensive of the two so the estimate is safe.
        const float perTile = std::max(fuel_per_tile_up(db, stats), fuel_per_tile_drill_up(cutCost));
        const float returnFuel = static_cast<float>(std::max<i64>(0, here.y - startDepth)) * perTile;

        if (!returning) {
            if (cargoFraction >= config.cargoReturnFraction || here.y >= config.targetDepth ||
                rig.fuel <= returnFuel * config.fuelSafetyFactor + 4.0f || rig.hull <= stats.hullMax * 0.3f) {
                returning = true;
                DF_LOG_DEBUG("autopilot", "returning from depth {} (fuel {:.1f}, need {:.1f})", here.y,
                             rig.fuel, returnFuel);
            }
        }

        TickCommands cmd;
        if (!returning && config.grabSideOre) {
            // Model a player who prospects: pick the best nearby valuable tile
            // and tunnel toward it instead of only taking what is adjacent.
            if (haveTarget && ++targetTicks > 60 * 6) {
                haveTarget = false; // give up on an unreachable detour
            }
            if (!haveTarget || !worth_mining(session.world(), targetTile)) {
                haveTarget = false;
                targetTicks = 0;
                float bestScore = 0.0f;
                // Short diversions only: a player detours for a nearby vein but
                // keeps descending rather than sweeping the whole layer.
                // Only detour to tiles at or below the current row: chasing a
                // tile overhead makes the pilot oscillate rather than descend.
                for (i32 dy = 0; dy <= 4; ++dy) {
                    for (i32 dx = -3; dx <= 3; ++dx) {
                        const TileCoord t{here.x + dx, here.y + dy};
                        if (!worth_mining(session.world(), t)) {
                            continue;
                        }
                        const auto& m = session.world().material(t);
                        const float dist = static_cast<float>(std::abs(dx) + std::abs(dy)) + 1.0f;
                        const float score = static_cast<float>(std::max(1, m.baseValue)) / dist;
                        if (score > bestScore) {
                            bestScore = score;
                            targetTile = t;
                            haveTarget = true;
                        }
                    }
                }
            }
        }
        if (returning) {
            // Never burn thrust against a ceiling: rest on the ledge, cut the
            // tile out, then climb. This is also how a player plays it.
            const TileCoord above{here.x, here.y - 1};
            const bool blocked = session.world().is_solid(above);
            cmd.mine = blocked ? MineDirection::Up : MineDirection::None;
            cmd.thrust = !blocked;
            if (here.y <= startDepth) {
                break;
            }
        } else if (haveTarget) {
            const i64 dx = targetTile.x - here.x;
            const i64 dy = targetTile.y - here.y;
            if (dx != 0 && dy <= 0) {
                cmd.mine = dx > 0 ? MineDirection::Right : MineDirection::Left;
                cmd.moveX = dx > 0 ? 0.7f : -0.7f;
            } else if (dy > 0) {
                cmd.mine = MineDirection::Down;
            } else {
                haveTarget = false;
            }
            if (rig.velocity.y > 8.0f) {
                cmd.thrust = true;
            }
        } else {
            if (session.world().is_solid(TileCoord{here.x, here.y + 1})) {
                cmd.mine = MineDirection::Down;
            }
            // Slow the descent so falls do not cause impact damage.
            if (rig.velocity.y > 8.0f) {
                cmd.thrust = true;
            }
        }

        session.tick(cmd);

        const Vec2 pos = session.rig().position;
        const bool makingProgress = session.rig().mineProgress > 0.0f || session.rig().anchored;
        if (std::fabs(pos.x - lastPos.x) < 0.001f && std::fabs(pos.y - lastPos.y) < 0.001f &&
            !makingProgress) {
            ++stuckTicks;
            haveTarget = false;
            if (stuckTicks > 240) {
                report.stalled = true;
                break;
            }
        } else {
            stuckTicks = 0;
        }
        lastPos = pos;
        report.ticks = tick + 1;
        if (config.verbose && tick % 600 == 0) {
            DF_LOG_INFO("autopilot",
                        "t={} pos=({:.2f},{:.2f}) fuel={:.1f} cargo={:.1f} ret={} tgt={} "
                        "tgtTile=({},{}) mine={} prog={:.2f} anchor={} ground={}",
                        tick, session.rig().position.x, session.rig().position.y, session.rig().fuel,
                        session.cargo().used_units(db), returning, haveTarget, targetTile.x, targetTile.y,
                        static_cast<int>(cmd.mine), session.rig().mineProgress, session.rig().anchored,
                        session.rig().onGround);
        }
    }

    report.depthReached = session.rig().deepestThisRun;
    report.tilesMined = session.sim().runtime().tilesMined;
    report.seconds = session.sim().runtime().elapsedSeconds;
    report.endFuel = session.rig().fuel;
    report.endHull = session.rig().hull;

    if (session.state() == GameStateId::FailureRecovery || session.rescue_pending()) {
        session.resolve_failure();
        report.rescued = true;
    } else {
        session.return_to_hub();
        report.returned = true;
    }
    return report;
}

} // namespace df
