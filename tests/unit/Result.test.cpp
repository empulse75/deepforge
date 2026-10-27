// Error::to_string interpolates fields that carry strings from data files
// and saves (SEC-07), and this rendering reaches terminals directly — every
// interpolation is JSON-encoded so a crafted id cannot forge a line. Clean
// inputs pin the exact rendering here; tests elsewhere check substrings.
#include "TestSupport.hpp"

#include "core/Result.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace df;

TEST_CASE("core/Result renders every interpolated field quoted", "[core][result]") {
    Error full{"expected a string", "settings.json", "video", "master", "delete it to restore defaults"};
    CHECK(full.to_string() ==
          "\"settings.json\" [\"video\"] .\"master\": \"expected a string\" (\"delete it to restore "
          "defaults\")");

    Error noFile{"unknown id", "", "material.ferrite", "name_key", ""};
    CHECK(noFile.to_string() == "\"material.ferrite\".\"name_key\": \"unknown id\"");
}

TEST_CASE("core/Result renders an untrusted id without raw control bytes", "[core][result]") {
    // The save-file variant: an unknown id straight from the file lands in
    // `definition` (SaveFile puts it there), ESC and newline and all.
    Error e{"field cannot be used", "save", "ferrite\x1b[31m\nx", "contracts", ""};
    CHECK(e.to_string() == "\"save\" [\"ferrite\\u001b[31m\\nx\"] .\"contracts\": \"field cannot be used\"");
    CHECK(e.to_string().find('\n') == std::string::npos);
    CHECK(e.to_string().find('\x1b') == std::string::npos);
}
