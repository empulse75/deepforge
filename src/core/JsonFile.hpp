// Shared plumbing for "read a file, parse it as JSON" boundaries. Every layer
// that ingests JSON (content, store, app, platform) used to carry its own copy
// of the ifstream/sstream/parse dance; this is the single implementation so
// error handling cannot drift between them.
#pragma once

#include "core/Result.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>

namespace df {

/// Read a whole file into a string. Fails with a context-rich Error if the
/// file cannot be opened.
Result<std::string> read_text_file(const std::filesystem::path& file);

/// Non-throwing JSON parse (the parser's exceptions are suppressed; a failed
/// parse comes back as an Error, not an exception). `ignore_comments` matches
/// the parser flag content and store files are read with.
Result<nlohmann::json> parse_json(const std::string& text, bool ignore_comments = false);

/// Read and parse in one step — the shape every JSON-ingesting layer needs.
Result<nlohmann::json> read_json_file(const std::filesystem::path& file);

} // namespace df
