// The log file sink used by main (and pinned by tests). The previous
// session's log is the only evidence after a crash, so the file is renamed
// to <path>.1 on startup instead of being truncated, and a size cap bounds
// a long session (OPS-OBS-01). The per-line flush stays: a crash must not
// lose the lines already recorded. The line format matches the console.
#pragma once

#include "core/Log.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>

namespace df {

class FileSink final : public LogSink {
public:
    /// The cap counts bytes into the current file; crossing it rotates the
    /// file onto <path>.1 (the oldest generation is lost) and logging
    /// continues in a fresh file. Text arrives already escaped — dispatch
    /// sanitises at the single choke point — so the sink trusts it.
    explicit FileSink(const std::filesystem::path& path, std::uint64_t sizeCap = kDefaultSizeCap);

    void write(LogLevel level, std::string_view category, std::string_view message) override;

    static constexpr std::uint64_t kDefaultSizeCap = 1024 * 1024;

private:
    void reopen();

    std::filesystem::path mPath;
    std::filesystem::path mBackup;
    std::uint64_t mSizeCap;
    std::uint64_t mBytes = 0;
    bool mRotateBroken = false;
    std::ofstream mOut;
};

} // namespace df
