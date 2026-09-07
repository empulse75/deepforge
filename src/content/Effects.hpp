// Typed effect registry shared by equipment tiers, modules, skills, research
// and facilities. One aggregation path means every source of a bonus is
// bounded by the same caps (Section 10.3).
#pragma once

#include "core/StringId.hpp"
#include "core/Types.hpp"

#include <array>
#include <string_view>

namespace df {

enum class EffectType : u16 {
    None = 0,
    DrillPowerMul,
    DrillPowerAdd,
    DrillEfficiencyMul, ///< Lower fuel per unit of drilling.
    ThrustMul,
    BrakeMul,
    FuelCapacityAdd,
    FuelCapacityMul,
    CargoCapacityAdd,
    CargoCapacityMul,
    HullMaxAdd,
    HullMaxMul,
    CoolingMul,
    HeatGenerationMul,
    HeatCapacityAdd,
    ScannerRangeAdd,
    ScannerDetailAdd,
    SaleValueMul,
    YieldMul,
    ImpactDamageMul,
    HazardDamageMul,
    RepairCostMul,
    RefuelCostMul,
    RescueFeeMul,
    ConvertHeatToFuel, ///< a = heat threshold (0..1), b = fuel per second.
    AutoCollectRadius,
    HoverFuelMul,
    VolatileStabilityAdd,
    InsightGainMul,
    ContractRewardMul,
    ProtectedSampleSlots,
    UtilityPowerAdd,
    WreckRecoveryAdd,
    DepthHeatMul,
    CargoMassMul,
    MineRadiusAdd, ///< Wide fracture heads break neighbouring soft tiles.
    VeinRevealAdd,
    RefineYieldMul,
    GrantFlag, ///< a is unused; `flag` names the capability.
    Count
};

/// Qualitative capabilities unlocked at milestone tiers or by modules.
enum class RigFlag : u32 {
    BrakeWithoutFuel = 0,
    SeeVeinDirection,
    BreakBrittleClusters,
    SurviveHeatSpike,
    AutoMarkValuables,
    ScanThroughRock,
    SporeImmunity,
    LavaShielding,
    MagneticStabiliser,
    FreeUnstuck,
    TransitAccess,
    GrappleAnchor,
    HoverAssist,
    PortableRefinery,
    DeployableBeacon,
    Count
};

std::string_view effect_type_name(EffectType t);
EffectType effect_type_from_string(std::string_view s);
std::string_view rig_flag_name(RigFlag f);
bool rig_flag_from_string(std::string_view s, RigFlag& out);

struct Effect {
    EffectType type = EffectType::None;
    float a = 0.0f;
    float b = 0.0f;
    float c = 0.0f;
    RigFlag flag = RigFlag::Count;
    StringId tag; ///< Optional scope, e.g. only applies to a material tag.
};

/// Bit set of rig capabilities.
class RigFlags {
public:
    constexpr void set(RigFlag f) noexcept { mBits |= (1u << static_cast<u32>(f)); }

    constexpr bool has(RigFlag f) const noexcept { return (mBits & (1u << static_cast<u32>(f))) != 0; }

    constexpr u32 bits() const noexcept { return mBits; }

    constexpr void set_bits(u32 b) noexcept { mBits = b; }

    friend constexpr bool operator==(const RigFlags&, const RigFlags&) = default;

private:
    u32 mBits = 0;
};

/// Fully aggregated rig statistics for one tick. Recomputed only when the
/// loadout changes, never per frame.
struct StatBlock {
    float drillPower = 1.0f;
    float drillEfficiency = 1.0f;
    float thrust = 1.0f;
    float brake = 1.0f;
    float fuelCapacity = 100.0f;
    float cargoCapacity = 30.0f;
    float hullMax = 100.0f;
    float cooling = 1.0f;
    float heatGeneration = 1.0f;
    float heatCapacity = 100.0f;
    float scannerRange = 0.0f;
    float scannerDetail = 0.0f;
    float saleValue = 1.0f;
    float yieldMul = 1.0f;
    float impactDamage = 1.0f;
    float hazardDamage = 1.0f;
    float repairCost = 1.0f;
    float refuelCost = 1.0f;
    float rescueFee = 1.0f;
    float heatToFuelThreshold = 2.0f; ///< >1 means inactive.
    float heatToFuelRate = 0.0f;
    float autoCollectRadius = 0.0f;
    float hoverFuel = 1.0f;
    float volatileStability = 0.0f;
    float insightGain = 1.0f;
    float contractReward = 1.0f;
    float wreckRecovery = 0.0f;
    float depthHeat = 1.0f;
    float cargoMass = 1.0f;
    float refineYield = 1.0f;
    int protectedSampleSlots = 1;
    int utilityPower = 0; ///< Added to the base module power budget.
    int mineRadius = 0;
    float veinReveal = 0.0f;
    RigFlags flags;

    /// Applies design caps so stacked multipliers cannot run away.
    void clamp_to_caps();
};

/// Applies one effect to an accumulating stat block.
void apply_effect(StatBlock& stats, const Effect& e);

} // namespace df
