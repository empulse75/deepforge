#include "app/GameSession.hpp"

#include "core/Log.hpp"
#include "core/Version.hpp"
#include "domain/Loadout.hpp"

#include <chrono>
#include <random>

namespace df {

std::string_view game_state_name(GameStateId s) {
    switch (s) {
        case GameStateId::Boot:
            return "Boot";
        case GameStateId::MainMenu:
            return "MainMenu";
        case GameStateId::LoadingWorld:
            return "LoadingWorld";
        case GameStateId::SurfaceHub:
            return "SurfaceHub";
        case GameStateId::Expedition:
            return "Expedition";
        case GameStateId::Paused:
            return "Paused";
        case GameStateId::ExpeditionResults:
            return "ExpeditionResults";
        case GameStateId::FailureRecovery:
            return "FailureRecovery";
        case GameStateId::ErrorRecovery:
            return "ErrorRecovery";
    }
    return "?";
}

GameSession::GameSession(ContentDatabase db, std::filesystem::path saveDir)
    : mDb(std::move(db)), mSaves(std::move(saveDir)) {
    mProgress.initialise(mDb);
    mState = GameStateId::MainMenu;
}

void GameSession::reset_world() {
    mWorld = std::make_unique<WorldMap>(mDb, WorldGenerator(mDb, mWorldSeed));
    mSim = std::make_unique<ExpeditionSim>(mDb, *mWorld, mProgress);
    mSim->set_assists(mAssists);
    mEconomy = std::make_unique<Economy>(mDb, mProgress);
}

void GameSession::place_rig_at_surface() {
    const auto& rules = mWorld->generator().rules();
    const i64 surface = mWorld->generator().surface_row(rules.shaftX);
    mRig.position = Vec2{static_cast<float>(rules.shaftX), static_cast<float>(surface - 2)};
    mRig.velocity = Vec2{};
    mRig.mode = RigMode::Surface;
    mPreviousRigPosition = mRig.position;
}

void GameSession::new_game(const NewGameOptions& options) {
    mWorldSeed = options.seed;
    if (options.randomSeed || mWorldSeed == 0) {
        // Only the initial seed uses a nondeterministic source; everything
        // downstream is derived from it.
        std::random_device rd;
        mWorldSeed = (static_cast<u64>(rd()) << 32) ^ static_cast<u64>(rd()) ^
                     static_cast<u64>(std::chrono::steady_clock::now().time_since_epoch().count());
        if (mWorldSeed == 0) {
            mWorldSeed = 0x9E3779B97F4A7C15ull;
        }
    }
    mWorldName = options.worldName;
    mAssists = options.assists;
    mProgress = PlayerProgress{};
    mProgress.initialise(mDb);
    mCargo = Inventory{};
    mSamples = Inventory{};
    mRig = RigState{};
    mPlaySeconds = 0.0;
    mLastSummary = ExpeditionSummary{};
    reset_world();
    refresh();
    place_rig_at_surface();
    mRig.fuel = mSim->stats().fuelCapacity;
    mRig.hull = mSim->stats().hullMax;
    mEconomy->refresh_contract_offers(mWorldSeed);
    mState = GameStateId::SurfaceHub;
    DF_LOG_INFO("session", "new world '{}' seed {}", mWorldName, mWorldSeed);
}

void GameSession::refresh() {
    if (!mSim) {
        return;
    }
    mSim->refresh_stats();
    const auto& stats = mSim->stats();
    mCargo.set_capacity(stats.cargoCapacity);
    mSamples.set_capacity(static_cast<float>(stats.protectedSampleSlots));
}

void GameSession::set_assists(const AssistSettings& a) {
    mAssists = a;
    if (mSim) {
        mSim->set_assists(a);
    }
}

Status GameSession::transition(GameStateId next) {
    // Transitions validate prerequisites and define save boundaries.
    switch (next) {
        case GameStateId::Expedition:
            if (mState != GameStateId::SurfaceHub && mState != GameStateId::Paused) {
                return fail(Error{"can only descend from the surface hub", {}, {}, {}, {}});
            }
            break;
        case GameStateId::SurfaceHub:
            if (mState == GameStateId::Expedition) {
                return fail(Error{"finish or abandon the expedition first", {}, {}, {}, {}});
            }
            break;
        case GameStateId::Paused:
            mStateBeforePause = mState;
            break;
        default:
            break;
    }
    mState = next;
    return {};
}

void GameSession::start_expedition(i32 transitNodeIndex) {
    refresh();
    if (transitNodeIndex >= 0 && static_cast<usize>(transitNodeIndex) < mProgress.transitNodes.size()) {
        const auto& node = mProgress.transitNodes[static_cast<usize>(transitNodeIndex)];
        mRig.position = Vec2{static_cast<float>(node.x), static_cast<float>(node.y)};
        // Nodes are carved rooms, but be defensive: never drop the rig inside
        // rock because a delta changed the room after activation.
        if (mWorld->is_solid(mRig.tile())) {
            EventQueue scratch;
            mSim->perform_unstuck(mRig, scratch);
        }
        DF_LOG_INFO("session", "launching from transit node at {} m", node.y);
    }
    mSim->begin_expedition(mRig, mCargo, mSamples);
    mProgress.stats.expeditions += 1;
    mState = GameStateId::Expedition;
    mPreviousRigPosition = mRig.position;
    DF_LOG_INFO("session", "expedition {} begins", mProgress.stats.expeditions);
}

void GameSession::tick(const TickCommands& cmd) {
    mEvents.clear();
    const float dt = static_cast<float>(kFixedDeltaSeconds);
    mPlaySeconds += kFixedDeltaSeconds;
    mProgress.stats.playSeconds = mPlaySeconds;
    if (mState != GameStateId::Expedition) {
        return;
    }
    mPreviousRigPosition = mRig.position;
    mSim->tick(mRig, mCargo, mSamples, cmd, dt, mEvents);

    if (mSim->runtime().rescuePending) {
        mPendingRescue = mEconomy->preview_rescue(mRig, mCargo, mSim->stats());
        // Autosave before resolving so save scumming cannot corrupt state.
        (void)autosave();
        mState = GameStateId::FailureRecovery;
    } else if (mSim->runtime().returnedToSurface) {
        mState = GameStateId::ExpeditionResults;
    }
}

ExpeditionSummary GameSession::return_to_hub() {
    ExpeditionSummary summary;
    summary.depthReached = mRig.deepestThisRun;
    summary.tilesMined = mSim->runtime().tilesMined;
    summary.seconds = mSim->runtime().elapsedSeconds;

    const Inventory soldSnapshot = mCargo;
    const SaleResult sale = mEconomy->sell_all(mCargo, mSim->stats());
    summary.sold = sale.lines;
    summary.creditsEarned = sale.totalCredits;

    // Anything unsold (strategic and refined stock) is banked for crafting.
    mEconomy->bank_cargo(mCargo);
    // Protected samples go to the lab.
    for (const auto& s : mSamples.stacks()) {
        mProgress.samples.add(mDb, s.material, s.amount, false);
        mEconomy->award_insight(1, mSim->stats());
    }
    mSamples.clear();

    summary.contractsCompleted = mEconomy->settle_contracts(soldSnapshot, summary.depthReached,
                                                            mSim->runtime().outcome, mSim->stats());

    // Proficiency is granted for progress, not for repetition.
    if (summary.depthReached > 0) {
        const i64 milestone = summary.depthReached / 150;
        static thread_local i64 lastMilestone = 0;
        if (milestone > lastMilestone) {
            mEconomy->award_skill_point(static_cast<i32>(milestone - lastMilestone));
            lastMilestone = milestone;
        }
    }
    for (const i32 t : summary.contractsCompleted) {
        (void)t;
        mEconomy->award_skill_point(1);
    }

    mEconomy->service_partial(mRig, mSim->stats());
    mEconomy->refresh_contract_offers(mWorldSeed);
    mRig.mode = RigMode::Surface;
    mRig.deepestThisRun = 0;
    place_rig_at_surface();
    refresh();
    mLastSummary = summary;
    mState = GameStateId::ExpeditionResults;
    (void)autosave();
    return summary;
}

ExpeditionSummary GameSession::resolve_failure() {
    ExpeditionSummary summary = mEconomy->resolve_rescue(mRig, mCargo, mSim->stats());
    summary.tilesMined = mSim->runtime().tilesMined;
    summary.seconds = mSim->runtime().elapsedSeconds;
    // Surviving cargo is still banked; permanent progression is untouched.
    mEconomy->bank_cargo(mCargo);
    for (const auto& s : mSamples.stacks()) {
        mProgress.samples.add(mDb, s.material, s.amount, false);
    }
    mSamples.clear();
    // The same free service floor a normal return gets. Without it a run of bad
    // luck could leave a player with a quarter tank and no credits, which is the
    // debt spiral the design forbids (Section 6.4).
    mEconomy->service_partial(mRig, mSim->stats());
    mEconomy->refresh_contract_offers(mWorldSeed);
    place_rig_at_surface();
    refresh();
    mLastSummary = summary;
    mState = GameStateId::SurfaceHub;
    (void)autosave();
    return summary;
}

RenderSnapshot GameSession::snapshot(float alpha) const {
    RenderSnapshot s;
    s.rigPosition = mPreviousRigPosition + (mRig.position - mPreviousRigPosition) * clamp(alpha, 0.0f, 1.0f);
    s.previousRigPosition = mPreviousRigPosition;
    s.rigVelocity = mRig.velocity;
    s.fuel = mRig.fuel;
    s.hull = mRig.hull;
    s.heat = mRig.heat;
    s.mineProgress = mRig.mineProgress;
    s.mineTarget = mRig.mineTarget;
    s.mining = mRig.activeMine != MineDirection::None;
    s.facing = mRig.facing;
    s.depth = mRig.depth_tiles();
    s.credits = mProgress.credits;
    s.insight = mProgress.insight;
    s.state = mState;
    if (mSim) {
        const auto& st = mSim->stats();
        s.fuelCapacity = st.fuelCapacity;
        s.hullMax = st.hullMax;
        s.heatCapacity = st.heatCapacity;
        s.cargoCapacity = st.cargoCapacity;
        s.cargoUsed = mCargo.used_units(mDb);
        s.scannerRange = st.scannerRange;
    }
    if (mWorld) {
        s.biomeIndex = mWorld->generator().biome_index_at(std::max<i64>(0, s.depth));
    }
    return s;
}

Status GameSession::save_game(int slot) {
    if (!mWorld) {
        return fail(Error{"no world to save", {}, {}, {}, {}});
    }
    SaveData data;
    data.worldSeed = mWorldSeed;
    data.worldName = mWorldName;
    data.progress = mProgress;
    data.rig = mRig;
    data.cargo = mCargo;
    data.samples = mSamples;
    data.worldDeltas = mWorld->collect_deltas();
    data.playSeconds = mPlaySeconds;
    return mSaves.write(mDb, slot, data);
}

Status GameSession::autosave() {
    return save_game(0);
}

Status GameSession::load_game(int slot, bool useBackup) {
    auto data = mSaves.read(mDb, slot, useBackup);
    if (!data) {
        mState = GameStateId::ErrorRecovery;
        return std::unexpected(data.error());
    }
    mWorldSeed = data->worldSeed;
    mWorldName = data->worldName;
    mProgress = std::move(data->progress);
    mRig = data->rig;
    mCargo = std::move(data->cargo);
    mSamples = std::move(data->samples);
    mPlaySeconds = data->playSeconds;
    reset_world();
    mWorld->apply_deltas(data->worldDeltas);
    refresh();
    mEconomy->refresh_contract_offers(mWorldSeed);
    if (mRig.mode == RigMode::Expedition) {
        mSim->begin_expedition(mRig, mCargo, mSamples);
        mState = GameStateId::Expedition;
    } else {
        mRig.mode = RigMode::Surface;
        mState = GameStateId::SurfaceHub;
    }
    DF_LOG_INFO("session", "loaded slot {} (seed {})", slot, mWorldSeed);
    return {};
}

} // namespace df
