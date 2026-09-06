#include "TestSupport.hpp"

#include "app/SaveFile.hpp"
#include "core/Version.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>

using namespace df;

namespace {

SaveData sample_save(const ContentDatabase& db) {
    SaveData data;
    data.worldSeed = 0xDEADBEEFCAFEull;
    data.worldName = "Test World";
    data.playSeconds = 1234.5;
    data.progress.initialise(db);
    data.progress.credits = 4321;
    data.progress.insight = 17;
    data.progress.skillPointsEarned = 6;
    data.progress.skillPointsSpent = 2;
    data.progress.equipmentTier[0] = 3;
    data.progress.facilityTier[0] = 2;
    if (!data.progress.reputation.empty()) {
        data.progress.reputation[0] = 55;
    }
    data.progress.ownedModules.insert(db.module("module.focused_cutter").index);
    data.progress.installedModules.push_back(db.module("module.focused_cutter").index);
    data.progress.unlockedSkills.insert(db.skill("skill.keen_eye").index);
    data.progress.completedResearch.insert(db.research_node("research.field_metallurgy").index);
    data.progress.vault.set_capacity(1e9f);
    data.progress.vault.add(db, db.material("material.ferrite").index, 42, false);
    data.progress.samples.set_capacity(1e9f);
    data.progress.samples.add(db, db.material("material.echo_shard").index, 1, false);
    data.progress.discoveredLandmarks.insert(987654321);
    data.progress.add_transit_node(112233ull, -37, 940);
    data.progress.stats.tilesMined = 999;
    data.progress.stats.deepestDepth = 812;
    data.progress.codex[db.material("material.ferrite").index] =
        CodexEntry{db.material("material.ferrite").index, 500, 12};

    data.rig.position = Vec2{12.5f, 340.25f};
    data.rig.fuel = 55.5f;
    data.rig.hull = 71.25f;
    data.cargo.set_capacity(60.0f);
    data.cargo.add(db, db.material("material.cuprite").index, 7);
    data.samples.set_capacity(4.0f);

    ChunkDelta delta;
    delta.coord = ChunkCoord{-2, 7};
    delta.tiles.push_back(ChunkDelta::TileChange{45, 0, 4});
    delta.mining.push_back(MiningProgress{46, 0.5f, 0.0f});
    delta.landmarks.push_back(ChunkDelta::LandmarkState{5150, true, false});
    delta.revealed[3] = 0xFF;
    delta.hasRevealed = true;
    data.worldDeltas.push_back(delta);
    return data;
}

} // namespace

TEST_CASE("save round trips every field", "[save]") {
    const auto& db = dft::shipped_content();
    SaveSystem saves(dft::temp_dir("save_roundtrip"));
    const SaveData original = sample_save(db);

    auto text = saves.serialise(db, original);
    REQUIRE(text.has_value());
    auto restored = saves.deserialise(db, *text);
    REQUIRE(restored.has_value());

    CHECK(restored->worldSeed == original.worldSeed);
    CHECK(restored->worldName == original.worldName);
    CHECK(restored->playSeconds == original.playSeconds);
    CHECK(restored->progress.credits == original.progress.credits);
    CHECK(restored->progress.insight == original.progress.insight);
    CHECK(restored->progress.skillPointsEarned == original.progress.skillPointsEarned);
    CHECK(restored->progress.equipmentTier == original.progress.equipmentTier);
    CHECK(restored->progress.facilityTier == original.progress.facilityTier);
    CHECK(restored->progress.reputation == original.progress.reputation);
    CHECK(restored->progress.ownedModules == original.progress.ownedModules);
    CHECK(restored->progress.installedModules == original.progress.installedModules);
    CHECK(restored->progress.unlockedSkills == original.progress.unlockedSkills);
    CHECK(restored->progress.completedResearch == original.progress.completedResearch);
    CHECK(restored->progress.vault.stacks() == original.progress.vault.stacks());
    CHECK(restored->progress.samples.stacks() == original.progress.samples.stacks());
    CHECK(restored->progress.discoveredLandmarks == original.progress.discoveredLandmarks);
    CHECK(restored->progress.activatedTransitNodes == original.progress.activatedTransitNodes);
    REQUIRE(restored->progress.transitNodes.size() == original.progress.transitNodes.size());
    CHECK(restored->progress.transitNodes[0].stableId == 112233ull);
    CHECK(restored->progress.transitNodes[0].x == -37);
    CHECK(restored->progress.transitNodes[0].y == 940);
    CHECK(restored->progress.stats.tilesMined == original.progress.stats.tilesMined);
    CHECK(restored->progress.stats.deepestDepth == original.progress.stats.deepestDepth);
    CHECK(restored->progress.codex.size() == original.progress.codex.size());
    CHECK(restored->rig.position == original.rig.position);
    CHECK(restored->rig.fuel == original.rig.fuel);
    CHECK(restored->cargo.stacks() == original.cargo.stacks());

    REQUIRE(restored->worldDeltas.size() == 1);
    const auto& d = restored->worldDeltas[0];
    CHECK(d.coord == original.worldDeltas[0].coord);
    REQUIRE(d.tiles.size() == 1);
    CHECK(d.tiles[0].localIndex == 45);
    REQUIRE(d.mining.size() == 1);
    CHECK(d.mining[0].localIndex == 46);
    REQUIRE(d.landmarks.size() == 1);
    CHECK(d.landmarks[0].stableId == 5150);
    CHECK(d.hasRevealed);
    CHECK(d.revealed[3] == 0xFF);
}

TEST_CASE("writing a save is atomic and keeps a backup", "[save][atomic]") {
    const auto& db = dft::shipped_content();
    const auto dir = dft::temp_dir("save_atomic");
    SaveSystem saves(dir);

    SaveData first = sample_save(db);
    first.progress.credits = 111;
    REQUIRE(saves.write(db, 1, first).has_value());
    CHECK(std::filesystem::exists(saves.slot_path(1)));
    CHECK_FALSE(std::filesystem::exists(saves.backup_path(1)));

    SaveData second = first;
    second.progress.credits = 222;
    REQUIRE(saves.write(db, 1, second).has_value());
    CHECK(std::filesystem::exists(saves.backup_path(1)));

    auto current = saves.read(db, 1);
    REQUIRE(current.has_value());
    CHECK(current->progress.credits == 222);
    auto backup = saves.read(db, 1, true);
    REQUIRE(backup.has_value());
    CHECK(backup->progress.credits == 111);

    // No temporary files are left behind.
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        CHECK(e.path().extension() != ".tmp");
    }
}

TEST_CASE("a corrupted save is reported and leaves the file untouched", "[save][recovery]") {
    const auto& db = dft::shipped_content();
    const auto dir = dft::temp_dir("save_corrupt");
    SaveSystem saves(dir);
    SaveData data = sample_save(db);
    REQUIRE(saves.write(db, 2, data).has_value());
    data.progress.credits = 777;
    REQUIRE(saves.write(db, 2, data).has_value()); // creates a good backup

    // Truncate the main file.
    {
        std::ofstream out(saves.slot_path(2), std::ios::trunc);
        out << "{\"save_schema_version\": 3, \"payload\": {\"world_seed\": 1}, \"checksum\": 12345}";
    }
    auto broken = saves.read(db, 2);
    REQUIRE_FALSE(broken.has_value());
    CHECK(broken.error().message.find("checksum") != std::string::npos);

    const auto info = saves.inspect(db, 2);
    CHECK(info.damaged);
    CHECK(info.backupAvailable);

    auto recovered = saves.read(db, 2, true);
    REQUIRE(recovered.has_value());
    CHECK(recovered->progress.credits == 4321);

    // The damaged file must still be on disk for the player to inspect.
    CHECK(std::filesystem::file_size(saves.slot_path(2)) > 0);
}

TEST_CASE("garbage input never crashes the loader", "[save][recovery]") {
    const auto& db = dft::shipped_content();
    SaveSystem saves(dft::temp_dir("save_garbage"));
    for (const char* text : {"", "{", "not json at all", "[]", "{\"payload\": 3}",
                             "{\"save_schema_version\": 99, \"payload\": {}}"}) {
        auto result = saves.deserialise(db, text);
        CHECK_FALSE(result.has_value());
        CHECK_FALSE(result.error().message.empty());
    }
}

TEST_CASE("saves from every released schema version migrate", "[save][migration]") {
    const auto& db = dft::shipped_content();
    SaveSystem saves(dft::temp_dir("save_migration"));

    // A schema-1 fixture: no samples container, no presets, no reputation.
    const std::string payloadV1 = R"({
      "world_seed": 4242,
      "world_name": "Legacy",
      "play_seconds": 90.0,
      "progress": {
        "credits": 500,
        "insight": 3,
        "equipment": {"equipment.fuel_tank": 2},
        "facilities": {},
        "owned_modules": ["module.focused_cutter"],
        "installed_modules": ["module.focused_cutter"],
        "skills": ["skill.keen_eye"],
        "research": ["research.field_metallurgy"],
        "vault": [{"id": "material.ferrite", "amount": 10}],
        "stats": {"deepest_depth": 300, "tiles_mined": 120}
      },
      "rig": {"x": 1.0, "y": 20.0, "fuel": 30.0, "hull": 80.0},
      "cargo": [{"id": "material.cuprite", "amount": 4}],
      "world_deltas": []
    })";

    auto make = [&](int version, const std::string& payload) {
        std::ostringstream os;
        os << "{\"save_schema_version\": " << version
           << ", \"build_version\": \"test\", \"timestamp\": 1, \"summary\": \"legacy\", \"payload\": "
           << payload << "}";
        return os.str();
    };

    for (int version = 1; version < kSaveSchemaVersion; ++version) {
        INFO("migrating from schema " << version);
        auto migrated = saves.deserialise(db, make(version, payloadV1));
        REQUIRE(migrated.has_value());
        CHECK(migrated->worldSeed == 4242);
        CHECK(migrated->progress.credits == 500);
        CHECK(migrated->progress
                  .equipmentTier[static_cast<usize>(db.equipment_track("equipment.fuel_tank").index)] == 2);
        CHECK(migrated->progress.unlockedSkills.size() == 1);
        CHECK(migrated->progress.stats.deepestDepth == 300);
        // Fields introduced later must be filled with usable defaults.
        CHECK(migrated->progress.loadoutPresets.size() >= 1);
        CHECK(migrated->progress.reputation.size() == db.factions().size());
        CHECK(migrated->progress.maxUtilityPower > 0);
        // Schema 4 added transit node positions; an older save simply has none
        // yet, and nothing it did contain is lost.
        CHECK(migrated->progress.transitNodes.empty());
    }
}

TEST_CASE("a save from a newer schema is refused with a clear message", "[save][migration]") {
    const auto& db = dft::shipped_content();
    SaveSystem saves(dft::temp_dir("save_future"));
    const std::string future = "{\"save_schema_version\": " + std::to_string(kSaveSchemaVersion + 5) +
                               ", \"payload\": {\"world_seed\": 1, \"progress\": {}}}";
    auto result = saves.deserialise(db, future);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().message.find("newer version") != std::string::npos);
}

TEST_CASE("content that disappeared is dropped rather than crashing", "[save][recovery]") {
    const auto& db = dft::shipped_content();
    SaveSystem saves(dft::temp_dir("save_missing_content"));
    SaveData data = sample_save(db);
    auto text = saves.serialise(db, data);
    REQUIRE(text.has_value());
    // Rewrite one id to something that no longer exists, and fix the checksum by
    // stripping it (the loader treats a zero checksum as unverified).
    std::string mutated = *text;
    const auto pos = mutated.find("material.ferrite");
    REQUIRE(pos != std::string::npos);
    mutated.replace(pos, std::string("material.ferrite").size(), "material.removedxx");
    const auto ck = mutated.find("\"checksum\"");
    REQUIRE(ck != std::string::npos);
    const auto colon = mutated.find(':', ck);
    const auto comma = mutated.find(',', colon);
    mutated.replace(colon + 1, comma - colon - 1, " 0");

    auto restored = saves.deserialise(db, mutated);
    REQUIRE(restored.has_value());
    // The unknown stack is gone; everything else survived.
    CHECK(restored->progress.credits == data.progress.credits);
}
