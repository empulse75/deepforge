#include "core/Result.hpp"

#include "core/Sanitize.hpp"

#include <sstream>

namespace df {

std::string Error::to_string() const {
    std::ostringstream os;
    // SEC-07: these fields carry strings that came out of data files and
    // saves (unknown ids, bad values), and this rendering reaches terminals
    // directly — console errors, CI logs — where a raw ESC byte or newline
    // would forge a line. Every interpolation is JSON-encoded.
    if (!file.empty()) {
        os << json_encode(file);
        if (!definition.empty()) {
            os << " [" << json_encode(definition) << "]";
        }
        if (!field.empty()) {
            os << " ." << json_encode(field);
        }
        os << ": ";
    } else if (!definition.empty()) {
        os << json_encode(definition);
        if (!field.empty()) {
            os << "." << json_encode(field);
        }
        os << ": ";
    }
    os << json_encode(message);
    if (!suggestion.empty()) {
        os << " (" << json_encode(suggestion) << ")";
    }
    return os.str();
}

std::string ErrorList::to_string() const {
    std::ostringstream os;
    for (std::size_t i = 0; i < mErrors.size(); ++i) {
        if (i != 0) {
            os << '\n';
        }
        os << "  - " << mErrors[i].to_string();
    }
    return os.str();
}

} // namespace df
