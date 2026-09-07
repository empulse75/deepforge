// Aggregates equipment, modules, skills, research and facilities into one
// StatBlock. Recomputed only when the loadout changes, never per tick.
#pragma once

#include "content/ContentDatabase.hpp"
#include "domain/PlayerProgress.hpp"

namespace df {

/// Computes the effective rig statistics for the current progression state.
StatBlock compute_stats(const ContentDatabase& db, const PlayerProgress& progress);

/// Module power budget the rig actually has: the hull's own allowance plus
/// whatever the power core track and any other source contributes.
i32 max_utility_power(const ContentDatabase& db, const PlayerProgress& progress);

/// Stats the player would have with one hypothetical change, for before/after
/// previews in the loadout and shop screens.
StatBlock preview_with_equipment(const ContentDatabase& db, const PlayerProgress& progress, i32 trackIndex,
                                 i32 tier);

StatBlock preview_with_module(const ContentDatabase& db, const PlayerProgress& progress, i32 moduleIndex,
                              bool install);

StatBlock preview_with_skill(const ContentDatabase& db, const PlayerProgress& progress, i32 skillIndex);

} // namespace df
