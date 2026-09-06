// Headless balance simulator (Section 10.5).
//
// Runs scripted expeditions across seeds and loadout tiers and reports the
// pacing figures the design brief asks us to verify: time to first upgrade,
// credits per minute, repair/fuel expense ratio and value per cargo slot.
#include "app/Autopilot.hpp"
#include "app/GameSession.hpp"
#include "core/Log.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <string>
#include <vector>

using namespace df;

namespace {

struct RunResult {
    float minutes = 0.0f;
    i64 credits = 0;
    i64 depth = 0;
    i64 tiles = 0;
    float cargoUnits = 0.0f;
    bool rescued = false;
};

double mean(const std::vector<double>& v) {
    return v.empty() ? 0.0 : std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
}

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path dataDir = "data";
    int seeds = 8;
    i64 targetDepth = 220;
    bool pacing = false;
    bool verbose = false;
    int loadoutTier = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if ((a == "--data") && i + 1 < argc) {
            dataDir = argv[++i];
        } else if (a == "--seeds" && i + 1 < argc) {
            seeds = std::atoi(argv[++i]);
        } else if (a == "--depth" && i + 1 < argc) {
            targetDepth = std::atoll(argv[++i]);
        } else if (a == "--tier" && i + 1 < argc) {
            loadoutTier = std::atoi(argv[++i]);
        } else if (a == "--verbose") {
            set_log_level(LogLevel::Info);
            verbose = true;
        } else if (a == "--pacing") {
            pacing = true;
        } else if (a == "--help" || a == "-h") {
            std::puts("usage: deepforge-balance [--data <dir>] [--seeds N] [--depth D] [--tier T]\n"
                      "                         [--pacing] [--verbose]");
            return 0;
        }
    }

    set_log_level(verbose ? LogLevel::Info : LogLevel::Error);
    ErrorList errors;
    auto db = ContentDatabase::load(dataDir, errors);
    if (!db) {
        std::fprintf(stderr, "content load failed:\n%s\n", errors.to_string().c_str());
        return 1;
    }

    const auto tmp = std::filesystem::temp_directory_path() / "deepforge-balance";
    std::filesystem::create_directories(tmp);

    if (pacing) {
        // How long until the first meaningful upgrade is affordable?
        std::vector<double> minutesToUpgrade;
        std::vector<double> firstRunCredits;
        int softlocks = 0;
        int stalls = 0;
        int rescues = 0;
        for (int s = 0; s < seeds; ++s) {
            GameSession session(*db, tmp);
            NewGameOptions opt;
            opt.seed = 100000u + static_cast<u64>(s) * 7919u;
            opt.randomSeed = false;
            session.new_game(opt);

            const i64 cheapest = [&] {
                i64 best = 1 << 30;
                for (usize t = 0; t < db->equipment().size(); ++t) {
                    const auto& track = db->equipment()[t];
                    if (track.tiers.size() > 1) {
                        best = std::min<i64>(best, track.tiers[1].costCredits);
                    }
                }
                return best;
            }();

            double minutes = 0.0;
            int runs = 0;
            bool bought = false;
            while (runs < 12 && !bought) {
                AutopilotConfig cfg;
                cfg.targetDepth = 60 + 40 * runs;
                const auto report = run_autopilot_expedition(session, cfg);
                minutes += static_cast<double>(report.seconds) / 60.0;
                if (report.stalled) {
                    ++stalls;
                }
                if (report.rescued) {
                    ++rescues;
                }
                if (runs == 0) {
                    firstRunCredits.push_back(static_cast<double>(session.progress().credits));
                }
                ++runs;
                for (usize t = 0; t < db->equipment().size(); ++t) {
                    if (session.economy().can_buy_equipment(static_cast<i32>(t))) {
                        bought = true;
                        break;
                    }
                }
                if (session.progress().credits >= cheapest) {
                    bought = true;
                }
            }
            if (!bought) {
                ++softlocks;
                std::printf("  [warn] seed %llu ended 12 runs with %lld credits, deepest %lld m\n",
                            static_cast<unsigned long long>(opt.seed),
                            static_cast<long long>(session.progress().credits),
                            static_cast<long long>(session.progress().stats.deepestDepth));
            } else {
                minutesToUpgrade.push_back(minutes);
            }
        }
        std::printf("PACING seeds=%d\n", seeds);
        std::printf("  cheapest tier-1 upgrade reachable in %.1f min (target 5-10)\n",
                    mean(minutesToUpgrade));
        std::printf("  credits after first expedition: %.0f\n", mean(firstRunCredits));
        std::printf("  seeds that could not reach an upgrade in 12 runs: %d\n", softlocks);
        std::printf("  autopilot stalls: %d, rescues: %d\n", stalls, rescues);
        return softlocks == 0 ? 0 : 2;
    }

    std::vector<RunResult> results;
    for (int s = 0; s < seeds; ++s) {
        GameSession session(*db, tmp);
        NewGameOptions opt;
        opt.seed = 4242u + static_cast<u64>(s) * 104729u;
        opt.randomSeed = false;
        session.new_game(opt);

        // Grant a loadout tier so deep-run economics can be measured without
        // simulating the whole climb to that tier first.
        if (loadoutTier > 0) {
            session.progress().credits = 100000000;
            session.progress().insight = 100000;
            for (int t = 0; t < loadoutTier; ++t) {
                for (usize k = 0; k < db->equipment().size(); ++k) {
                    (void)session.economy().buy_equipment(static_cast<i32>(k));
                }
            }
            session.refresh();
            session.rig().fuel = session.sim().stats().fuelCapacity;
            session.rig().hull = session.sim().stats().hullMax;
            session.progress().credits = 0;
        }

        const i64 before = session.progress().credits;
        AutopilotConfig cfg;
        cfg.targetDepth = targetDepth;
        cfg.verbose = verbose;
        const auto report = run_autopilot_expedition(session, cfg);

        RunResult r;
        r.minutes = report.seconds / 60.0f;
        r.credits = session.progress().credits - before;
        r.depth = report.depthReached;
        r.tiles = report.tilesMined;
        r.rescued = report.rescued;
        results.push_back(r);
    }

    std::vector<double> mins, creds, depths, cpm;
    for (const auto& r : results) {
        mins.push_back(r.minutes);
        creds.push_back(static_cast<double>(r.credits));
        depths.push_back(static_cast<double>(r.depth));
        cpm.push_back(r.minutes > 0.01f ? static_cast<double>(r.credits) / static_cast<double>(r.minutes)
                                        : 0.0);
    }
    const int rescues = static_cast<int>(
        std::count_if(results.begin(), results.end(), [](const RunResult& r) { return r.rescued; }));
    std::printf("EXPEDITION MODEL  seeds=%d target_depth=%lld loadout_tier=%d\n", seeds,
                static_cast<long long>(targetDepth), loadoutTier);
    std::printf("  mean duration      %.2f min\n", mean(mins));
    std::printf("  mean net credits   %.0f\n", mean(creds));
    std::printf("  mean depth reached %.0f m\n", mean(depths));
    std::printf("  credits per minute %.0f\n", mean(cpm));
    std::printf("  rescues            %d / %d\n", rescues, seeds);
    double cargoValue = 0.0;
    for (const auto& r : results) {
        cargoValue += r.tiles > 0 ? static_cast<double>(r.credits) / static_cast<double>(r.tiles) : 0.0;
    }
    std::printf("  credits per tile   %.2f\n",
                results.empty() ? 0.0 : cargoValue / static_cast<double>(results.size()));
    return 0;
}
