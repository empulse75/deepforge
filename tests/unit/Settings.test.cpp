// load_settings() has to distinguish "there is no settings file yet" from
// "the settings file is corrupt": the first is an ordinary first-run state, the
// second needs the user to delete the file. Both used to collapse into one
// error, so the two cases are pinned by exact message here. The file is officially
// user-editable, so a hand-edit that puts the wrong type somewhere is a third case
// — and it has to fail with a named diagnosis and that remedy, not with a nlohmann
// type_error in std::terminate (SEC-01/QUAL-01).
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

TEST_CASE("platform/Settings a missing settings file is reported as not found", "[platform][settings]") {
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

TEST_CASE("platform/Settings a malformed settings file is reported as invalid JSON", "[platform][settings]") {
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

TEST_CASE("platform/Settings a section that is not an object is a named failure, not a crash",
          "[platform][settings]") {
    // The audit's planted input: valid JSON, one section written as an array.
    const auto path = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "settings_section_array.json";
    std::filesystem::create_directories(path.parent_path());
    write_text(path, R"({"version":1,"video":[1,2,3]})");

    ErrorList warnings;
    auto loaded = load_settings(path, &warnings);
    REQUIRE_FALSE(loaded.has_value());
    CHECK(loaded.error().file == path.string());
    CHECK(loaded.error().definition == "video");
    CHECK(loaded.error().message.find("object") != std::string::npos);
    // The point of the whole finding: the advice must survive to reach the player.
    CHECK(loaded.error().suggestion == "delete it to restore defaults");
    CHECK(warnings.empty()); // a wrong type is fatal, not a fallback
}

TEST_CASE("platform/Settings a string where a number belongs is a named failure", "[platform][settings]") {
    const auto path = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "settings_string_value.json";
    std::filesystem::create_directories(path.parent_path());
    write_text(path, R"({"version":1,"audio":{"master":"loud"}})");

    auto loaded = load_settings(path);
    REQUIRE_FALSE(loaded.has_value());
    CHECK(loaded.error().definition == "audio");
    CHECK(loaded.error().field == "master");
    CHECK(loaded.error().message.find("string") != std::string::npos);
    CHECK(loaded.error().suggestion == "delete it to restore defaults");
}

TEST_CASE("platform/Settings an out-of-domain value falls back to the default with a warning",
          "[platform][settings]") {
    // The audit's planted input: an 18-digit width used to narrow and be accepted
    // silently (window 1x900), and a 19-digit negative assist scale just landed.
    const auto path = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "settings_out_of_range.json";
    std::filesystem::create_directories(path.parent_path());
    write_text(
        path,
        R"({"version":1,"video":{"width":999999999999999999},"assists":{"damage":-9999999999999999999}})");

    ErrorList warnings;
    auto loaded = load_settings(path, &warnings);
    REQUIRE(loaded.has_value());              // the game still starts
    CHECK(loaded->video.windowWidth == 1600); // the documented default
    CHECK(loaded->assists.damageScale == 1.0f);
    REQUIRE(warnings.size() == 2);
    CHECK(warnings.all()[0].field == "width");
    CHECK(warnings.all()[1].field == "damage");
}

TEST_CASE("platform/Settings a whole-number float in an integer field is accepted", "[platform][settings]") {
    // JSON has no integer type; a text editor writing 1280.0 loses nothing, and the
    // pinned headers already read it back as 1280 — so it is not a failure.
    const auto path = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "settings_whole_float.json";
    std::filesystem::create_directories(path.parent_path());
    write_text(path, R"({"version":1,"video":{"width":1280.0}})");

    ErrorList warnings;
    auto loaded = load_settings(path, &warnings);
    REQUIRE(loaded.has_value());
    CHECK(loaded->video.windowWidth == 1280);
    CHECK(warnings.empty());
}

TEST_CASE("platform/Settings a zoom off the documented steps warns and keeps the default",
          "[platform][settings]") {
    const auto path = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "settings_zoom.json";
    std::filesystem::create_directories(path.parent_path());
    write_text(path, R"({"version":1,"video":{"zoom":1.5}})");

    ErrorList warnings;
    auto loaded = load_settings(path, &warnings);
    REQUIRE(loaded.has_value());
    CHECK(loaded->video.zoom == 1.0f); // SPEC documents integer steps 1x and 2x
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.all()[0].field == "zoom");
}

TEST_CASE("platform/Settings a settings file over its documented ceiling is refused",
          "[platform][settings]") {
    const auto path = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "settings_oversize.json";
    std::filesystem::create_directories(path.parent_path());
    write_text(path,
               std::string("{\"version\":1,\"video\":{\"zoom\":\"") + std::string(1'000'002, 'x') + "\"}}");

    auto loaded = load_settings(path);
    REQUIRE_FALSE(loaded.has_value());
    CHECK(loaded.error().field == "size");
    CHECK(loaded.error().file == path.string());
    CHECK(loaded.error().suggestion == "delete it to restore defaults");
}

TEST_CASE("platform/Settings refuses to write through a symlinked temp file", "[platform][settings]") {
    // SEC-06 measured: the fixed name settings.json.tmp is the seam — the
    // open follows a planted symlink, the settings text lands wherever the
    // link points, and the rename moves the link into place. The write must
    // refuse; the settings file itself may legitimately be a symlink (a
    // dotfiles repo), the temp file may not.
    const auto dir = dft::temp_dir("settings_symlinked_temp");
    const auto path = dir / "settings.json";
    const auto victim = dir / "victim.txt";
    {
        std::ofstream v(victim, std::ios::binary | std::ios::trunc);
        REQUIRE(v.good());
    }
    std::filesystem::create_symlink(victim, path.string() + ".tmp");

    auto saved = save_settings(path, default_settings());
    REQUIRE_FALSE(saved.has_value());
    CHECK(saved.error().message == "refusing to write settings through a symlinked temp file");
    CHECK(std::filesystem::is_symlink(path.string() + ".tmp")); // still there, untouched
    CHECK(std::filesystem::file_size(victim) == 0);
    REQUIRE_FALSE(std::filesystem::exists(path));

    std::filesystem::remove(path.string() + ".tmp");
    REQUIRE(save_settings(path, default_settings()).has_value()); // plain path still works
    REQUIRE(std::filesystem::exists(path));
    auto loaded = load_settings(path);
    REQUIRE(loaded.has_value());
}
