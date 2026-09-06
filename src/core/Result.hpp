// Error handling types used across load and transaction boundaries.
#pragma once

#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace df {

/// Context-rich error suitable for content load failures (Section 15.2).
struct Error {
    std::string message;
    std::string file;       ///< Source data file, when applicable.
    std::string definition; ///< Definition id, when applicable.
    std::string field;      ///< Offending field, when applicable.
    std::string suggestion; ///< Human-actionable correction hint.

    std::string to_string() const;
};

template <typename T>
using Result = std::expected<T, Error>;

using Status = std::expected<void, Error>;

inline std::unexpected<Error> fail(std::string message) {
    return std::unexpected(Error{std::move(message), {}, {}, {}, {}});
}

inline std::unexpected<Error> fail(Error e) {
    return std::unexpected(std::move(e));
}

/// Accumulates every failure in one pass so content authors see all problems at
/// once rather than one per run.
class ErrorList {
public:
    void add(Error e) { mErrors.push_back(std::move(e)); }

    void add(std::string message) { mErrors.push_back(Error{std::move(message), {}, {}, {}, {}}); }

    bool empty() const noexcept { return mErrors.empty(); }

    std::size_t size() const noexcept { return mErrors.size(); }

    const std::vector<Error>& all() const noexcept { return mErrors; }

    std::string to_string() const;

private:
    std::vector<Error> mErrors;
};

} // namespace df
