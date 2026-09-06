#include "core/Result.hpp"

#include <sstream>

namespace df {

std::string Error::to_string() const {
    std::ostringstream os;
    if (!file.empty()) {
        os << file;
        if (!definition.empty()) {
            os << " [" << definition << "]";
        }
        if (!field.empty()) {
            os << " ." << field;
        }
        os << ": ";
    } else if (!definition.empty()) {
        os << definition;
        if (!field.empty()) {
            os << "." << field;
        }
        os << ": ";
    }
    os << message;
    if (!suggestion.empty()) {
        os << " (" << suggestion << ")";
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
