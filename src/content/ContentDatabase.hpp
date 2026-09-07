// Owns every loaded definition table and resolves stable string ids to handles.
#pragma once

#include "content/Definitions.hpp"
#include "core/Result.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace df {

/// A validated, immutable snapshot of all game content.
class ContentDatabase {
public:
    /// Loads every data file from `dataDir`. All validation problems are
    /// reported together rather than one per run.
    static Result<ContentDatabase> load(const std::filesystem::path& dataDir, ErrorList& errors);

    /// Builds a database directly from in-memory JSON text; used by tests.
    static Result<ContentDatabase>
    load_from_strings(const std::unordered_map<std::string, std::string>& files, ErrorList& errors);

    const std::vector<MaterialDefinition>& materials() const noexcept { return mMaterials; }

    const std::vector<BiomeDefinition>& biomes() const noexcept { return mBiomes; }

    const std::vector<EquipmentDefinition>& equipment() const noexcept { return mEquipment; }

    const std::vector<ModuleDefinition>& modules() const noexcept { return mModules; }

    const std::vector<SkillNodeDefinition>& skills() const noexcept { return mSkills; }

    const std::vector<RecipeDefinition>& recipes() const noexcept { return mRecipes; }

    const std::vector<FacilityDefinition>& facilities() const noexcept { return mFacilities; }

    const std::vector<ContractTemplate>& contracts() const noexcept { return mContracts; }

    const std::vector<LandmarkDefinition>& landmarks() const noexcept { return mLandmarks; }

    const std::vector<HazardDefinition>& hazards() const noexcept { return mHazards; }

    const std::vector<ResearchDefinition>& research() const noexcept { return mResearch; }

    const std::vector<FactionDefinition>& factions() const noexcept { return mFactions; }

    const EconomyConfig& economy() const noexcept { return mEconomy; }

    // --- lookups ------------------------------------------------------------
    MaterialHandle material(std::string_view id) const;
    BiomeHandle biome(std::string_view id) const;
    EquipmentHandle equipment_track(std::string_view id) const;
    ModuleHandle module(std::string_view id) const;
    SkillHandle skill(std::string_view id) const;
    RecipeHandle recipe(std::string_view id) const;
    FacilityHandle facility(std::string_view id) const;
    ContractHandle contract(std::string_view id) const;
    LandmarkHandle landmark(std::string_view id) const;
    HazardHandle hazard(std::string_view id) const;
    ResearchHandle research_node(std::string_view id) const;
    i32 faction_index(std::string_view id) const;

    const MaterialDefinition& material_def(MaterialHandle h) const {
        return mMaterials[static_cast<usize>(h.index)];
    }

    const BiomeDefinition& biome_def(BiomeHandle h) const { return mBiomes[static_cast<usize>(h.index)]; }

    const ModuleDefinition& module_def(ModuleHandle h) const { return mModules[static_cast<usize>(h.index)]; }

    const SkillNodeDefinition& skill_def(SkillHandle h) const { return mSkills[static_cast<usize>(h.index)]; }

    const HazardDefinition& hazard_def(HazardHandle h) const { return mHazards[static_cast<usize>(h.index)]; }

    const LandmarkDefinition& landmark_def(LandmarkHandle h) const {
        return mLandmarks[static_cast<usize>(h.index)];
    }

    const ResearchDefinition& research_def(ResearchHandle h) const {
        return mResearch[static_cast<usize>(h.index)];
    }

    /// Convenience handles resolved once at load time.
    MaterialHandle airMaterial{};
    MaterialHandle boundaryMaterial{};

    /// Biome covering a given depth in tiles.
    BiomeHandle biome_at_depth(i64 depthTiles) const;

    int content_schema_version() const noexcept { return mSchemaVersion; }

    /// Player-facing text for a content key. Returns an empty view when the key
    /// has no entry, so callers can decide between a fallback and a hard error.
    std::string_view text(std::string_view key) const;

    /// Text for a key, or `fallback` when the table has no entry.
    std::string_view text_or(std::string_view key, std::string_view fallback) const {
        const auto s = text(key);
        return s.empty() ? fallback : s;
    }

    const std::unordered_map<std::string, std::string>& strings() const noexcept { return mStrings; }

private:
    friend class ContentLoader;

    std::vector<MaterialDefinition> mMaterials;
    std::vector<BiomeDefinition> mBiomes;
    std::vector<EquipmentDefinition> mEquipment;
    std::vector<ModuleDefinition> mModules;
    std::vector<SkillNodeDefinition> mSkills;
    std::vector<RecipeDefinition> mRecipes;
    std::vector<FacilityDefinition> mFacilities;
    std::vector<ContractTemplate> mContracts;
    std::vector<LandmarkDefinition> mLandmarks;
    std::vector<HazardDefinition> mHazards;
    std::vector<ResearchDefinition> mResearch;
    std::vector<FactionDefinition> mFactions;
    EconomyConfig mEconomy;
    std::unordered_map<std::string, std::string> mStrings;
    int mSchemaVersion = 0;

    std::unordered_map<std::string, i32> mMaterialIndex;
    std::unordered_map<std::string, i32> mBiomeIndex;
    std::unordered_map<std::string, i32> mEquipmentIndex;
    std::unordered_map<std::string, i32> mModuleIndex;
    std::unordered_map<std::string, i32> mSkillIndex;
    std::unordered_map<std::string, i32> mRecipeIndex;
    std::unordered_map<std::string, i32> mFacilityIndex;
    std::unordered_map<std::string, i32> mContractIndex;
    std::unordered_map<std::string, i32> mLandmarkIndex;
    std::unordered_map<std::string, i32> mHazardIndex;
    std::unordered_map<std::string, i32> mResearchIndex;
    std::unordered_map<std::string, i32> mFactionIndex;

    void build_indices();
};

} // namespace df
