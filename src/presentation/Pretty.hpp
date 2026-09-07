// Turns a stable content id or localisation key into readable display text.
//
// Until a translation table exists the UI derives its labels from ids, which
// keeps every screen readable without inventing a second source of truth for
// names.
#pragma once

#include <algorithm>
#include <string>
#include <string_view>

namespace df {

inline std::string pretty_key(std::string_view key) {
    std::string s{key};
    // Drop a trailing ".name" / ".description" suffix, then keep the last
    // segment: "material.prism_faults.name" -> "prism_faults".
    for (const char* suffix : {".name", ".description", ".summary"}) {
        const auto n = std::string_view{suffix}.size();
        if (s.size() > n && s.compare(s.size() - n, n, suffix) == 0) {
            s = s.substr(0, s.size() - n);
            break;
        }
    }
    const auto dot = s.rfind('.');
    if (dot != std::string::npos) {
        s = s.substr(dot + 1);
    }
    std::replace(s.begin(), s.end(), '_', ' ');
    bool upper = true;
    for (auto& c : s) {
        if (upper && c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 32);
        }
        upper = (c == ' ');
    }
    return s;
}

} // namespace df
