#include "app/SaveFile.hpp"

#include "core/Hash.hpp"
#include "core/JsonFile.hpp"
#include "core/Log.hpp"
#include "core/Version.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace df {
namespace {

using json = nlohmann::json;

json encode_inventory(const ContentDatabase& db, const Inventory& inv) {
    json j = json::array();
    for (const auto& s : inv.stacks()) {
        if (s.material < 0 || static_cast<usize>(s.material) >= db.materials().size()) {
            continue;
        }
        // Stable string ids in the file; typed handles only at runtime.
        j.push_back({{"id", db.materials()[static_cast<usize>(s.material)].id.text()}, {"amount", s.amount}});
    }
    return j;
}

void decode_inventory(const ContentDatabase& db, const json& j, Inventory& inv, ErrorList& warnings) {
    std::vector<InventoryStack> stacks;
    if (!j.is_array()) {
        return;
    }
    for (const auto& e : j) {
        const std::string id = e.value("id", std::string{});
        const auto h = db.material(id);
        if (!h.valid()) {
            warnings.add(Error{"save references an unknown material and it was dropped", "save", id,
                               "inventory", "the material may have been removed from content"});
            continue;
        }
        stacks.push_back(InventoryStack{h.index, e.value("amount", 0)});
    }
    inv.set_stacks(std::move(stacks));
}

template <typename Container, typename NameFn>
json encode_id_set(const Container& set, NameFn name) {
    json j = json::array();
    std::vector<std::string> names;
    for (const i32 i : set) {
        names.push_back(name(i));
    }
    std::sort(names.begin(), names.end()); // stable output for diffing
    for (auto& n : names) {
        j.push_back(n);
    }
    return j;
}

json encode_deltas(const std::vector<ChunkDelta>& deltas) {
    json arr = json::array();
    for (const auto& d : deltas) {
        json jd;
        jd["cx"] = d.coord.x;
        jd["cy"] = d.coord.y;
        if (!d.tiles.empty()) {
            json t = json::array();
            for (const auto& tc : d.tiles) {
                t.push_back({tc.localIndex, tc.material, tc.flags});
            }
            jd["tiles"] = std::move(t);
        }
        if (!d.mining.empty()) {
            json m = json::array();
            for (const auto& mp : d.mining) {
                m.push_back({mp.localIndex, mp.progress});
            }
            jd["mining"] = std::move(m);
        }
        if (!d.landmarks.empty()) {
            json l = json::array();
            for (const auto& ls : d.landmarks) {
                l.push_back({{"id", ls.stableId}, {"a", ls.activated}, {"l", ls.looted}});
            }
            jd["landmarks"] = std::move(l);
        }
        if (d.hasRevealed) {
            json r = json::array();
            for (const u8 b : d.revealed) {
                r.push_back(b);
            }
            jd["revealed"] = std::move(r);
        }
        arr.push_back(std::move(jd));
    }
    return arr;
}

/// The save decoder's element read (SEC-01 class): a member the decoder cannot
/// read used to throw straight into std::terminate. The file's convention for data
/// it cannot use is to drop it and say so — that is what this does, reporting
/// through the existing warnings machinery.
i64 readJsonInt(const json& node, const char* key, ErrorList& warnings, i64 def = 0) {
    if (!node.contains(key) || node.at(key).is_null()) {
        return def;
    }
    const auto& v = node.at(key);
    if (!matches_type(v, FieldType::Integer)) {
        warnings.add(Error{std::string("a saved value the loader could not read and it was dropped") + " (" +
                               std::string(field_type_name(v)) + " where an integer belongs)",
                           "save",
                           {},
                           key,
                           "the entry was skipped"});
        return def;
    }
    return v.get<i64>();
}

std::vector<ChunkDelta> decode_deltas(const json& arr, ErrorList& warnings) {
    std::vector<ChunkDelta> out;
    if (!arr.is_array()) {
        return out;
    }
    for (const auto& jd : arr) {
        if (!jd.is_object()) {
            // SEC-01 class: a member the decoder cannot read used to throw straight
            // into std::terminate. The save loader's convention for data it cannot
            // use is to drop it and say so — that is what happens here.
            warnings.add(Error{"a world delta the loader could not read and it was dropped",
                               "save",
                               {},
                               "world_deltas",
                               "the entry was skipped"});
            continue;
        }
        ChunkDelta d;
        d.coord.x = readJsonInt(jd, "cx", warnings);
        d.coord.y = readJsonInt(jd, "cy", warnings);
        if (jd.contains("tiles")) {
            for (const auto& t : jd.at("tiles")) {
                if (t.is_array() && t.size() >= 2 && matches_type(t[0], FieldType::Integer) &&
                    matches_type(t[1], FieldType::Integer) &&
                    (t.size() < 3 || matches_type(t[2], FieldType::Integer))) {
                    d.tiles.push_back(
                        ChunkDelta::TileChange{t[0].get<i32>(), t[1].get<i32>(),
                                               t.size() > 2 ? t[2].get<u8>() : static_cast<u8>(0)});
                } else {
                    warnings.add(Error{"a tile change the loader could not read and it was dropped",
                                       "save",
                                       {},
                                       "world_deltas",
                                       "the entry was skipped"});
                }
            }
        }
        if (jd.contains("mining")) {
            for (const auto& m : jd.at("mining")) {
                if (m.is_array() && m.size() >= 2 && matches_type(m[0], FieldType::Integer) &&
                    matches_type(m[1], FieldType::Number)) {
                    d.mining.push_back(MiningProgress{m[0].get<i32>(), m[1].get<float>(), 0.0f});
                } else {
                    warnings.add(Error{"a mining progress the loader could not read and it was dropped",
                                       "save",
                                       {},
                                       "world_deltas",
                                       "the entry was skipped"});
                }
            }
        }
        if (jd.contains("landmarks")) {
            for (const auto& l : jd.at("landmarks")) {
                if (!l.is_object() || (l.contains("id") && !matches_type(l.at("id"), FieldType::Integer)) ||
                    (l.contains("a") && !matches_type(l.at("a"), FieldType::Boolean)) ||
                    (l.contains("l") && !matches_type(l.at("l"), FieldType::Boolean))) {
                    warnings.add(Error{"a landmark state the loader could not read and it was dropped",
                                       "save",
                                       {},
                                       "world_deltas",
                                       "the entry was skipped"});
                    continue;
                }
                d.landmarks.push_back(ChunkDelta::LandmarkState{l.value("id", static_cast<u64>(0)),
                                                                l.value("a", false), l.value("l", false)});
            }
        }
        if (jd.contains("revealed") && jd.at("revealed").is_array()) {
            usize i = 0;
            for (const auto& b : jd.at("revealed")) {
                if (i < d.revealed.size()) {
                    if (!matches_type(b, FieldType::Integer)) {
                        warnings.add(Error{"a revealed byte the loader could not read and it was dropped",
                                           "save",
                                           {},
                                           "world_deltas",
                                           "the entry was skipped"});
                        continue;
                    }
                    d.revealed[i++] = b.get<u8>();
                }
            }
            d.hasRevealed = true;
        }
        out.push_back(std::move(d));
    }
    return out;
}

/// Migration steps. Each converts the payload in place from `from` to `from+1`.
void migrate_1_to_2(json& payload) {
    // v2 introduced protected sample storage as its own container.
    if (!payload.contains("samples")) {
        payload["samples"] = json::array();
    }
    if (!payload["progress"].contains("samples")) {
        payload["progress"]["samples"] = json::array();
    }
}

void migrate_2_to_3(json& payload) {
    // v3 introduced loadout presets, faction reputation and codex entries.
    auto& p = payload["progress"];
    if (!p.contains("loadout_presets")) {
        p["loadout_presets"] = json::array({json::array(), json::array(), json::array()});
    }
    if (!p.contains("reputation")) {
        p["reputation"] = json::array();
    }
    if (!p.contains("codex")) {
        p["codex"] = json::array();
    }
    if (!p.contains("max_utility_power")) {
        p["max_utility_power"] = 6;
    }
}

void migrate_3_to_4(json& payload) {
    // v4 records the position of each activated transit node so the hub can
    // offer it as a launch point. Older saves keep their node ids; the nodes
    // become selectable again the next time the player visits one.
    if (!payload["progress"].contains("transit_node_positions")) {
        payload["progress"]["transit_node_positions"] = json::array();
    }
}

void migrate_4_to_5(json& payload) {
    // v5 records which content pack generated the world, so a future expansion
    // pack cannot reinterpret a save carved from different content. The base
    // game has always been "base".
    if (!payload.contains("content_pack_id")) {
        payload["content_pack_id"] = "base";
    }
}

} // namespace

u64 SaveSystem::checksum(std::string_view payload) {
    return fnv1a64(payload);
}

std::filesystem::path SaveSystem::slot_path(int slot) const {
    return mDir / ("slot" + std::to_string(slot) + ".dfsave");
}

std::filesystem::path SaveSystem::backup_path(int slot) const {
    return mDir / ("slot" + std::to_string(slot) + ".dfsave.bak");
}

Result<std::string> SaveSystem::serialise(const ContentDatabase& db, const SaveData& data) const {
    json payload;
    payload["world_seed"] = data.worldSeed;
    payload["world_name"] = data.worldName;
    payload["play_seconds"] = data.playSeconds;
    payload["content_pack_id"] = data.contentPackId.empty() ? std::string("base") : data.contentPackId;

    const auto& p = data.progress;
    json jp;
    jp["credits"] = p.credits;
    jp["insight"] = p.insight;
    jp["skill_points_earned"] = p.skillPointsEarned;
    jp["skill_points_spent"] = p.skillPointsSpent;
    jp["free_rescue_used"] = p.freeRescueUsed;
    jp["ending_reached"] = p.endingReached;
    jp["new_game_plus"] = p.newGamePlusLevel;
    jp["max_utility_power"] = p.maxUtilityPower;

    json tiers = json::object();
    for (usize i = 0; i < db.equipment().size() && i < p.equipmentTier.size(); ++i) {
        tiers[db.equipment()[i].id.text()] = p.equipmentTier[i];
    }
    jp["equipment"] = std::move(tiers);

    json facilities = json::object();
    for (usize i = 0; i < db.facilities().size() && i < p.facilityTier.size(); ++i) {
        facilities[db.facilities()[i].id.text()] = p.facilityTier[i];
    }
    jp["facilities"] = std::move(facilities);

    json reputation = json::object();
    for (usize i = 0; i < db.factions().size() && i < p.reputation.size(); ++i) {
        reputation[db.factions()[i].id.text()] = p.reputation[i];
    }
    jp["reputation"] = std::move(reputation);

    jp["owned_modules"] =
        encode_id_set(p.ownedModules, [&](i32 i) { return db.modules()[static_cast<usize>(i)].id.text(); });
    json installed = json::array();
    for (const i32 m : p.installedModules) {
        if (m >= 0 && static_cast<usize>(m) < db.modules().size()) {
            installed.push_back(db.modules()[static_cast<usize>(m)].id.text());
        }
    }
    jp["installed_modules"] = std::move(installed);
    jp["skills"] =
        encode_id_set(p.unlockedSkills, [&](i32 i) { return db.skills()[static_cast<usize>(i)].id.text(); });
    jp["research"] = encode_id_set(p.completedResearch,
                                   [&](i32 i) { return db.research()[static_cast<usize>(i)].id.text(); });
    jp["unlocked_biomes"] =
        encode_id_set(p.unlockedBiomes, [&](i32 i) { return db.biomes()[static_cast<usize>(i)].id.text(); });

    json presets = json::array();
    for (const auto& preset : p.loadoutPresets) {
        json pr = json::array();
        for (const i32 m : preset) {
            if (m >= 0 && static_cast<usize>(m) < db.modules().size()) {
                pr.push_back(db.modules()[static_cast<usize>(m)].id.text());
            }
        }
        presets.push_back(std::move(pr));
    }
    jp["loadout_presets"] = std::move(presets);

    json codex = json::array();
    for (const auto& [mi, entry] : p.codex) {
        if (mi < 0 || static_cast<usize>(mi) >= db.materials().size()) {
            continue;
        }
        codex.push_back({{"id", db.materials()[static_cast<usize>(mi)].id.text()},
                         {"mined", entry.minedCount},
                         {"first_depth", entry.firstDepth}});
    }
    jp["codex"] = std::move(codex);

    json landmarks = json::array();
    for (const i64 id : p.discoveredLandmarks) {
        landmarks.push_back(id);
    }
    jp["discovered_landmarks"] = std::move(landmarks);
    json transit = json::array();
    for (const u64 id : p.activatedTransitNodes) {
        transit.push_back(id);
    }
    jp["transit_nodes"] = std::move(transit);
    json transitPositions = json::array();
    for (const auto& n : p.transitNodes) {
        transitPositions.push_back({{"id", n.stableId}, {"x", n.x}, {"y", n.y}});
    }
    jp["transit_node_positions"] = std::move(transitPositions);

    auto encode_contract = [&](const ActiveContract& c) {
        json jc;
        jc["template"] = c.templateIndex >= 0 && static_cast<usize>(c.templateIndex) < db.contracts().size()
                             ? db.contracts()[static_cast<usize>(c.templateIndex)].id.text()
                             : std::string{};
        jc["material"] = c.targetMaterial >= 0 && static_cast<usize>(c.targetMaterial) < db.materials().size()
                             ? db.materials()[static_cast<usize>(c.targetMaterial)].id.text()
                             : std::string{};
        jc["amount"] = c.targetAmount;
        jc["depth"] = c.targetDepth;
        jc["progress"] = c.progress;
        jc["completed"] = c.completed;
        jc["claimed"] = c.claimed;
        jc["instance"] = c.instanceId;
        jc["forbidden_slot"] = c.forbiddenSlot;
        return jc;
    };
    json contracts = json::array();
    for (const auto& c : p.contracts) {
        contracts.push_back(encode_contract(c));
    }
    jp["contracts"] = std::move(contracts);
    json done = json::array();
    for (const auto& c : p.completedContracts) {
        done.push_back(encode_contract(c));
    }
    jp["completed_contracts"] = std::move(done);

    jp["vault"] = encode_inventory(db, p.vault);
    jp["samples"] = encode_inventory(db, p.samples);
    jp["stats"] = {{"tiles_mined", p.stats.tilesMined},
                   {"credits_earned", p.stats.creditsEarned},
                   {"credits_spent", p.stats.creditsSpent},
                   {"expeditions", p.stats.expeditions},
                   {"rescues", p.stats.rescues},
                   {"deepest_depth", p.stats.deepestDepth},
                   {"play_seconds", p.stats.playSeconds}};
    payload["progress"] = std::move(jp);

    payload["rig"] = {{"x", data.rig.position.x},
                      {"y", data.rig.position.y},
                      {"vx", data.rig.velocity.x},
                      {"vy", data.rig.velocity.y},
                      {"fuel", data.rig.fuel},
                      {"hull", data.rig.hull},
                      {"heat", data.rig.heat},
                      {"mode", static_cast<int>(data.rig.mode)},
                      {"deepest_run", data.rig.deepestThisRun}};
    payload["cargo"] = encode_inventory(db, data.cargo);
    payload["samples"] = encode_inventory(db, data.samples);
    payload["world_deltas"] = encode_deltas(data.worldDeltas);

    const std::string payloadText = payload.dump();

    json root;
    root["save_schema_version"] = kSaveSchemaVersion;
    root["content_schema_version"] = db.content_schema_version();
    root["world_gen_version"] = kWorldGenVersion;
    root["build_version"] = kBuildVersion;
    root["timestamp"] =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    root["play_seconds"] = data.playSeconds;
    root["summary"] = data.summary.empty() ? std::string("Depth ") + std::to_string(p.stats.deepestDepth) +
                                                 " m, " + std::to_string(p.credits) + " credits"
                                           : data.summary;
    root["checksum"] = checksum(payloadText);
    root["payload"] = std::move(payload);
    return root.dump(1, ' ');
}

Result<SaveData> SaveSystem::deserialise(const ContentDatabase& db, const std::string& text) const {
    auto parsed = df::parse_json(text);
    if (!parsed) {
        return fail(Error{
            "save file is not valid JSON", "save", {}, {}, "the file may be truncated; try the backup"});
    }
    json root = std::move(*parsed);
    if (!root.contains("payload") || !root.contains("save_schema_version")) {
        return fail(Error{"save file is missing required sections",
                          "save",
                          {},
                          {},
                          "this file was not written by Deepforge"});
    }
    i32 version = root.value("save_schema_version", 0);
    if (version > kSaveSchemaVersion) {
        return fail(
            Error{"save was written by a newer version of the game (schema " + std::to_string(version) + ")",
                  "save",
                  {},
                  "save_schema_version",
                  "update the game to load this save"});
    }

    json payload = root["payload"];
    const u64 storedChecksum = root.value("checksum", static_cast<u64>(0));
    if (storedChecksum != 0 && checksum(payload.dump()) != storedChecksum) {
        return fail(Error{"save checksum mismatch; the file is corrupted",
                          "save",
                          {},
                          "checksum",
                          "the original file is untouched, try the rotating backup"});
    }

    // Migrations run in order; unknown required data is never silently dropped.
    while (version < kSaveSchemaVersion) {
        switch (version) {
            case 1:
                migrate_1_to_2(payload);
                break;
            case 2:
                migrate_2_to_3(payload);
                break;
            case 3:
                migrate_3_to_4(payload);
                break;
            case 4:
                migrate_4_to_5(payload);
                break;
            default:
                return fail(Error{"no migration path from save schema " + std::to_string(version),
                                  "save",
                                  {},
                                  "save_schema_version",
                                  {}});
        }
        ++version;
        DF_LOG_INFO("save", "migrated save to schema {}", version);
    }

    ErrorList warnings;
    SaveData data;
    data.saveSchemaVersion = root.value("save_schema_version", 0);
    data.contentVersion = root.value("content_schema_version", 0);
    data.worldGenVersion = root.value("world_gen_version", 0);
    data.buildVersion = root.value("build_version", std::string{});
    data.timestamp = root.value("timestamp", static_cast<i64>(0));
    data.summary = root.value("summary", std::string{});
    data.worldSeed = payload.value("world_seed", static_cast<u64>(0));
    data.worldName = payload.value("world_name", std::string{});
    data.playSeconds = payload.value("play_seconds", 0.0);
    data.contentPackId = payload.value("content_pack_id", std::string{"base"});

    auto& p = data.progress;
    p.initialise(db);
    const json& jp = payload["progress"];
    p.credits = jp.value("credits", static_cast<i64>(0));
    p.insight = jp.value("insight", static_cast<i64>(0));
    p.skillPointsEarned = jp.value("skill_points_earned", 0);
    p.skillPointsSpent = jp.value("skill_points_spent", 0);
    p.freeRescueUsed = jp.value("free_rescue_used", false);
    p.endingReached = jp.value("ending_reached", false);
    p.newGamePlusLevel = jp.value("new_game_plus", 0);
    p.maxUtilityPower = jp.value("max_utility_power", 6);

    if (jp.contains("equipment")) {
        for (const auto& [key, value] : jp["equipment"].items()) {
            const auto h = db.equipment_track(key);
            if (h.valid()) {
                p.equipmentTier[static_cast<usize>(h.index)] = value.get<i32>();
            } else {
                warnings.add(Error{"unknown equipment track in save", "save", key, "equipment", {}});
            }
        }
    }
    if (jp.contains("facilities")) {
        for (const auto& [key, value] : jp["facilities"].items()) {
            const auto h = db.facility(key);
            if (h.valid()) {
                p.facilityTier[static_cast<usize>(h.index)] = value.get<i32>();
            }
        }
    }
    if (jp.contains("reputation")) {
        for (const auto& [key, value] : jp["reputation"].items()) {
            const i32 f = db.faction_index(key);
            if (f >= 0 && static_cast<usize>(f) < p.reputation.size()) {
                p.reputation[static_cast<usize>(f)] = value.get<i32>();
            }
        }
    }
    auto read_ids = [&](const char* key, auto resolve, auto&& sink) {
        if (!jp.contains(key) || !jp[key].is_array()) {
            return;
        }
        for (const auto& e : jp[key]) {
            const auto h = resolve(e.get<std::string>());
            if (h.valid()) {
                sink(h.index);
            } else {
                warnings.add(Error{"save references content that no longer exists", "save",
                                   e.get<std::string>(), key, "the entry was skipped"});
            }
        }
    };
    read_ids(
        "owned_modules", [&](const std::string& s) { return db.module(s); },
        [&](i32 i) { p.ownedModules.insert(i); });
    read_ids(
        "installed_modules", [&](const std::string& s) { return db.module(s); },
        [&](i32 i) { p.installedModules.push_back(i); });
    read_ids(
        "skills", [&](const std::string& s) { return db.skill(s); },
        [&](i32 i) { p.unlockedSkills.insert(i); });
    read_ids(
        "research", [&](const std::string& s) { return db.research_node(s); },
        [&](i32 i) { p.completedResearch.insert(i); });
    read_ids(
        "unlocked_biomes", [&](const std::string& s) { return db.biome(s); },
        [&](i32 i) { p.unlockedBiomes.insert(i); });

    if (jp.contains("loadout_presets")) {
        p.loadoutPresets.clear();
        for (const auto& preset : jp["loadout_presets"]) {
            std::vector<i32> list;
            for (const auto& e : preset) {
                const auto h = db.module(e.get<std::string>());
                if (h.valid()) {
                    list.push_back(h.index);
                }
            }
            p.loadoutPresets.push_back(std::move(list));
        }
    }
    if (jp.contains("codex")) {
        for (const auto& e : jp["codex"]) {
            const auto h = db.material(e.value("id", std::string{}));
            if (!h.valid()) {
                continue;
            }
            CodexEntry entry;
            entry.material = h.index;
            entry.minedCount = e.value("mined", static_cast<i64>(0));
            entry.firstDepth = e.value("first_depth", static_cast<i64>(0));
            p.codex[h.index] = entry;
        }
    }
    if (jp.contains("discovered_landmarks")) {
        for (const auto& e : jp["discovered_landmarks"]) {
            p.discoveredLandmarks.insert(e.get<i64>());
        }
    }
    if (jp.contains("transit_nodes")) {
        for (const auto& e : jp["transit_nodes"]) {
            p.activatedTransitNodes.insert(e.get<u64>());
        }
    }
    if (jp.contains("transit_node_positions")) {
        for (const auto& e : jp["transit_node_positions"]) {
            p.add_transit_node(e.value("id", static_cast<u64>(0)), e.value("x", static_cast<i64>(0)),
                               e.value("y", static_cast<i64>(0)));
        }
    }
    auto decode_contract = [&](const json& jc) {
        ActiveContract c;
        const auto th = db.contract(jc.value("template", std::string{}));
        c.templateIndex = th.index;
        const auto mh = db.material(jc.value("material", std::string{}));
        c.targetMaterial = mh.index;
        c.targetAmount = jc.value("amount", 0);
        c.targetDepth = jc.value("depth", 0);
        c.progress = jc.value("progress", 0);
        c.completed = jc.value("completed", false);
        c.claimed = jc.value("claimed", false);
        c.instanceId = jc.value("instance", static_cast<u64>(0));
        c.forbiddenSlot = jc.value("forbidden_slot", -1);
        return c;
    };
    if (jp.contains("contracts")) {
        for (const auto& jc : jp["contracts"]) {
            auto c = decode_contract(jc);
            if (c.templateIndex >= 0) {
                p.contracts.push_back(c);
            } else {
                warnings.add(Error{"active contract template no longer exists and was dropped",
                                   "save",
                                   jc.value("template", std::string{}),
                                   "contracts",
                                   {}});
            }
        }
    }
    if (jp.contains("completed_contracts")) {
        for (const auto& jc : jp["completed_contracts"]) {
            auto c = decode_contract(jc);
            if (c.templateIndex >= 0) {
                p.completedContracts.push_back(c);
            }
        }
    }
    if (jp.contains("vault")) {
        decode_inventory(db, jp["vault"], p.vault, warnings);
    }
    if (jp.contains("samples")) {
        decode_inventory(db, jp["samples"], p.samples, warnings);
    }
    if (jp.contains("stats")) {
        const auto& js = jp["stats"];
        p.stats.tilesMined = js.value("tiles_mined", static_cast<i64>(0));
        p.stats.creditsEarned = js.value("credits_earned", static_cast<i64>(0));
        p.stats.creditsSpent = js.value("credits_spent", static_cast<i64>(0));
        p.stats.expeditions = js.value("expeditions", static_cast<i64>(0));
        p.stats.rescues = js.value("rescues", static_cast<i64>(0));
        p.stats.deepestDepth = js.value("deepest_depth", static_cast<i64>(0));
        p.stats.playSeconds = js.value("play_seconds", 0.0);
    }

    if (payload.contains("rig")) {
        const auto& jr = payload["rig"];
        data.rig.position = Vec2{jr.value("x", 0.0f), jr.value("y", 0.0f)};
        data.rig.velocity = Vec2{jr.value("vx", 0.0f), jr.value("vy", 0.0f)};
        data.rig.fuel = jr.value("fuel", 100.0f);
        data.rig.hull = jr.value("hull", 100.0f);
        data.rig.heat = jr.value("heat", 0.0f);
        data.rig.mode = static_cast<RigMode>(jr.value("mode", 0));
        data.rig.deepestThisRun = jr.value("deepest_run", static_cast<i64>(0));
    }
    if (payload.contains("cargo")) {
        decode_inventory(db, payload["cargo"], data.cargo, warnings);
    }
    if (payload.contains("samples")) {
        decode_inventory(db, payload["samples"], data.samples, warnings);
    }
    if (payload.contains("world_deltas")) {
        data.worldDeltas = decode_deltas(payload["world_deltas"], warnings);
    }

    for (const auto& w : warnings.all()) {
        DF_LOG_WARN("save", "{}", w.to_string());
    }
    return data;
}

Status SaveSystem::write(const ContentDatabase& db, int slot, const SaveData& data) const {
    std::error_code ec;
    std::filesystem::create_directories(mDir, ec);
    if (ec) {
        return fail(Error{"cannot create save directory: " + ec.message(),
                          mDir.string(),
                          {},
                          {},
                          "check that the user data directory is writable"});
    }
    auto text = serialise(db, data);
    if (!text) {
        return std::unexpected(text.error());
    }

    const auto target = slot_path(slot);
    const auto temp = mDir / ("slot" + std::to_string(slot) + ".dfsave.tmp");
    {
        // SEC-06: predictable temp name, same trap as the settings path — a
        // planted symlink here sends the bytes wherever it points and the
        // rename moves the link into the slot. Refuse before opening.
        std::error_code linkEc;
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(temp, linkEc))) {
            return fail(Error{"refusing to write the save through a symlinked temp file",
                              temp.string(),
                              {},
                              {},
                              "delete the symlink and retry; the previous save is untouched"});
        }
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return fail(Error{"cannot open temporary save file",
                              temp.string(),
                              {},
                              {},
                              "the save directory may be read-only or full"});
        }
        out << *text;
        out.flush();
        if (!out) {
            return fail(Error{"failed while writing the temporary save file",
                              temp.string(),
                              {},
                              {},
                              "the disk may be full; the previous save is untouched"});
        }
    }
    // Validate the temp file before it is allowed to replace anything.
    {
        auto reread = read_text_file(temp);
        if (!reread) {
            return fail(reread.error());
        }
        auto check = deserialise(db, *reread);
        if (!check) {
            std::filesystem::remove(temp, ec);
            return fail(Error{"the new save failed validation and was discarded",
                              temp.string(),
                              {},
                              {},
                              check.error().message});
        }
    }
    // Rotate the previous good save into the backup slot.
    if (std::filesystem::exists(target, ec)) {
        std::filesystem::remove(backup_path(slot), ec);
        std::filesystem::rename(target, backup_path(slot), ec);
        if (ec) {
            std::filesystem::copy_file(target, backup_path(slot),
                                       std::filesystem::copy_options::overwrite_existing, ec);
            ec.clear();
        }
    }
    std::filesystem::rename(temp, target, ec);
    if (ec) {
        return fail(Error{"could not replace the save file: " + ec.message(),
                          target.string(),
                          {},
                          {},
                          "the previous save and its backup are intact"});
    }
    DF_LOG_INFO("save", "wrote slot {} ({} bytes)", slot, text->size());
    return {};
}

Result<SaveData> SaveSystem::read(const ContentDatabase& db, int slot, bool useBackup) const {
    const auto path = useBackup ? backup_path(slot) : slot_path(slot);
    auto text = read_text_file(path);
    if (!text) {
        return fail(Error{"save file not found", path.string(), {}, {}, {}});
    }
    return deserialise(db, *text);
}

SaveSlotInfo SaveSystem::inspect(const ContentDatabase& db, int slot) const {
    SaveSlotInfo info;
    info.slot = slot;
    std::error_code ec;
    info.backupAvailable = std::filesystem::exists(backup_path(slot), ec);
    if (!std::filesystem::exists(slot_path(slot), ec)) {
        return info;
    }
    info.exists = true;
    auto data = read(db, slot);
    if (!data) {
        info.damaged = true;
        info.summary = data.error().message;
        return info;
    }
    info.summary = data->summary;
    info.buildVersion = data->buildVersion;
    info.schemaVersion = data->saveSchemaVersion;
    info.timestamp = data->timestamp;
    info.playSeconds = data->playSeconds;
    info.credits = data->progress.credits;
    info.deepestDepth = data->progress.stats.deepestDepth;
    return info;
}

std::vector<SaveSlotInfo> SaveSystem::list_slots(const ContentDatabase& db, int maxSlots) const {
    std::vector<SaveSlotInfo> out;
    for (int i = 0; i < maxSlots; ++i) {
        out.push_back(inspect(db, i));
    }
    return out;
}

std::filesystem::path default_user_data_dir() {
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA")) {
        return std::filesystem::path(appdata) / "Deepforge";
    }
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME")) {
        return std::filesystem::path(xdg) / "deepforge";
    }
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".local" / "share" / "deepforge";
    }
#endif
    return std::filesystem::current_path() / "deepforge-data";
}

} // namespace df
