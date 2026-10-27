#include "content/ContentDatabase.hpp"

#include "core/JsonFile.hpp"
#include "core/Log.hpp"
#include "core/Version.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>

namespace df {
namespace {

using json = nlohmann::json;

/// Field reader that accumulates context-rich errors instead of throwing.
struct Ctx {
    std::string file;
    std::string def;
    ErrorList* errors = nullptr;

    void err(std::string field, std::string message, std::string suggestion = {}) const {
        errors->add(Error{std::move(message), file, def, std::move(field), std::move(suggestion)});
    }
};

bool has(const json& j, const char* key) {
    return j.is_object() && j.contains(key) && !j.at(key).is_null();
}

std::string req_string(const json& j, const char* key, const Ctx& c) {
    if (!has(j, key) || !j.at(key).is_string()) {
        c.err(key, "missing or non-string required field", "add a string value for this field");
        return {};
    }
    return j.at(key).get<std::string>();
}

std::string opt_string(const json& j, const char* key, std::string fallback = {}) {
    if (has(j, key) && j.at(key).is_string()) {
        return j.at(key).get<std::string>();
    }
    return fallback;
}

i32 opt_int(const json& j, const char* key, i32 fallback, const Ctx* c = nullptr) {
    if (!has(j, key)) {
        return fallback;
    }
    const auto& v = j.at(key);
    if (!matches_type(v, FieldType::Integer)) {
        // QUAL-01: a value present with the wrong type used to be reported nowhere —
        // a float in an integer field silently truncated, a string silently fell back
        // to the default. A caller that can name the definition now does; the rest
        // keep the documented silent default they have always had.
        if (c != nullptr) {
            c->err(key,
                   std::string("field must be ") + field_type_label(FieldType::Integer) + ", but is " +
                       std::string(field_type_name(v)),
                   "use a whole number, or delete the field to take the default");
        }
        return fallback;
    }
    return v.get<i32>();
}

i32 req_int(const json& j, const char* key, const Ctx& c) {
    if (!has(j, key)) {
        c.err(key, "missing or non-numeric required field");
        return 0;
    }
    const auto& v = j.at(key);
    if (!matches_type(v, FieldType::Integer)) {
        c.err(key,
              std::string("field must be ") + field_type_label(FieldType::Integer) + ", but is " +
                  std::string(field_type_name(v)),
              "use a whole number for this field");
        return 0;
    }
    return j.at(key).get<i32>();
}

float opt_float(const json& j, const char* key, float fallback) {
    if (has(j, key) && j.at(key).is_number()) {
        return j.at(key).get<float>();
    }
    return fallback;
}

bool opt_bool(const json& j, const char* key, bool fallback) {
    if (has(j, key) && j.at(key).is_boolean()) {
        return j.at(key).get<bool>();
    }
    return fallback;
}

std::vector<StringId> opt_id_list(const json& j, const char* key) {
    std::vector<StringId> out;
    if (has(j, key) && j.at(key).is_array()) {
        for (const auto& e : j.at(key)) {
            if (e.is_string()) {
                out.emplace_back(e.get<std::string>());
            }
        }
    }
    return out;
}

void read_color(const json& j, const char* key, u8& r, u8& g, u8& b, const Ctx& c) {
    if (!has(j, key)) {
        return;
    }
    if (!j.at(key).is_array() || j.at(key).size() < 3) {
        c.err(key, "colour must be an array of three integers",
              "write it as [r, g, b], each one an integer 0-255");
        return;
    }
    // QUAL-01: the array check was here, the element checks were not — a quoted
    // channel aborted the process inside nlohmann.
    for (size_t i = 0; i < 3; ++i) {
        if (!matches_type(j.at(key)[i], FieldType::Integer)) {
            c.err(key,
                  std::string("colour element ") + std::to_string(i) + " must be an integer, but is " +
                      std::string(field_type_name(j.at(key)[i])),
                  "remove the quotes from the colour channels");
            return;
        }
    }
    r = static_cast<u8>(std::clamp(j.at(key)[0].get<int>(), 0, 255));
    g = static_cast<u8>(std::clamp(j.at(key)[1].get<int>(), 0, 255));
    b = static_cast<u8>(std::clamp(j.at(key)[2].get<int>(), 0, 255));
}

std::vector<Effect> read_effects(const json& j, const char* key, const Ctx& c) {
    std::vector<Effect> out;
    if (!has(j, key)) {
        return out;
    }
    if (!j.at(key).is_array()) {
        c.err(key, "effects must be an array");
        return out;
    }
    for (const auto& e : j.at(key)) {
        Effect fx;
        const std::string type = opt_string(e, "type");
        fx.type = effect_type_from_string(type);
        if (fx.type == EffectType::None) {
            c.err(key, "unknown effect type '" + type + "'",
                  "see docs/CONTENT_AUTHORING.md for the effect list");
            continue;
        }
        fx.a = opt_float(e, "value", opt_float(e, "a", 0.0f));
        fx.b = opt_float(e, "rate", opt_float(e, "b", 0.0f));
        fx.c = opt_float(e, "c", 0.0f);
        if (has(e, "threshold")) {
            fx.a = opt_float(e, "threshold", fx.a);
        }
        if (has(e, "flag")) {
            RigFlag flag{};
            const std::string flagName = opt_string(e, "flag");
            if (!rig_flag_from_string(flagName, flag)) {
                c.err(key, "unknown rig flag '" + flagName + "'");
                continue;
            }
            fx.flag = flag;
        }
        if (fx.type == EffectType::GrantFlag && fx.flag == RigFlag::Count) {
            c.err(key, "grant_flag effect requires a 'flag' field");
            continue;
        }
        if (has(e, "tag")) {
            fx.tag = StringId{opt_string(e, "tag")};
        }
        out.push_back(fx);
    }
    return out;
}

std::vector<RecipeIngredient> read_ingredients(const json& j, const char* key, const Ctx* c = nullptr) {
    std::vector<RecipeIngredient> out;
    if (has(j, key) && j.at(key).is_array()) {
        for (const auto& e : j.at(key)) {
            RecipeIngredient ing;
            ing.materialId = StringId{opt_string(e, "material", opt_string(e, "id"))};
            ing.amount = opt_int(e, "amount", 1, c);
            out.push_back(ing);
        }
    }
    return out;
}

std::vector<std::pair<StringId, i32>> read_component_costs(const json& j, const char* key,
                                                           const Ctx* c = nullptr) {
    std::vector<std::pair<StringId, i32>> out;
    for (const auto& ing : read_ingredients(j, key, c)) {
        out.emplace_back(ing.materialId, ing.amount);
    }
    return out;
}

Result<json> parse_json_with_context(const std::string& text, const std::string& fileLabel,
                                     ErrorList& errors) {
    auto parsed = df::parse_json(text, /*ignore_comments=*/true);
    if (!parsed) {
        errors.add(Error{"file is not valid JSON", fileLabel, {}, {}, "check for trailing commas or quotes"});
        return fail(Error{"invalid JSON", fileLabel, {}, {}, {}});
    }
    return parsed;
}

} // namespace

// ---------------------------------------------------------------------------
// Enum string mappings
// ---------------------------------------------------------------------------

MaterialCategory material_category_from_string(std::string_view s) {
    if (s == "commodity")
        return MaterialCategory::Commodity;
    if (s == "strategic")
        return MaterialCategory::Strategic;
    if (s == "volatile")
        return MaterialCategory::Volatile;
    if (s == "artifact")
        return MaterialCategory::Artifact;
    if (s == "structural")
        return MaterialCategory::Structural;
    if (s == "boundary")
        return MaterialCategory::Boundary;
    if (s == "fluid")
        return MaterialCategory::Fluid;
    if (s == "empty")
        return MaterialCategory::Empty;
    return MaterialCategory::Rock;
}

std::string_view material_category_name(MaterialCategory c) {
    switch (c) {
        case MaterialCategory::Rock:
            return "rock";
        case MaterialCategory::Commodity:
            return "commodity";
        case MaterialCategory::Strategic:
            return "strategic";
        case MaterialCategory::Volatile:
            return "volatile";
        case MaterialCategory::Artifact:
            return "artifact";
        case MaterialCategory::Structural:
            return "structural";
        case MaterialCategory::Boundary:
            return "boundary";
        case MaterialCategory::Fluid:
            return "fluid";
        case MaterialCategory::Empty:
            return "empty";
    }
    return "rock";
}

ModuleSlot module_slot_from_string(std::string_view s) {
    if (s == "drill")
        return ModuleSlot::Drill;
    if (s == "mobility")
        return ModuleSlot::Mobility;
    if (s == "survival")
        return ModuleSlot::Survival;
    if (s == "survey")
        return ModuleSlot::Survey;
    return ModuleSlot::Utility;
}

std::string_view module_slot_name(ModuleSlot s) {
    switch (s) {
        case ModuleSlot::Drill:
            return "drill";
        case ModuleSlot::Mobility:
            return "mobility";
        case ModuleSlot::Survival:
            return "survival";
        case ModuleSlot::Survey:
            return "survey";
        case ModuleSlot::Utility:
            return "utility";
        case ModuleSlot::Count:
            break;
    }
    return "utility";
}

SkillBranch skill_branch_from_string(std::string_view s) {
    if (s == "engineer")
        return SkillBranch::Engineer;
    if (s == "pathfinder")
        return SkillBranch::Pathfinder;
    return SkillBranch::Prospector;
}

std::string_view skill_branch_name(SkillBranch b) {
    switch (b) {
        case SkillBranch::Prospector:
            return "prospector";
        case SkillBranch::Engineer:
            return "engineer";
        case SkillBranch::Pathfinder:
            return "pathfinder";
        case SkillBranch::Count:
            break;
    }
    return "prospector";
}

ContractKind contract_kind_from_string(std::string_view s) {
    if (s == "reach_depth")
        return ContractKind::ReachDepth;
    if (s == "scan_anomaly")
        return ContractKind::ScanAnomaly;
    if (s == "recover_cache")
        return ContractKind::RecoverCache;
    if (s == "extract_volatile")
        return ContractKind::ExtractVolatile;
    if (s == "activate_infrastructure")
        return ContractKind::ActivateInfrastructure;
    if (s == "rescue_prospector")
        return ContractKind::RescueProspector;
    if (s == "no_module_class")
        return ContractKind::NoModuleClass;
    return ContractKind::DeliverMaterials;
}

std::string_view contract_kind_name(ContractKind k) {
    switch (k) {
        case ContractKind::DeliverMaterials:
            return "deliver_materials";
        case ContractKind::ReachDepth:
            return "reach_depth";
        case ContractKind::ScanAnomaly:
            return "scan_anomaly";
        case ContractKind::RecoverCache:
            return "recover_cache";
        case ContractKind::ExtractVolatile:
            return "extract_volatile";
        case ContractKind::ActivateInfrastructure:
            return "activate_infrastructure";
        case ContractKind::RescueProspector:
            return "rescue_prospector";
        case ContractKind::NoModuleClass:
            return "no_module_class";
        case ContractKind::Count:
            break;
    }
    return "deliver_materials";
}

LandmarkKind landmark_kind_from_string(std::string_view s) {
    if (s == "repair_station")
        return LandmarkKind::RepairStation;
    if (s == "transit_node")
        return LandmarkKind::TransitNode;
    if (s == "thermal_vent")
        return LandmarkKind::ThermalVent;
    if (s == "cooling_pocket")
        return LandmarkKind::CoolingPocket;
    if (s == "collapsed_facility")
        return LandmarkKind::CollapsedFacility;
    if (s == "artifact_chamber")
        return LandmarkKind::ArtifactChamber;
    if (s == "challenge_room")
        return LandmarkKind::ChallengeRoom;
    if (s == "rescue_beacon")
        return LandmarkKind::RescueBeacon;
    if (s == "crumbly_layer")
        return LandmarkKind::CrumblyLayer;
    if (s == "cavern")
        return LandmarkKind::Cavern;
    if (s == "set_piece")
        return LandmarkKind::SetPiece;
    return LandmarkKind::Cache;
}

std::string_view landmark_kind_name(LandmarkKind k) {
    switch (k) {
        case LandmarkKind::Cache:
            return "cache";
        case LandmarkKind::RepairStation:
            return "repair_station";
        case LandmarkKind::TransitNode:
            return "transit_node";
        case LandmarkKind::ThermalVent:
            return "thermal_vent";
        case LandmarkKind::CoolingPocket:
            return "cooling_pocket";
        case LandmarkKind::CollapsedFacility:
            return "collapsed_facility";
        case LandmarkKind::ArtifactChamber:
            return "artifact_chamber";
        case LandmarkKind::ChallengeRoom:
            return "challenge_room";
        case LandmarkKind::RescueBeacon:
            return "rescue_beacon";
        case LandmarkKind::CrumblyLayer:
            return "crumbly_layer";
        case LandmarkKind::Cavern:
            return "cavern";
        case LandmarkKind::SetPiece:
            return "set_piece";
        case LandmarkKind::Count:
            break;
    }
    return "cache";
}

HazardKind hazard_kind_from_string(std::string_view s) {
    if (s == "gas_vent")
        return HazardKind::GasVent;
    if (s == "unstable_ceiling")
        return HazardKind::UnstableCeiling;
    if (s == "spore_cloud")
        return HazardKind::SporeCloud;
    if (s == "corrosion")
        return HazardKind::Corrosion;
    if (s == "electric_arc")
        return HazardKind::ElectricArc;
    if (s == "crushing_piston")
        return HazardKind::CrushingPiston;
    if (s == "magnetic_field")
        return HazardKind::MagneticField;
    if (s == "sentry_turret")
        return HazardKind::SentryTurret;
    if (s == "shard_burst")
        return HazardKind::ShardBurst;
    return HazardKind::LavaPocket;
}

std::string_view hazard_kind_name(HazardKind k) {
    switch (k) {
        case HazardKind::LavaPocket:
            return "lava_pocket";
        case HazardKind::GasVent:
            return "gas_vent";
        case HazardKind::UnstableCeiling:
            return "unstable_ceiling";
        case HazardKind::SporeCloud:
            return "spore_cloud";
        case HazardKind::Corrosion:
            return "corrosion";
        case HazardKind::ElectricArc:
            return "electric_arc";
        case HazardKind::CrushingPiston:
            return "crushing_piston";
        case HazardKind::MagneticField:
            return "magnetic_field";
        case HazardKind::SentryTurret:
            return "sentry_turret";
        case HazardKind::ShardBurst:
            return "shard_burst";
        case HazardKind::Count:
            break;
    }
    return "lava_pocket";
}

// ---------------------------------------------------------------------------
// Loader
// ---------------------------------------------------------------------------

class ContentLoader {
public:
    ContentLoader(ContentDatabase& db, ErrorList& errors) : mDb(db), mErrors(errors) {}

    void load_materials(const json& root, const std::string& file);
    void load_biomes(const json& root, const std::string& file);
    void load_equipment(const json& root, const std::string& file);
    void load_modules(const json& root, const std::string& file);
    void load_skills(const json& root, const std::string& file);
    void load_recipes(const json& root, const std::string& file);
    void load_facilities(const json& root, const std::string& file);
    void load_contracts(const json& root, const std::string& file);
    void load_landmarks(const json& root, const std::string& file);
    void load_hazards(const json& root, const std::string& file);
    void load_research(const json& root, const std::string& file);
    void load_factions(const json& root, const std::string& file);
    void load_economy(const json& root, const std::string& file);
    void load_strings(const json& root, const std::string& file);
    void validate();

    void note_schema_version(int v) { mDb.mSchemaVersion = std::max(mDb.mSchemaVersion, v); }

    void set_air_and_boundary() {
        mDb.airMaterial = mDb.material("material.air");
        mDb.boundaryMaterial = mDb.material("material.bedrock");
    }

private:
    template <typename T>
    void check_duplicate(const std::vector<T>& list, const StringId& id, const Ctx& c) {
        for (const auto& e : list) {
            if (e.id == id) {
                c.err("id", "duplicate definition id '" + id.text() + "'",
                      "ids must be unique within their table");
                return;
            }
        }
    }

    void require_material(const StringId& id, const Ctx& c, const char* field);
    void require_biome(const StringId& id, const Ctx& c, const char* field);

    ContentDatabase& mDb;
    ErrorList& mErrors;
};

void ContentLoader::require_material(const StringId& id, const Ctx& c, const char* field) {
    if (id.empty()) {
        return;
    }
    if (!mDb.material(id.text()).valid()) {
        c.err(field, "unresolved material reference '" + id.text() + "'",
              "check data/materials.json for the correct id");
    }
}

void ContentLoader::require_biome(const StringId& id, const Ctx& c, const char* field) {
    if (id.empty()) {
        return;
    }
    if (!mDb.biome(id.text()).valid()) {
        c.err(field, "unresolved biome reference '" + id.text() + "'");
    }
}

void ContentLoader::load_materials(const json& root, const std::string& file) {
    if (!root.contains("materials") || !root["materials"].is_array()) {
        mErrors.add(Error{"expected a 'materials' array", file, {}, {}, {}});
        return;
    }
    for (const auto& j : root["materials"]) {
        Ctx c{file, opt_string(j, "id"), &mErrors};
        MaterialDefinition m;
        m.id = StringId{req_string(j, "id", c)};
        if (m.id.empty()) {
            continue;
        }
        check_duplicate(mDb.mMaterials, m.id, c);
        m.nameKey = opt_string(j, "name_key", m.id.text() + ".name");
        m.descriptionKey = opt_string(j, "description_key", m.id.text() + ".description");
        m.category = material_category_from_string(opt_string(j, "category", "rock"));
        m.hardness = opt_float(j, "hardness", 1.0f);
        m.density = opt_float(j, "density", 1.0f);
        m.baseValue = opt_int(j, "base_value", 0, &c);
        m.cargoUnits = opt_int(j, "cargo_units", 1, &c);
        m.solid = opt_bool(j, "solid", m.category != MaterialCategory::Empty);
        m.mineable = opt_bool(
            j, "mineable", m.category != MaterialCategory::Boundary && m.category != MaterialCategory::Empty);
        m.protectedSample = opt_bool(j, "protected_sample", m.category == MaterialCategory::Artifact);
        m.volatileDamage = opt_float(j, "volatile_damage", 0.0f);
        m.heatOnMine = opt_float(j, "heat_on_mine", 0.0f);
        m.lightEmission = opt_float(j, "light_emission", 0.0f);
        m.tags = opt_id_list(j, "tags");
        m.breakAudioId = opt_string(j, "break_audio_id", "audio.rock_break");
        m.patternId = static_cast<u8>(opt_int(j, "pattern_id", 0, &c));
        read_color(j, "color", m.colorR, m.colorG, m.colorB, c);

        if (has(j, "yield")) {
            m.yieldMin = opt_int(j["yield"], "min", 1, &c);
            m.yieldMax = opt_int(j["yield"], "max", m.yieldMin, &c);
        }
        if (has(j, "generation")) {
            const auto& g = j["generation"];
            m.biomes = opt_id_list(g, "biomes");
            m.minDepth = opt_int(g, "min_depth", 0, &c);
            m.maxDepth = opt_int(g, "max_depth", 1 << 30, &c);
            if (has(g, "vein_size") && g["vein_size"].is_array() && g["vein_size"].size() >= 2) {
                m.veinSizeMin = g["vein_size"][0].get<i32>();
                m.veinSizeMax = g["vein_size"][1].get<i32>();
            }
            m.rarityWeightBp = opt_int(g, "rarity_weight_bp", 0, &c);
            m.fieldPeriod = opt_int(g, "field_period", 24, &c);
            m.fieldThresholdBp = opt_int(g, "field_threshold_bp", 0, &c);
        }

        if (m.hardness < 0.0f) {
            c.err("hardness", "hardness must not be negative");
        }
        if (m.baseValue < 0) {
            c.err("base_value", "value must not be negative");
        }
        if (m.cargoUnits < 0) {
            c.err("cargo_units", "cargo units must not be negative");
        }
        if (m.yieldMax < m.yieldMin) {
            c.err("yield", "yield.max is lower than yield.min");
        }
        if (m.veinSizeMax < m.veinSizeMin) {
            c.err("generation.vein_size", "vein size max is lower than min");
        }
        if (m.maxDepth < m.minDepth) {
            c.err("generation", "max_depth is above min_depth");
        }
        mDb.mMaterials.push_back(std::move(m));
    }
    mDb.build_indices();
}

void ContentLoader::load_biomes(const json& root, const std::string& file) {
    if (!root.contains("biomes") || !root["biomes"].is_array()) {
        mErrors.add(Error{"expected a 'biomes' array", file, {}, {}, {}});
        return;
    }
    for (const auto& j : root["biomes"]) {
        Ctx c{file, opt_string(j, "id"), &mErrors};
        BiomeDefinition b;
        b.id = StringId{req_string(j, "id", c)};
        if (b.id.empty()) {
            continue;
        }
        check_duplicate(mDb.mBiomes, b.id, c);
        b.nameKey = opt_string(j, "name_key", b.id.text() + ".name");
        b.descriptionKey = opt_string(j, "description_key", b.id.text() + ".description");
        b.minDepth = req_int(j, "min_depth", c);
        b.maxDepth = req_int(j, "max_depth", c);
        b.transitionBand = opt_int(j, "transition_band", 40, &c);
        b.baseMaterial = StringId{req_string(j, "base_material", c)};
        b.softMaterial = StringId{opt_string(j, "soft_material", b.baseMaterial.text())};
        b.structuralMaterial = StringId{opt_string(j, "structural_material", b.baseMaterial.text())};
        b.caveThresholdBp = opt_int(j, "cave_threshold_bp", 3000, &c);
        b.cavePeriod = opt_int(j, "cave_period", 20, &c);
        b.ambientHeat = opt_float(j, "ambient_heat", 0.0f);
        b.hardnessScale = opt_float(j, "hardness_scale", 1.0f);
        b.landmarks = opt_id_list(j, "landmarks");
        b.musicId = opt_string(j, "music_id");
        b.gateResearch = StringId{opt_string(j, "gate_research")};
        b.gateDepthProof = opt_int(j, "gate_depth_proof", 0, &c);
        read_color(j, "sky_color", b.skyR, b.skyG, b.skyB, c);
        read_color(j, "fog_color", b.fogR, b.fogG, b.fogB, c);
        if (has(j, "hazards") && j["hazards"].is_array()) {
            for (const auto& h : j["hazards"]) {
                BiomeHazardEntry e;
                e.hazardId = StringId{opt_string(h, "id")};
                e.weightBp = opt_int(h, "weight_bp", 0, &c);
                b.hazards.push_back(e);
            }
        }
        if (b.maxDepth <= b.minDepth) {
            c.err("max_depth", "biome depth range is empty");
        }
        if (b.cavePeriod < 2) {
            c.err("cave_period", "cave period must be at least 2");
        }
        mDb.mBiomes.push_back(std::move(b));
    }
    std::sort(mDb.mBiomes.begin(), mDb.mBiomes.end(),
              [](const BiomeDefinition& a, const BiomeDefinition& b) { return a.minDepth < b.minDepth; });
    mDb.build_indices();
}

void ContentLoader::load_equipment(const json& root, const std::string& file) {
    if (!root.contains("equipment") || !root["equipment"].is_array()) {
        mErrors.add(Error{"expected an 'equipment' array", file, {}, {}, {}});
        return;
    }
    for (const auto& j : root["equipment"]) {
        Ctx c{file, opt_string(j, "id"), &mErrors};
        EquipmentDefinition e;
        e.id = StringId{req_string(j, "id", c)};
        if (e.id.empty()) {
            continue;
        }
        check_duplicate(mDb.mEquipment, e.id, c);
        e.nameKey = opt_string(j, "name_key", e.id.text() + ".name");
        e.descriptionKey = opt_string(j, "description_key", e.id.text() + ".description");
        e.track = opt_string(j, "track", e.id.text());
        if (has(j, "tiers") && j["tiers"].is_array()) {
            i32 expected = 0;
            for (const auto& t : j["tiers"]) {
                EquipmentTier tier;
                tier.tier = opt_int(t, "tier", expected, &c);
                tier.costCredits = opt_int(t, "cost_credits", 0, &c);
                tier.costComponents = read_component_costs(t, "cost_components", &c);
                tier.nameKey = opt_string(t, "name_key", e.id.text() + ".tier" + std::to_string(tier.tier));
                tier.summaryKey = opt_string(t, "summary_key");
                tier.effects = read_effects(t, "effects", c);
                if (tier.tier != expected) {
                    c.err("tiers", "tier numbers must be consecutive starting at 0");
                }
                if (tier.costCredits < 0) {
                    c.err("tiers.cost_credits", "cost must not be negative");
                }
                ++expected;
                e.tiers.push_back(std::move(tier));
            }
        }
        if (e.tiers.size() < 2) {
            c.err("tiers", "an equipment track needs at least a base tier and one upgrade");
        }
        mDb.mEquipment.push_back(std::move(e));
    }
    mDb.build_indices();
}

void ContentLoader::load_modules(const json& root, const std::string& file) {
    if (!root.contains("modules") || !root["modules"].is_array()) {
        mErrors.add(Error{"expected a 'modules' array", file, {}, {}, {}});
        return;
    }
    for (const auto& j : root["modules"]) {
        Ctx c{file, opt_string(j, "id"), &mErrors};
        ModuleDefinition m;
        m.id = StringId{req_string(j, "id", c)};
        if (m.id.empty()) {
            continue;
        }
        check_duplicate(mDb.mModules, m.id, c);
        m.nameKey = opt_string(j, "name_key", m.id.text() + ".name");
        m.descriptionKey = opt_string(j, "description_key", m.id.text() + ".description");
        m.slot = module_slot_from_string(opt_string(j, "slot", "utility"));
        m.powerCost = opt_int(j, "power_cost", 1, &c);
        m.costCredits = opt_int(j, "cost_credits", 0, &c);
        m.costComponents = read_component_costs(j, "cost_components", &c);
        m.unlockResearch = StringId{opt_string(j, "unlock")};
        m.unlockFaction = StringId{opt_string(j, "unlock_faction")};
        m.unlockReputation = opt_int(j, "unlock_reputation", 0, &c);
        m.effects = read_effects(j, "effects", c);
        m.tradeoffs = read_effects(j, "tradeoffs", c);
        m.tags = opt_id_list(j, "tags");
        if (m.powerCost < 0) {
            c.err("power_cost", "power cost must not be negative");
        }
        if (m.effects.empty()) {
            c.err("effects", "a module with no effects cannot create a decision",
                  "give it an effect or remove the definition");
        }
        mDb.mModules.push_back(std::move(m));
    }
    mDb.build_indices();
}

void ContentLoader::load_skills(const json& root, const std::string& file) {
    if (!root.contains("skills") || !root["skills"].is_array()) {
        mErrors.add(Error{"expected a 'skills' array", file, {}, {}, {}});
        return;
    }
    for (const auto& j : root["skills"]) {
        Ctx c{file, opt_string(j, "id"), &mErrors};
        SkillNodeDefinition s;
        s.id = StringId{req_string(j, "id", c)};
        if (s.id.empty()) {
            continue;
        }
        check_duplicate(mDb.mSkills, s.id, c);
        s.nameKey = opt_string(j, "name_key", s.id.text() + ".name");
        s.descriptionKey = opt_string(j, "description_key", s.id.text() + ".description");
        s.branch = skill_branch_from_string(opt_string(j, "branch", "prospector"));
        s.row = opt_int(j, "row", 0, &c);
        s.cost = opt_int(j, "cost", 1, &c);
        s.prerequisites = opt_id_list(j, "prerequisites");
        s.exclusiveGroup = StringId{opt_string(j, "exclusive_group")};
        s.keystone = opt_bool(j, "keystone", false);
        s.effects = read_effects(j, "effects", c);
        if (s.cost <= 0) {
            c.err("cost", "skill cost must be positive");
        }
        mDb.mSkills.push_back(std::move(s));
    }
    mDb.build_indices();
}

void ContentLoader::load_recipes(const json& root, const std::string& file) {
    if (!root.contains("recipes") || !root["recipes"].is_array()) {
        mErrors.add(Error{"expected a 'recipes' array", file, {}, {}, {}});
        return;
    }
    for (const auto& j : root["recipes"]) {
        Ctx c{file, opt_string(j, "id"), &mErrors};
        RecipeDefinition r;
        r.id = StringId{req_string(j, "id", c)};
        if (r.id.empty()) {
            continue;
        }
        check_duplicate(mDb.mRecipes, r.id, c);
        r.nameKey = opt_string(j, "name_key", r.id.text() + ".name");
        r.descriptionKey = opt_string(j, "description_key", r.id.text() + ".description");
        r.facilityId = StringId{opt_string(j, "facility", "facility.refinery")};
        r.facilityTier = opt_int(j, "facility_tier", 1, &c);
        r.inputs = read_ingredients(j, "inputs", &c);
        r.outputs = read_ingredients(j, "outputs", &c);
        r.byproducts = read_ingredients(j, "byproducts", &c);
        r.creditCost = opt_int(j, "credit_cost", 0, &c);
        r.unlockResearch = StringId{opt_string(j, "unlock")};
        if (r.inputs.empty()) {
            c.err("inputs", "a recipe with no inputs would create free value", "add at least one input");
        }
        if (r.outputs.empty()) {
            c.err("outputs", "a recipe must produce something");
        }
        mDb.mRecipes.push_back(std::move(r));
    }
    mDb.build_indices();
}

void ContentLoader::load_facilities(const json& root, const std::string& file) {
    if (!root.contains("facilities") || !root["facilities"].is_array()) {
        mErrors.add(Error{"expected a 'facilities' array", file, {}, {}, {}});
        return;
    }
    for (const auto& j : root["facilities"]) {
        Ctx c{file, opt_string(j, "id"), &mErrors};
        FacilityDefinition f;
        f.id = StringId{req_string(j, "id", c)};
        if (f.id.empty()) {
            continue;
        }
        check_duplicate(mDb.mFacilities, f.id, c);
        f.nameKey = opt_string(j, "name_key", f.id.text() + ".name");
        f.descriptionKey = opt_string(j, "description_key", f.id.text() + ".description");
        if (has(j, "tiers") && j["tiers"].is_array()) {
            i32 expected = 0;
            for (const auto& t : j["tiers"]) {
                FacilityTier tier;
                tier.tier = opt_int(t, "tier", expected, &c);
                tier.costCredits = opt_int(t, "cost_credits", 0, &c);
                tier.costComponents = read_ingredients(t, "cost_components", &c);
                tier.summaryKey = opt_string(t, "summary_key");
                tier.effects = read_effects(t, "effects", c);
                tier.unlocksRecipes = opt_id_list(t, "unlocks_recipes");
                if (tier.tier != expected) {
                    c.err("tiers", "facility tiers must be consecutive from 0");
                }
                ++expected;
                f.tiers.push_back(std::move(tier));
            }
        }
        if (f.tiers.empty()) {
            c.err("tiers", "facility needs at least one tier");
        }
        mDb.mFacilities.push_back(std::move(f));
    }
    mDb.build_indices();
}

void ContentLoader::load_contracts(const json& root, const std::string& file) {
    if (!root.contains("contracts") || !root["contracts"].is_array()) {
        mErrors.add(Error{"expected a 'contracts' array", file, {}, {}, {}});
        return;
    }
    for (const auto& j : root["contracts"]) {
        Ctx c{file, opt_string(j, "id"), &mErrors};
        ContractTemplate t;
        t.id = StringId{req_string(j, "id", c)};
        if (t.id.empty()) {
            continue;
        }
        check_duplicate(mDb.mContracts, t.id, c);
        t.nameKey = opt_string(j, "name_key", t.id.text() + ".name");
        t.descriptionKey = opt_string(j, "description_key", t.id.text() + ".description");
        t.kind = contract_kind_from_string(opt_string(j, "kind", "deliver_materials"));
        t.factionId = StringId{opt_string(j, "faction")};
        t.minReputation = opt_int(j, "min_reputation", 0, &c);
        t.minDepthUnlocked = opt_int(j, "min_depth_unlocked", 0, &c);
        t.requiresResearch = StringId{opt_string(j, "requires_research")};
        t.materialPool = opt_id_list(j, "material_pool");
        t.amountMin = opt_int(j, "amount_min", 1, &c);
        t.amountMax = opt_int(j, "amount_max", t.amountMin, &c);
        t.depthMin = opt_int(j, "depth_min", 0, &c);
        t.depthMax = opt_int(j, "depth_max", t.depthMin, &c);
        t.rewardCredits = opt_int(j, "reward_credits", 0, &c);
        t.rewardCreditsPerUnit = opt_int(j, "reward_credits_per_unit", 0, &c);
        t.rewardInsight = opt_int(j, "reward_insight", 0, &c);
        t.rewardReputation = opt_int(j, "reward_reputation", 0, &c);
        t.rewardComponents = read_ingredients(j, "reward_components", &c);
        t.authored = opt_bool(j, "authored", false);
        t.repeatable = opt_bool(j, "repeatable", !t.authored);
        if (t.amountMax < t.amountMin) {
            c.err("amount_max", "amount_max is below amount_min");
        }
        if (t.kind == ContractKind::DeliverMaterials && t.materialPool.empty()) {
            c.err("material_pool", "a delivery contract needs at least one candidate material",
                  "contracts must never request unavailable content");
        }
        if (t.rewardCredits == 0 && t.rewardCreditsPerUnit == 0 && t.rewardInsight == 0 &&
            t.rewardComponents.empty() && t.rewardReputation == 0) {
            c.err("reward", "contract has no reward");
        }
        mDb.mContracts.push_back(std::move(t));
    }
    mDb.build_indices();
}

void ContentLoader::load_landmarks(const json& root, const std::string& file) {
    if (!root.contains("landmarks") || !root["landmarks"].is_array()) {
        mErrors.add(Error{"expected a 'landmarks' array", file, {}, {}, {}});
        return;
    }
    for (const auto& j : root["landmarks"]) {
        Ctx c{file, opt_string(j, "id"), &mErrors};
        LandmarkDefinition l;
        l.id = StringId{req_string(j, "id", c)};
        if (l.id.empty()) {
            continue;
        }
        check_duplicate(mDb.mLandmarks, l.id, c);
        l.nameKey = opt_string(j, "name_key", l.id.text() + ".name");
        l.descriptionKey = opt_string(j, "description_key", l.id.text() + ".description");
        l.kind = landmark_kind_from_string(opt_string(j, "kind", "cache"));
        l.minDepth = opt_int(j, "min_depth", 0, &c);
        l.maxDepth = opt_int(j, "max_depth", 1 << 30, &c);
        l.width = opt_int(j, "width", 7, &c);
        l.height = opt_int(j, "height", 5, &c);
        l.minSpacing = opt_int(j, "min_spacing", 220, &c);
        l.perRegionMin = opt_int(j, "per_region_min", 1, &c);
        l.perRegionMax = opt_int(j, "per_region_max", l.perRegionMin, &c);
        l.regionHeight = opt_int(j, "region_height", 250, &c);
        l.biomes = opt_id_list(j, "biomes");
        l.lootMaterials = opt_id_list(j, "loot_materials");
        l.lootAmountMin = opt_int(j, "loot_amount_min", 0, &c);
        l.lootAmountMax = opt_int(j, "loot_amount_max", l.lootAmountMin, &c);
        l.grantsResearch = StringId{opt_string(j, "grants_research")};
        l.unique = opt_bool(j, "unique", false);
        if (l.width < 1 || l.height < 1) {
            c.err("size", "landmark size must be positive");
        }
        if (l.regionHeight < 16) {
            c.err("region_height", "region height must be at least 16 tiles");
        }
        if (l.perRegionMax < l.perRegionMin) {
            c.err("per_region_max", "per_region_max is below per_region_min");
        }
        mDb.mLandmarks.push_back(std::move(l));
    }
    mDb.build_indices();
}

void ContentLoader::load_hazards(const json& root, const std::string& file) {
    if (!root.contains("hazards") || !root["hazards"].is_array()) {
        mErrors.add(Error{"expected a 'hazards' array", file, {}, {}, {}});
        return;
    }
    for (const auto& j : root["hazards"]) {
        Ctx c{file, opt_string(j, "id"), &mErrors};
        HazardDefinition h;
        h.id = StringId{req_string(j, "id", c)};
        if (h.id.empty()) {
            continue;
        }
        check_duplicate(mDb.mHazards, h.id, c);
        h.nameKey = opt_string(j, "name_key", h.id.text() + ".name");
        h.descriptionKey = opt_string(j, "description_key", h.id.text() + ".description");
        h.kind = hazard_kind_from_string(opt_string(j, "kind", "lava_pocket"));
        h.contactDamage = opt_float(j, "contact_damage", 4.0f);
        h.areaDamage = opt_float(j, "area_damage", 0.0f);
        h.radius = opt_float(j, "radius", 1.5f);
        h.heatPerSecond = opt_float(j, "heat_per_second", 0.0f);
        h.telegraphSeconds = opt_float(j, "telegraph_seconds", 0.6f);
        h.minDepth = opt_int(j, "min_depth", 0, &c);
        read_color(j, "color", h.colorR, h.colorG, h.colorB, c);
        if (has(j, "countered_by")) {
            RigFlag flag{};
            if (rig_flag_from_string(opt_string(j, "countered_by"), flag)) {
                h.counteredBy = flag;
            } else {
                c.err("countered_by", "unknown rig flag");
            }
        }
        if (h.telegraphSeconds < 0.0f) {
            c.err("telegraph_seconds", "telegraph must not be negative");
        }
        mDb.mHazards.push_back(std::move(h));
    }
    mDb.build_indices();
}

void ContentLoader::load_research(const json& root, const std::string& file) {
    if (!root.contains("research") || !root["research"].is_array()) {
        mErrors.add(Error{"expected a 'research' array", file, {}, {}, {}});
        return;
    }
    for (const auto& j : root["research"]) {
        Ctx c{file, opt_string(j, "id"), &mErrors};
        ResearchDefinition r;
        r.id = StringId{req_string(j, "id", c)};
        if (r.id.empty()) {
            continue;
        }
        check_duplicate(mDb.mResearch, r.id, c);
        r.nameKey = opt_string(j, "name_key", r.id.text() + ".name");
        r.descriptionKey = opt_string(j, "description_key", r.id.text() + ".description");
        r.insightCost = opt_int(j, "insight_cost", 1, &c);
        r.prerequisites = opt_id_list(j, "prerequisites");
        r.requiresSample = StringId{opt_string(j, "requires_sample")};
        r.requiresSampleAmount = opt_int(j, "requires_sample_amount", 1, &c);
        r.unlocksModules = opt_id_list(j, "unlocks_modules");
        r.unlocksRecipes = opt_id_list(j, "unlocks_recipes");
        r.unlocksBiomes = opt_id_list(j, "unlocks_biomes");
        r.effects = read_effects(j, "effects", c);
        r.capstone = opt_bool(j, "capstone", false);
        if (r.insightCost < 0) {
            c.err("insight_cost", "insight cost must not be negative");
        }
        mDb.mResearch.push_back(std::move(r));
    }
    mDb.build_indices();
}

void ContentLoader::load_factions(const json& root, const std::string& file) {
    if (!root.contains("factions") || !root["factions"].is_array()) {
        return; // optional table
    }
    for (const auto& j : root["factions"]) {
        Ctx c{file, opt_string(j, "id"), &mErrors};
        FactionDefinition f;
        f.id = StringId{req_string(j, "id", c)};
        if (f.id.empty()) {
            continue;
        }
        check_duplicate(mDb.mFactions, f.id, c);
        f.nameKey = opt_string(j, "name_key", f.id.text() + ".name");
        f.descriptionKey = opt_string(j, "description_key", f.id.text() + ".description");
        if (has(j, "tier_thresholds") && j["tier_thresholds"].is_array()) {
            for (const auto& t : j["tier_thresholds"]) {
                f.tierThresholds.push_back(t.get<i32>());
            }
        }
        f.tierUnlocksModules = opt_id_list(j, "tier_unlocks_modules");
        mDb.mFactions.push_back(std::move(f));
    }
    mDb.build_indices();
}

void ContentLoader::load_strings(const json& root, const std::string& file) {
    if (!root.contains("strings") || !root["strings"].is_object()) {
        mErrors.add(Error{"expected a \"strings\" object",
                          file,
                          {},
                          "strings",
                          "the file maps content keys to player-facing text"});
        return;
    }
    for (const auto& [key, value] : root["strings"].items()) {
        if (!value.is_string()) {
            mErrors.add(Error{"string table entries must be strings", file, {}, key, {}});
            continue;
        }
        auto text = value.get<std::string>();
        if (text.empty()) {
            mErrors.add(Error{"string table entry is empty",
                              file,
                              {},
                              key,
                              "write the player-facing text, or remove the key"});
            continue;
        }
        if (!mDb.mStrings.emplace(key, std::move(text)).second) {
            mErrors.add(Error{"duplicate string key", file, {}, key, {}});
        }
    }
}

void ContentLoader::load_economy(const json& root, const std::string& file) {
    if (!root.contains("economy")) {
        return;
    }
    const auto& j = root["economy"];
    Ctx c{file, "economy", &mErrors};
    EconomyConfig& e = mDb.mEconomy;
    e.baseFuelCapacity = opt_float(j, "base_fuel_capacity", e.baseFuelCapacity);
    e.baseCargoCapacity = opt_float(j, "base_cargo_capacity", e.baseCargoCapacity);
    e.baseHull = opt_float(j, "base_hull", e.baseHull);
    e.baseHeatCapacity = opt_float(j, "base_heat_capacity", e.baseHeatCapacity);
    e.freeServiceFuelFraction = opt_float(j, "free_service_fuel_fraction", e.freeServiceFuelFraction);
    e.freeServiceHullFraction = opt_float(j, "free_service_hull_fraction", e.freeServiceHullFraction);
    e.baseMineSeconds = opt_float(j, "base_mine_seconds", e.baseMineSeconds);
    e.minMineSeconds = opt_float(j, "min_mine_seconds", e.minMineSeconds);
    e.maxMineSeconds = opt_float(j, "max_mine_seconds", e.maxMineSeconds);
    e.mineHardnessExponent = opt_float(j, "mine_hardness_exponent", e.mineHardnessExponent);
    e.baseDrillPower = opt_float(j, "base_drill_power", e.baseDrillPower);
    e.mineFuelBase = opt_float(j, "mine_fuel_base", e.mineFuelBase);
    e.mineFuelHardnessExponent = opt_float(j, "mine_fuel_hardness_exponent", e.mineFuelHardnessExponent);
    e.thrustFuelPerSecond = opt_float(j, "thrust_fuel_per_second", e.thrustFuelPerSecond);
    e.lateralFuelPerSecond = opt_float(j, "lateral_fuel_per_second", e.lateralFuelPerSecond);
    e.idleFuelPerSecond = opt_float(j, "idle_fuel_per_second", e.idleFuelPerSecond);
    e.gravity = opt_float(j, "gravity", e.gravity);
    e.maxFallSpeed = opt_float(j, "max_fall_speed", e.maxFallSpeed);
    e.impactDamageThreshold = opt_float(j, "impact_damage_threshold", e.impactDamageThreshold);
    e.impactDamagePerUnit = opt_float(j, "impact_damage_per_unit", e.impactDamagePerUnit);
    e.heatDecayPerSecond = opt_float(j, "heat_decay_per_second", e.heatDecayPerSecond);
    e.heatDamagePerSecond = opt_float(j, "heat_damage_per_second", e.heatDamagePerSecond);
    e.repairCreditsPerHull = opt_float(j, "repair_credits_per_hull", e.repairCreditsPerHull);
    e.refuelCreditsPerUnit = opt_float(j, "refuel_credits_per_unit", e.refuelCreditsPerUnit);
    e.rescueFeeBase = opt_float(j, "rescue_fee_base", e.rescueFeeBase);
    e.rescueFeeDepthFactor = opt_float(j, "rescue_fee_depth_factor", e.rescueFeeDepthFactor);
    e.rescueFeeCapFraction = opt_float(j, "rescue_fee_cap_fraction", e.rescueFeeCapFraction);
    e.rescueCargoLossFraction = opt_float(j, "rescue_cargo_loss_fraction", e.rescueCargoLossFraction);
    e.wreckRecoveryFraction = opt_float(j, "wreck_recovery_fraction", e.wreckRecoveryFraction);
    e.mineProgressDecayDelay = opt_float(j, "mine_progress_decay_delay", e.mineProgressDecayDelay);
    e.mineProgressDecayRate = opt_float(j, "mine_progress_decay_rate", e.mineProgressDecayRate);
    e.startingCredits = opt_int(j, "starting_credits", e.startingCredits, &c);
    e.firstUpgradeTargetCost = opt_int(j, "first_upgrade_target_cost", e.firstUpgradeTargetCost, &c);
    e.insightPerDiscovery = opt_float(j, "insight_per_discovery", e.insightPerDiscovery);
    e.sellPriceDepthBonusPer100m =
        opt_float(j, "sell_price_depth_bonus_per_100m", e.sellPriceDepthBonusPer100m);
    e.cargoMassAccelPenalty = opt_float(j, "cargo_mass_accel_penalty", e.cargoMassAccelPenalty);

    if (e.minMineSeconds <= 0.0f || e.maxMineSeconds <= e.minMineSeconds) {
        c.err("mine_seconds", "min/max mine seconds are inconsistent");
    }
    if (e.rescueFeeCapFraction < 0.0f || e.rescueFeeCapFraction > 1.0f) {
        c.err("rescue_fee_cap_fraction", "must be between 0 and 1",
              "a rescue must never be able to create a debt spiral");
    }
}

void ContentLoader::validate() {
    // Every player-facing key must have authored text. Without this check a
    // missing string degrades silently into a title-cased identifier on screen,
    // which is exactly the kind of defect nobody notices until a player does.
    {
        Ctx c{"strings.en.json", "strings", &mErrors};
        auto require_text = [&](const std::string& key, const char* where) {
            if (!key.empty() && mDb.text(key).empty()) {
                c.err(where, "no text for key '" + key + "'", "add it to strings.en.json");
            }
        };
        for (const auto& m : mDb.mMaterials) {
            require_text(m.nameKey, "material");
            require_text(m.descriptionKey, "material");
        }
        for (const auto& b : mDb.mBiomes) {
            require_text(b.nameKey, "biome");
            require_text(b.descriptionKey, "biome");
        }
        for (const auto& e : mDb.mEquipment) {
            require_text(e.nameKey, "equipment");
            require_text(e.descriptionKey, "equipment");
            for (const auto& t : e.tiers) {
                require_text(t.nameKey, "equipment tier");
                require_text(t.summaryKey, "equipment tier");
            }
        }
        for (const auto& m : mDb.mModules) {
            require_text(m.nameKey, "module");
            require_text(m.descriptionKey, "module");
        }
        for (const auto& x : mDb.mSkills) {
            require_text(x.nameKey, "skill");
            require_text(x.descriptionKey, "skill");
        }
        for (const auto& x : mDb.mRecipes) {
            require_text(x.nameKey, "recipe");
            require_text(x.descriptionKey, "recipe");
        }
        for (const auto& x : mDb.mFacilities) {
            require_text(x.nameKey, "facility");
            require_text(x.descriptionKey, "facility");
        }
        for (const auto& x : mDb.mContracts) {
            require_text(x.nameKey, "contract");
            require_text(x.descriptionKey, "contract");
        }
        for (const auto& x : mDb.mLandmarks) {
            require_text(x.nameKey, "landmark");
            require_text(x.descriptionKey, "landmark");
        }
        for (const auto& x : mDb.mHazards) {
            require_text(x.nameKey, "hazard");
            require_text(x.descriptionKey, "hazard");
        }
        for (const auto& x : mDb.mResearch) {
            require_text(x.nameKey, "research");
            require_text(x.descriptionKey, "research");
        }
        for (const auto& x : mDb.mFactions) {
            require_text(x.nameKey, "faction");
            require_text(x.descriptionKey, "faction");
        }
    }

    // Cross-table reference resolution and structural invariants.
    for (const auto& b : mDb.mBiomes) {
        Ctx c{"biomes.json", b.id.text(), &mErrors};
        require_material(b.baseMaterial, c, "base_material");
        require_material(b.softMaterial, c, "soft_material");
        require_material(b.structuralMaterial, c, "structural_material");
        for (const auto& h : b.hazards) {
            if (!mDb.hazard(h.hazardId.text()).valid()) {
                c.err("hazards", "unresolved hazard '" + h.hazardId.text() + "'");
            }
        }
        for (const auto& l : b.landmarks) {
            if (!mDb.landmark(l.text()).valid()) {
                c.err("landmarks", "unresolved landmark '" + l.text() + "'");
            }
        }
        if (!b.gateResearch.empty() && !mDb.research_node(b.gateResearch.text()).valid()) {
            c.err("gate_research", "unresolved research '" + b.gateResearch.text() + "'");
        }
    }
    for (const auto& m : mDb.mMaterials) {
        Ctx c{"materials.json", m.id.text(), &mErrors};
        for (const auto& bid : m.biomes) {
            require_biome(bid, c, "generation.biomes");
        }
    }
    for (const auto& mod : mDb.mModules) {
        Ctx c{"modules.json", mod.id.text(), &mErrors};
        if (!mod.unlockResearch.empty() && !mDb.research_node(mod.unlockResearch.text()).valid()) {
            c.err("unlock", "unresolved research '" + mod.unlockResearch.text() + "'");
        }
        for (const auto& cc : mod.costComponents) {
            require_material(cc.first, c, "cost_components");
        }
    }
    for (const auto& e : mDb.mEquipment) {
        Ctx c{"equipment.json", e.id.text(), &mErrors};
        i32 previousCost = -1;
        for (const auto& t : e.tiers) {
            for (const auto& cc : t.costComponents) {
                require_material(cc.first, c, "tiers.cost_components");
            }
            if (t.tier > 0 && t.costCredits <= 0 && t.costComponents.empty()) {
                c.err("tiers.cost_credits", "upgrade tier " + std::to_string(t.tier) + " is free");
            }
            if (t.costCredits < previousCost) {
                c.err("tiers.cost_credits",
                      "tier " + std::to_string(t.tier) + " costs less than the previous tier");
            }
            previousCost = t.costCredits;
        }
    }
    for (const auto& s : mDb.mSkills) {
        Ctx c{"skills.json", s.id.text(), &mErrors};
        for (const auto& p : s.prerequisites) {
            if (!mDb.skill(p.text()).valid()) {
                c.err("prerequisites", "unresolved skill '" + p.text() + "'");
            }
        }
        if (s.effects.empty()) {
            c.err("effects", "skill node has no effect");
        }
    }
    // Recipes: resolve materials and reject circular zero-cost conversions.
    for (const auto& r : mDb.mRecipes) {
        Ctx c{"recipes.json", r.id.text(), &mErrors};
        i64 inputValue = 0;
        i64 outputValue = 0;
        for (const auto& in : r.inputs) {
            require_material(in.materialId, c, "inputs");
            const auto h = mDb.material(in.materialId.text());
            if (h.valid()) {
                inputValue += static_cast<i64>(mDb.material_def(h).baseValue) * in.amount;
            }
            if (in.amount <= 0) {
                c.err("inputs.amount", "input amount must be positive");
            }
        }
        for (const auto& out : r.outputs) {
            require_material(out.materialId, c, "outputs");
            const auto h = mDb.material(out.materialId.text());
            if (h.valid()) {
                outputValue += static_cast<i64>(mDb.material_def(h).baseValue) * out.amount;
            }
            if (out.amount <= 0) {
                c.err("outputs.amount", "output amount must be positive");
            }
        }
        for (const auto& bp : r.byproducts) {
            require_material(bp.materialId, c, "byproducts");
        }
        if (!mDb.facility(r.facilityId.text()).valid()) {
            c.err("facility", "unresolved facility '" + r.facilityId.text() + "'");
        }
        // Detect a direct A -> A loop that would print value.
        for (const auto& in : r.inputs) {
            for (const auto& out : r.outputs) {
                if (in.materialId == out.materialId && out.amount >= in.amount && r.creditCost <= 0) {
                    c.err("outputs", "recipe converts a material into more of itself for free",
                          "this would allow unbounded value creation");
                }
            }
        }
        if (outputValue > inputValue * 6 && r.creditCost <= 0) {
            c.err("outputs", "recipe multiplies raw value more than 6x with no cost",
                  "review docs/BALANCE_NOTES.md conversion guidance");
        }
    }
    for (const auto& f : mDb.mFacilities) {
        Ctx c{"facilities.json", f.id.text(), &mErrors};
        for (const auto& t : f.tiers) {
            for (const auto& cc : t.costComponents) {
                require_material(cc.materialId, c, "tiers.cost_components");
            }
            for (const auto& rid : t.unlocksRecipes) {
                if (!mDb.recipe(rid.text()).valid()) {
                    c.err("tiers.unlocks_recipes", "unresolved recipe '" + rid.text() + "'");
                }
            }
        }
    }
    for (const auto& t : mDb.mContracts) {
        Ctx c{"contracts.json", t.id.text(), &mErrors};
        for (const auto& mid : t.materialPool) {
            require_material(mid, c, "material_pool");
        }
        for (const auto& rc : t.rewardComponents) {
            require_material(rc.materialId, c, "reward_components");
        }
        if (!t.requiresResearch.empty() && !mDb.research_node(t.requiresResearch.text()).valid()) {
            c.err("requires_research", "unresolved research '" + t.requiresResearch.text() + "'");
        }
        if (!t.factionId.empty() && mDb.faction_index(t.factionId.text()) < 0) {
            c.err("faction", "unresolved faction '" + t.factionId.text() + "'");
        }
    }
    for (const auto& l : mDb.mLandmarks) {
        Ctx c{"landmarks.json", l.id.text(), &mErrors};
        for (const auto& bid : l.biomes) {
            require_biome(bid, c, "biomes");
        }
        for (const auto& mid : l.lootMaterials) {
            require_material(mid, c, "loot_materials");
        }
        if (!l.grantsResearch.empty() && !mDb.research_node(l.grantsResearch.text()).valid()) {
            c.err("grants_research", "unresolved research '" + l.grantsResearch.text() + "'");
        }
    }
    // Research graph: unresolved prerequisites and cycles.
    for (const auto& r : mDb.mResearch) {
        Ctx c{"research.json", r.id.text(), &mErrors};
        for (const auto& p : r.prerequisites) {
            if (!mDb.research_node(p.text()).valid()) {
                c.err("prerequisites", "unresolved research '" + p.text() + "'");
            }
        }
        require_material(r.requiresSample, c, "requires_sample");
        for (const auto& mid : r.unlocksModules) {
            if (!mDb.module(mid.text()).valid()) {
                c.err("unlocks_modules", "unresolved module '" + mid.text() + "'");
            }
        }
        for (const auto& rid : r.unlocksRecipes) {
            if (!mDb.recipe(rid.text()).valid()) {
                c.err("unlocks_recipes", "unresolved recipe '" + rid.text() + "'");
            }
        }
        for (const auto& bid : r.unlocksBiomes) {
            require_biome(bid, c, "unlocks_biomes");
        }
    }
    // Cycle detection over research prerequisites.
    {
        const usize n = mDb.mResearch.size();
        std::vector<int> state(n, 0);
        std::vector<usize> stack;
        auto visit = [&](auto&& self, usize i) -> bool {
            if (state[i] == 1) {
                return true;
            }
            if (state[i] == 2) {
                return false;
            }
            state[i] = 1;
            for (const auto& p : mDb.mResearch[i].prerequisites) {
                const auto h = mDb.research_node(p.text());
                if (h.valid() && self(self, static_cast<usize>(h.index))) {
                    return true;
                }
            }
            state[i] = 2;
            return false;
        };
        for (usize i = 0; i < n; ++i) {
            if (state[i] == 0 && visit(visit, i)) {
                mErrors.add(Error{"research prerequisites contain a cycle", "research.json",
                                  mDb.mResearch[i].id.text(), "prerequisites", "break the cycle"});
                break;
            }
        }
    }
    // Skill prerequisite cycles.
    {
        const usize n = mDb.mSkills.size();
        std::vector<int> state(n, 0);
        auto visit = [&](auto&& self, usize i) -> bool {
            if (state[i] == 1) {
                return true;
            }
            if (state[i] == 2) {
                return false;
            }
            state[i] = 1;
            for (const auto& p : mDb.mSkills[i].prerequisites) {
                const auto h = mDb.skill(p.text());
                if (h.valid() && self(self, static_cast<usize>(h.index))) {
                    return true;
                }
            }
            state[i] = 2;
            return false;
        };
        for (usize i = 0; i < n; ++i) {
            if (state[i] == 0 && visit(visit, i)) {
                mErrors.add(Error{"skill prerequisites contain a cycle", "skills.json",
                                  mDb.mSkills[i].id.text(), "prerequisites", "break the cycle"});
                break;
            }
        }
    }
    // Biome coverage must be continuous from the surface downwards.
    if (!mDb.mBiomes.empty()) {
        if (mDb.mBiomes.front().minDepth > 0) {
            mErrors.add(Error{"no biome covers depth 0",
                              "biomes.json",
                              {},
                              "min_depth",
                              "the first biome must start at depth 0"});
        }
        for (usize i = 1; i < mDb.mBiomes.size(); ++i) {
            if (mDb.mBiomes[i].minDepth > mDb.mBiomes[i - 1].maxDepth) {
                mErrors.add(Error{"gap in biome depth coverage before '" + mDb.mBiomes[i].id.text() + "'",
                                  "biomes.json", mDb.mBiomes[i].id.text(), "min_depth",
                                  "biome depth ranges must be contiguous"});
            }
        }
    }
    // Every material with generation rules must be reachable in some biome.
    for (const auto& m : mDb.mMaterials) {
        if (m.rarityWeightBp <= 0 || m.biomes.empty()) {
            continue;
        }
        bool reachable = false;
        for (const auto& bid : m.biomes) {
            const auto h = mDb.biome(bid.text());
            if (!h.valid()) {
                continue;
            }
            const auto& b = mDb.biome_def(h);
            if (m.minDepth <= b.maxDepth && m.maxDepth >= b.minDepth) {
                reachable = true;
                break;
            }
        }
        if (!reachable) {
            mErrors.add(Error{"material can never generate: depth range does not intersect its biomes",
                              "materials.json", m.id.text(), "generation",
                              "widen min_depth/max_depth or change the biome list"});
        }
    }
}

// ---------------------------------------------------------------------------
// ContentDatabase
// ---------------------------------------------------------------------------

void ContentDatabase::build_indices() {
    auto rebuild = [](auto& index, const auto& list) {
        index.clear();
        for (usize i = 0; i < list.size(); ++i) {
            index[list[i].id.text()] = static_cast<i32>(i);
        }
    };
    rebuild(mMaterialIndex, mMaterials);
    rebuild(mBiomeIndex, mBiomes);
    rebuild(mEquipmentIndex, mEquipment);
    rebuild(mModuleIndex, mModules);
    rebuild(mSkillIndex, mSkills);
    rebuild(mRecipeIndex, mRecipes);
    rebuild(mFacilityIndex, mFacilities);
    rebuild(mContractIndex, mContracts);
    rebuild(mLandmarkIndex, mLandmarks);
    rebuild(mHazardIndex, mHazards);
    rebuild(mResearchIndex, mResearch);
    rebuild(mFactionIndex, mFactions);
}

namespace {
template <typename H>
H lookup(const std::unordered_map<std::string, i32>& index, std::string_view id) {
    const auto it = index.find(std::string{id});
    return it == index.end() ? H{} : H{it->second};
}
} // namespace

MaterialHandle ContentDatabase::material(std::string_view id) const {
    return lookup<MaterialHandle>(mMaterialIndex, id);
}

BiomeHandle ContentDatabase::biome(std::string_view id) const {
    return lookup<BiomeHandle>(mBiomeIndex, id);
}

EquipmentHandle ContentDatabase::equipment_track(std::string_view id) const {
    return lookup<EquipmentHandle>(mEquipmentIndex, id);
}

ModuleHandle ContentDatabase::module(std::string_view id) const {
    return lookup<ModuleHandle>(mModuleIndex, id);
}

SkillHandle ContentDatabase::skill(std::string_view id) const {
    return lookup<SkillHandle>(mSkillIndex, id);
}

RecipeHandle ContentDatabase::recipe(std::string_view id) const {
    return lookup<RecipeHandle>(mRecipeIndex, id);
}

FacilityHandle ContentDatabase::facility(std::string_view id) const {
    return lookup<FacilityHandle>(mFacilityIndex, id);
}

ContractHandle ContentDatabase::contract(std::string_view id) const {
    return lookup<ContractHandle>(mContractIndex, id);
}

LandmarkHandle ContentDatabase::landmark(std::string_view id) const {
    return lookup<LandmarkHandle>(mLandmarkIndex, id);
}

HazardHandle ContentDatabase::hazard(std::string_view id) const {
    return lookup<HazardHandle>(mHazardIndex, id);
}

ResearchHandle ContentDatabase::research_node(std::string_view id) const {
    return lookup<ResearchHandle>(mResearchIndex, id);
}

i32 ContentDatabase::faction_index(std::string_view id) const {
    const auto it = mFactionIndex.find(std::string{id});
    return it == mFactionIndex.end() ? -1 : it->second;
}

BiomeHandle ContentDatabase::biome_at_depth(i64 depthTiles) const {
    if (mBiomes.empty()) {
        return {};
    }
    for (usize i = 0; i < mBiomes.size(); ++i) {
        if (depthTiles >= mBiomes[i].minDepth && depthTiles <= mBiomes[i].maxDepth) {
            return BiomeHandle{static_cast<i32>(i)};
        }
    }
    return depthTiles < mBiomes.front().minDepth ? BiomeHandle{0}
                                                 : BiomeHandle{static_cast<i32>(mBiomes.size() - 1)};
}

std::string_view ContentDatabase::text(std::string_view key) const {
    // Heterogeneous lookup would avoid this allocation, but the table is only
    // consulted when a screen is built, never inside the simulation loop.
    const auto it = mStrings.find(std::string(key));
    return it == mStrings.end() ? std::string_view{} : std::string_view{it->second};
}

namespace {

Result<ContentDatabase> build_database(const std::unordered_map<std::string, std::string>& files,
                                       ErrorList& errors) {
    ContentDatabase db;
    ContentLoader loader(db, errors);

    struct Entry {
        const char* name;
        void (ContentLoader::*fn)(const json&, const std::string&);
        bool required;
    };

    // Order matters: materials and hazards resolve before things referencing them.
    const Entry order[] = {
        {"materials.json", &ContentLoader::load_materials, true},
        {"hazards.json", &ContentLoader::load_hazards, true},
        {"landmarks.json", &ContentLoader::load_landmarks, true},
        {"biomes.json", &ContentLoader::load_biomes, true},
        {"equipment.json", &ContentLoader::load_equipment, true},
        {"research.json", &ContentLoader::load_research, true},
        {"modules.json", &ContentLoader::load_modules, true},
        {"skills.json", &ContentLoader::load_skills, true},
        {"facilities.json", &ContentLoader::load_facilities, true},
        {"recipes.json", &ContentLoader::load_recipes, true},
        {"factions.json", &ContentLoader::load_factions, false},
        {"contracts.json", &ContentLoader::load_contracts, true},
        {"economy.json", &ContentLoader::load_economy, false},
        {"strings.en.json", &ContentLoader::load_strings, true},
    };

    for (const auto& entry : order) {
        const auto it = files.find(entry.name);
        if (it == files.end()) {
            if (entry.required) {
                errors.add(Error{"required content file is missing",
                                 entry.name,
                                 {},
                                 {},
                                 "check the data directory passed to the game"});
            }
            continue;
        }
        auto parsed = parse_json_with_context(it->second, entry.name, errors);
        if (!parsed) {
            continue;
        }
        if (parsed->contains("schema_version")) {
            const int v = (*parsed)["schema_version"].get<int>();
            loader.note_schema_version(v);
            if (v > kContentSchemaVersion) {
                errors.add(Error{"content schema version " + std::to_string(v) +
                                     " is newer than this build supports (" +
                                     std::to_string(kContentSchemaVersion) + ")",
                                 entry.name,
                                 {},
                                 "schema_version",
                                 "update the game"});
            }
        }
        (loader.*(entry.fn))(*parsed, entry.name);
    }

    loader.validate();

    loader.set_air_and_boundary();
    if (!db.airMaterial.valid()) {
        errors.add(
            Error{"material.air must exist", "materials.json", {}, {}, "air is required as the empty tile"});
    }
    if (!db.boundaryMaterial.valid()) {
        errors.add(Error{"material.bedrock must exist",
                         "materials.json",
                         {},
                         {},
                         "bedrock is required as the indestructible world boundary"});
    }

    if (!errors.empty()) {
        return fail(Error{"content validation failed with " + std::to_string(errors.size()) + " problem(s)",
                          {},
                          {},
                          {},
                          {}});
    }
    return db;
}

} // namespace

Result<ContentDatabase>
ContentDatabase::load_from_strings(const std::unordered_map<std::string, std::string>& files,
                                   ErrorList& errors) {
    return build_database(files, errors);
}

Result<ContentDatabase> ContentDatabase::load(const std::filesystem::path& dataDir, ErrorList& errors) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(dataDir, ec)) {
        errors.add(Error{"data directory not found: " + dataDir.string(),
                         {},
                         {},
                         {},
                         "pass --data <dir> or install the game data next to the executable"});
        return fail(Error{"missing data directory", dataDir.string(), {}, {}, {}});
    }
    std::unordered_map<std::string, std::string> files;
    for (const auto& e : fs::directory_iterator(dataDir, ec)) {
        if (!e.is_regular_file() || e.path().extension() != ".json") {
            continue;
        }
        auto text = read_text_file(e.path());
        if (!text) {
            errors.add(Error{"could not open file", e.path().filename().string(), {}, {}, {}});
            continue;
        }
        files[e.path().filename().string()] = std::move(*text);
    }
    DF_LOG_INFO("content", "loading {} data file(s) from {}", files.size(), dataDir.string());
    return build_database(files, errors);
}

} // namespace df
