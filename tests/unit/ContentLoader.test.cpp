// QUAL-01: the load boundary used to abort the process on a type mismatch and say
// nothing when it truncated. A string element inside a colour array aborted with a
// nlohmann type_error; a float in an integer field was silently truncated and the
// validator exited 0. CONTRIBUTING's rule — "prefer a loud, named failure at load"
// — is exactly what broke here, so the boundary now reports through the existing
// Ctx/ErrorList machinery and the wrong-type corpus is tested. The shipped corpus
// has no present-wrong-type values, so well-formed input is unchanged.
#include "TestSupport.hpp"

#include "content/ContentDatabase.hpp"

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

using namespace df;

namespace {

std::string read_all(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE(in.good());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/// The shipped corpus, as the in-memory shape the loader takes, with one field of
/// one definition replaced by `mutate`'s result.
std::unordered_map<std::string, std::string> corpus(const std::string& defId, const std::string& field,
                                                    const nlohmann::json& value) {
    const std::string fileName = "materials.json";
    std::unordered_map<std::string, std::string> files;
    for (const auto& e : std::filesystem::directory_iterator(DEEPFORGE_DATA_DIR)) {
        if (e.path().extension() != ".json") {
            continue;
        }
        files[e.path().filename().string()] = read_all(e.path());
    }
    auto doc = nlohmann::json::parse(files[fileName]);
    for (auto& def : doc["materials"]) {
        if (def.value("id", "") == defId) {
            def[field] = value;
            break;
        }
    }
    files[fileName] = doc.dump();
    return files;
}

bool reports(const ErrorList& errors, const std::string& file, const std::string& field) {
    for (const auto& e : errors.all()) {
        if (e.file == file && e.field == field) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("content loader a string inside a colour array is a named failure, not a crash",
          "[content][boundary]") {
    // The audit's planted mutation, verbatim.
    auto files = corpus("material.ferrite", "color", nlohmann::json::parse(R"(["x", 8, 12])"));

    ErrorList errors;
    auto db = ContentDatabase::load_from_strings(files, errors);
    REQUIRE_FALSE(db.has_value());
    REQUIRE_FALSE(errors.empty());
    CHECK(reports(errors, "materials.json", "color"));
    // The rest of the load still runs, so the author sees every problem at once.
    CHECK_FALSE(errors.to_string().find("integer") == std::string::npos);
}

TEST_CASE("content loader a float in an integer field is reported, not silently truncated",
          "[content][boundary]") {
    // The audit's planted mutation: base_value 2.5 used to pass validation with no
    // complaint and land as 2. The choice made on this card is an explicit error,
    // pinned here.
    auto files = corpus("material.ferrite", "base_value", nlohmann::json::parse("2.5"));

    ErrorList errors;
    auto db = ContentDatabase::load_from_strings(files, errors);
    REQUIRE_FALSE(db.has_value());
    CHECK(reports(errors, "materials.json", "base_value"));
}

TEST_CASE("content loader the shipped corpus still loads clean through the checked boundary",
          "[content][boundary]") {
    std::unordered_map<std::string, std::string> files;
    for (const auto& e : std::filesystem::directory_iterator(DEEPFORGE_DATA_DIR)) {
        if (e.path().extension() == ".json") {
            files[e.path().filename().string()] = read_all(e.path());
        }
    }
    REQUIRE_FALSE(files.empty());

    ErrorList errors;
    auto db = ContentDatabase::load_from_strings(files, errors);
    REQUIRE(db.has_value());
    CHECK(errors.empty());
}

TEST_CASE("content loader an integer field given a whole-number float is still accepted",
          "[content][boundary]") {
    // A text editor writes 1280.0 for an integer field innocently; against the
    // pinned headers that value already read back as 1280 with nothing lost, so it
    // is accepted — it is the fractional case above that has to be loud.
    auto files = corpus("material.ferrite", "base_value", nlohmann::json::parse("8.0"));

    ErrorList errors;
    auto db = ContentDatabase::load_from_strings(files, errors);
    REQUIRE(db.has_value());
    CHECK(errors.empty());
}
