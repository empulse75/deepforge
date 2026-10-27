#include "core/JsonFile.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace df {

namespace {

/// The one size check every whole-file ingest runs before reading anything into
/// memory (SEC-05): the cost is the parse tree, so refusing an oversized file has
/// to happen at the door. `file_size` answers with an error code for the special
/// cases (a pipe, a device); those are let through — they cannot be a planted
/// multi-gigabyte blob, and the read itself fails on them anyway.
Result<void> check_ingest_ceiling(const std::filesystem::path& file, std::size_t size, std::size_t maxBytes) {
    if (size <= maxBytes) {
        return {};
    }
    return fail(Error{"file is larger than the documented ingest ceiling of " + std::to_string(maxBytes) +
                          " bytes (it is " + std::to_string(size) + ")",
                      file.string(),
                      {},
                      "size",
                      "delete it to restore defaults"});
}

const char* label(FieldType type) {
    switch (type) {
        case FieldType::Boolean:
            return "a boolean";
        case FieldType::Integer:
            return "an integer";
        case FieldType::Number:
            return "a number";
        case FieldType::String:
            return "a string";
        case FieldType::Object:
            return "an object";
        case FieldType::Array:
            return "an array";
    }
    return "a known type";
}

} // namespace

Result<std::string> read_text_file(const std::filesystem::path& file, std::size_t maxBytes) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);
    if (!ec) {
        if (auto ceiling = check_ingest_ceiling(file, size, maxBytes); !ceiling) {
            return std::unexpected(ceiling.error());
        }
    }
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return fail(Error{"could not open file", file.string(), {}, {}, {}});
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

Result<nlohmann::json> parse_json(const std::string& text, bool ignore_comments, std::size_t maxBytes) {
    if (text.size() > maxBytes) {
        return fail(Error{"text is larger than the documented ingest ceiling of " + std::to_string(maxBytes) +
                              " bytes (it is " + std::to_string(text.size()) + ")",
                          {},
                          {},
                          "size",
                          "delete it to restore defaults"});
    }
    // Exceptions suppressed: a failed parse yields a discarded value.
    nlohmann::json parsed = nlohmann::json::parse(text, nullptr, false, ignore_comments);
    if (parsed.is_discarded()) {
        return fail(Error{"text is not valid JSON", {}, {}, {}, {}});
    }
    return parsed;
}

Result<nlohmann::json> read_json_file(const std::filesystem::path& file, std::size_t maxBytes) {
    auto text = read_text_file(file, maxBytes);
    if (!text) {
        return text;
    }
    auto parsed = parse_json(*text, /*ignore_comments=*/false, maxBytes);
    if (!parsed) {
        return fail(Error{"file is not valid JSON", file.string(), {}, {}, {}});
    }
    return parsed;
}

const char* field_type_label(FieldType type) {
    return label(type);
}

bool matches_type(const nlohmann::json& node, FieldType expected) {
    switch (expected) {
        case FieldType::Boolean:
            return node.is_boolean();
        case FieldType::Integer:
            if (node.is_number_integer()) {
                return true;
            }
            if (node.is_number_float()) {
                const double v = node.get<double>();
                return v == static_cast<long long>(v);
            }
            return false;
        case FieldType::Number:
            return node.is_number();
        case FieldType::String:
            return node.is_string();
        case FieldType::Object:
            return node.is_object();
        case FieldType::Array:
            return node.is_array();
    }
    return false;
}

std::string_view field_type_name(const nlohmann::json& node) {
    if (node.is_boolean()) {
        return "boolean";
    }
    if (node.is_number_integer()) {
        return "integer";
    }
    if (node.is_number_float()) {
        return "float";
    }
    if (node.is_string()) {
        return "string";
    }
    if (node.is_object()) {
        return "object";
    }
    if (node.is_array()) {
        return "array";
    }
    return "null";
}

bool checked_read(const nlohmann::json& node, std::string_view key, FieldType expected,
                  const nlohmann::json*& out, std::string_view file, std::string_view definition,
                  ErrorList& problems) {
    out = nullptr;
    if (!node.is_object() || !node.contains(key) || node.at(key).is_null()) {
        return false; // absent: the caller's documented default stands, silently
    }
    const auto& value = node.at(key);
    if (!matches_type(value, expected)) {
        problems.add(Error{std::string("field must be ") + label(expected) + ", but is " +
                               std::string(field_type_name(value)),
                           std::string(file),
                           std::string(definition),
                           std::string(key),
                           {}});
        return false;
    }
    out = &value;
    return true;
}

} // namespace df
