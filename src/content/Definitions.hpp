// Immutable content definitions. These are loaded once, validated, then never
// mutated. Runtime instances live in the domain layer (Section 15.2).
#pragma once

#include "content/Effects.hpp"
#include "core/StringId.hpp"
#include "core/Types.hpp"

#include <string>
#include <vector>

namespace df {

/// How a material behaves in the world and the economy (Section 10.1).
enum class MaterialCategory : u8 {
    Rock,       ///< Ordinary spoil; little or no value.
    Commodity,  ///< Sold for credits.
    Strategic,  ///< Refining ingredient / upgrade input.
    Volatile,   ///< High value, damages the rig if handled carelessly.
    Artifact,   ///< Research sample; uses protected storage.
    Structural, ///< Very hard; blocks routes, rarely worth mining.
    Boundary,   ///< Indestructible world edge.
    Fluid,      ///< Lava / gas emitter tiles.
    Empty,      ///< Air.
};

MaterialCategory material_category_from_string(std::string_view s);
std::string_view material_category_name(MaterialCategory c);

struct MaterialDefinition {
    StringId id;
    std::string nameKey;
    MaterialCategory category = MaterialCategory::Rock;
    float hardness = 1.0f; ///< Drives mine time; 0 for air.
    float density = 1.0f;  ///< Cargo mass per unit.
    i32 baseValue = 0;     ///< Credits per unit at the surface.
    i32 cargoUnits = 1;    ///< Cargo slots consumed per unit.
    i32 yieldMin = 1;
    i32 yieldMax = 1;
    bool solid = true;
    bool mineable = true;
    bool protectedSample = false; ///< Stored outside normal cargo.
    float volatileDamage = 0.0f;  ///< Hull damage when mined roughly.
    float heatOnMine = 0.0f;
    float lightEmission = 0.0f;
    std::vector<StringId> tags;
    // Placeholder presentation data - kept out of the simulation.
    u8 colorR = 128, colorG = 128, colorB = 128;
    u8 patternId = 0;
    std::string breakAudioId;

    /// Generation rules.
    std::vector<StringId> biomes;
    i32 minDepth = 0;
    i32 maxDepth = 1 << 30;
    i32 veinSizeMin = 1;
    i32 veinSizeMax = 1;
    i32 rarityWeightBp = 0;   ///< Basis points; probability weight in a field.
    i32 fieldPeriod = 24;     ///< Noise period controlling clumping.
    i32 fieldThresholdBp = 0; ///< Higher = rarer.
};

struct BiomeHazardEntry {
    StringId hazardId;
    i32 weightBp = 0;
};

struct BiomeDefinition {
    StringId id;
    std::string nameKey;
    i32 minDepth = 0;
    i32 maxDepth = 0;
    i32 transitionBand = 40; ///< Tiles over which the previous biome blends out.
    StringId baseMaterial;
    StringId softMaterial; ///< Used for cave linings and soft pockets.
    StringId structuralMaterial;
    i32 caveThresholdBp = 0; ///< Higher = more open space.
    i32 cavePeriod = 20;
    float ambientHeat = 0.0f; ///< Heat per second added at this depth.
    float hardnessScale = 1.0f;
    std::vector<BiomeHazardEntry> hazards;
    std::vector<StringId> landmarks;
    u8 skyR = 24, skyG = 22, skyB = 30;
    u8 fogR = 12, fogG = 11, fogB = 16;
    std::string musicId;
    std::string descriptionKey;
    /// Requirement the player must demonstrate before the gate opens.
    StringId gateResearch;
    i32 gateDepthProof = 0;
};

struct EquipmentTier {
    i32 tier = 0;
    i32 costCredits = 0;
    std::vector<std::pair<StringId, i32>> costComponents;
    std::string nameKey;
    std::string summaryKey;
    std::vector<Effect> effects;
};

struct EquipmentDefinition {
    StringId id; ///< e.g. equipment.drill_power
    std::string nameKey;
    std::string descriptionKey;
    std::string track; ///< Track key used by UI grouping.
    std::vector<EquipmentTier> tiers;
};

enum class ModuleSlot : u8 { Drill, Mobility, Survival, Survey, Utility, Count };

ModuleSlot module_slot_from_string(std::string_view s);
std::string_view module_slot_name(ModuleSlot s);

struct ModuleDefinition {
    StringId id;
    std::string nameKey;
    std::string descriptionKey;
    ModuleSlot slot = ModuleSlot::Utility;
    i32 powerCost = 1;
    i32 costCredits = 0;
    std::vector<std::pair<StringId, i32>> costComponents;
    StringId unlockResearch;
    StringId unlockFaction;
    i32 unlockReputation = 0;
    std::vector<Effect> effects;
    std::vector<Effect> tradeoffs;
    std::vector<StringId> tags;
};

enum class SkillBranch : u8 { Prospector, Engineer, Pathfinder, Count };

SkillBranch skill_branch_from_string(std::string_view s);
std::string_view skill_branch_name(SkillBranch b);

struct SkillNodeDefinition {
    StringId id;
    std::string nameKey;
    std::string descriptionKey;
    SkillBranch branch = SkillBranch::Prospector;
    i32 row = 0;
    i32 cost = 1;
    std::vector<StringId> prerequisites;
    StringId exclusiveGroup; ///< Only one node per group may be taken.
    bool keystone = false;
    std::vector<Effect> effects;
};

struct RecipeIngredient {
    StringId materialId;
    i32 amount = 1;
};

struct RecipeDefinition {
    StringId id;
    std::string nameKey;
    StringId facilityId;
    i32 facilityTier = 1;
    std::vector<RecipeIngredient> inputs;
    std::vector<RecipeIngredient> outputs;
    i32 creditCost = 0;
    StringId unlockResearch;
    std::vector<RecipeIngredient> byproducts;
};

struct FacilityTier {
    i32 tier = 0;
    i32 costCredits = 0;
    std::vector<RecipeIngredient> costComponents;
    std::string summaryKey;
    std::vector<Effect> effects;
    std::vector<StringId> unlocksRecipes;
};

struct FacilityDefinition {
    StringId id;
    std::string nameKey;
    std::string descriptionKey;
    std::vector<FacilityTier> tiers;
};

enum class ContractKind : u8 {
    DeliverMaterials,
    ReachDepth,
    ScanAnomaly,
    RecoverCache,
    ExtractVolatile,
    ActivateInfrastructure,
    RescueProspector,
    NoModuleClass,
    Count
};

ContractKind contract_kind_from_string(std::string_view s);
std::string_view contract_kind_name(ContractKind k);

struct ContractTemplate {
    StringId id;
    std::string nameKey;
    std::string descriptionKey;
    ContractKind kind = ContractKind::DeliverMaterials;
    StringId factionId;
    i32 minReputation = 0;
    i32 minDepthUnlocked = 0; ///< Only offered once the player reached this depth.
    StringId requiresResearch;
    std::vector<StringId> materialPool; ///< Valid targets for DeliverMaterials.
    i32 amountMin = 1;
    i32 amountMax = 1;
    i32 depthMin = 0;
    i32 depthMax = 0;
    i32 rewardCredits = 0;
    i32 rewardCreditsPerUnit = 0;
    i32 rewardInsight = 0;
    i32 rewardReputation = 0;
    std::vector<RecipeIngredient> rewardComponents;
    bool authored = false;
    bool repeatable = true;
};

enum class LandmarkKind : u8 {
    Cache,
    RepairStation,
    TransitNode,
    ThermalVent,
    CoolingPocket,
    CollapsedFacility,
    ArtifactChamber,
    ChallengeRoom,
    RescueBeacon,
    CrumblyLayer,
    Cavern,
    SetPiece,
    Count
};

LandmarkKind landmark_kind_from_string(std::string_view s);
std::string_view landmark_kind_name(LandmarkKind k);

struct LandmarkDefinition {
    StringId id;
    std::string nameKey;
    std::string descriptionKey;
    LandmarkKind kind = LandmarkKind::Cache;
    i32 minDepth = 0;
    i32 maxDepth = 1 << 30;
    i32 width = 7;
    i32 height = 5;
    i32 minSpacing = 220; ///< Minimum tile distance between two of these.
    i32 perRegionMin = 1; ///< Guaranteed count per region band.
    i32 perRegionMax = 2;
    i32 regionHeight = 250;
    std::vector<StringId> biomes;
    std::vector<StringId> lootMaterials;
    i32 lootAmountMin = 0;
    i32 lootAmountMax = 0;
    StringId grantsResearch;
    bool unique = false;
};

enum class HazardKind : u8 {
    LavaPocket,
    GasVent,
    UnstableCeiling,
    SporeCloud,
    Corrosion,
    ElectricArc,
    CrushingPiston,
    MagneticField,
    SentryTurret,
    ShardBurst,
    Count
};

HazardKind hazard_kind_from_string(std::string_view s);
std::string_view hazard_kind_name(HazardKind k);

struct HazardDefinition {
    StringId id;
    std::string nameKey;
    std::string descriptionKey;
    HazardKind kind = HazardKind::LavaPocket;
    float contactDamage = 4.0f;
    float areaDamage = 0.0f;
    float radius = 1.5f;
    float heatPerSecond = 0.0f;
    float telegraphSeconds = 0.6f; ///< Warning time before it can hurt you.
    i32 minDepth = 0;
    RigFlag counteredBy = RigFlag::Count;
    u8 colorR = 220, colorG = 80, colorB = 40;
};

struct ResearchDefinition {
    StringId id;
    std::string nameKey;
    std::string descriptionKey;
    i32 insightCost = 1;
    std::vector<StringId> prerequisites;
    StringId requiresSample; ///< Material that must be delivered.
    i32 requiresSampleAmount = 1;
    std::vector<StringId> unlocksModules;
    std::vector<StringId> unlocksRecipes;
    std::vector<StringId> unlocksBiomes;
    std::vector<Effect> effects;
    bool capstone = false;
};

struct FactionDefinition {
    StringId id;
    std::string nameKey;
    std::string descriptionKey;
    std::vector<i32> tierThresholds;
    std::vector<StringId> tierUnlocksModules;
};

/// Global balance constants, kept out of the source (Section 17.1).
struct EconomyConfig {
    /// Tier-0 rig capabilities. Everything else is an upgrade on top of these.
    float baseFuelCapacity = 100.0f;
    float baseCargoCapacity = 40.0f;
    float baseHull = 100.0f;
    float baseHeatCapacity = 100.0f;
    /// Free safety net applied on every return so a bad run cannot soft lock.
    float freeServiceFuelFraction = 0.35f;
    float freeServiceHullFraction = 0.25f;
    float baseMineSeconds = 0.55f;
    float minMineSeconds = 0.08f;
    float maxMineSeconds = 9.0f;
    float mineHardnessExponent = 0.85f;
    float baseDrillPower = 1.0f;
    float mineFuelBase = 0.055f;
    float mineFuelHardnessExponent = 0.60f;
    float thrustFuelPerSecond = 2.6f;
    float lateralFuelPerSecond = 0.55f;
    float idleFuelPerSecond = 0.02f;
    float gravity = 24.0f;
    float maxFallSpeed = 22.0f;
    float impactDamageThreshold = 13.0f;
    float impactDamagePerUnit = 2.4f;
    float heatDecayPerSecond = 7.0f;
    float heatDamagePerSecond = 6.0f;
    float repairCreditsPerHull = 1.1f;
    float refuelCreditsPerUnit = 0.65f;
    float rescueFeeBase = 60.0f;
    float rescueFeeDepthFactor = 0.16f;
    float rescueFeeCapFraction = 0.35f; ///< Never take more than this share of credits.
    float rescueCargoLossFraction = 0.55f;
    float wreckRecoveryFraction = 0.4f;
    float mineProgressDecayDelay = 2.5f;
    float mineProgressDecayRate = 0.35f;
    i32 startingCredits = 120;
    i32 firstUpgradeTargetCost = 240;
    float insightPerDiscovery = 1.0f;
    float sellPriceDepthBonusPer100m = 0.015f;
    float cargoMassAccelPenalty = 0.22f; ///< Max fraction of acceleration lost at full cargo.
};

} // namespace df
