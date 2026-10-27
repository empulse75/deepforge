#include "core/Sanitize.hpp"

namespace df {

namespace {

constexpr char kHex[] = "0123456789abcdef";

bool is_control(unsigned char c) {
    return c < 0x20 || c == 0x7F;
}

} // namespace

std::string escape_control(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const char raw : s) {
        const unsigned char c = static_cast<unsigned char>(raw);
        if (!is_control(c)) {
            out += raw;
            continue;
        }
        switch (c) {
            case '\n':
                out += "\\n";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\r':
                out += "\\r";
                break;
            default:
                out += "\\x";
                out += kHex[c >> 4];
                out += kHex[c & 0xF];
                break;
        }
    }
    return out;
}

std::string json_encode(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (const char raw : s) {
        const unsigned char c = static_cast<unsigned char>(raw);
        if (c == '"') {
            out += "\\\"";
        } else if (c == '\\') {
            out += "\\\\";
        } else if (c == '\b') {
            out += "\\b";
        } else if (c == '\f') {
            out += "\\f";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else if (is_control(c)) {
            out += "\\u00";
            out += kHex[c >> 4];
            out += kHex[c & 0xF];
        } else {
            out += raw;
        }
    }
    out += '"';
    return out;
}

} // namespace df
