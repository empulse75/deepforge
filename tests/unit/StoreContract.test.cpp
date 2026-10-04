// The non-P2W contract, as an executable invariant rather than a promise
// (SPEC.md, "Monetisation - non-P2W, testable"). Three checks:
//   1. every entitlement in the shipped catalog is cosmetic and carries no
//      sim-visible effect data;
//   2. the simulation layers (domain/world/content/core) contain zero
//      references to store identifiers - the store is parsed in its own layer
//      (src/store, ADR 10) so the spec's literal grep passes;
//   3. every cosmetic the store grants exists in cosmetics.json.
#include "TestSupport.hpp"

#include "core/Version.hpp"
#include "store/StoreCatalog.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace df;

namespace {

const StoreCatalog& shipped_store() {
    static const StoreCatalog catalog = [] {
        ErrorList errors;
        auto loaded = StoreCatalog::load(DEEPFORGE_DATA_DIR, errors);
        if (!loaded) {
            FAIL("store content failed to load:\n" << errors.to_string());
        }
        return std::move(*loaded);
    }();
    return catalog;
}

std::string read_all(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream os;
    os << in.rdbuf();
    return os.str();
}

} // namespace

TEST_CASE("every shipped entitlement is cosmetic with no sim-visible effects", "[store][p2w]") {
    const auto& catalog = shipped_store();
    REQUIRE_FALSE(catalog.entitlements().empty());

    for (const auto& e : catalog.entitlements()) {
        INFO("entitlement: " << e.id.text());
        CHECK(e.kind == EntitlementKind::Cosmetic);
        // "Effect data empty by construction" - the loader counted any effect
        // entries on the definition; a cosmetic must have none.
        CHECK(e.simVisibleEffects == 0);
        CHECK_FALSE(e.grants.empty());
    }
}

TEST_CASE("the entitlement type can express a non-cosmetic kind without a schema migration",
          "[store][p2w]") {
    // The kind enum is wider than what ships, on purpose: phase 2/3 categories
    // must be *expressible* now so a future pack does not force a content-schema
    // migration. Nothing in the shipped catalog may use them yet.
    CHECK(entitlement_kind_from_string("expansion") == EntitlementKind::Expansion);
    CHECK(entitlement_kind_from_string("convenience") == EntitlementKind::Convenience);
    CHECK(entitlement_kind_name(EntitlementKind::Expansion) == "expansion");
    for (const auto& e : shipped_store().entitlements()) {
        CHECK(e.kind == EntitlementKind::Cosmetic);
    }
}

TEST_CASE("the simulation layers are closed against the store", "[store][p2w]") {
    // The literal grep from the spec: src/domain src/world src/content src/core
    // must contain zero references to store identifiers. CI runs the same grep.
    // The store lives in src/store, above the closure, and nothing below it
    // links it (CMake: deepforge_store is linked only by tools and tests).
    const std::filesystem::path src{DEEPFORGE_SOURCE_DIR};
    const std::vector<std::string> closed = {"domain", "world", "content", "core"};
    const std::vector<std::string> needles = {"store_catalog", "entitlement"};
    std::string hits;
    for (const auto& dir : closed) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(src / dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const std::string text = read_all(entry.path());
            for (const auto& n : needles) {
                if (text.find(n) != std::string::npos) {
                    hits += "  " + entry.path().string() + ": " + n + "\n";
                }
            }
        }
    }
    INFO("references found in closed layers:\n" << hits);
    CHECK(hits.empty());
}

TEST_CASE("every cosmetic the store grants exists in cosmetics.json", "[store][p2w]") {
    const auto& catalog = shipped_store();
    REQUIRE_FALSE(catalog.cosmetics().empty());
    for (const auto& e : catalog.entitlements()) {
        for (const auto& g : e.grants) {
            INFO("grant " << g.text() << " from " << e.id.text());
            bool found = false;
            for (const auto& c : catalog.cosmetics()) {
                if (c.id == g) {
                    found = true;
                    break;
                }
            }
            CHECK(found);
        }
    }
    // Cosmetics are presentation data only: family is one of the three draw-time
    // families and every palette entry is a colour string.
    for (const auto& c : catalog.cosmetics()) {
        CHECK((c.family == "livery" || c.family == "trail" || c.family == "decor"));
        CHECK_FALSE(c.palette.empty());
    }
}

TEST_CASE("the save records which content pack generated the world", "[store][p2w][save]") {
    // Mandated from slice 1 so a later expansion pack cannot reinterpret a save
    // carved from different content.
    const auto& db = dft::shipped_content();
    SaveSystem saves(dft::temp_dir("store_content_pack"));
    SaveData data = SaveData{};
    data.worldSeed = 3407;
    data.progress.initialise(db);
    data.contentPackId = "base";
    auto text = saves.serialise(db, data);
    REQUIRE(text.has_value());
    auto back = saves.deserialise(db, *text);
    REQUIRE(back.has_value());
    CHECK(back->contentPackId == "base");
    CHECK(back->saveSchemaVersion == kSaveSchemaVersion);
    // A pre-v5 save migrates to the same default.
    const std::string legacy =
        "{\"save_schema_version\": 4, \"build_version\": \"test\", \"timestamp\": 1, \"payload\": "
        "{\"world_seed\": 1, \"progress\": {}}}";
    auto migrated = saves.deserialise(db, legacy);
    REQUIRE(migrated.has_value());
    CHECK(migrated->contentPackId == "base");
}
