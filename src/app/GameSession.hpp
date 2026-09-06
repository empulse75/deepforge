// Application-level orchestration: owns long-lived services, drives the fixed
// step loop, and defines the save boundaries between game states.
#pragma once

#include "app/SaveFile.hpp"
#include "content/ContentDatabase.hpp"
#include "domain/Economy.hpp"
#include "domain/Expedition.hpp"
#include "world/WorldMap.hpp"

#include <memory>
#include <optional>
#include <string>

namespace df {

enum class GameStateId : u8 {
    Boot,
    MainMenu,
    LoadingWorld,
    SurfaceHub,
    Expedition,
    Paused,
    ExpeditionResults,
    FailureRecovery,
    ErrorRecovery,
};

std::string_view game_state_name(GameStateId s);

/// Presentation-facing snapshot produced at the end of each tick. Rendering
/// never reads authoritative simulation state directly.
struct RenderSnapshot {
    Vec2 rigPosition{};
    Vec2 rigVelocity{};
    Vec2 previousRigPosition{};
    float fuel = 0.0f;
    float fuelCapacity = 1.0f;
    float hull = 0.0f;
    float hullMax = 1.0f;
    float heat = 0.0f;
    float heatCapacity = 1.0f;
    float cargoUsed = 0.0f;
    float cargoCapacity = 1.0f;
    float mineProgress = 0.0f;
    TileCoord mineTarget{};
    bool mining = false;
    MineDirection facing = MineDirection::Down;
    i64 depth = 0;
    i32 biomeIndex = 0;
    i64 credits = 0;
    i64 insight = 0;
    GameStateId state = GameStateId::Boot;
    float scannerRange = 0.0f;
};

/// Options for creating a world.
struct NewGameOptions {
    u64 seed = 0;
    bool randomSeed = true;
    std::string worldName = "Deepforge";
    AssistSettings assists{};
};

class GameSession {
public:
    GameSession(ContentDatabase db, std::filesystem::path saveDir);

    // --- lifecycle ---------------------------------------------------------
    void new_game(const NewGameOptions& options);
    Status load_game(int slot, bool useBackup = false);
    Status save_game(int slot);
    Status autosave();

    // --- state -------------------------------------------------------------
    GameStateId state() const noexcept { return mState; }

    Status transition(GameStateId next);

    /// Launches an expedition. `transitNodeIndex` selects an activated transit
    /// node from PlayerProgress::transitNodes; -1 launches from the surface.
    void start_expedition(i32 transitNodeIndex = -1);
    /// Ends the run: banks cargo, sells, settles contracts, autosaves.
    ExpeditionSummary return_to_hub();
    ExpeditionSummary resolve_failure();

    /// Advances one fixed simulation step.
    void tick(const TickCommands& cmd);

    /// Interpolation-ready snapshot for the renderer.
    RenderSnapshot snapshot(float alpha) const;

    // --- accessors ---------------------------------------------------------
    const ContentDatabase& content() const noexcept { return mDb; }

    WorldMap& world() { return *mWorld; }

    const WorldMap& world() const { return *mWorld; }

    PlayerProgress& progress() noexcept { return mProgress; }

    const PlayerProgress& progress() const noexcept { return mProgress; }

    RigState& rig() noexcept { return mRig; }

    const RigState& rig() const noexcept { return mRig; }

    Inventory& cargo() noexcept { return mCargo; }

    const Inventory& cargo() const noexcept { return mCargo; }

    Inventory& samples() noexcept { return mSamples; }

    Economy& economy() noexcept { return *mEconomy; }

    ExpeditionSim& sim() { return *mSim; }

    const ExpeditionSim& sim() const { return *mSim; }

    SaveSystem& saves() noexcept { return mSaves; }

    EventQueue& events() noexcept { return mEvents; }

    u64 world_seed() const noexcept { return mWorldSeed; }

    double play_seconds() const noexcept { return mPlaySeconds; }

    const ExpeditionSummary& last_summary() const noexcept { return mLastSummary; }

    const RescuePreview& pending_rescue() const noexcept { return mPendingRescue; }

    bool rescue_pending() const noexcept { return mSim && mSim->runtime().rescuePending; }

    void set_assists(const AssistSettings& a);

    /// Recomputes derived stats after any loadout or progression change.
    void refresh();

private:
    void reset_world();
    void place_rig_at_surface();

    ContentDatabase mDb;
    SaveSystem mSaves;
    std::unique_ptr<WorldMap> mWorld;
    std::unique_ptr<ExpeditionSim> mSim;
    std::unique_ptr<Economy> mEconomy;
    PlayerProgress mProgress;
    RigState mRig;
    Inventory mCargo;
    Inventory mSamples;
    EventQueue mEvents;
    GameStateId mState = GameStateId::Boot;
    GameStateId mStateBeforePause = GameStateId::SurfaceHub;
    u64 mWorldSeed = 0;
    std::string mWorldName;
    double mPlaySeconds = 0.0;
    Vec2 mPreviousRigPosition{};
    ExpeditionSummary mLastSummary;
    RescuePreview mPendingRescue;
    AssistSettings mAssists;
};

} // namespace df
