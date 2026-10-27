// OPS-OBS-01: the previous session's log is the only evidence after a crash,
// so it must survive the next start — renamed to <path>.1, not truncated.
// A size cap bounds the live file for long sessions, the per-line flush
// stays, and the file format now matches the console line for line.
#include "TestSupport.hpp"

#include "core/FileSink.hpp"
#include "core/Log.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace df;

namespace {

std::string slurp(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream os;
    os << in.rdbuf();
    return os.str();
}

void seed(const std::filesystem::path& p, const std::string& text) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out << text;
}

} // namespace

TEST_CASE("core/FileSink keeps the previous session's log on startup", "[core][log][rotation]") {
    const auto dir = dft::temp_dir("filesink_startup");
    seed(dir / "deepforge.log", "[3] [boot] the previous session was here\n");

    FileSink sink(dir / "deepforge.log");
    sink.write(LogLevel::Info, "boot", "this session starts");

    CHECK(slurp(dir / "deepforge.log.1") == "[3] [boot] the previous session was here\n");
    CHECK(slurp(dir / "deepforge.log") == "[INFO] [boot] this session starts\n"); // aligned format
}

TEST_CASE("core/FileSink rotates when the size cap is crossed", "[core][log][rotation]") {
    const auto dir = dft::temp_dir("filesink_cap");
    FileSink sink(dir / "deepforge.log", 40);
    sink.write(LogLevel::Info, "t", "one");   // 15 bytes
    sink.write(LogLevel::Info, "t", "two");   // 30
    sink.write(LogLevel::Info, "t", "three"); // 47 — over the cap, rotates
    sink.write(LogLevel::Info, "t", "four");  // lands in the fresh file

    const std::string rotated = slurp(dir / "deepforge.log.1");
    CHECK(rotated == "[INFO] [t] one\n[INFO] [t] two\n[INFO] [t] three\n");
    CHECK(slurp(dir / "deepforge.log") == "[INFO] [t] four\n");
}

TEST_CASE("core/FileSink shows each line before the file is closed", "[core][log]") {
    const auto dir = dft::temp_dir("filesink_flush");
    FileSink sink(dir / "deepforge.log");
    sink.write(LogLevel::Warn, "t", "flushed now");
    CHECK(slurp(dir / "deepforge.log") == "[WARN] [t] flushed now\n");
}

TEST_CASE("core/FileSink appends when rotation is impossible", "[core][log][rotation]") {
    const auto dir = dft::temp_dir("filesink_stuck");
    seed(dir / "deepforge.log", "old evidence\n");
    std::filesystem::create_directories(dir / "deepforge.log.1"); // rename onto a dir fails

    FileSink stuck(dir / "deepforge.log");
    stuck.write(LogLevel::Info, "t", "kept");
    const std::string all = slurp(dir / "deepforge.log");
    CHECK(all.find("old evidence") != std::string::npos);
    CHECK(all.find("kept") != std::string::npos);
}
