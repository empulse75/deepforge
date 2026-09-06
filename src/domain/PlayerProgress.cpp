#include "domain/PlayerProgress.hpp"

#include <algorithm>

namespace df {

void PlayerProgress::initialise(const ContentDatabase& db) {
    equipmentTier.assign(db.equipment().size(), 0);
    facilityTier.assign(db.facilities().size(), 0);
    reputation.assign(db.factions().size(), 0);
    credits = db.economy().startingCredits;
    insight = 0;
    installedModules.clear();
    ownedModules.clear();
    unlockedBiomes.clear();
    if (!db.biomes().empty()) {
        unlockedBiomes.insert(0);
    }
    // The first biome after the surface is always open; the rest are gated.
    if (db.biomes().size() > 1) {
        unlockedBiomes.insert(1);
    }
    loadoutPresets.assign(3, {});
    samples.set_capacity(1e9f);
    vault.set_capacity(1e9f);
}

bool PlayerProgress::module_installed(i32 index) const {
    return std::find(installedModules.begin(), installedModules.end(), index) != installedModules.end();
}

void PlayerProgress::add_transit_node(u64 id, i64 x, i64 y) {
    activatedTransitNodes.insert(id);
    for (const auto& n : transitNodes) {
        if (n.stableId == id) {
            return;
        }
    }
    transitNodes.push_back(TransitNode{id, x, y});
    std::sort(transitNodes.begin(), transitNodes.end(),
              [](const TransitNode& a, const TransitNode& b) { return a.y < b.y; });
}

i32 PlayerProgress::used_utility_power(const ContentDatabase& db) const {
    i32 total = 0;
    for (const i32 m : installedModules) {
        if (m >= 0 && static_cast<usize>(m) < db.modules().size()) {
            total += db.modules()[static_cast<usize>(m)].powerCost;
        }
    }
    return total;
}

} // namespace df
