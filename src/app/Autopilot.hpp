// A deterministic scripted pilot.
//
// It is not game AI: it exists so the balance simulator, CI smoke tests and the
// headless expedition test can drive a real expedition end to end without a
// window, and so pacing targets can be measured rather than guessed.
#pragma once

#include "app/GameSession.hpp"

namespace df {

struct AutopilotConfig {
    i64 targetDepth = 220;
    float cargoReturnFraction = 0.92f;
    float fuelSafetyFactor = 1.45f;
    bool grabSideOre = true;
    i64 maxTicks = 60 * 60 * 25; ///< 25 minutes of simulated time.
    bool verbose = false;        ///< Log a progress line every 10 simulated seconds.
};

struct AutopilotReport {
    i64 ticks = 0;
    i64 depthReached = 0;
    i64 tilesMined = 0;
    float seconds = 0.0f;
    bool returned = false;
    bool rescued = false;
    bool stalled = false;
    float endFuel = 0.0f;
    float endHull = 0.0f;
};

/// Runs one full expedition: descend, mine, return. The session must be in the
/// SurfaceHub state; on return it is left in ExpeditionResults or SurfaceHub.
AutopilotReport run_autopilot_expedition(GameSession& session, const AutopilotConfig& config);

} // namespace df
