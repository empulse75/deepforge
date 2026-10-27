// Shared plumbing for "read a file, parse it as JSON" boundaries. Every layer
// that ingests JSON (content, store, app, platform) used to carry its own copy
// of the ifstream/sstream/parse dance; this is the single implementation so
// error handling cannot drift between them — including the ingest ceiling, which
// lives here, and the type rule every untrusted read goes through.
#pragma once

#include "core/Result.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <filesystem>

namespace df {

/// Documented ingest ceilings, in bytes (SEC-05). Reading a whole file and
/// building a parse tree over it costs roughly four times the file in resident
/// memory — a planted 200 MB file measured ~839 MB peak RSS — so every ingest path
/// declares a ceiling and the size is checked *before* anything is read or parsed.
/// Machine-written ingest (the data dir, the store, save slots) shares 8 MB, the
/// number the security audit wrote down for the largest of them; the settings file,
/// the one file a player is expected to touch by hand, is capped at 1 MB. The
/// shipped data corpus (largest file 52 KB), today's settings (~4 KB) and today's
/// save slots (a few KB) all sit three orders of magnitude under their ceiling —
/// the ceiling is there for a planted file, not for anything the game produces. The
/// ceiling Error is marked `field == "size"`, so a boundary that maps read failures
/// onto its own documented message can still tell a refused file from an unreadable
/// one.
inline constexpr std::size_t kIngestCeilingBytes = 8'000'000;
inline constexpr std::size_t kIngestCeilingSettingsBytes = 1'000'000;

/// Read a whole file into a string. Fails with a context-rich Error if the file
/// cannot be opened, or if it is larger than `maxBytes`.
Result<std::string> read_text_file(const std::filesystem::path& file,
                                   std::size_t maxBytes = kIngestCeilingBytes);

/// Non-throwing JSON parse (the parser's exceptions are suppressed; a failed
/// parse comes back as an Error, not an exception). `ignore_comments` matches the
/// parser flag content and store files are read with. Text larger than `maxBytes`
/// is refused before any of it becomes a tree.
Result<nlohmann::json> parse_json(const std::string& text, bool ignore_comments = false,
                                  std::size_t maxBytes = kIngestCeilingBytes);

/// Read and parse in one step — the shape every JSON-ingesting layer needs.
Result<nlohmann::json> read_json_file(const std::filesystem::path& file,
                                      std::size_t maxBytes = kIngestCeilingBytes);

/// The types a field on an untrusted file is documented to hold.
enum class FieldType { Boolean, Integer, Number, String, Object, Array };

/// The one accessor every read of an untrusted file goes through (SEC-01). The
/// parse is already non-throwing; it is the accessors *after* it that killed the
/// process on a hand-edited file, so nothing here throws either.
///
/// A missing key is normal and silent: the answer is false, `out` is null, and the
/// caller's documented default is the answer for it. A key present with the wrong
/// type is a problem: the answer is false, `out` is null, and a context-rich Error
/// naming file, definition and field is added to `problems` — the caller then
/// decides whether that is fatal (a settings file, a content definition) or a skip
/// with a warning (a save). Only a key that matches comes back in `out`.
bool checked_read(const nlohmann::json& node, std::string_view key, FieldType expected,
                  const nlohmann::json*& out, std::string_view file, std::string_view definition,
                  ErrorList& problems);

/// True when `node` already holds a type the field is documented to accept — for
/// element reads (array members), where there is no key to name.
///
/// A number field accepts integers and floats alike: today's code already reads
/// both, and the conversion loses nothing at these magnitudes. An integer field
/// takes integers, and a float whose value is exactly a whole number — JSON has no
/// integer type, a text editor writes 1280.0 innocently, and against the pinned
/// headers that value already reads back as 1280 with nothing lost. A fractional
/// float in an integer field is the loud case (QUAL-01's measured 2.5 → 2), because
/// reading it as an integer is what silently truncates.
bool matches_type(const nlohmann::json& node, FieldType expected);

/// What a node actually holds — for the message when it does not match.
std::string_view field_type_name(const nlohmann::json& node);

/// The documented name of a field type, for the message when it does not match.
const char* field_type_label(FieldType type);

} // namespace df
