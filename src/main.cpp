// Project Deepforge entry point.
#include "app/SaveFile.hpp"
#include "content/ContentDatabase.hpp"
#include "core/Log.hpp"
#include "core/Version.hpp"
#include "presentation/Client.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

/// Writes the log to the user data directory so crash reports have a location.
class FileSink final : public df::LogSink {
public:
    explicit FileSink(const std::filesystem::path& path) : mOut(path, std::ios::trunc) {}

    void write(df::LogLevel level, std::string_view category, std::string_view message) override {
        if (!mOut) {
            return;
        }
        mOut << "[" << static_cast<int>(level) << "] [" << category << "] " << message << '\n';
        mOut.flush();
    }

private:
    std::ofstream mOut;
};

std::filesystem::path find_data_dir(const std::filesystem::path& override) {
    if (!override.empty()) {
        return override;
    }
    // Look next to the executable first (packaged layout), then the source tree.
    const char* base = SDL_GetBasePath();
    const std::filesystem::path exeDir = base != nullptr ? std::filesystem::path(base) : ".";
    const std::filesystem::path candidates[] = {
        exeDir / "data",
        exeDir / ".." / "share" / "deepforge" / "data",
        exeDir / ".." / ".." / "data",
        std::filesystem::path("data"),
    };
    std::error_code ec;
    for (const auto& c : candidates) {
        if (std::filesystem::is_directory(c, ec)) {
            return c;
        }
    }
    return "data";
}

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path dataOverride;
    std::filesystem::path userOverride;
    int headlessFrames = 0;
    std::filesystem::path screenshot;
    unsigned long long screenshotSeed = 20260905;
    int screenshotTicks = 2400;
    std::string screenshotScreen = "expedition";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--data" && i + 1 < argc) {
            dataOverride = argv[++i];
        } else if (a == "--user-dir" && i + 1 < argc) {
            userOverride = argv[++i];
        } else if (a == "--headless-frames" && i + 1 < argc) {
            headlessFrames = std::atoi(argv[++i]);
        } else if (a == "--screenshot" && i + 1 < argc) {
            screenshot = argv[++i];
        } else if (a == "--screenshot-seed" && i + 1 < argc) {
            screenshotSeed = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--screenshot-ticks" && i + 1 < argc) {
            screenshotTicks = std::atoi(argv[++i]);
        } else if (a == "--screenshot-screen" && i + 1 < argc) {
            screenshotScreen = argv[++i];
        } else if (a == "--version") {
            std::printf("Project Deepforge %s (%s, %s)\n", df::kBuildVersion, df::kBuildConfig,
                        df::kBuildDate);
            return 0;
        } else if (a == "--help" || a == "-h") {
            std::puts("usage: deepforge [--data <dir>] [--user-dir <dir>] [--headless-frames N]");
            return 0;
        }
    }

    const std::filesystem::path userDir = userOverride.empty() ? df::default_user_data_dir() : userOverride;
    std::error_code ec;
    std::filesystem::create_directories(userDir, ec);
    static FileSink sink(userDir / "deepforge.log");
    df::add_log_sink(&sink);
    df::set_log_level(df::LogLevel::Info);
    DF_LOG_INFO("boot", "Project Deepforge {} ({}) starting", df::kBuildVersion, df::kBuildConfig);
    DF_LOG_INFO("boot", "user data directory: {}", userDir.string());

    const auto dataDir = find_data_dir(dataOverride);
    df::ErrorList errors;
    auto db = df::ContentDatabase::load(dataDir, errors);
    if (!db) {
        // Graceful failure with a usable message rather than a crash.
        std::fprintf(stderr,
                     "Project Deepforge could not start: game data is missing or invalid.\n"
                     "Looked in: %s\n%s\n",
                     dataDir.string().c_str(), errors.to_string().c_str());
        DF_LOG_ERROR("boot", "content load failed ({} problems)", errors.size());
        return 2;
    }

    df::Client client(std::move(*db), userDir);
    auto init = client.initialise();
    if (!init) {
        std::fprintf(stderr, "Project Deepforge could not start: %s\n", init.error().to_string().c_str());
        DF_LOG_ERROR("boot", "{}", init.error().to_string());
        SDL_Quit();
        return 3;
    }

    int result = 0;
    if (!screenshot.empty()) {
        df::Screen target = df::Screen::Expedition;
        if (screenshotScreen == "hub") {
            target = df::Screen::Hub;
        } else if (screenshotScreen == "menu") {
            target = df::Screen::MainMenu;
        } else if (screenshotScreen == "map") {
            target = df::Screen::Map;
        } else if (screenshotScreen == "garage") {
            target = df::Screen::Garage;
        } else if (screenshotScreen == "modules") {
            target = df::Screen::Modules;
        } else if (screenshotScreen == "skills") {
            target = df::Screen::Skills;
        }
        auto shot = client.capture_demo(screenshot, screenshotSeed, screenshotTicks, target);
        if (!shot) {
            std::fprintf(stderr, "screenshot failed: %s\n", shot.error().to_string().c_str());
            result = 4;
        }
    } else {
        result = headlessFrames > 0 ? client.run_headless_frames(headlessFrames) : client.run();
    }
    DF_LOG_INFO("boot", "shutting down");
    SDL_Quit();
    return result;
}
