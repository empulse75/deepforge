#include "TestSupport.hpp"

#include "domain/Economy.hpp"
#include "domain/Loadout.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace df;

namespace {

PlayerProgress fresh(const ContentDatabase& db) {
    PlayerProgress p;
    p.initialise(db);
    return p;
}

} // namespace

TEST_CASE("selling converts cargo to credits and empties the hold", "[economy]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    const auto ferrite = db.material("material.ferrite");
    Inventory cargo;
    cargo.set_capacity(100.0f);
    cargo.add(db, ferrite.index, 10);

    const StatBlock stats = compute_stats(db, p);
    const auto preview = econ.preview_sale(cargo, stats);
    const i64 before = p.credits;
    const auto sale = econ.sell_all(cargo, stats);
    CHECK(sale.totalCredits == preview.totalCredits);
    CHECK(sale.totalCredits == db.material_def(ferrite).baseValue * 10);
    CHECK(p.credits == before + sale.totalCredits);
    CHECK(cargo.count(ferrite.index) == 0);
}

TEST_CASE("equipment purchases check prerequisites and stop at the cap", "[economy][upgrades]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    const auto track = db.equipment_track("equipment.fuel_tank");
    REQUIRE(track.valid());

    p.credits = 0;
    CHECK_FALSE(econ.can_buy_equipment(track.index));
    CHECK_FALSE(econ.buy_equipment(track.index).has_value());

    p.credits = 100000;
    const StatBlock before = compute_stats(db, p);
    REQUIRE(econ.buy_equipment(track.index).has_value());
    CHECK(p.equipmentTier[static_cast<usize>(track.index)] == 1);
    const StatBlock after = compute_stats(db, p);
    CHECK(after.fuelCapacity > before.fuelCapacity);

    // Credits alone are not enough: the last tiers also want refined
    // components, so the track stalls with a stated reason rather than silently.
    for (int i = 0; i < 40; ++i) {
        (void)econ.buy_equipment(track.index);
    }
    std::string reason;
    CHECK_FALSE(econ.can_buy_equipment(track.index, &reason));
    CHECK(reason == "missing refined components");
    const i32 stalledAt = p.equipmentTier[static_cast<usize>(track.index)];
    CHECK(stalledAt > 0);
    CHECK(static_cast<usize>(stalledAt) + 1 < db.equipment()[static_cast<usize>(track.index)].tiers.size());

    // With the components in the vault it runs to the cap and then stops.
    for (const auto& m : db.materials()) {
        if (m.rarityWeightBp == 0 && !m.mineable && m.baseValue > 0) {
            p.vault.add(db, db.material(m.id.text()).index, 200, false);
        }
    }
    for (int i = 0; i < 40; ++i) {
        (void)econ.buy_equipment(track.index);
    }
    CHECK(static_cast<usize>(p.equipmentTier[static_cast<usize>(track.index)]) + 1 ==
          db.equipment()[static_cast<usize>(track.index)].tiers.size());
    CHECK_FALSE(econ.can_buy_equipment(track.index, &reason));
    CHECK(reason == "already at maximum tier");
}

TEST_CASE("modules respect the utility power budget", "[economy][modules]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    p.credits = 1000000;
    p.maxUtilityPower = 3;

    int installed = 0;
    for (usize i = 0; i < db.modules().size(); ++i) {
        if (!econ.module_unlocked(static_cast<i32>(i))) {
            continue;
        }
        if (econ.buy_module(static_cast<i32>(i)) && econ.install_module(static_cast<i32>(i))) {
            ++installed;
        }
    }
    CHECK(installed > 0);
    CHECK(p.used_utility_power(db) <= p.maxUtilityPower);

    // Removing one frees its power again.
    const i32 first = p.installedModules.front();
    const i32 freed = db.modules()[static_cast<usize>(first)].powerCost;
    const i32 used = p.used_utility_power(db);
    REQUIRE(econ.uninstall_module(first).has_value());
    CHECK(p.used_utility_power(db) == used - freed);
}

TEST_CASE("a locked module cannot be bought", "[economy][modules]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    p.credits = 1000000;
    const auto gated = db.module("module.thermal_reclaimer");
    REQUIRE(gated.valid());
    CHECK_FALSE(econ.module_unlocked(gated.index));
    CHECK_FALSE(econ.buy_module(gated.index).has_value());
}

TEST_CASE("skills enforce prerequisites and exclusive keystones", "[economy][skills]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    p.skillPointsEarned = 200;

    const auto second = db.skill("skill.rich_seams");
    REQUIRE(second.valid());
    CHECK_FALSE(econ.unlock_skill(second.index).has_value()); // needs keen_eye

    const auto root = db.skill("skill.keen_eye");
    REQUIRE(econ.unlock_skill(root.index).has_value());
    REQUIRE(econ.unlock_skill(second.index).has_value());
    CHECK_FALSE(econ.unlock_skill(second.index).has_value()); // already taken

    for (const char* id :
         {"skill.vein_reader", "skill.deep_pockets", "skill.clean_extraction", "skill.prospector_mastery"}) {
        const auto h = db.skill(id);
        REQUIRE(h.valid());
        REQUIRE(econ.unlock_skill(h.index).has_value());
    }
    const auto k1 = db.skill("skill.keystone_lodefinder");
    const auto k2 = db.skill("skill.keystone_highgrader");
    REQUIRE(econ.unlock_skill(k1.index).has_value());
    CHECK_FALSE(econ.unlock_skill(k2.index).has_value());
}

TEST_CASE("respec refunds every point for a bounded fee", "[economy][skills]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    p.skillPointsEarned = 10;
    p.credits = 100000;
    const auto root = db.skill("skill.keen_eye");
    REQUIRE(econ.unlock_skill(root.index).has_value());
    CHECK(p.skill_points_available() == 9);
    const i64 cost = econ.respec_cost();
    const i64 before = p.credits;
    REQUIRE(econ.respec_skills().has_value());
    CHECK(p.skill_points_available() == 10);
    CHECK(p.unlockedSkills.empty());
    CHECK(p.credits == before - cost);
}

TEST_CASE("research is idempotent and consumes its sample once", "[economy][research]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    const auto node = db.research_node("research.crystal_optics");
    const auto prereq = db.research_node("research.field_metallurgy");
    REQUIRE(node.valid());
    REQUIRE(prereq.valid());
    p.insight = 100;
    REQUIRE(econ.complete_research(prereq.index).has_value());

    CHECK_FALSE(econ.complete_research(node.index).has_value()); // sample missing
    const auto shard = db.material("material.echo_shard");
    p.samples.set_capacity(10.0f);
    p.samples.add(db, shard.index, 1);
    REQUIRE(econ.complete_research(node.index).has_value());
    CHECK(p.samples.count(shard.index) == 0);

    const i64 insight = p.insight;
    CHECK_FALSE(econ.complete_research(node.index).has_value());
    CHECK(p.insight == insight);
}

TEST_CASE("refining consumes inputs and produces outputs", "[economy][refining]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    const auto recipe = db.recipe("recipe.refined_plate");
    REQUIRE(recipe.valid());
    p.insight = 100;
    REQUIRE(econ.complete_research(db.research_node("research.field_metallurgy").index).has_value());

    const auto ferrite = db.material("material.ferrite");
    const auto cuprite = db.material("material.cuprite");
    const auto plate = db.material("material.refined_plate");
    p.credits = 1000;
    p.vault.add(db, ferrite.index, 8, false);
    p.vault.add(db, cuprite.index, 3, false);
    const StatBlock stats = compute_stats(db, p);
    REQUIRE(econ.run_recipe(recipe.index, 1, stats).has_value());
    CHECK(p.vault.count(plate.index) >= 1);
    CHECK(p.vault.count(ferrite.index) == 0);

    // Not enough input for a second batch: nothing may change.
    const auto snapshot = p.vault.stacks();
    const i64 credits = p.credits;
    CHECK_FALSE(econ.run_recipe(recipe.index, 1, stats).has_value());
    CHECK(p.vault.stacks() == snapshot);
    CHECK(p.credits == credits);
}

TEST_CASE("rescue is capped and never creates a debt spiral", "[economy][failure]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    RigState rig;
    rig.position = Vec2{0.0f, 2400.0f};
    Inventory cargo;
    cargo.set_capacity(100.0f);
    cargo.add(db, db.material("material.aurelite").index, 30);
    const StatBlock stats = compute_stats(db, p);

    p.freeRescueUsed = true;
    p.credits = 50;
    const auto preview = econ.preview_rescue(rig, cargo, stats);
    CHECK(preview.fee <= 50);
    CHECK(preview.cargoValueLost > 0);
    econ.resolve_rescue(rig, cargo, stats);
    CHECK(p.credits >= 0);
    CHECK(p.equipmentTier.size() == db.equipment().size());
    CHECK(rig.hull > 0.0f);
    CHECK(rig.fuel > 0.0f);
}

TEST_CASE("the first rescue is free", "[economy][failure]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    RigState rig;
    rig.position = Vec2{0.0f, 500.0f};
    Inventory cargo;
    const StatBlock stats = compute_stats(db, p);
    const auto preview = econ.preview_rescue(rig, cargo, stats);
    CHECK(preview.freeRescue);
    CHECK(preview.fee == 0);
}

TEST_CASE("contracts are only offered for reachable content", "[economy][contracts]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    p.stats.deepestDepth = 0;
    econ.refresh_contract_offers(12345);
    CHECK_FALSE(econ.offers().empty());
    for (const auto& offer : econ.offers()) {
        const auto& t = db.contracts()[static_cast<usize>(offer.templateIndex)];
        INFO("contract " << t.id.text());
        CHECK(t.minDepthUnlocked <= p.stats.deepestDepth);
        if (offer.targetMaterial >= 0) {
            const auto& m = db.materials()[static_cast<usize>(offer.targetMaterial)];
            CHECK(m.minDepth <= p.stats.deepestDepth + 260);
        }
        if (t.kind == ContractKind::ReachDepth) {
            CHECK(offer.targetDepth <= p.stats.deepestDepth + 700);
        }
    }
}

TEST_CASE("delivery contracts pay out exactly once", "[economy][contracts]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    econ.refresh_contract_offers(999);
    REQUIRE_FALSE(econ.offers().empty());

    usize chosen = 0;
    bool found = false;
    for (usize i = 0; i < econ.offers().size(); ++i) {
        const auto& t = db.contracts()[static_cast<usize>(econ.offers()[i].templateIndex)];
        if (t.kind == ContractKind::DeliverMaterials && econ.offers()[i].targetMaterial >= 0) {
            chosen = i;
            found = true;
            break;
        }
    }
    REQUIRE(found);
    const auto contract = econ.offers()[chosen];
    REQUIRE(econ.accept_contract(chosen).has_value());

    Inventory sold;
    sold.set_capacity(1e6f);
    sold.add(db, contract.targetMaterial, contract.targetAmount);
    const StatBlock stats = compute_stats(db, p);
    ExpeditionOutcome outcome;
    const i64 before = p.credits;
    const auto completed = econ.settle_contracts(sold, 400, outcome, stats);
    CHECK(completed.size() == 1);
    CHECK(p.credits > before);

    const i64 after = p.credits;
    const auto again = econ.settle_contracts(sold, 400, outcome, stats);
    CHECK(again.empty());
    CHECK(p.credits == after);
}

TEST_CASE("landmark contracts need the landmark, not just the depth", "[economy][contracts]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    p.stats.deepestDepth = 4000;

    // Accept one contract of each landmark-driven kind that is on offer.
    econ.refresh_contract_offers(20260906, 40);
    std::vector<ContractKind> wanted{ContractKind::RecoverCache, ContractKind::ActivateInfrastructure,
                                     ContractKind::RescueProspector, ContractKind::ScanAnomaly};
    for (const auto kind : wanted) {
        for (usize i = 0; i < econ.offers().size(); ++i) {
            const auto& t = db.contracts()[static_cast<usize>(econ.offers()[i].templateIndex)];
            if (t.kind == kind) {
                (void)econ.accept_contract(i);
                break;
            }
        }
    }
    REQUIRE_FALSE(p.contracts.empty());

    const StatBlock stats = compute_stats(db, p);
    // Reaching the depth alone must not complete any of them.
    ExpeditionOutcome nothing;
    const auto none = econ.settle_contracts(Inventory{}, 4000, nothing, stats);
    for (const auto& c : p.contracts) {
        const auto& t = db.contracts()[static_cast<usize>(c.templateIndex)];
        if (t.kind == ContractKind::RecoverCache || t.kind == ContractKind::ActivateInfrastructure ||
            t.kind == ContractKind::RescueProspector || t.kind == ContractKind::ScanAnomaly) {
            INFO("contract " << t.id.text());
            CHECK_FALSE(c.completed);
        }
    }

    // Doing the actual thing completes them.
    ExpeditionOutcome did;
    did.landmarksActivated[static_cast<usize>(LandmarkKind::Cache)] = 1;
    did.landmarksActivated[static_cast<usize>(LandmarkKind::TransitNode)] = 1;
    did.landmarksDiscovered[static_cast<usize>(LandmarkKind::RescueBeacon)] = 1;
    did.landmarksDiscovered[static_cast<usize>(LandmarkKind::ArtifactChamber)] = 1;
    did.scanPulses = 3;
    const auto completed = econ.settle_contracts(Inventory{}, 4000, did, stats);
    CHECK_FALSE(completed.empty());
    CHECK(none.size() < completed.size() + 1);
}

TEST_CASE("stat modifiers stay inside their caps", "[economy][stats]") {
    const auto& db = dft::shipped_content();
    auto p = fresh(db);
    Economy econ(db, p);
    p.credits = 100000000;
    p.insight = 100000;
    p.skillPointsEarned = 500;
    for (usize i = 0; i < db.equipment().size(); ++i) {
        for (int t = 0; t < 20; ++t) {
            (void)econ.buy_equipment(static_cast<i32>(i));
        }
    }
    for (int pass = 0; pass < 3; ++pass) {
        for (usize i = 0; i < db.research().size(); ++i) {
            (void)econ.complete_research(static_cast<i32>(i));
        }
        for (usize i = 0; i < db.skills().size(); ++i) {
            (void)econ.unlock_skill(static_cast<i32>(i));
        }
    }
    const StatBlock s = compute_stats(db, p);
    CHECK(s.drillPower <= 60.0f);
    CHECK(s.saleValue <= 2.5f);
    CHECK(s.yieldMul <= 3.0f);
    CHECK(s.cargoCapacity <= 2000.0f);
    CHECK(s.fuelCapacity <= 4000.0f);
    CHECK(s.hazardDamage >= 0.1f);
    CHECK(s.rescueFee >= 0.0f);
    CHECK(s.protectedSampleSlots <= 12);
    CHECK(s.mineRadius <= 2);
}

TEST_CASE("no recipe creates value from nothing", "[economy][balance]") {
    const auto& db = dft::shipped_content();
    for (const auto& r : db.recipes()) {
        i64 in = 0;
        i64 out = 0;
        for (const auto& e : r.inputs) {
            in += static_cast<i64>(db.material_def(db.material(e.materialId.text())).baseValue) * e.amount;
        }
        for (const auto& e : r.outputs) {
            out += static_cast<i64>(db.material_def(db.material(e.materialId.text())).baseValue) * e.amount;
        }
        INFO("recipe " << r.id.text() << " in=" << in << " out=" << out);
        CHECK(in > 0);
        CHECK(out <= in * 6 + 400);
    }
}

TEST_CASE("selling a progression material cannot soft lock a save", "[economy][balance]") {
    const auto& db = dft::shipped_content();
    auto obtainable = [&](const StringId& id) {
        const auto h = db.material(id.text());
        if (!h.valid()) {
            return false;
        }
        if (db.material_def(h).rarityWeightBp > 0) {
            return true;
        }
        for (const auto& r : db.recipes()) {
            for (const auto& out : r.outputs) {
                if (out.materialId == id) {
                    return true;
                }
            }
        }
        return false;
    };
    for (const auto& e : db.equipment()) {
        for (const auto& t : e.tiers) {
            for (const auto& [mid, amount] : t.costComponents) {
                INFO(e.id.text() << " requires " << mid.text());
                CHECK(obtainable(mid));
            }
        }
    }
    for (const auto& r : db.recipes()) {
        for (const auto& in : r.inputs) {
            INFO(r.id.text() << " requires " << in.materialId.text());
            CHECK(obtainable(in.materialId));
        }
    }
    for (const auto& m : db.modules()) {
        for (const auto& [mid, amount] : m.costComponents) {
            INFO(m.id.text() << " requires " << mid.text());
            CHECK(obtainable(mid));
        }
    }
}
