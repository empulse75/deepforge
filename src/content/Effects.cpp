#include "content/Effects.hpp"

#include "core/Math.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace df {
namespace {

struct NameEntry {
    EffectType type;
    std::string_view name;
};

constexpr std::array<NameEntry, 39> kEffectNames{{
    {EffectType::None, "none"},
    {EffectType::DrillPowerMul, "drill_power_mul"},
    {EffectType::DrillPowerAdd, "drill_power_add"},
    {EffectType::DrillEfficiencyMul, "drill_efficiency_mul"},
    {EffectType::ThrustMul, "thrust_mul"},
    {EffectType::BrakeMul, "brake_mul"},
    {EffectType::FuelCapacityAdd, "fuel_capacity_add"},
    {EffectType::FuelCapacityMul, "fuel_capacity_mul"},
    {EffectType::CargoCapacityAdd, "cargo_capacity_add"},
    {EffectType::CargoCapacityMul, "cargo_capacity_mul"},
    {EffectType::HullMaxAdd, "hull_max_add"},
    {EffectType::HullMaxMul, "hull_max_mul"},
    {EffectType::CoolingMul, "cooling_mul"},
    {EffectType::HeatGenerationMul, "heat_generation_mul"},
    {EffectType::HeatCapacityAdd, "heat_capacity_add"},
    {EffectType::ScannerRangeAdd, "scanner_range_add"},
    {EffectType::ScannerDetailAdd, "scanner_detail_add"},
    {EffectType::SaleValueMul, "sale_value_mul"},
    {EffectType::YieldMul, "yield_mul"},
    {EffectType::ImpactDamageMul, "impact_damage_mul"},
    {EffectType::HazardDamageMul, "hazard_damage_mul"},
    {EffectType::RepairCostMul, "repair_cost_mul"},
    {EffectType::RefuelCostMul, "refuel_cost_mul"},
    {EffectType::RescueFeeMul, "rescue_fee_mul"},
    {EffectType::ConvertHeatToFuel, "convert_heat_to_fuel"},
    {EffectType::AutoCollectRadius, "auto_collect_radius"},
    {EffectType::HoverFuelMul, "hover_fuel_mul"},
    {EffectType::VolatileStabilityAdd, "volatile_stability_add"},
    {EffectType::InsightGainMul, "insight_gain_mul"},
    {EffectType::ContractRewardMul, "contract_reward_mul"},
    {EffectType::ProtectedSampleSlots, "protected_sample_slots"},
    {EffectType::WreckRecoveryAdd, "wreck_recovery_add"},
    {EffectType::DepthHeatMul, "depth_heat_mul"},
    {EffectType::CargoMassMul, "cargo_mass_mul"},
    {EffectType::MineRadiusAdd, "mine_radius_add"},
    {EffectType::VeinRevealAdd, "vein_reveal_add"},
    {EffectType::RefineYieldMul, "refine_yield_mul"},
    {EffectType::GrantFlag, "grant_flag"},
    {EffectType::Count, "count"},
}};

constexpr std::array<std::pair<RigFlag, std::string_view>, 15> kFlagNames{{
    {RigFlag::BrakeWithoutFuel, "brake_without_fuel"},
    {RigFlag::SeeVeinDirection, "see_vein_direction"},
    {RigFlag::BreakBrittleClusters, "break_brittle_clusters"},
    {RigFlag::SurviveHeatSpike, "survive_heat_spike"},
    {RigFlag::AutoMarkValuables, "auto_mark_valuables"},
    {RigFlag::ScanThroughRock, "scan_through_rock"},
    {RigFlag::SporeImmunity, "spore_immunity"},
    {RigFlag::LavaShielding, "lava_shielding"},
    {RigFlag::MagneticStabiliser, "magnetic_stabiliser"},
    {RigFlag::FreeUnstuck, "free_unstuck"},
    {RigFlag::TransitAccess, "transit_access"},
    {RigFlag::GrappleAnchor, "grapple_anchor"},
    {RigFlag::HoverAssist, "hover_assist"},
    {RigFlag::PortableRefinery, "portable_refinery"},
    {RigFlag::DeployableBeacon, "deployable_beacon"},
}};

} // namespace

std::string_view effect_type_name(EffectType t) {
    for (const auto& e : kEffectNames) {
        if (e.type == t) {
            return e.name;
        }
    }
    return "unknown";
}

EffectType effect_type_from_string(std::string_view s) {
    for (const auto& e : kEffectNames) {
        if (e.name == s) {
            return e.type;
        }
    }
    return EffectType::None;
}

std::string_view rig_flag_name(RigFlag f) {
    for (const auto& e : kFlagNames) {
        if (e.first == f) {
            return e.second;
        }
    }
    return "unknown";
}

bool rig_flag_from_string(std::string_view s, RigFlag& out) {
    for (const auto& e : kFlagNames) {
        if (e.second == s) {
            out = e.first;
            return true;
        }
    }
    return false;
}

void apply_effect(StatBlock& s, const Effect& e) {
    switch (e.type) {
        case EffectType::DrillPowerMul:
            s.drillPower *= e.a;
            break;
        case EffectType::DrillPowerAdd:
            s.drillPower += e.a;
            break;
        case EffectType::DrillEfficiencyMul:
            s.drillEfficiency *= e.a;
            break;
        case EffectType::ThrustMul:
            s.thrust *= e.a;
            break;
        case EffectType::BrakeMul:
            s.brake *= e.a;
            break;
        case EffectType::FuelCapacityAdd:
            s.fuelCapacity += e.a;
            break;
        case EffectType::FuelCapacityMul:
            s.fuelCapacity *= e.a;
            break;
        case EffectType::CargoCapacityAdd:
            s.cargoCapacity += e.a;
            break;
        case EffectType::CargoCapacityMul:
            s.cargoCapacity *= e.a;
            break;
        case EffectType::HullMaxAdd:
            s.hullMax += e.a;
            break;
        case EffectType::HullMaxMul:
            s.hullMax *= e.a;
            break;
        case EffectType::CoolingMul:
            s.cooling *= e.a;
            break;
        case EffectType::HeatGenerationMul:
            s.heatGeneration *= e.a;
            break;
        case EffectType::HeatCapacityAdd:
            s.heatCapacity += e.a;
            break;
        case EffectType::ScannerRangeAdd:
            s.scannerRange += e.a;
            break;
        case EffectType::ScannerDetailAdd:
            s.scannerDetail += e.a;
            break;
        case EffectType::SaleValueMul:
            s.saleValue *= e.a;
            break;
        case EffectType::YieldMul:
            s.yieldMul *= e.a;
            break;
        case EffectType::ImpactDamageMul:
            s.impactDamage *= e.a;
            break;
        case EffectType::HazardDamageMul:
            s.hazardDamage *= e.a;
            break;
        case EffectType::RepairCostMul:
            s.repairCost *= e.a;
            break;
        case EffectType::RefuelCostMul:
            s.refuelCost *= e.a;
            break;
        case EffectType::RescueFeeMul:
            s.rescueFee *= e.a;
            break;
        case EffectType::ConvertHeatToFuel:
            s.heatToFuelThreshold = std::min(s.heatToFuelThreshold, e.a);
            s.heatToFuelRate += e.b;
            break;
        case EffectType::AutoCollectRadius:
            s.autoCollectRadius += e.a;
            break;
        case EffectType::HoverFuelMul:
            s.hoverFuel *= e.a;
            break;
        case EffectType::VolatileStabilityAdd:
            s.volatileStability += e.a;
            break;
        case EffectType::InsightGainMul:
            s.insightGain *= e.a;
            break;
        case EffectType::ContractRewardMul:
            s.contractReward *= e.a;
            break;
        case EffectType::ProtectedSampleSlots:
            s.protectedSampleSlots += static_cast<int>(e.a);
            break;
        case EffectType::WreckRecoveryAdd:
            s.wreckRecovery += e.a;
            break;
        case EffectType::DepthHeatMul:
            s.depthHeat *= e.a;
            break;
        case EffectType::CargoMassMul:
            s.cargoMass *= e.a;
            break;
        case EffectType::MineRadiusAdd:
            s.mineRadius += static_cast<int>(e.a);
            break;
        case EffectType::VeinRevealAdd:
            s.veinReveal += e.a;
            break;
        case EffectType::RefineYieldMul:
            s.refineYield *= e.a;
            break;
        case EffectType::GrantFlag:
            if (e.flag != RigFlag::Count) {
                s.flags.set(e.flag);
            }
            break;
        case EffectType::None:
        case EffectType::Count:
            break;
    }
}

void StatBlock::clamp_to_caps() {
    drillPower = clamp(drillPower, 0.05f, 60.0f);
    drillEfficiency = clamp(drillEfficiency, 0.25f, 4.0f);
    thrust = clamp(thrust, 0.25f, 6.0f);
    brake = clamp(brake, 0.25f, 6.0f);
    fuelCapacity = clamp(fuelCapacity, 10.0f, 4000.0f);
    cargoCapacity = clamp(cargoCapacity, 5.0f, 2000.0f);
    hullMax = clamp(hullMax, 10.0f, 4000.0f);
    cooling = clamp(cooling, 0.2f, 8.0f);
    heatGeneration = clamp(heatGeneration, 0.15f, 4.0f);
    heatCapacity = clamp(heatCapacity, 40.0f, 600.0f);
    scannerRange = clamp(scannerRange, 0.0f, 48.0f);
    scannerDetail = clamp(scannerDetail, 0.0f, 4.0f);
    saleValue = clamp(saleValue, 0.5f, 2.5f);
    yieldMul = clamp(yieldMul, 0.5f, 3.0f);
    impactDamage = clamp(impactDamage, 0.15f, 3.0f);
    hazardDamage = clamp(hazardDamage, 0.1f, 3.0f);
    repairCost = clamp(repairCost, 0.3f, 2.0f);
    refuelCost = clamp(refuelCost, 0.3f, 2.0f);
    rescueFee = clamp(rescueFee, 0.0f, 2.0f);
    heatToFuelRate = clamp(heatToFuelRate, 0.0f, 3.0f);
    autoCollectRadius = clamp(autoCollectRadius, 0.0f, 12.0f);
    hoverFuel = clamp(hoverFuel, 0.2f, 2.0f);
    volatileStability = clamp(volatileStability, 0.0f, 1.0f);
    insightGain = clamp(insightGain, 0.5f, 3.0f);
    contractReward = clamp(contractReward, 0.5f, 3.0f);
    wreckRecovery = clamp(wreckRecovery, 0.0f, 1.0f);
    depthHeat = clamp(depthHeat, 0.1f, 2.0f);
    cargoMass = clamp(cargoMass, 0.2f, 2.0f);
    refineYield = clamp(refineYield, 0.5f, 3.0f);
    protectedSampleSlots = clamp(protectedSampleSlots, 1, 12);
    mineRadius = clamp(mineRadius, 0, 2);
    veinReveal = clamp(veinReveal, 0.0f, 1.0f);
}

} // namespace df
