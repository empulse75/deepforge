// The shared JSON/file-read plumbing (core/JsonFile) replaced five near-identical
// copies across content/store/app/platform. These cases pin the behaviour every
// caller depends on — non-throwing parse, error reporting, the comment flag the
// content/store loaders use — plus the documented ingest ceiling (SEC-05), which
// lives here so it cannot drift between them: reading a whole file and building a
// parse tree costs roughly four times the file in resident memory, and a planted
// 200 MB file measured ~839 MB peak RSS. The ceiling has to fire before anything
// is read.
#include "TestSupport.hpp"

#include "core/JsonFile.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <string>

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

TEST_CASE("core/JsonFile refuses a file over its documented ceiling, before reading it", "[core][json]") {
    const auto path = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "jsonfile_over_ceiling.json";
    std::filesystem::create_directories(path.parent_path());
    write_file(path, std::string(4096, 'x'));

    const std::size_t ceiling = 1024;
    auto refused = read_text_file(path, ceiling);
    REQUIRE_FALSE(refused.has_value());
    // Machine-readable: the message names the ceiling and the size, the error says
    // which field was at fault, and the boundary's documented remedy travels with
    // the diagnosis so the player gets an action, not just a failure.
    CHECK(refused.error().field == "size");
    CHECK(refused.error().file == path.string());
    CHECK(refused.error().suggestion == "delete it to restore defaults");
    CHECK(refused.error().message.find(std::to_string(ceiling)) != std::string::npos);
    CHECK(refused.error().message.find(std::to_string(4096)) != std::string::npos);

    // Under the ceiling, unchanged — and exactly at it, since the domain is
    // inclusive. A file that cannot answer a size query is let through to the read.
    write_file(std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "jsonfile_at_ceiling.json",
               std::string(ceiling, 'x'));
    CHECK(read_text_file(std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / "jsonfile_at_ceiling.json", ceiling)
              .has_value());
    CHECK_FALSE(parse_json(std::string(4096, 'x'), false, ceiling).has_value());

    // The documented ceilings, pinned so the numbers cannot drift.
    CHECK(kIngestCeilingBytes == 8'000'000);
    CHECK(kIngestCeilingSettingsBytes == 1'000'000);
}

TEST_CASE("core/JsonFile the checked accessor reports a wrong type through the Error path", "[core][json]") {
    auto parsed = parse_json(R"({"n": 2.5, "i": 3, "whole": 1280.0, "s": "x", "b": true})");
    REQUIRE(parsed.has_value());

    ErrorList problems;
    const nlohmann::json* out = nullptr;

    // A missing key is normal, and silent.
    CHECK_FALSE(checked_read(*parsed, "absent", FieldType::Integer, out, "f", "d", problems));
    CHECK(out == nullptr);
    CHECK(problems.empty());

    // A fractional float in an integer field is the loud case (QUAL-01: this is the
    // value that used to be silently truncated to 2).
    CHECK_FALSE(checked_read(*parsed, "n", FieldType::Integer, out, "f", "d", problems));
    REQUIRE_FALSE(problems.empty());
    CHECK(problems.all().back().field == "n");
    CHECK(problems.all().back().message.find("float") != std::string::npos);

    // An integer and an exactly-whole float both pass; a number field takes either;
    // a string in a number field is reported.
    CHECK(checked_read(*parsed, "i", FieldType::Integer, out, "f", "d", problems));
    CHECK(out != nullptr);
    CHECK(checked_read(*parsed, "whole", FieldType::Integer, out, "f", "d", problems));
    CHECK(checked_read(*parsed, "i", FieldType::Number, out, "f", "d", problems));
    CHECK_FALSE(checked_read(*parsed, "s", FieldType::Number, out, "f", "d", problems));
    CHECK(checked_read(*parsed, "b", FieldType::Boolean, out, "f", "d", problems));
}
