#include "domain/Loadout.hpp"

#include <algorithm>

namespace df {
namespace {

void apply_all(StatBlock& stats, const std::vector<Effect>& effects) {
    for (const auto& e : effects) {
        apply_effect(stats, e);
    }
}

StatBlock compute_internal(const ContentDatabase& db, const PlayerProgress& p, i32 overrideTrack,
                           i32 overrideTier, i32 addModule, i32 removeModule, i32 addSkill) {
    StatBlock stats;
    stats.fuelCapacity = db.economy().baseFuelCapacity;
    stats.cargoCapacity = db.economy().baseCargoCapacity;
    stats.hullMax = db.economy().baseHull;
    stats.heatCapacity = db.economy().baseHeatCapacity;
    stats.drillPower = db.economy().baseDrillPower;

    // Equipment tiers: every tier up to and including the owned one applies.
    for (usize t = 0; t < db.equipment().size(); ++t) {
        i32 tier = t < p.equipmentTier.size() ? p.equipmentTier[t] : 0;
        if (overrideTrack >= 0 && static_cast<i32>(t) == overrideTrack) {
            tier = overrideTier;
        }
        const auto& track = db.equipment()[t];
        for (const auto& tierDef : track.tiers) {
            if (tierDef.tier <= tier) {
                apply_all(stats, tierDef.effects);
            }
        }
    }

    // Installed modules, with their tradeoffs.
    for (const i32 m : p.installedModules) {
        if (m == removeModule) {
            continue;
        }
        if (m < 0 || static_cast<usize>(m) >= db.modules().size()) {
            continue;
        }
        apply_all(stats, db.modules()[static_cast<usize>(m)].effects);
        apply_all(stats, db.modules()[static_cast<usize>(m)].tradeoffs);
    }
    if (addModule >= 0 && static_cast<usize>(addModule) < db.modules().size() &&
        !p.module_installed(addModule)) {
        apply_all(stats, db.modules()[static_cast<usize>(addModule)].effects);
        apply_all(stats, db.modules()[static_cast<usize>(addModule)].tradeoffs);
    }

    // Skills.
    for (const i32 s : p.unlockedSkills) {
        if (s >= 0 && static_cast<usize>(s) < db.skills().size()) {
            apply_all(stats, db.skills()[static_cast<usize>(s)].effects);
        }
    }
    if (addSkill >= 0 && static_cast<usize>(addSkill) < db.skills().size() &&
        p.unlockedSkills.count(addSkill) == 0) {
        apply_all(stats, db.skills()[static_cast<usize>(addSkill)].effects);
    }

    // Research effects.
    for (const i32 r : p.completedResearch) {
        if (r >= 0 && static_cast<usize>(r) < db.research().size()) {
            apply_all(stats, db.research()[static_cast<usize>(r)].effects);
        }
    }

    // Facility tiers.
    for (usize f = 0; f < db.facilities().size(); ++f) {
        const i32 tier = f < p.facilityTier.size() ? p.facilityTier[f] : 0;
        for (const auto& t : db.facilities()[f].tiers) {
            if (t.tier <= tier) {
                apply_all(stats, t.effects);
            }
        }
    }

    stats.clamp_to_caps();
    return stats;
}

} // namespace

StatBlock compute_stats(const ContentDatabase& db, const PlayerProgress& p) {
    return compute_internal(db, p, -1, 0, -1, -1, -1);
}

StatBlock preview_with_equipment(const ContentDatabase& db, const PlayerProgress& p, i32 trackIndex,
                                 i32 tier) {
    return compute_internal(db, p, trackIndex, tier, -1, -1, -1);
}

StatBlock preview_with_module(const ContentDatabase& db, const PlayerProgress& p, i32 moduleIndex,
                              bool install) {
    return install ? compute_internal(db, p, -1, 0, moduleIndex, -1, -1)
                   : compute_internal(db, p, -1, 0, -1, moduleIndex, -1);
}

StatBlock preview_with_skill(const ContentDatabase& db, const PlayerProgress& p, i32 skillIndex) {
    return compute_internal(db, p, -1, 0, -1, -1, skillIndex);
}

} // namespace df
