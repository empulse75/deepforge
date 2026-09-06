// The authoritative expedition simulation: movement, mining, hazards, heat and
// cargo for one fixed 60 Hz tick.
#pragma once

#include "content/ContentDatabase.hpp"
#include "core/Rng.hpp"
#include "domain/Commands.hpp"
#include "domain/Events.hpp"
#include "domain/Inventory.hpp"
#include "domain/PlayerProgress.hpp"
#include "domain/RigState.hpp"
#include "world/WorldMap.hpp"

namespace df {

/// Difficulty assists. All default to neutral and never shame the player.
struct AssistSettings {
    float fuelDrainScale = 1.0f;
    float damageScale = 1.0f;
    float heatScale = 1.0f;
    float mineTimeScale = 1.0f;
    bool infiniteCargoWarnings = true;
};

/// Result of a mining attempt, used by tests and the balance simulator.
struct MineQuote {
    float seconds = 0.0f;
    float fuelPerTile = 0.0f;
    float fuelPerSecond = 0.0f;
    float hardness = 0.0f;
    i32 material = -1;
    bool mineable = false;
};

struct ExpeditionRuntime {
    float elapsedSeconds = 0.0f;
    i64 startDepth = 0;
    i64 entryDepth = 0; ///< Depth the run launched from; reaching it again extracts.
    i32 biomeIndex = 0;
    i32 lastBiomeIndex = -1;
    bool rescuePending = false;
    bool returnedToSurface = false;
    bool hasDescended = false; ///< Set once the rig is genuinely underground.
    u64 tickCount = 0;
    i64 tilesMined = 0;
    i64 creditsAtStart = 0;
};

/// Pure simulation over a world map and a player state. Holds no rendering,
/// filesystem, wall clock or platform dependency of any kind.
class ExpeditionSim {
public:
    ExpeditionSim(const ContentDatabase& db, WorldMap& map, PlayerProgress& progress);

    void begin_expedition(RigState& rig, Inventory& cargo, Inventory& samples);

    /// Advances one fixed step. `dt` must be kFixedDeltaSeconds.
    void tick(RigState& rig, Inventory& cargo, Inventory& samples, const TickCommands& cmd, float dt,
              EventQueue& events);

    /// Time and fuel to mine one tile with the current stats.
    MineQuote quote(const RigState& rig, TileCoord target) const;

    void refresh_stats();

    const StatBlock& stats() const noexcept { return mStats; }

    const ExpeditionRuntime& runtime() const noexcept { return mRuntime; }

    ExpeditionRuntime& runtime() noexcept { return mRuntime; }

    void set_assists(const AssistSettings& a) noexcept { mAssists = a; }

    const AssistSettings& assists() const noexcept { return mAssists; }

    /// Deterministic unstuck: moves the rig to the nearest valid free position.
    bool perform_unstuck(RigState& rig, EventQueue& events);

    static TileCoord target_tile(const RigState& rig, MineDirection dir);

private:
    void integrate_movement(RigState& rig, const TickCommands& cmd, float dt, EventQueue& events,
                            float cargoLoadFraction, bool anchored);
    /// True while the drill is engaged against solid rock the rig can brace on.
    bool drill_anchored(const RigState& rig, const TickCommands& cmd) const;
    void resolve_collision(RigState& rig, Vec2 previous, EventQueue& events);
    void update_mining(RigState& rig, Inventory& cargo, Inventory& samples, const TickCommands& cmd, float dt,
                       EventQueue& events);
    void break_tile(RigState& rig, Inventory& cargo, Inventory& samples, TileCoord t, EventQueue& events);
    void update_heat(RigState& rig, const TickCommands& cmd, float dt, EventQueue& events);
    void update_hazards(RigState& rig, float dt, EventQueue& events);
    void update_discovery(RigState& rig, const TickCommands& cmd, float dt, EventQueue& events);
    bool solid_at(float x, float y) const;
    bool box_blocked(Rect r) const;
    void damage(RigState& rig, float amount, EventQueue& events, EventKind kind, i32 source);

    const ContentDatabase* mDb;
    WorldMap* mMap;
    PlayerProgress* mProgress;
    StatBlock mStats;
    AssistSettings mAssists;
    ExpeditionRuntime mRuntime;
    Rng mCosmeticRng{0x9E3779B9u};
};

} // namespace df
