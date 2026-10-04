// The shared JSON/file-read plumbing (core/JsonFile) replaced five
// near-identical copies across content/store/app/platform. These cases pin
// the behaviour every caller depends on: non-throwing parse, error reporting,
// and the comment flag the content/store loaders use.
#include "TestSupport.hpp"

#include "core/JsonFile.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>

using namespace df;

namespace {

void write_file(const std::filesystem::path& p, const std::string& text) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out << text;
}

} // namespace

TEST_CASE("core/JsonFile parses valid JSON", "[core][json]") {
    auto parsed = parse_json(R"({"a": 1, "b": [true, null]})");
    REQUIRE(parsed.has_value());
    CHECK(parsed->at("a").get<int>() == 1);
    CHECK(parsed->at("b").size() == 2);
}

TEST_CASE("core/JsonFile reports invalid JSON as an Error, never an exception", "[core][json]") {
    auto parsed = parse_json("{oops");
    REQUIRE_FALSE(parsed.has_value());
    CHECK_FALSE(parsed.error().message.empty());
}

TEST_CASE("core/JsonFile honours the ignore_comments flag", "[core][json]") {
    const std::string text = R"({"a": 1 /* ok */})";
    CHECK_FALSE(parse_json(text).has_value()); // strict: comments rejected
    CHECK(parse_json(text, /*ignore_comments=*/true).has_value());
}

TEST_CASE("core/JsonFile reads whole files verbatim", "[core][json]") {
    const auto path = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "jsonfile_read.txt";
    std::filesystem::create_directories(path.parent_path());
    write_file(path, "line one\nline two\n");
    auto text = read_text_file(path);
    REQUIRE(text.has_value());
    CHECK(*text == "line one\nline two\n");
}

TEST_CASE("core/JsonFile read failures carry the file path", "[core][json]") {
    const auto path = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "jsonfile_missing.json";
    std::filesystem::remove(path);
    auto parsed = read_json_file(path);
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error().file == path.string());
}

TEST_CASE("core/JsonFile read_json_file parses a file on disk", "[core][json]") {
    const auto path = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "jsonfile_ok.json";
    std::filesystem::create_directories(path.parent_path());
    write_file(path, R"({"hello": "world"})");
    auto parsed = read_json_file(path);
    REQUIRE(parsed.has_value());
    CHECK(parsed->at("hello").get<std::string>() == "world");
}
