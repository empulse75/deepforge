// load_settings() has to distinguish "there is no settings file yet" from
// "the settings file is corrupt": the first is an ordinary first-run state, the
// second needs the user to delete the file. Both used to collapse into one
// error, so the two cases are pinned by exact message here.
#include "TestSupport.hpp"

#include "platform/Settings.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using namespace df;

namespace {

void write_text(const std::filesystem::path& p, const std::string& text) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out << text;
}

} // namespace

TEST_CASE("platform/Settings a missing settings file is reported as not found",
          "[platform][settings]") {
    const auto path = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "settings_absent.json";
    std::filesystem::remove(path);
    REQUIRE_FALSE(std::filesystem::exists(path));

    auto loaded = load_settings(path);
    REQUIRE_FALSE(loaded.has_value());
    // Exact message: this is the diagnosis the caller shows the player.
    CHECK(loaded.error().message == "settings file not found");
    CHECK(loaded.error().file == path.string());
    // Nothing to delete, so no remedy is offered.
    CHECK(loaded.error().suggestion.empty());
}

TEST_CASE("platform/Settings a malformed settings file is reported as invalid JSON",
          "[platform][settings]") {
    const auto path = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "settings_malformed.json";
    std::filesystem::create_directories(path.parent_path());
    write_text(path, "{ this is not json");

    auto loaded = load_settings(path);
    REQUIRE_FALSE(loaded.has_value());
    // Distinct from the missing-file error, and it carries the remedy.
    CHECK(loaded.error().message == "settings file is not valid JSON");
    CHECK(loaded.error().suggestion == "delete it to restore defaults");
}

TEST_CASE("platform/Settings a well-formed settings file loads", "[platform][settings]") {
    const auto path = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "settings_ok.json";
    std::filesystem::create_directories(path.parent_path());
    write_text(path, R"({"video": {"width": 1280, "height": 720, "vsync": false}})");

    auto loaded = load_settings(path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->video.windowWidth == 1280);
    CHECK(loaded->video.windowHeight == 720);
    CHECK_FALSE(loaded->video.vsync);
}