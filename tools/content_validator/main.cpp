// Standalone content validator. Run in CI before packaging so no build ships
// with an unresolved reference or an impossible progression rule.
#include "content/ContentDatabase.hpp"
#include "core/Log.hpp"
#include "core/Version.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    std::filesystem::path dataDir = "data";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--data" || arg == "-d") && i + 1 < argc) {
            dataDir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::puts("usage: deepforge-validate [--data <dir>]");
            return 0;
        }
    }

    df::set_log_level(df::LogLevel::Warn);
    df::ErrorList errors;
    auto db = df::ContentDatabase::load(dataDir, errors);
    if (!db) {
        std::fprintf(stderr, "content validation FAILED (%zu problem(s)):\n%s\n", errors.size(),
                     errors.to_string().c_str());
        return 1;
    }

    std::printf("Deepforge content OK (%s, schema %d)\n", df::kBuildVersion, db->content_schema_version());
    std::printf("  materials  %zu\n", db->materials().size());
    std::printf("  biomes     %zu\n", db->biomes().size());
    std::printf("  equipment  %zu tracks\n", db->equipment().size());
    std::printf("  modules    %zu\n", db->modules().size());
    std::printf("  skills     %zu\n", db->skills().size());
    std::printf("  recipes    %zu\n", db->recipes().size());
    std::printf("  facilities %zu\n", db->facilities().size());
    std::printf("  contracts  %zu\n", db->contracts().size());
    std::printf("  landmarks  %zu\n", db->landmarks().size());
    std::printf("  hazards    %zu\n", db->hazards().size());
    std::printf("  research   %zu\n", db->research().size());
    return 0;
}
