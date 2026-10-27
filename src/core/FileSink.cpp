#include "core/FileSink.hpp"

#include <system_error>

namespace df {

FileSink::FileSink(const std::filesystem::path& path, std::uint64_t sizeCap)
    : mPath(path), mBackup(path.string() + ".1"), mSizeCap(sizeCap) {
    std::error_code ec;
    if (std::filesystem::exists(mPath, ec) && !ec) {
        std::filesystem::rename(mPath, mBackup, ec); // overwrite semantics on both POSIX and Windows
        // A rename that failed means the old bytes are still in place;
        // appending below keeps them instead of truncating the evidence away.
    }
    // A successful rename leaves nothing at mPath, so app just creates the
    // new file; after a failed rename app preserves what survived.
    mOut.open(mPath, std::ios::app);
    if (mOut) {
        mBytes = std::filesystem::file_size(mPath, ec); // 0 on ec: fresh file
    }
}

void FileSink::write(LogLevel level, std::string_view category, std::string_view message) {
    if (!mOut) {
        return;
    }
    mOut << "[" << level_name(level) << "] [" << category << "] " << message << '\n';
    mOut.flush();
    mBytes += 2 + level_name(level).size() + 2 + category.size() + 1 + message.size() + 1;
    if (mBytes >= mSizeCap && !mRotateBroken) {
        std::error_code ec;
        std::filesystem::rename(mPath, mBackup, ec);
        if (ec) {
            // Nothing sane to do mid-line but keep logging where it is; a
            // rename that fails once on this pair will keep failing every line.
            mRotateBroken = true;
            return;
        }
        reopen();
    }
}

void FileSink::reopen() {
    std::error_code ec;
    mOut.close();
    mOut.open(mPath, std::ios::app);
    mBytes = mOut ? std::filesystem::file_size(mPath, ec) : 0;
}

} // namespace df
