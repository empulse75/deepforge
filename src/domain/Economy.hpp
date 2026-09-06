// Surface-side transactions: selling, repair, refuelling, upgrades, modules,
// skills, research, facilities, refining, contracts and rescue resolution.
//
// Every operation validates its prerequisites and either applies completely or
// changes nothing.
#pragma once

#include "content/ContentDatabase.hpp"
#include "core/Result.hpp"
#include "domain/Expedition.hpp"
#include "domain/PlayerProgress.hpp"
#include "domain/RigState.hpp"

namespace df {

struct SaleLine {
    i32 material = -1;
    i32 amount = 0;
    i64 credits = 0;
};

struct SaleResult {
    std::vector<SaleLine> lines;
    i64 totalCredits = 0;
};

/// Preview of what a failure will cost, shown before the player confirms.
struct RescuePreview {
    i64 fee = 0;
    i32 cargoUnitsLost = 0;
    i64 cargoValueLost = 0;
    bool wreckCreated = false;
    bool freeRescue = false;
};

struct ExpeditionSummary {
    i64 creditsEarned = 0;
    i64 depthReached = 0;
    i64 tilesMined = 0;
    float seconds = 0.0f;
    std::vector<SaleLine> sold;
    std::vector<i32> contractsCompleted;
    bool rescued = false;
};

class Economy {
public:
    Economy(const ContentDatabase& db, PlayerProgress& progress) : mDb(&db), mProgress(&progress) {}

    // --- selling and services ---------------------------------------------
    i64 unit_price(i32 material, i64 depthMined, const StatBlock& stats) const;
    SaleResult preview_sale(const Inventory& cargo, const StatBlock& stats) const;
    SaleResult sell_all(Inventory& cargo, const StatBlock& stats);

    i64 repair_cost(const RigState& rig, const StatBlock& stats) const;
    i64 refuel_cost(const RigState& rig, const StatBlock& stats) const;
    Status repair(RigState& rig, const StatBlock& stats);
    Status refuel(RigState& rig, const StatBlock& stats);
    /// Repairs and refuels as far as the player can afford, never into debt.
    void service_partial(RigState& rig, const StatBlock& stats);

    // --- progression -------------------------------------------------------
    bool can_buy_equipment(i32 trackIndex, std::string* reason = nullptr) const;
    Status buy_equipment(i32 trackIndex);
    i64 equipment_cost(i32 trackIndex, i32 tier) const;

    Status buy_module(i32 moduleIndex);
    Status install_module(i32 moduleIndex);
    Status uninstall_module(i32 moduleIndex);
    bool module_unlocked(i32 moduleIndex) const;

    Status unlock_skill(i32 skillIndex);
    Status respec_skills();
    i64 respec_cost() const;

    Status complete_research(i32 researchIndex);
    bool research_available(i32 researchIndex, std::string* reason = nullptr) const;

    Status upgrade_facility(i32 facilityIndex);
    i64 facility_cost(i32 facilityIndex, i32 tier) const;

    Status run_recipe(i32 recipeIndex, i32 times, const StatBlock& stats);
    bool recipe_available(i32 recipeIndex, std::string* reason = nullptr) const;

    /// Moves cargo into the surface vault (used on return).
    void bank_cargo(Inventory& cargo);

    // --- contracts ---------------------------------------------------------
    /// Regenerates the offered contract list from valid templates only.
    void refresh_contract_offers(u64 worldSeed, i32 count = 5);

    const std::vector<ActiveContract>& offers() const noexcept { return mOffers; }

    Status accept_contract(usize offerIndex);
    Status abandon_contract(u64 instanceId);
    /// Applies delivery/depth progress after an expedition and claims rewards.
    /// Applies expedition results to active contracts and claims what completed.
    std::vector<i32> settle_contracts(const Inventory& soldCargo, i64 depthReached,
                                      const ExpeditionOutcome& outcome, const StatBlock& stats);

    bool contract_eligible(const ContractTemplate& t) const;

    // --- failure -----------------------------------------------------------
    RescuePreview preview_rescue(const RigState& rig, const Inventory& cargo, const StatBlock& stats) const;
    ExpeditionSummary resolve_rescue(RigState& rig, Inventory& cargo, const StatBlock& stats);

    void award_insight(i64 amount, const StatBlock& stats);
    void award_skill_point(i32 amount = 1);
    void add_reputation(i32 factionIndex, i32 amount);

private:
    const ContentDatabase* mDb;
    PlayerProgress* mProgress;
    std::vector<ActiveContract> mOffers;
    u64 mNextInstanceId = 1;
};

} // namespace df
