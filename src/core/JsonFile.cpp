#include "core/JsonFile.hpp"

#include <fstream>
#include <sstream>
#include <utility>

namespace df {

Result<std::string> read_text_file(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return fail(Error{"could not open file", file.string(), {}, {}, {}});
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

Result<nlohmann::json> parse_json(const std::string& text, bool ignore_comments) {
    // Exceptions suppressed: a failed parse yields a discarded value.
    nlohmann::json parsed = nlohmann::json::parse(text, nullptr, false, ignore_comments);
    if (parsed.is_discarded()) {
        return fail(Error{"text is not valid JSON", {}, {}, {}, {}});
    }
    return parsed;
}

Result<nlohmann::json> read_json_file(const std::filesystem::path& file) {
    auto text = read_text_file(file);
    if (!text) {
        return text;
    }
    auto parsed = parse_json(*text);
    if (!parsed) {
        return fail(Error{"file is not valid JSON", file.string(), {}, {}, {}});
    }
    return parsed;
}

} // namespace df
