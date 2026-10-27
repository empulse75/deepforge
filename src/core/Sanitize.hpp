// Escaping for every string that came in from outside: data files, saves,
// argv, env. A log record is one physical line, so a control character —
// ESC above all — must stop being a raw byte before it reaches a terminal,
// the log file or the debug overlay, or a crafted name can restyle the
// output and forge a line indistinguishable from a real entry (SEC-07).
#pragma once

#include <string>
#include <string_view>

namespace df {

/// Returns the text unchanged when it is free of ASCII control characters;
/// otherwise each control character becomes a printable backslash escape
/// (\n, \t, \r, and \xHH for the rest). Bytes >= 0x80 pass through: in
/// UTF-8 they are part of real text, not C1 control codes.
std::string escape_control(std::string_view s);

/// Full JSON string encoding, quotes included — what the error renderer
/// uses when it interpolates fields an untrusted file put there.
std::string json_encode(std::string_view s);

} // namespace df
