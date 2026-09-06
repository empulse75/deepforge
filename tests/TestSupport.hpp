// Shared helpers for the test suite. Everything here is headless: no window,
// no GPU, no wall clock.
#pragma once

#include "app/GameSession.hpp"
#include "content/ContentDatabase.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

namespace dft {

inline const df::ContentDatabase& shipped_content() {
    static df::ContentDatabase db = [] {
        df::ErrorList errors;
        auto loaded = df::ContentDatabase::load(DEEPFORGE_DATA_DIR, errors);
        if (!loaded) {
            FAIL("shipped content failed to load:\n" << errors.to_string());
        }
        return std::move(*loaded);
    }();
    return db;
}

inline std::filesystem::path temp_dir(const std::string& name) {
    const auto p = std::filesystem::path(DEEPFORGE_TEST_TMP_DIR) / name;
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
    std::filesystem::create_directories(p, ec);
    return p;
}

inline df::GameSession make_session(const std::string& name, df::u64 seed) {
    df::GameSession session(shipped_content(), temp_dir(name));
    df::NewGameOptions options;
    options.seed = seed;
    options.randomSeed = false;
    session.new_game(options);
    return session;
}

} // namespace dft
