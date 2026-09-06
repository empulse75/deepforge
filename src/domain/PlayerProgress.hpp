// All persistent player progression. This is the authoritative save payload
// alongside the world deltas.
#pragma once

#include "content/ContentDatabase.hpp"
#include "domain/Inventory.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace df {

/// One accepted contract instance generated from a template.
struct ActiveContract {
    i32 templateIndex = -1;
    i32 targetMaterial = -1;
    i32 targetAmount = 0;
    i32 targetDepth = 0;
    i32 progress = 0;
    bool completed = false;
    bool claimed = false;
    u64 instanceId = 0;
    /// For no_module_class contracts: the module slot that must stay empty.
    i32 forbiddenSlot = -1;
};

struct CodexEntry {
    i32 material = -1;
    i64 minedCount = 0;
    i64 firstDepth = 0;
};

/// Statistics shown on the results screen and in the codex.
struct PlayStats {
    i64 tilesMined = 0;
    i64 creditsEarned = 0;
    i64 creditsSpent = 0;
    i64 expeditions = 0;
    i64 rescues = 0;
    i64 deepestDepth = 0;
    double playSeconds = 0.0;
};

/// Everything the player permanently owns.
struct PlayerProgress {
    i64 credits = 0;
    i64 insight = 0;

    /// Equipment tier per track, indexed like ContentDatabase::equipment().
    std::vector<i32> equipmentTier;

    /// Modules owned and the ones currently installed, by module index.
    std::unordered_set<i32> ownedModules;
    std::vector<i32> installedModules;

    /// Skill points spent per node index, plus the pool.
    std::unordered_set<i32> unlockedSkills;
    i32 skillPointsEarned = 0;
    i32 skillPointsSpent = 0;

    std::unordered_set<i32> completedResearch;
    /// Samples delivered to the lab, awaiting or already used by research.
    Inventory samples;

    /// Facility tier per facility index.
    std::vector<i32> facilityTier;

    /// Reputation per faction index.
    std::vector<i32> reputation;

    std::vector<ActiveContract> contracts;
    std::vector<ActiveContract> completedContracts;

    std::unordered_map<i32, CodexEntry> codex;
    std::unordered_set<i64> discoveredLandmarks;
    std::unordered_set<i32> unlockedBiomes;
    std::unordered_set<u64> activatedTransitNodes;

    /// Surface storage: materials banked but not yet sold or refined.
    Inventory vault;

    PlayStats stats;
    bool freeRescueUsed = false;
    bool endingReached = false;
    i32 newGamePlusLevel = 0;
    i32 maxUtilityPower = 6;

    /// Loadout presets, each a list of module indices.
    std::vector<std::vector<i32>> loadoutPresets;

    void initialise(const ContentDatabase& db);

    i32 skill_points_available() const noexcept { return skillPointsEarned - skillPointsSpent; }

    bool has_research(i32 index) const { return completedResearch.count(index) > 0; }

    bool module_installed(i32 index) const;

    i32 used_utility_power(const ContentDatabase& db) const;
};

} // namespace df
