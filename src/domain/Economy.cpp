#include "domain/Economy.hpp"

#include "core/Log.hpp"
#include "domain/Loadout.hpp"

#include <algorithm>
#include <cmath>

namespace df {
namespace {

Error transaction_error(std::string message, std::string suggestion = {}) {
    return Error{std::move(message), {}, {}, {}, std::move(suggestion)};
}

} // namespace

// ---------------------------------------------------------------------------
// Selling and services
// ---------------------------------------------------------------------------

i64 Economy::unit_price(i32 material, i64 depthMined, const StatBlock& stats) const {
    if (material < 0 || static_cast<usize>(material) >= mDb->materials().size()) {
        return 0;
    }
    const auto& m = mDb->materials()[static_cast<usize>(material)];
    const float depthBonus =
        1.0f + mDb->economy().sellPriceDepthBonusPer100m * static_cast<float>(depthMined) / 100.0f;
    const float price = static_cast<float>(m.baseValue) * stats.saleValue * clamp(depthBonus, 1.0f, 2.0f);
    return static_cast<i64>(std::llround(price));
}

SaleResult Economy::preview_sale(const Inventory& cargo, const StatBlock& stats) const {
    SaleResult result;
    for (const auto& s : cargo.stacks()) {
        if (s.material < 0 || static_cast<usize>(s.material) >= mDb->materials().size()) {
            continue;
        }
        const auto& m = mDb->materials()[static_cast<usize>(s.material)];
        if (m.baseValue <= 0 || m.protectedSample) {
            continue;
        }
        const i64 credits = unit_price(s.material, 0, stats) * s.amount;
        result.lines.push_back(SaleLine{s.material, s.amount, credits});
        result.totalCredits += credits;
    }
    return result;
}

SaleResult Economy::sell_all(Inventory& cargo, const StatBlock& stats) {
    SaleResult result = preview_sale(cargo, stats);
    for (const auto& line : result.lines) {
        cargo.remove(line.material, line.amount);
    }
    mProgress->credits += result.totalCredits;
    mProgress->stats.creditsEarned += result.totalCredits;
    return result;
}

void Economy::bank_cargo(Inventory& cargo) {
    for (const auto& s : cargo.stacks()) {
        mProgress->vault.add(*mDb, s.material, s.amount, false);
    }
    cargo.clear();
}

i64 Economy::repair_cost(const RigState& rig, const StatBlock& stats) const {
    const float missing = std::max(0.0f, stats.hullMax - rig.hull);
    return static_cast<i64>(std::llround(missing * mDb->economy().repairCreditsPerHull * stats.repairCost));
}

i64 Economy::refuel_cost(const RigState& rig, const StatBlock& stats) const {
    const float missing = std::max(0.0f, stats.fuelCapacity - rig.fuel);
    return static_cast<i64>(std::llround(missing * mDb->economy().refuelCreditsPerUnit * stats.refuelCost));
}

Status Economy::repair(RigState& rig, const StatBlock& stats) {
    const i64 cost = repair_cost(rig, stats);
    if (cost > mProgress->credits) {
        return fail(transaction_error("not enough credits to fully repair",
                                      "partial service is applied automatically on return"));
    }
    mProgress->credits -= cost;
    mProgress->stats.creditsSpent += cost;
    rig.hull = stats.hullMax;
    return {};
}

Status Economy::refuel(RigState& rig, const StatBlock& stats) {
    const i64 cost = refuel_cost(rig, stats);
    if (cost > mProgress->credits) {
        return fail(transaction_error("not enough credits to fully refuel"));
    }
    mProgress->credits -= cost;
    mProgress->stats.creditsSpent += cost;
    rig.fuel = stats.fuelCapacity;
    return {};
}

void Economy::service_partial(RigState& rig, const StatBlock& stats) {
    // Anti-soft-lock floor: the cooperative always tops a returning rig up to a
    // usable minimum for free, so no sequence of bad runs can strand a player
    // with no fuel and no credits (Section 6.4, "fair failure").
    const auto& e = mDb->economy();
    rig.fuel = std::max(rig.fuel, stats.fuelCapacity * e.freeServiceFuelFraction);
    rig.hull = std::max(rig.hull, stats.hullMax * e.freeServiceHullFraction);

    // Beyond the floor, buy what the player can afford; never create debt.
    const float fuelPerCredit =
        1.0f / std::max(0.01f, mDb->economy().refuelCreditsPerUnit * stats.refuelCost);
    const float hullPerCredit =
        1.0f / std::max(0.01f, mDb->economy().repairCreditsPerHull * stats.repairCost);
    // Both rates are per credit; the partial service below never goes negative.

    i64 fuelCost = refuel_cost(rig, stats);
    if (fuelCost > mProgress->credits) {
        const float affordable = static_cast<float>(mProgress->credits) * fuelPerCredit;
        rig.fuel = std::min(stats.fuelCapacity, rig.fuel + affordable);
        mProgress->stats.creditsSpent += mProgress->credits;
        mProgress->credits = 0;
        return;
    }
    mProgress->credits -= fuelCost;
    mProgress->stats.creditsSpent += fuelCost;
    rig.fuel = stats.fuelCapacity;

    i64 hullCost = repair_cost(rig, stats);
    if (hullCost > mProgress->credits) {
        const float affordable = static_cast<float>(mProgress->credits) * hullPerCredit;
        rig.hull = std::min(stats.hullMax, rig.hull + affordable);
        mProgress->stats.creditsSpent += mProgress->credits;
        mProgress->credits = 0;
        return;
    }
    mProgress->credits -= hullCost;
    mProgress->stats.creditsSpent += hullCost;
    rig.hull = stats.hullMax;
}

// ---------------------------------------------------------------------------
// Equipment
// ---------------------------------------------------------------------------

i64 Economy::equipment_cost(i32 trackIndex, i32 tier) const {
    if (trackIndex < 0 || static_cast<usize>(trackIndex) >= mDb->equipment().size()) {
        return 0;
    }
    const auto& track = mDb->equipment()[static_cast<usize>(trackIndex)];
    if (tier < 0 || static_cast<usize>(tier) >= track.tiers.size()) {
        return 0;
    }
    return track.tiers[static_cast<usize>(tier)].costCredits;
}

bool Economy::can_buy_equipment(i32 trackIndex, std::string* reason) const {
    if (trackIndex < 0 || static_cast<usize>(trackIndex) >= mDb->equipment().size()) {
        if (reason) {
            *reason = "unknown equipment track";
        }
        return false;
    }
    const auto& track = mDb->equipment()[static_cast<usize>(trackIndex)];
    const i32 current = mProgress->equipmentTier[static_cast<usize>(trackIndex)];
    const i32 next = current + 1;
    if (static_cast<usize>(next) >= track.tiers.size()) {
        if (reason) {
            *reason = "already at maximum tier";
        }
        return false;
    }
    const auto& tier = track.tiers[static_cast<usize>(next)];
    if (mProgress->credits < tier.costCredits) {
        if (reason) {
            *reason = "not enough credits";
        }
        return false;
    }
    for (const auto& [mid, amount] : tier.costComponents) {
        const auto h = mDb->material(mid.text());
        if (!h.valid() || mProgress->vault.count(h.index) < amount) {
            if (reason) {
                *reason = "missing refined components";
            }
            return false;
        }
    }
    return true;
}

Status Economy::buy_equipment(i32 trackIndex) {
    std::string reason;
    if (!can_buy_equipment(trackIndex, &reason)) {
        return fail(transaction_error(reason));
    }
    const auto& track = mDb->equipment()[static_cast<usize>(trackIndex)];
    const i32 next = mProgress->equipmentTier[static_cast<usize>(trackIndex)] + 1;
    const auto& tier = track.tiers[static_cast<usize>(next)];

    InventoryTransaction tx(*mDb, mProgress->vault);
    for (const auto& [mid, amount] : tier.costComponents) {
        const auto h = mDb->material(mid.text());
        if (!h.valid() || !tx.take(h.index, amount)) {
            return fail(transaction_error("missing refined components"));
        }
    }
    tx.commit();
    mProgress->credits -= tier.costCredits;
    mProgress->stats.creditsSpent += tier.costCredits;
    mProgress->equipmentTier[static_cast<usize>(trackIndex)] = next;
    DF_LOG_INFO("economy", "bought {} tier {}", track.id.text(), next);
    return {};
}

// ---------------------------------------------------------------------------
// Modules
// ---------------------------------------------------------------------------

bool Economy::module_unlocked(i32 moduleIndex) const {
    if (moduleIndex < 0 || static_cast<usize>(moduleIndex) >= mDb->modules().size()) {
        return false;
    }
    const auto& m = mDb->modules()[static_cast<usize>(moduleIndex)];
    if (!m.unlockResearch.empty()) {
        const auto h = mDb->research_node(m.unlockResearch.text());
        if (!h.valid() || !mProgress->has_research(h.index)) {
            return false;
        }
    }
    if (!m.unlockFaction.empty()) {
        const i32 f = mDb->faction_index(m.unlockFaction.text());
        if (f < 0 || static_cast<usize>(f) >= mProgress->reputation.size() ||
            mProgress->reputation[static_cast<usize>(f)] < m.unlockReputation) {
            return false;
        }
    }
    return true;
}

Status Economy::buy_module(i32 moduleIndex) {
    if (moduleIndex < 0 || static_cast<usize>(moduleIndex) >= mDb->modules().size()) {
        return fail(transaction_error("unknown module"));
    }
    if (mProgress->ownedModules.count(moduleIndex) > 0) {
        return fail(transaction_error("module already owned"));
    }
    if (!module_unlocked(moduleIndex)) {
        return fail(transaction_error("module is not unlocked yet",
                                      "complete the required research or reputation tier"));
    }
    const auto& m = mDb->modules()[static_cast<usize>(moduleIndex)];
    if (mProgress->credits < m.costCredits) {
        return fail(transaction_error("not enough credits"));
    }
    InventoryTransaction tx(*mDb, mProgress->vault);
    for (const auto& [mid, amount] : m.costComponents) {
        const auto h = mDb->material(mid.text());
        if (!h.valid() || !tx.take(h.index, amount)) {
            return fail(transaction_error("missing refined components"));
        }
    }
    tx.commit();
    mProgress->credits -= m.costCredits;
    mProgress->stats.creditsSpent += m.costCredits;
    mProgress->ownedModules.insert(moduleIndex);
    return {};
}

Status Economy::install_module(i32 moduleIndex) {
    if (mProgress->ownedModules.count(moduleIndex) == 0) {
        return fail(transaction_error("module not owned"));
    }
    if (mProgress->module_installed(moduleIndex)) {
        return fail(transaction_error("module already installed"));
    }
    const auto& m = mDb->modules()[static_cast<usize>(moduleIndex)];
    if (mProgress->used_utility_power(*mDb) + m.powerCost > max_utility_power(*mDb, *mProgress)) {
        return fail(transaction_error("not enough utility power",
                                      "remove a module or upgrade the utility power core"));
    }
    mProgress->installedModules.push_back(moduleIndex);
    return {};
}

Status Economy::uninstall_module(i32 moduleIndex) {
    auto& list = mProgress->installedModules;
    const auto it = std::find(list.begin(), list.end(), moduleIndex);
    if (it == list.end()) {
        return fail(transaction_error("module not installed"));
    }
    list.erase(it);
    return {};
}

// ---------------------------------------------------------------------------
// Skills
// ---------------------------------------------------------------------------

Status Economy::unlock_skill(i32 skillIndex) {
    if (skillIndex < 0 || static_cast<usize>(skillIndex) >= mDb->skills().size()) {
        return fail(transaction_error("unknown skill"));
    }
    if (mProgress->unlockedSkills.count(skillIndex) > 0) {
        return fail(transaction_error("skill already unlocked"));
    }
    const auto& s = mDb->skills()[static_cast<usize>(skillIndex)];
    if (mProgress->skill_points_available() < s.cost) {
        return fail(transaction_error("not enough proficiency points"));
    }
    for (const auto& p : s.prerequisites) {
        const auto h = mDb->skill(p.text());
        if (!h.valid() || mProgress->unlockedSkills.count(h.index) == 0) {
            return fail(transaction_error("prerequisite skill not unlocked: " + p.text()));
        }
    }
    if (!s.exclusiveGroup.empty()) {
        for (usize i = 0; i < mDb->skills().size(); ++i) {
            if (static_cast<i32>(i) == skillIndex) {
                continue;
            }
            if (mDb->skills()[i].exclusiveGroup == s.exclusiveGroup &&
                mProgress->unlockedSkills.count(static_cast<i32>(i)) > 0) {
                return fail(
                    transaction_error("a competing keystone is already chosen: " + mDb->skills()[i].id.text(),
                                      "respec to change keystone"));
            }
        }
    }
    mProgress->unlockedSkills.insert(skillIndex);
    mProgress->skillPointsSpent += s.cost;
    return {};
}

i64 Economy::respec_cost() const {
    // Fair, bounded and always affordable relative to progress.
    return 60 + static_cast<i64>(mProgress->skillPointsSpent) * 35;
}

Status Economy::respec_skills() {
    const i64 cost = respec_cost();
    if (mProgress->credits < cost) {
        return fail(transaction_error("not enough credits to respec"));
    }
    mProgress->credits -= cost;
    mProgress->stats.creditsSpent += cost;
    mProgress->unlockedSkills.clear();
    mProgress->skillPointsSpent = 0;
    return {};
}

// ---------------------------------------------------------------------------
// Research
// ---------------------------------------------------------------------------

bool Economy::research_available(i32 researchIndex, std::string* reason) const {
    if (researchIndex < 0 || static_cast<usize>(researchIndex) >= mDb->research().size()) {
        if (reason) {
            *reason = "unknown research";
        }
        return false;
    }
    if (mProgress->has_research(researchIndex)) {
        if (reason) {
            *reason = "already researched";
        }
        return false;
    }
    const auto& r = mDb->research()[static_cast<usize>(researchIndex)];
    for (const auto& p : r.prerequisites) {
        const auto h = mDb->research_node(p.text());
        if (!h.valid() || !mProgress->has_research(h.index)) {
            if (reason) {
                *reason = "requires " + p.text();
            }
            return false;
        }
    }
    if (!r.requiresSample.empty()) {
        const auto h = mDb->material(r.requiresSample.text());
        if (!h.valid() || mProgress->samples.count(h.index) < r.requiresSampleAmount) {
            if (reason) {
                *reason = "requires sample " + r.requiresSample.text();
            }
            return false;
        }
    }
    if (mProgress->insight < r.insightCost) {
        if (reason) {
            *reason = "not enough insight";
        }
        return false;
    }
    return true;
}

Status Economy::complete_research(i32 researchIndex) {
    std::string reason;
    if (!research_available(researchIndex, &reason)) {
        return fail(transaction_error(reason));
    }
    const auto& r = mDb->research()[static_cast<usize>(researchIndex)];
    InventoryTransaction tx(*mDb, mProgress->samples);
    if (!r.requiresSample.empty()) {
        const auto h = mDb->material(r.requiresSample.text());
        if (!tx.take(h.index, r.requiresSampleAmount)) {
            return fail(transaction_error("sample missing"));
        }
    }
    tx.commit();
    mProgress->insight -= r.insightCost;
    // Idempotent: repeating this call is a no-op because of the guard above.
    mProgress->completedResearch.insert(researchIndex);
    for (const auto& mid : r.unlocksModules) {
        const auto h = mDb->module(mid.text());
        if (h.valid()) {
            // Unlocking makes it purchasable; it is not granted for free.
        }
    }
    for (const auto& bid : r.unlocksBiomes) {
        const auto h = mDb->biome(bid.text());
        if (h.valid()) {
            mProgress->unlockedBiomes.insert(h.index);
        }
    }
    DF_LOG_INFO("economy", "research complete: {}", r.id.text());
    return {};
}

// ---------------------------------------------------------------------------
// Facilities and refining
// ---------------------------------------------------------------------------

i64 Economy::facility_cost(i32 facilityIndex, i32 tier) const {
    if (facilityIndex < 0 || static_cast<usize>(facilityIndex) >= mDb->facilities().size()) {
        return 0;
    }
    const auto& f = mDb->facilities()[static_cast<usize>(facilityIndex)];
    if (tier < 0 || static_cast<usize>(tier) >= f.tiers.size()) {
        return 0;
    }
    return f.tiers[static_cast<usize>(tier)].costCredits;
}

Status Economy::upgrade_facility(i32 facilityIndex) {
    if (facilityIndex < 0 || static_cast<usize>(facilityIndex) >= mDb->facilities().size()) {
        return fail(transaction_error("unknown facility"));
    }
    const auto& f = mDb->facilities()[static_cast<usize>(facilityIndex)];
    const i32 next = mProgress->facilityTier[static_cast<usize>(facilityIndex)] + 1;
    if (static_cast<usize>(next) >= f.tiers.size()) {
        return fail(transaction_error("facility already at maximum tier"));
    }
    const auto& tier = f.tiers[static_cast<usize>(next)];
    if (mProgress->credits < tier.costCredits) {
        return fail(transaction_error("not enough credits"));
    }
    InventoryTransaction tx(*mDb, mProgress->vault);
    for (const auto& ing : tier.costComponents) {
        const auto h = mDb->material(ing.materialId.text());
        if (!h.valid() || !tx.take(h.index, ing.amount)) {
            return fail(transaction_error("missing components for this facility tier"));
        }
    }
    tx.commit();
    mProgress->credits -= tier.costCredits;
    mProgress->stats.creditsSpent += tier.costCredits;
    mProgress->facilityTier[static_cast<usize>(facilityIndex)] = next;
    return {};
}

bool Economy::recipe_available(i32 recipeIndex, std::string* reason) const {
    if (recipeIndex < 0 || static_cast<usize>(recipeIndex) >= mDb->recipes().size()) {
        if (reason) {
            *reason = "unknown recipe";
        }
        return false;
    }
    const auto& r = mDb->recipes()[static_cast<usize>(recipeIndex)];
    if (!r.unlockResearch.empty()) {
        const auto h = mDb->research_node(r.unlockResearch.text());
        if (!h.valid() || !mProgress->has_research(h.index)) {
            if (reason) {
                *reason = "requires research " + r.unlockResearch.text();
            }
            return false;
        }
    }
    const auto fh = mDb->facility(r.facilityId.text());
    if (!fh.valid() || mProgress->facilityTier[static_cast<usize>(fh.index)] < r.facilityTier) {
        if (reason) {
            *reason = "requires a higher facility tier";
        }
        return false;
    }
    return true;
}

Status Economy::run_recipe(i32 recipeIndex, i32 times, const StatBlock& stats) {
    std::string reason;
    if (!recipe_available(recipeIndex, &reason)) {
        return fail(transaction_error(reason));
    }
    if (times <= 0) {
        return fail(transaction_error("invalid batch size"));
    }
    const auto& r = mDb->recipes()[static_cast<usize>(recipeIndex)];
    const i64 cost = static_cast<i64>(r.creditCost) * times;
    if (mProgress->credits < cost) {
        return fail(transaction_error("not enough credits for the processing fee"));
    }
    InventoryTransaction tx(*mDb, mProgress->vault);
    for (const auto& in : r.inputs) {
        const auto h = mDb->material(in.materialId.text());
        if (!h.valid() || !tx.take(h.index, in.amount * times)) {
            return fail(transaction_error("missing input material: " + in.materialId.text()));
        }
    }
    for (const auto& out : r.outputs) {
        const auto h = mDb->material(out.materialId.text());
        if (!h.valid()) {
            return fail(transaction_error("recipe output is unresolved"));
        }
        const i32 amount = std::max(
            1, static_cast<i32>(std::lround(static_cast<float>(out.amount * times) * stats.refineYield)));
        tx.give(h.index, amount, false);
    }
    for (const auto& bp : r.byproducts) {
        const auto h = mDb->material(bp.materialId.text());
        if (h.valid()) {
            tx.give(h.index, bp.amount * times, false);
        }
    }
    tx.commit();
    mProgress->credits -= cost;
    mProgress->stats.creditsSpent += cost;
    return {};
}

void Economy::award_insight(i64 amount, const StatBlock& stats) {
    if (amount <= 0) {
        return;
    }
    mProgress->insight +=
        static_cast<i64>(std::llround(static_cast<double>(amount) * static_cast<double>(stats.insightGain)));
}

void Economy::award_skill_point(i32 amount) {
    mProgress->skillPointsEarned += amount;
}

void Economy::add_reputation(i32 factionIndex, i32 amount) {
    if (factionIndex < 0 || static_cast<usize>(factionIndex) >= mProgress->reputation.size()) {
        return;
    }
    mProgress->reputation[static_cast<usize>(factionIndex)] += amount;
}

// ---------------------------------------------------------------------------
// Contracts
// ---------------------------------------------------------------------------

bool Economy::contract_eligible(const ContractTemplate& t) const {
    if (mProgress->stats.deepestDepth < t.minDepthUnlocked) {
        return false;
    }
    if (!t.requiresResearch.empty()) {
        const auto h = mDb->research_node(t.requiresResearch.text());
        if (!h.valid() || !mProgress->has_research(h.index)) {
            return false;
        }
    }
    if (!t.factionId.empty()) {
        const i32 f = mDb->faction_index(t.factionId.text());
        if (f < 0 || mProgress->reputation[static_cast<usize>(f)] < t.minReputation) {
            return false;
        }
    }
    // Never offer a contract for content that cannot be obtained.
    for (const auto& mid : t.materialPool) {
        const auto h = mDb->material(mid.text());
        if (!h.valid()) {
            return false;
        }
        const auto& m = mDb->material_def(h);
        const bool generated = m.rarityWeightBp > 0;
        const bool refinable = !generated;
        if (refinable) {
            bool haveRecipe = false;
            for (usize ri = 0; ri < mDb->recipes().size(); ++ri) {
                for (const auto& out : mDb->recipes()[ri].outputs) {
                    if (out.materialId == mid && recipe_available(static_cast<i32>(ri), nullptr)) {
                        haveRecipe = true;
                    }
                }
            }
            if (!haveRecipe) {
                return false;
            }
        } else if (m.minDepth > mProgress->stats.deepestDepth + 260) {
            return false;
        }
    }
    if (t.kind == ContractKind::ReachDepth && t.depthMin > mProgress->stats.deepestDepth + 700) {
        return false;
    }
    if (!t.repeatable) {
        for (const auto& c : mProgress->completedContracts) {
            if (c.templateIndex >= 0 && mDb->contracts()[static_cast<usize>(c.templateIndex)].id == t.id) {
                return false;
            }
        }
    }
    for (const auto& c : mProgress->contracts) {
        if (c.templateIndex >= 0 && mDb->contracts()[static_cast<usize>(c.templateIndex)].id == t.id) {
            return false;
        }
    }
    return true;
}

void Economy::refresh_contract_offers(u64 worldSeed, i32 count) {
    mOffers.clear();
    std::vector<i32> eligible;
    for (usize i = 0; i < mDb->contracts().size(); ++i) {
        if (contract_eligible(mDb->contracts()[i])) {
            eligible.push_back(static_cast<i32>(i));
        }
    }
    Rng rng{hash_combine(derive_seed(worldSeed, RngStream::Contracts),
                         static_cast<u64>(mProgress->stats.expeditions))};
    // Authored contracts always come first so story beats are never buried.
    std::stable_sort(eligible.begin(), eligible.end(), [this](i32 a, i32 b) {
        return mDb->contracts()[static_cast<usize>(a)].authored >
               mDb->contracts()[static_cast<usize>(b)].authored;
    });
    for (i32 idx : eligible) {
        if (static_cast<i32>(mOffers.size()) >= count) {
            break;
        }
        const auto& t = mDb->contracts()[static_cast<usize>(idx)];
        ActiveContract c;
        c.templateIndex = idx;
        c.instanceId = mNextInstanceId++;
        if (!t.materialPool.empty()) {
            const auto& mid = t.materialPool[rng.below(static_cast<u32>(t.materialPool.size()))];
            const auto h = mDb->material(mid.text());
            c.targetMaterial = h.index;
            c.targetAmount = rng.range(t.amountMin, t.amountMax);
        }
        if (t.kind == ContractKind::ReachDepth) {
            c.targetDepth = rng.range(t.depthMin, std::max(t.depthMin, t.depthMax));
        }
        if (t.kind == ContractKind::NoModuleClass) {
            c.forbiddenSlot = static_cast<i32>(rng.below(static_cast<u32>(ModuleSlot::Count)));
            c.targetDepth = rng.range(t.depthMin, std::max(t.depthMin, t.depthMax));
        }
        mOffers.push_back(c);
    }
}

Status Economy::accept_contract(usize offerIndex) {
    if (offerIndex >= mOffers.size()) {
        return fail(transaction_error("no such contract offer"));
    }
    if (mProgress->contracts.size() >= 4) {
        return fail(transaction_error("contract board is full", "complete or abandon a contract first"));
    }
    mProgress->contracts.push_back(mOffers[offerIndex]);
    mOffers.erase(mOffers.begin() + static_cast<std::ptrdiff_t>(offerIndex));
    return {};
}

Status Economy::abandon_contract(u64 instanceId) {
    auto& list = mProgress->contracts;
    const auto it = std::find_if(list.begin(), list.end(), [instanceId](const ActiveContract& c) {
        return c.instanceId == instanceId;
    });
    if (it == list.end()) {
        return fail(transaction_error("contract not active"));
    }
    list.erase(it);
    return {};
}

std::vector<i32> Economy::settle_contracts(const Inventory& soldCargo, i64 depthReached,
                                           const ExpeditionOutcome& outcome, const StatBlock& stats) {
    std::vector<i32> completed;
    for (auto& c : mProgress->contracts) {
        if (c.completed || c.templateIndex < 0) {
            continue;
        }
        const auto& t = mDb->contracts()[static_cast<usize>(c.templateIndex)];
        switch (t.kind) {
            case ContractKind::DeliverMaterials:
            case ContractKind::ExtractVolatile:
                if (c.targetMaterial >= 0) {
                    c.progress += soldCargo.count(c.targetMaterial);
                    if (c.progress >= c.targetAmount) {
                        c.completed = true;
                    }
                }
                break;
            case ContractKind::ReachDepth:
            case ContractKind::NoModuleClass:
                if (depthReached >= c.targetDepth) {
                    c.progress = static_cast<i32>(depthReached);
                    c.completed = true;
                }
                break;
            case ContractKind::ScanAnomaly:
                // An anomaly scan means finding an artifact chamber or set piece
                // in the contracted band, with the scanner actually used.
                if (depthReached >= t.depthMin && outcome.scanPulses > 0 &&
                    (outcome.discovered(LandmarkKind::ArtifactChamber) > 0 ||
                     outcome.discovered(LandmarkKind::SetPiece) > 0 ||
                     outcome.discovered(LandmarkKind::CollapsedFacility) > 0)) {
                    c.progress = 1;
                    c.completed = true;
                }
                break;
            case ContractKind::RecoverCache:
                if (depthReached >= t.depthMin && (outcome.activated(LandmarkKind::Cache) > 0 ||
                                                   outcome.activated(LandmarkKind::CollapsedFacility) > 0)) {
                    c.progress = 1;
                    c.completed = true;
                }
                break;
            case ContractKind::ActivateInfrastructure:
                if (outcome.activated(LandmarkKind::TransitNode) > 0 ||
                    outcome.activated(LandmarkKind::RepairStation) > 0) {
                    c.progress = 1;
                    c.completed = true;
                }
                break;
            case ContractKind::RescueProspector:
                if (outcome.activated(LandmarkKind::RescueBeacon) > 0 ||
                    outcome.discovered(LandmarkKind::RescueBeacon) > 0) {
                    c.progress = 1;
                    c.completed = true;
                }
                break;
            case ContractKind::Count:
                break;
        }
        if (!c.completed || c.claimed) {
            continue;
        }
        c.claimed = true;
        i64 credits = t.rewardCredits;
        if (t.rewardCreditsPerUnit > 0 && c.targetAmount > 0) {
            credits += static_cast<i64>(t.rewardCreditsPerUnit) * c.targetAmount;
        }
        credits = static_cast<i64>(
            std::llround(static_cast<double>(credits) * static_cast<double>(stats.contractReward)));
        mProgress->credits += credits;
        mProgress->stats.creditsEarned += credits;
        award_insight(t.rewardInsight, stats);
        if (!t.factionId.empty()) {
            add_reputation(mDb->faction_index(t.factionId.text()), t.rewardReputation);
        }
        for (const auto& rc : t.rewardComponents) {
            const auto h = mDb->material(rc.materialId.text());
            if (h.valid()) {
                mProgress->vault.add(*mDb, h.index, rc.amount, false);
            }
        }
        completed.push_back(c.templateIndex);
    }
    // Move claimed contracts to history.
    auto& list = mProgress->contracts;
    for (auto it = list.begin(); it != list.end();) {
        if (it->claimed) {
            mProgress->completedContracts.push_back(*it);
            it = list.erase(it);
        } else {
            ++it;
        }
    }
    return completed;
}

// ---------------------------------------------------------------------------
// Failure and rescue
// ---------------------------------------------------------------------------

RescuePreview Economy::preview_rescue(const RigState& rig, const Inventory& cargo,
                                      const StatBlock& stats) const {
    const auto& econ = mDb->economy();
    RescuePreview p;
    p.freeRescue = !mProgress->freeRescueUsed;
    if (!p.freeRescue) {
        const double raw = static_cast<double>(econ.rescueFeeBase) +
                           static_cast<double>(econ.rescueFeeDepthFactor) *
                               static_cast<double>(std::max<i64>(0, rig.depth_tiles()));
        // Capped so a failure can never create a debt spiral (Section 6.4).
        const double cap =
            static_cast<double>(mProgress->credits) * static_cast<double>(econ.rescueFeeCapFraction);
        p.fee = static_cast<i64>(
            std::llround(std::min(raw * static_cast<double>(stats.rescueFee), std::max(0.0, cap))));
    }
    const float keepFraction = 1.0f - econ.rescueCargoLossFraction;
    for (const auto& s : cargo.stacks()) {
        const auto& m = mDb->materials()[static_cast<usize>(s.material)];
        const i32 lost = s.amount - static_cast<i32>(static_cast<float>(s.amount) * keepFraction);
        p.cargoUnitsLost += lost * m.cargoUnits;
        p.cargoValueLost += static_cast<i64>(m.baseValue) * lost;
    }
    p.wreckCreated = stats.wreckRecovery > 0.0f && p.cargoValueLost > 0;
    return p;
}

ExpeditionSummary Economy::resolve_rescue(RigState& rig, Inventory& cargo, const StatBlock& stats) {
    const auto& econ = mDb->economy();
    const RescuePreview preview = preview_rescue(rig, cargo, stats);
    ExpeditionSummary summary;
    summary.rescued = true;
    summary.depthReached = rig.deepestThisRun;

    mProgress->credits = std::max<i64>(0, mProgress->credits - preview.fee);
    mProgress->stats.creditsSpent += preview.fee;
    mProgress->stats.rescues += 1;
    if (preview.freeRescue) {
        mProgress->freeRescueUsed = true;
    }

    // Unbanked commodity cargo is partially lost; a share can be recovered by
    // recovery technology. Progression, equipment and samples are untouched.
    const float keepFraction =
        1.0f - econ.rescueCargoLossFraction + econ.rescueCargoLossFraction * stats.wreckRecovery;
    std::vector<InventoryStack> kept;
    for (const auto& s : cargo.stacks()) {
        const i32 keep = static_cast<i32>(static_cast<float>(s.amount) * keepFraction);
        if (keep > 0) {
            kept.push_back(InventoryStack{s.material, keep});
        }
    }
    cargo.set_stacks(std::move(kept));

    rig.hull = std::max(1.0f, stats.hullMax * 0.25f);
    rig.fuel = std::max(1.0f, stats.fuelCapacity * 0.25f);
    rig.position = Vec2{0.0f, 0.0f};
    rig.velocity = Vec2{};
    rig.heat = 0.0f;
    rig.mode = RigMode::Rescued;
    return summary;
}

} // namespace df
