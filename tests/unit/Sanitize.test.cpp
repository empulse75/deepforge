// SEC-07 pinned at the escaper: log records are single physical lines, so a
// control character arriving from a file, a save, argv or env must reach the
// terminal, the sinks and the overlay as a printable escape — never as raw
// bytes that restyle the output or forge a record boundary. The P14 probe was
// an ANSI escape plus a newline in an untrusted name; that case lives here.
#include "TestSupport.hpp"

#include "core/Sanitize.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace df;

TEST_CASE("core/Sanitize passes clean text through untouched", "[core][sanitize]") {
    CHECK(escape_control("plain [brackets] and UTF-8 \xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80") ==
          "plain [brackets] and UTF-8 \xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80");
    CHECK(json_encode("ferrite_ore") == "\"ferrite_ore\"");
}

TEST_CASE("core/Sanitize escapes control characters, the newline included", "[core][sanitize]") {
    CHECK(escape_control("name\x1b[31mrestyled\n[INFO] [boot] forged") ==
          "name\\x1b[31mrestyled\\n[INFO] [boot] forged");
    CHECK(escape_control("tab\there\r") == "tab\\there\\r");
    CHECK(escape_control(std::string("nul\0del\x7f", 8)) == "nul\\x00del\\x7f");
}

TEST_CASE("core/Sanitize json_encode quotes whole and escapes inside", "[core][sanitize]") {
    CHECK(json_encode("say \"hi\"\n") == "\"say \\\"hi\\\"\\n\"");
    CHECK(json_encode("back\\slash") == "\"back\\\\slash\"");
    CHECK(json_encode("\x1b") == "\"\\u001b\"");
    CHECK(json_encode("") == "\"\"");
}
