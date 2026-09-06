#include "presentation/Client.hpp"

#include "app/Autopilot.hpp"
#include "core/Log.hpp"
#include "core/Version.hpp"
#include "domain/Loadout.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace df {
namespace {

constexpr float kBaseTilePixels = 34.0f;

// Menu action ids. Kept in ranges so screens can pack list indices alongside.
enum : int {
    kIdNone = 0,
    kIdContinue = 1,
    kIdNewGame,
    kIdLoadGame,
    kIdSettings,
    kIdBindings,
    kIdCredits,
    kIdQuit,
    kIdBack,
    kIdStartWorld,
    kIdToggleRandomSeed,
    kIdSeedUp,
    kIdSeedDown,
    kIdDescend,
    kIdGarage,
    kIdModules,
    kIdSkills,
    kIdResearch,
    kIdRefinery,
    kIdContracts,
    kIdFacilities,
    kIdCodex,
    kIdSaveGame,
    kIdReturnToMenu,
    kIdResume,
    kIdAbandonRun,
    kIdConfirmRescue,
    kIdResultsOk,
    kIdRespec,
    kIdResetBindings,
    kIdToggleFullscreen,
    kIdToggleVsync,
    kIdShakeUp,
    kIdShakeDown,
    kIdFlashToggle,
    kIdParticlesUp,
    kIdParticlesDown,
    kIdPaletteToggle,
    kIdHoldMineToggle,
    kIdHoldThrustToggle,
    kIdTextScaleUp,
    kIdTextScaleDown,
    kIdAssistFuelUp,
    kIdAssistFuelDown,
    kIdAssistDamageUp,
    kIdAssistDamageDown,
    kIdAssistHeatUp,
    kIdAssistHeatDown,
    kIdAssistMineUp,
    kIdAssistMineDown,
    kIdVolumeUp,
    kIdVolumeDown,
    kIdRecoverBackup,
    kIdDynamicBase = 10000, ///< Screen-specific list entries start here.
};

std::string pretty(const std::string& key) {
    // Turn "material.prism_faults.name" into "Prism Faults" without a
    // localisation table, which arrives with the localisation pass.
    std::string s = key;
    const auto lastDot = s.rfind('.');
    if (lastDot != std::string::npos &&
        (s.substr(lastDot) == ".name" || s.substr(lastDot) == ".description")) {
        s = s.substr(0, lastDot);
    }
    const auto pos = s.rfind('.');
    if (pos != std::string::npos) {
        s = s.substr(pos + 1);
    }
    std::replace(s.begin(), s.end(), '_', ' ');
    bool upper = true;
    for (auto& c : s) {
        if (upper && c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 32);
        }
        upper = (c == ' ');
    }
    return s;
}

} // namespace

Client::Client(ContentDatabase db, std::filesystem::path userDir)
    : mDb(std::move(db)), mUserDir(std::move(userDir)) {
    auto loaded = load_settings(mUserDir / "settings.json");
    mSettings = loaded ? *loaded : default_settings();
    mSession = std::make_unique<GameSession>(mDb, mUserDir / "saves");
    mTutorials = {
        {"move", "Piloting",
         "A and D steer. W or SPACE fires the lift thrusters - they burn fuel, "
         "so climbing back out is the expensive half of every run."},
        {"mine", "Drilling",
         "Hold S to cut downward, J and L for the sides, I for overhead. The rig "
         "braces while the drill is engaged, so cutting upward costs no thrust."},
        {"cargo", "Cargo",
         "Ore fills the hold. When it is full, head for the surface - unsold cargo "
         "is the only thing a failed run can cost you."},
        {"fuel", "Fuel",
         "Watch the fuel bar against your depth. Cutting a shaft home is cheap but "
         "slow; flying up an open shaft is fast but thirsty."},
        {"upgrade", "The Garage",
         "Sell your haul, then spend credits in the Garage. The first tank "
         "or drill tier changes how deep a single run can reach."},
        {"heat", "Heat",
         "Below the Ember Mantle the rock itself is hot. Cooling and heat sinks turn "
         "that from a wall into a route choice."},
    };
}

Client::~Client() {
    (void)save_settings(mUserDir / "settings.json", mSettings);
}

Status Client::initialise() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        return fail(Error{std::string("SDL_Init failed: ") + SDL_GetError(),
                          {},
                          {},
                          {},
                          "a display and input subsystem are required"});
    }
    auto window = Window::create(mSettings.video, "Project Deepforge");
    if (!window) {
        return std::unexpected(window.error());
    }
    mWindow = std::move(*window);
    mInput = std::make_unique<InputSystem>();
    mAudio = std::make_unique<AudioSystem>();
    mAudio->set_volumes(mSettings.audio.master, mSettings.audio.sfx);
    set_screen(Screen::MainMenu);
    return {};
}

void Client::set_screen(Screen s) {
    if (mScreen != s) {
        mPreviousScreen = mScreen;
    }
    mScreen = s;
    mSelected = 0;
    mScroll = 0;
    build_menu();
}

void Client::push_toast(std::string text, Color c) {
    mToasts.push_back(Toast{std::move(text), 4.0f, c});
    while (mToasts.size() > 6) {
        mToasts.pop_front();
    }
}

void Client::play(Sfx s, float pitch) {
    if (mAudio) {
        mAudio->play(s, pitch);
    }
    if (mSettings.audio.subtitles) {
        const auto caption = sfx_caption(s);
        if (!caption.empty()) {
            push_toast(std::string(caption), palette::kTextDim);
        }
    }
}

// ---------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------

void Client::build_menu() {
    mMenu.clear();
    const auto& p = mSession->progress();
    switch (mScreen) {
        case Screen::MainMenu: {
            mMenuTitle = "PROJECT DEEPFORGE";
            mMenuSubtitle = std::format("build {} - {}", kBuildVersion, kBuildDate);
            const auto slots = mSession->saves().list_slots(mDb, 1);
            if (slots[0].exists && !slots[0].damaged) {
                mMenu.push_back({"Continue", slots[0].summary, "", true, kIdContinue});
            }
            mMenu.push_back({"New expedition", "Start a new world", "", true, kIdNewGame});
            mMenu.push_back({"Load slot", "Choose a save slot", "", true, kIdLoadGame});
            mMenu.push_back({"Settings", "Video, audio, accessibility, assists", "", true, kIdSettings});
            mMenu.push_back({"Controls", "Remap keyboard and controller", "", true, kIdBindings});
            mMenu.push_back({"Credits", "", "", true, kIdCredits});
            mMenu.push_back({"Quit", "", "", true, kIdQuit});
            break;
        }
        case Screen::NewGame: {
            mMenuTitle = "NEW WORLD";
            mMenuSubtitle = "The seed decides the geology. The same seed always builds the same world.";
            mMenu.push_back({"Seed source", mUseRandomSeed ? "Random" : std::to_string(mSeedInput), "", true,
                             kIdToggleRandomSeed});
            if (!mUseRandomSeed) {
                mMenu.push_back({"Seed +1000", std::to_string(mSeedInput), "", true, kIdSeedUp});
                mMenu.push_back({"Seed -1000", std::to_string(mSeedInput), "", true, kIdSeedDown});
            }
            mMenu.push_back({"Assists", "Adjust in Settings at any time", "", false, kIdNone});
            mMenu.push_back({"Begin", "Descend for the first time", "", true, kIdStartWorld});
            mMenu.push_back({"Back", "", "", true, kIdBack});
            break;
        }
        case Screen::LoadGame: {
            mMenuTitle = "LOAD";
            mMenuSubtitle = "Damaged saves keep a rotating backup.";
            const auto slots = mSession->saves().list_slots(mDb, 4);
            for (const auto& s : slots) {
                std::string label = s.slot == 0 ? "Autosave" : std::format("Slot {}", s.slot);
                std::string detail = !s.exists   ? "empty"
                                     : s.damaged ? std::string("DAMAGED: ") + s.summary
                                                 : std::format("{} - {:.0f} min - schema {}", s.summary,
                                                               s.playSeconds / 60.0, s.schemaVersion);
                mMenu.push_back({label, detail, "", s.exists, kIdDynamicBase + s.slot,
                                 s.damaged ? palette::kDanger : palette::kText});
            }
            mMenu.push_back(
                {"Restore backup of selected slot", "Use if a save is damaged", "", true, kIdRecoverBackup});
            mMenu.push_back({"Back", "", "", true, kIdBack});
            break;
        }
        case Screen::Hub: {
            mMenuTitle = "SURFACE HUB";
            mMenuSubtitle = std::format("{} credits - {} insight - {} proficiency - deepest {} m", p.credits,
                                        p.insight, p.skill_points_available(), p.stats.deepestDepth);
            mMenu.push_back({"Descend", "Begin an expedition from the surface shaft", "", true, kIdDescend,
                             palette::kAccent});
            // Activated transit nodes become permanent launch points, deepest
            // first, so late-game runs do not begin with a repeat of the climb.
            for (usize i = 0; i < p.transitNodes.size(); ++i) {
                const auto& node = p.transitNodes[p.transitNodes.size() - 1 - i];
                const i32 biome = mSession->world().generator().biome_index_at(node.y);
                mMenu.push_back({std::format("Transit lift to {} m", node.y),
                                 std::format("Launch and extract at {}",
                                             pretty(mDb.biomes()[static_cast<usize>(biome)].nameKey)),
                                 "", true,
                                 kIdDynamicBase + 900 + static_cast<int>(p.transitNodes.size() - 1 - i),
                                 palette::kAccent});
            }
            mMenu.push_back({"Garage", "Rig equipment tiers", "", true, kIdGarage});
            mMenu.push_back({"Modules", "Loadout and buildcraft", "", true, kIdModules});
            mMenu.push_back(
                {"Proficiency", std::format("{} unspent", p.skill_points_available()), "", true, kIdSkills});
            mMenu.push_back({"Research lab", std::format("{} insight", p.insight), "", true, kIdResearch});
            mMenu.push_back({"Refinery", "Convert raw ore into components", "", true, kIdRefinery});
            mMenu.push_back(
                {"Contract terminal", std::format("{} active", p.contracts.size()), "", true, kIdContracts});
            mMenu.push_back({"Facilities", "Upgrade the base", "", true, kIdFacilities});
            mMenu.push_back({"Codex", "Materials, landmarks, records", "", true, kIdCodex});
            mMenu.push_back({"Save", "Write to the autosave slot", "", true, kIdSaveGame});
            mMenu.push_back({"Main menu", "", "", true, kIdReturnToMenu});
            break;
        }
        case Screen::Garage: {
            mMenuTitle = "GARAGE";
            mMenuSubtitle = std::format("{} credits", p.credits);
            for (usize i = 0; i < mDb.equipment().size(); ++i) {
                const auto& track = mDb.equipment()[i];
                const i32 tier = p.equipmentTier[i];
                const bool maxed = static_cast<usize>(tier + 1) >= track.tiers.size();
                std::string reason;
                const bool can = mSession->economy().can_buy_equipment(static_cast<i32>(i), &reason);
                std::string detail;
                if (maxed) {
                    detail = std::format("tier {} - maximum", tier);
                } else {
                    const auto& next = track.tiers[static_cast<usize>(tier + 1)];
                    const StatBlock before = compute_stats(mDb, p);
                    const StatBlock after = preview_with_equipment(mDb, p, static_cast<i32>(i), tier + 1);
                    std::string delta;
                    if (track.track == "fuel_tank") {
                        delta = format_stat_delta(before.fuelCapacity, after.fuelCapacity);
                    } else if (track.track == "cargo_bay") {
                        delta = format_stat_delta(before.cargoCapacity, after.cargoCapacity);
                    } else if (track.track == "hull") {
                        delta = format_stat_delta(before.hullMax, after.hullMax);
                    } else if (track.track == "drill_power") {
                        delta = format_stat_delta(before.drillPower, after.drillPower);
                    } else if (track.track == "cooling") {
                        delta = format_stat_delta(before.cooling, after.cooling);
                    } else if (track.track == "scanner") {
                        delta = format_stat_delta(before.scannerRange, after.scannerRange);
                    } else if (track.track == "engine") {
                        delta = format_stat_delta(before.thrust, after.thrust);
                    } else if (track.track == "drill_efficiency") {
                        delta = format_stat_delta(before.drillEfficiency, after.drillEfficiency, true);
                    } else {
                        delta = "new capability";
                    }
                    detail = std::format("tier {} -> {}  {} cr  {}", tier, tier + 1, next.costCredits, delta);
                    if (!next.costComponents.empty()) {
                        detail += "  +components";
                    }
                    if (!can && reason != "not enough credits") {
                        detail += "  (" + reason + ")";
                    }
                }
                mMenu.push_back(
                    {pretty(track.nameKey), detail, "", !maxed && can, kIdDynamicBase + static_cast<int>(i)});
            }
            mMenu.push_back({"Back", "", "", true, kIdBack});
            break;
        }
        case Screen::Modules: {
            mMenuTitle = "MODULES";
            mMenuSubtitle = std::format("power {} / {} - {} credits", p.used_utility_power(mDb),
                                        p.maxUtilityPower, p.credits);
            for (usize i = 0; i < mDb.modules().size(); ++i) {
                const auto& m = mDb.modules()[i];
                const bool owned = p.ownedModules.count(static_cast<i32>(i)) > 0;
                const bool installed = p.module_installed(static_cast<i32>(i));
                const bool unlocked = mSession->economy().module_unlocked(static_cast<i32>(i));
                std::string detail;
                if (!unlocked) {
                    detail = std::format("locked - {}", m.unlockResearch.empty()
                                                            ? std::string("reputation")
                                                            : pretty(m.unlockResearch.text()));
                } else if (installed) {
                    detail = std::format("INSTALLED - {} power - {}", m.powerCost,
                                         std::string(module_slot_name(m.slot)));
                } else if (owned) {
                    detail = std::format("owned - {} power - {}", m.powerCost,
                                         std::string(module_slot_name(m.slot)));
                } else {
                    detail = std::format("{} cr - {} power - {}", m.costCredits, m.powerCost,
                                         std::string(module_slot_name(m.slot)));
                }
                mMenu.push_back({pretty(m.nameKey), detail, "", unlocked,
                                 kIdDynamicBase + static_cast<int>(i),
                                 installed ? palette::kGood : palette::kText});
            }
            mMenu.push_back({"Back", "", "", true, kIdBack});
            break;
        }
        case Screen::Skills: {
            mMenuTitle = "PROFICIENCY";
            mMenuSubtitle = std::format("{} points available - respec costs {} cr",
                                        p.skill_points_available(), mSession->economy().respec_cost());
            for (usize i = 0; i < mDb.skills().size(); ++i) {
                const auto& s = mDb.skills()[i];
                const bool taken = p.unlockedSkills.count(static_cast<i32>(i)) > 0;
                std::string detail = std::format("{} - row {} - cost {}",
                                                 std::string(skill_branch_name(s.branch)), s.row, s.cost);
                if (s.keystone) {
                    detail += " - KEYSTONE";
                }
                if (taken) {
                    detail = "TAKEN - " + detail;
                }
                mMenu.push_back({pretty(s.nameKey), detail, "", !taken, kIdDynamicBase + static_cast<int>(i),
                                 taken ? palette::kGood : palette::kText});
            }
            mMenu.push_back({"Respec", "Refund every point for a fee", "", true, kIdRespec});
            mMenu.push_back({"Back", "", "", true, kIdBack});
            break;
        }
        case Screen::Research: {
            mMenuTitle = "RESEARCH LAB";
            mMenuSubtitle =
                std::format("{} insight - {} samples stored", p.insight, p.samples.stacks().size());
            for (usize i = 0; i < mDb.research().size(); ++i) {
                const auto& r = mDb.research()[i];
                const bool done = p.has_research(static_cast<i32>(i));
                std::string reason;
                const bool ok = mSession->economy().research_available(static_cast<i32>(i), &reason);
                std::string detail =
                    done
                        ? "COMPLETE"
                        : std::format("{} insight{}{}", r.insightCost,
                                      r.requiresSample.empty() ? "" : " + " + pretty(r.requiresSample.text()),
                                      ok ? "" : "  (" + reason + ")");
                mMenu.push_back({pretty(r.nameKey), detail, "", ok, kIdDynamicBase + static_cast<int>(i),
                                 done ? palette::kGood : palette::kText});
            }
            mMenu.push_back({"Back", "", "", true, kIdBack});
            break;
        }
        case Screen::Refinery: {
            mMenuTitle = "REFINERY";
            mMenuSubtitle = "Vault stock is consumed by recipes";
            for (usize i = 0; i < mDb.recipes().size(); ++i) {
                const auto& r = mDb.recipes()[i];
                std::string reason;
                const bool ok = mSession->economy().recipe_available(static_cast<i32>(i), &reason);
                std::string detail;
                for (const auto& in : r.inputs) {
                    const auto h = mDb.material(in.materialId.text());
                    detail += std::format("{}x{} ({}) ", in.amount, pretty(in.materialId.text()),
                                          h.valid() ? p.vault.count(h.index) : 0);
                }
                detail += "-> ";
                for (const auto& out : r.outputs) {
                    detail += std::format("{}x{} ", out.amount, pretty(out.materialId.text()));
                }
                if (r.creditCost > 0) {
                    detail += std::format(" [{} cr]", r.creditCost);
                }
                if (!ok) {
                    detail += "  (" + reason + ")";
                }
                mMenu.push_back({pretty(r.nameKey), detail, "", ok, kIdDynamicBase + static_cast<int>(i)});
            }
            mMenu.push_back({"Back", "", "", true, kIdBack});
            break;
        }
        case Screen::Contracts: {
            mMenuTitle = "CONTRACT TERMINAL";
            mMenuSubtitle = std::format("{} active of 4", p.contracts.size());
            for (usize i = 0; i < p.contracts.size(); ++i) {
                const auto& c = p.contracts[i];
                const auto& t = mDb.contracts()[static_cast<usize>(c.templateIndex)];
                std::string detail;
                if (c.targetMaterial >= 0) {
                    detail =
                        std::format("deliver {} x {} ({}/{})", c.targetAmount,
                                    pretty(mDb.materials()[static_cast<usize>(c.targetMaterial)].nameKey),
                                    c.progress, c.targetAmount);
                } else if (c.targetDepth > 0) {
                    detail = std::format("reach {} m", c.targetDepth);
                } else {
                    detail = std::string(contract_kind_name(t.kind));
                }
                detail += std::format(" - {} cr", t.rewardCredits);
                mMenu.push_back({"ACTIVE: " + pretty(t.nameKey), detail, "", true,
                                 kIdDynamicBase + 500 + static_cast<int>(i), palette::kGood});
            }
            const auto& offers = mSession->economy().offers();
            for (usize i = 0; i < offers.size(); ++i) {
                const auto& c = offers[i];
                const auto& t = mDb.contracts()[static_cast<usize>(c.templateIndex)];
                std::string detail;
                if (c.targetMaterial >= 0) {
                    detail =
                        std::format("deliver {} x {}", c.targetAmount,
                                    pretty(mDb.materials()[static_cast<usize>(c.targetMaterial)].nameKey));
                } else if (c.targetDepth > 0) {
                    detail = std::format("reach {} m", c.targetDepth);
                } else {
                    detail = std::string(contract_kind_name(t.kind));
                }
                detail += std::format(" - {} cr", t.rewardCredits);
                if (t.rewardInsight > 0) {
                    detail += std::format(" + {} insight", t.rewardInsight);
                }
                mMenu.push_back({pretty(t.nameKey), detail, "", p.contracts.size() < 4,
                                 kIdDynamicBase + static_cast<int>(i)});
            }
            mMenu.push_back({"Back", "", "", true, kIdBack});
            break;
        }
        case Screen::Facilities: {
            mMenuTitle = "BASE FACILITIES";
            mMenuSubtitle = std::format("{} credits", p.credits);
            for (usize i = 0; i < mDb.facilities().size(); ++i) {
                const auto& f = mDb.facilities()[i];
                const i32 tier = p.facilityTier[i];
                const bool maxed = static_cast<usize>(tier + 1) >= f.tiers.size();
                std::string detail = maxed ? std::format("tier {} - maximum", tier)
                                           : std::format("tier {} -> {} - {} cr", tier, tier + 1,
                                                         f.tiers[static_cast<usize>(tier + 1)].costCredits);
                mMenu.push_back(
                    {pretty(f.nameKey), detail, "", !maxed, kIdDynamicBase + static_cast<int>(i)});
            }
            mMenu.push_back({"Back", "", "", true, kIdBack});
            break;
        }
        case Screen::Codex: {
            mMenuTitle = "CODEX";
            mMenuSubtitle = std::format("{} materials logged - {} landmarks found - {} expeditions",
                                        p.codex.size(), p.discoveredLandmarks.size(), p.stats.expeditions);
            for (const auto& [mi, entry] : p.codex) {
                const auto& m = mDb.materials()[static_cast<usize>(mi)];
                mMenu.push_back({pretty(m.nameKey),
                                 std::format("{} mined - first seen {} m - {} cr - hardness {:.1f} - {}",
                                             entry.minedCount, entry.firstDepth, m.baseValue, m.hardness,
                                             std::string(material_category_name(m.category))),
                                 "", false, kIdNone});
            }
            for (const auto& t : mTutorials) {
                if (t.shown) {
                    mMenu.push_back({t.title, t.body, "", false, kIdNone, palette::kAccent});
                }
            }
            mMenu.push_back({"Back", "", "", true, kIdBack});
            break;
        }
        case Screen::Pause: {
            mMenuTitle = "PAUSED";
            mMenuSubtitle = "";
            mMenu.push_back({"Resume", "", "", true, kIdResume});
            mMenu.push_back({"Map", "", "", true, kIdDynamicBase});
            mMenu.push_back({"Settings", "", "", true, kIdSettings});
            mMenu.push_back({"Controls", "", "", true, kIdBindings});
            mMenu.push_back({"Abandon run", "Call for extraction; unsold cargo is at risk", "", true,
                             kIdAbandonRun, palette::kWarn});
            mMenu.push_back({"Save and quit to menu", "", "", true, kIdReturnToMenu});
            break;
        }
        case Screen::Results: {
            const auto& s = mSession->last_summary();
            mMenuTitle = s.rescued ? "EXTRACTED" : "EXPEDITION COMPLETE";
            mMenuSubtitle = std::format("{} m - {:.1f} min - {} tiles - {} credits earned", s.depthReached,
                                        s.seconds / 60.0f, s.tilesMined, s.creditsEarned);
            for (const auto& line : s.sold) {
                mMenu.push_back({pretty(mDb.materials()[static_cast<usize>(line.material)].nameKey),
                                 std::format("{} x -> {} cr", line.amount, line.credits), "", false,
                                 kIdNone});
            }
            for (const i32 t : s.contractsCompleted) {
                mMenu.push_back({"Contract complete", pretty(mDb.contracts()[static_cast<usize>(t)].nameKey),
                                 "", false, kIdNone, palette::kGood});
            }
            mMenu.push_back({"Continue", "", "", true, kIdResultsOk, palette::kAccent});
            break;
        }
        case Screen::Failure: {
            const auto& r = mSession->pending_rescue();
            mMenuTitle = "RESCUE BEACON";
            mMenuSubtitle = r.freeRescue ? "Your first extraction is covered by the cooperative."
                                         : "Extraction has a fee, capped so it can never bury you.";
            mMenu.push_back(
                {"Extraction fee", std::format("{} credits", r.fee), "", false, kIdNone, palette::kWarn});
            mMenu.push_back({"Cargo at risk",
                             std::format("{} units (about {} credits)", r.cargoUnitsLost, r.cargoValueLost),
                             "", false, kIdNone, palette::kWarn});
            mMenu.push_back({"Kept", "All equipment, research, proficiency, samples and base upgrades", "",
                             false, kIdNone, palette::kGood});
            mMenu.push_back({"Accept extraction", "", "", true, kIdConfirmRescue, palette::kAccent});
            break;
        }
        case Screen::Settings: {
            mMenuTitle = "SETTINGS";
            mMenuSubtitle = "Left and right adjust; every option applies immediately.";
            mMenu.push_back(
                {"Fullscreen", mSettings.video.fullscreen ? "on" : "off", "", true, kIdToggleFullscreen});
            mMenu.push_back({"VSync", mSettings.video.vsync ? "on" : "off", "", true, kIdToggleVsync});
            mMenu.push_back({"Master volume", std::format("{:.0f}%", mSettings.audio.master * 100.0f), "",
                             true, kIdVolumeUp});
            mMenu.push_back({"Screen shake", std::format("{:.0f}%", mSettings.access.screenShake * 100.0f),
                             "", true, kIdShakeUp});
            mMenu.push_back({"Flashes", mSettings.access.flashIntensity > 0.0f ? "on" : "reduced", "", true,
                             kIdFlashToggle});
            mMenu.push_back({"Particle density",
                             std::format("{:.0f}%", mSettings.access.particleDensity * 100.0f), "", true,
                             kIdParticlesUp});
            mMenu.push_back({"Colour-vision palette", mSettings.access.colorBlindPalette ? "on" : "off", "",
                             true, kIdPaletteToggle});
            mMenu.push_back({"Text size", std::format("{:.0f}%", mSettings.access.textScale * 100.0f), "",
                             true, kIdTextScaleUp});
            mMenu.push_back({"Mining input", mSettings.access.holdToMine ? "hold" : "toggle", "", true,
                             kIdHoldMineToggle});
            mMenu.push_back({"Thrust input", mSettings.access.holdToThrust ? "hold" : "toggle", "", true,
                             kIdHoldThrustToggle});
            mMenu.push_back({"Assist: fuel drain",
                             std::format("{:.0f}%", mSettings.assists.fuelDrainScale * 100.0f), "", true,
                             kIdAssistFuelUp});
            mMenu.push_back({"Assist: damage taken",
                             std::format("{:.0f}%", mSettings.assists.damageScale * 100.0f), "", true,
                             kIdAssistDamageUp});
            mMenu.push_back({"Assist: heat build-up",
                             std::format("{:.0f}%", mSettings.assists.heatScale * 100.0f), "", true,
                             kIdAssistHeatUp});
            mMenu.push_back({"Assist: drilling time",
                             std::format("{:.0f}%", mSettings.assists.mineTimeScale * 100.0f), "", true,
                             kIdAssistMineUp});
            mMenu.push_back({"Back", "", "", true, kIdBack});
            break;
        }
        case Screen::Bindings: {
            mMenuTitle = "CONTROLS";
            mMenuSubtitle = mAwaitingRebind ? "Press any key or gamepad button..."
                                            : "Select an action, then press the new input.";
            for (int i = 0; i < static_cast<int>(Action::Count); ++i) {
                const Action a = static_cast<Action>(i);
                mMenu.push_back({std::string(action_label(a)), mSettings.describe_binding(a), "", true,
                                 kIdDynamicBase + i});
            }
            mMenu.push_back({"Reset to defaults", "", "", true, kIdResetBindings});
            mMenu.push_back({"Back", "", "", true, kIdBack});
            break;
        }
        case Screen::Credits: {
            mMenuTitle = "CREDITS";
            mMenuSubtitle = "Project Deepforge";
            mMenu.push_back({"Design, code, content", "Original work for this project", "", false, kIdNone});
            mMenu.push_back({"Engine", "SDL3 (zlib licence)", "", false, kIdNone});
            mMenu.push_back({"Data", "nlohmann/json (MIT), Catch2 (BSL-1.0)", "", false, kIdNone});
            mMenu.push_back(
                {"Art and audio", "Generated at runtime; no third-party assets", "", false, kIdNone});
            mMenu.push_back({"Back", "", "", true, kIdBack});
            break;
        }
        case Screen::Error: {
            mMenuTitle = "PROBLEM";
            mMenuSubtitle = mErrorText;
            mMenu.push_back({"Try the backup save", "", "", true, kIdRecoverBackup});
            mMenu.push_back({"Back to menu", "", "", true, kIdReturnToMenu});
            break;
        }
        default:
            break;
    }
    mSelected = std::min(mSelected, static_cast<int>(mMenu.size()) - 1);
    if (mSelected < 0) {
        mSelected = 0;
    }
}

std::string Client::format_stat_delta(float before, float after, bool lowerIsBetter) const {
    const float delta = after - before;
    if (std::fabs(delta) < 0.001f) {
        return "no change";
    }
    const bool good = lowerIsBetter ? delta < 0.0f : delta > 0.0f;
    return std::format("{:.1f} -> {:.1f} ({}{:.1f})", before, after, good ? "+" : "", delta);
}

void Client::navigate(int delta) {
    if (mMenu.empty()) {
        return;
    }
    const int n = static_cast<int>(mMenu.size());
    for (int step = 0; step < n; ++step) {
        mSelected = (mSelected + delta + n) % n;
        if (mMenu[static_cast<usize>(mSelected)].enabled || step == n - 1) {
            break;
        }
    }
    play(Sfx::UiMove);
}

} // namespace df

namespace df {

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void Client::activate(int id) {
    auto& econ = mSession->economy();
    auto& p = mSession->progress();
    switch (id) {
        case kIdContinue: {
            auto r = mSession->load_game(0);
            if (!r) {
                mErrorText = r.error().to_string();
                set_screen(Screen::Error);
            } else {
                mRenderer.invalidate_all();
                set_screen(mSession->state() == GameStateId::Expedition ? Screen::Expedition : Screen::Hub);
            }
            break;
        }
        case kIdNewGame:
            set_screen(Screen::NewGame);
            break;
        case kIdLoadGame:
            set_screen(Screen::LoadGame);
            break;
        case kIdSettings:
            set_screen(Screen::Settings);
            break;
        case kIdBindings:
            set_screen(Screen::Bindings);
            break;
        case kIdCredits:
            set_screen(Screen::Credits);
            break;
        case kIdQuit:
            mRunning = false;
            break;
        case kIdBack:
            set_screen(mPreviousScreen == mScreen ? Screen::MainMenu : mPreviousScreen);
            break;
        case kIdToggleRandomSeed:
            mUseRandomSeed = !mUseRandomSeed;
            if (!mUseRandomSeed && mSeedInput == 0) {
                mSeedInput = 20260905;
            }
            build_menu();
            break;
        case kIdSeedUp:
            mSeedInput += 1000;
            build_menu();
            break;
        case kIdSeedDown:
            mSeedInput = mSeedInput > 1000 ? mSeedInput - 1000 : 0;
            build_menu();
            break;
        case kIdStartWorld: {
            NewGameOptions options;
            options.randomSeed = mUseRandomSeed;
            options.seed = mSeedInput;
            options.assists = mSettings.assists;
            mSession->new_game(options);
            mRenderer.invalidate_all();
            set_screen(Screen::Hub);
            push_toast(std::format("World seed {}", mSession->world_seed()), palette::kAccent);
            break;
        }
        case kIdDescend: {
            mSession->start_expedition();
            mRenderer.invalidate_all();
            set_screen(Screen::Expedition);
            break;
        }
        case kIdGarage:
            set_screen(Screen::Garage);
            break;
        case kIdModules:
            set_screen(Screen::Modules);
            break;
        case kIdSkills:
            set_screen(Screen::Skills);
            break;
        case kIdResearch:
            set_screen(Screen::Research);
            break;
        case kIdRefinery:
            set_screen(Screen::Refinery);
            break;
        case kIdContracts:
            set_screen(Screen::Contracts);
            break;
        case kIdFacilities:
            set_screen(Screen::Facilities);
            break;
        case kIdCodex:
            set_screen(Screen::Codex);
            break;
        case kIdSaveGame: {
            auto r = mSession->save_game(0);
            push_toast(r ? "Saved." : "Save failed: " + r.error().message,
                       r ? palette::kGood : palette::kDanger);
            break;
        }
        case kIdReturnToMenu:
            if (mSession->state() != GameStateId::MainMenu) {
                (void)mSession->save_game(0);
            }
            set_screen(Screen::MainMenu);
            break;
        case kIdResume:
            set_screen(Screen::Expedition);
            break;
        case kIdAbandonRun: {
            mSession->last_summary();
            const auto summary = mSession->return_to_hub();
            (void)summary;
            mRenderer.invalidate_all();
            set_screen(Screen::Results);
            play(Sfx::Return);
            break;
        }
        case kIdConfirmRescue: {
            mSession->resolve_failure();
            mRenderer.invalidate_all();
            set_screen(Screen::Hub);
            push_toast("Extracted. Progression intact.", palette::kGood);
            break;
        }
        case kIdResultsOk:
            set_screen(Screen::Hub);
            break;
        case kIdRespec: {
            auto r = econ.respec_skills();
            push_toast(r ? "Proficiency refunded." : r.error().message,
                       r ? palette::kGood : palette::kDanger);
            mSession->refresh();
            build_menu();
            break;
        }
        case kIdResetBindings:
            mSettings.reset_bindings_to_defaults();
            build_menu();
            break;
        case kIdToggleFullscreen:
            mSettings.video.fullscreen = !mSettings.video.fullscreen;
            mWindow.set_fullscreen(mSettings.video.fullscreen);
            build_menu();
            break;
        case kIdToggleVsync:
            mSettings.video.vsync = !mSettings.video.vsync;
            mWindow.set_vsync(mSettings.video.vsync);
            build_menu();
            break;
        case kIdFlashToggle:
            mSettings.access.flashIntensity = mSettings.access.flashIntensity > 0.0f ? 0.0f : 1.0f;
            build_menu();
            break;
        case kIdPaletteToggle:
            mSettings.access.colorBlindPalette = !mSettings.access.colorBlindPalette;
            mRenderer.invalidate_all();
            build_menu();
            break;
        case kIdHoldMineToggle:
            mSettings.access.holdToMine = !mSettings.access.holdToMine;
            build_menu();
            break;
        case kIdHoldThrustToggle:
            mSettings.access.holdToThrust = !mSettings.access.holdToThrust;
            build_menu();
            break;
        case kIdRecoverBackup: {
            auto r = mSession->load_game(mLoadSlot, true);
            if (!r) {
                mErrorText = r.error().to_string();
                set_screen(Screen::Error);
            } else {
                mRenderer.invalidate_all();
                push_toast("Restored from backup.", palette::kGood);
                set_screen(Screen::Hub);
            }
            break;
        }
        default:
            break;
    }

    if (id < kIdDynamicBase) {
        return;
    }
    const int index = id - kIdDynamicBase;
    switch (mScreen) {
        case Screen::LoadGame: {
            mLoadSlot = index;
            auto r = mSession->load_game(index);
            if (!r) {
                mErrorText = r.error().to_string();
                set_screen(Screen::Error);
            } else {
                mRenderer.invalidate_all();
                set_screen(Screen::Hub);
            }
            break;
        }
        case Screen::Garage: {
            auto r = econ.buy_equipment(index);
            if (r) {
                play(Sfx::Upgrade);
                push_toast("Upgrade installed.", palette::kGood);
                mSession->refresh();
                (void)mSession->autosave();
            } else {
                push_toast(r.error().message, palette::kDanger);
            }
            build_menu();
            break;
        }
        case Screen::Modules: {
            if (p.ownedModules.count(index) == 0) {
                auto r = econ.buy_module(index);
                push_toast(r ? "Module purchased." : r.error().message,
                           r ? palette::kGood : palette::kDanger);
                if (r) {
                    play(Sfx::Purchase);
                }
            } else if (p.module_installed(index)) {
                (void)econ.uninstall_module(index);
                push_toast("Module removed.");
            } else {
                auto r = econ.install_module(index);
                push_toast(r ? "Module installed." : r.error().message,
                           r ? palette::kGood : palette::kDanger);
            }
            mSession->refresh();
            (void)mSession->autosave();
            build_menu();
            break;
        }
        case Screen::Skills: {
            auto r = econ.unlock_skill(index);
            push_toast(r ? "Proficiency unlocked." : r.error().message,
                       r ? palette::kGood : palette::kDanger);
            if (r) {
                play(Sfx::Upgrade);
            }
            mSession->refresh();
            (void)mSession->autosave();
            build_menu();
            break;
        }
        case Screen::Research: {
            auto r = econ.complete_research(index);
            push_toast(r ? "Research complete." : r.error().message, r ? palette::kGood : palette::kDanger);
            if (r) {
                play(Sfx::Discovery);
            }
            mSession->refresh();
            (void)mSession->autosave();
            build_menu();
            break;
        }
        case Screen::Refinery: {
            auto r = econ.run_recipe(index, 1, mSession->sim().stats());
            push_toast(r ? "Batch refined." : r.error().message, r ? palette::kGood : palette::kDanger);
            if (r) {
                play(Sfx::Purchase);
            }
            (void)mSession->autosave();
            build_menu();
            break;
        }
        case Screen::Contracts: {
            if (index >= 500) {
                const usize activeIndex = static_cast<usize>(index - 500);
                if (activeIndex < p.contracts.size()) {
                    (void)econ.abandon_contract(p.contracts[activeIndex].instanceId);
                    push_toast("Contract abandoned.");
                }
            } else {
                auto r = econ.accept_contract(static_cast<usize>(index));
                push_toast(r ? "Contract accepted." : r.error().message,
                           r ? palette::kGood : palette::kDanger);
            }
            (void)mSession->autosave();
            build_menu();
            break;
        }
        case Screen::Facilities: {
            auto r = econ.upgrade_facility(index);
            push_toast(r ? "Facility upgraded." : r.error().message, r ? palette::kGood : palette::kDanger);
            if (r) {
                play(Sfx::Upgrade);
            }
            mSession->refresh();
            (void)mSession->autosave();
            build_menu();
            break;
        }
        case Screen::Bindings: {
            mRebindAction = static_cast<Action>(index);
            mAwaitingRebind = true;
            mInput->begin_capture();
            build_menu();
            break;
        }
        case Screen::Hub: {
            if (index >= 900) {
                mSession->start_expedition(index - 900);
                mRenderer.invalidate_all();
                set_screen(Screen::Expedition);
            }
            break;
        }
        case Screen::Pause:
            set_screen(Screen::Map);
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

void Client::poll_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        mInput->handle_event(e);
        if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            mWindow.refresh_size();
            mSettings.video.windowWidth = mWindow.width();
            mSettings.video.windowHeight = mWindow.height();
        }
    }
}

void Client::drain_domain_events() {
    for (const auto& e : mSession->events().events()) {
        switch (e.kind) {
            case EventKind::TileMined: {
                const auto& m = mDb.materials()[static_cast<usize>(std::max(0, e.a))];
                mRenderer.spawn_break_particles(
                    Vec2{static_cast<float>(e.tile.x) + 0.5f, static_cast<float>(e.tile.y) + 0.5f},
                    WorldRenderer::material_color(m, mSettings), mSettings);
                mRenderer.add_shake(m.hardness > 3.0f ? 0.12f : 0.05f, mSettings);
                Sfx s = Sfx::TileBreak;
                if (m.category == MaterialCategory::Volatile) {
                    s = Sfx::VolatileBreak;
                } else if (m.breakAudioId.find("crystal") != std::string::npos) {
                    s = Sfx::CrystalBreak;
                } else if (m.baseValue > 0) {
                    s = Sfx::OreBreak;
                }
                if (mAudio) {
                    mAudio->play(s, static_cast<float>((e.tile.x * 31 + e.tile.y * 17) % 11) / 60.0f);
                }
                break;
            }
            case EventKind::CargoAdded:
            case EventKind::SampleFound: {
                const auto& m = mDb.materials()[static_cast<usize>(std::max(0, e.a))];
                mRenderer.spawn_pickup(
                    Vec2{static_cast<float>(e.tile.x) + 0.5f, static_cast<float>(e.tile.y) + 0.5f},
                    WorldRenderer::material_color(m, mSettings), mSettings);
                if (mAudio) {
                    mAudio->play(Sfx::Pickup);
                }
                if (e.kind == EventKind::SampleFound) {
                    push_toast(std::format("Sample recovered: {}", pretty(m.nameKey)), palette::kAccent);
                }
                break;
            }
            case EventKind::CargoFull:
                push_toast("Cargo hold full - return to the surface", palette::kWarn);
                play(Sfx::CargoFull);
                break;
            case EventKind::FuelLow:
                push_toast("Fuel low", palette::kWarn);
                play(Sfx::LowFuel);
                break;
            case EventKind::FuelEmpty:
                push_toast("Out of fuel", palette::kDanger);
                break;
            case EventKind::VehicleDamaged:
                mRenderer.add_shake(0.25f, mSettings);
                if (mAudio) {
                    mAudio->play(Sfx::HullDamage);
                }
                break;
            case EventKind::HeatCritical:
                if (e.a == 1) {
                    push_toast("Heat critical", palette::kDanger);
                }
                play(Sfx::HeatWarning);
                break;
            case EventKind::LandmarkDiscovered:
                push_toast(std::format("Discovered {}", pretty(e.text)), palette::kAccent);
                play(Sfx::Discovery);
                break;
            case EventKind::LandmarkActivated:
                push_toast(std::format("Activated {}", pretty(e.text)), palette::kGood);
                play(Sfx::Upgrade);
                break;
            case EventKind::BiomeEntered:
                push_toast(std::format("Entering {}", pretty(e.text)), palette::kAccent);
                break;
            case EventKind::DepthRecord:
                push_toast(std::format("New depth record: {:.0f} m", e.value), palette::kGood);
                break;
            case EventKind::Unstuck:
                push_toast("Emergency reposition used", palette::kWarn);
                break;
            case EventKind::ScanPulse:
                play(Sfx::Scan);
                break;
            default:
                break;
        }
    }
}

void Client::update_tutorials() {
    auto trigger = [&](const char* id) {
        for (auto& t : mTutorials) {
            if (t.id == id && !t.shown) {
                t.shown = true;
                mActiveTutorial = t.title + ": " + t.body;
                push_toast(t.title, palette::kAccent);
                return;
            }
        }
    };
    const auto& p = mSession->progress();
    if (mScreen == Screen::Expedition) {
        trigger("move");
        if (mSession->sim().runtime().tilesMined > 2) {
            trigger("mine");
        }
        const auto snap = mSession->snapshot(1.0f);
        if (snap.cargoUsed > snap.cargoCapacity * 0.5f) {
            trigger("cargo");
        }
        if (snap.fuel < snap.fuelCapacity * 0.5f) {
            trigger("fuel");
        }
        if (snap.depth > 900) {
            trigger("heat");
        }
    }
    if (mScreen == Screen::Hub && p.credits > 100) {
        trigger("upgrade");
    }
}

void Client::update(float frameSeconds) {
    mInput->update(mSettings);
    if (mAudio) {
        mAudio->set_volumes(mSettings.audio.master, mSettings.audio.sfx);
    }

    if (mAwaitingRebind) {
        if (mInput->capture_ready()) {
            mSettings.rebind(mRebindAction, mInput->captured_binding());
            mAwaitingRebind = false;
            build_menu();
            play(Sfx::UiSelect);
        }
        return;
    }

    if (mInput->quit_requested()) {
        mRunning = false;
    }
    if (mInput->pressed(Action::DebugOverlay)) {
        mDebugOverlay = !mDebugOverlay;
    }

    for (auto& t : mToasts) {
        t.life -= frameSeconds;
    }
    while (!mToasts.empty() && mToasts.front().life <= 0.0f) {
        mToasts.pop_front();
    }
    mRenderer.update_effects(frameSeconds);

    if (mScreen == Screen::Expedition) {
        if (mInput->pressed(Action::Pause)) {
            set_screen(Screen::Pause);
            return;
        }
        if (mInput->pressed(Action::Map)) {
            set_screen(Screen::Map);
            return;
        }
        // Fixed-step accumulator with a bounded catch-up (Section 15.3).
        mAccumulator += static_cast<double>(frameSeconds);
        int steps = 0;
        const auto tickStart = SDL_GetPerformanceCounter();
        while (mAccumulator >= kFixedDeltaSeconds && steps < 5) {
            const TickCommands cmd = mInput->to_commands(mSettings);
            mSession->tick(cmd);
            drain_domain_events();
            mAccumulator -= kFixedDeltaSeconds;
            ++steps;
        }
        if (mAccumulator > kFixedDeltaSeconds * 5.0) {
            // Degrade rather than spiral after a long stall.
            mAccumulator = 0.0;
        }
        mTickTimeMs = static_cast<double>(SDL_GetPerformanceCounter() - tickStart) * 1000.0 /
                      static_cast<double>(SDL_GetPerformanceFrequency());

        if (mAudio) {
            const auto snap = mSession->snapshot(1.0f);
            mAudio->set_drill_active(snap.mining, 0.8f);
        }
        if (mSession->state() == GameStateId::FailureRecovery) {
            set_screen(Screen::Failure);
        } else if (mSession->state() == GameStateId::ExpeditionResults) {
            mSession->return_to_hub();
            mRenderer.invalidate_all();
            play(Sfx::Return);
            set_screen(Screen::Results);
        }
    } else {
        if (mAudio) {
            mAudio->set_drill_active(false, 0.0f);
        }
        if (mInput->pressed(Action::NavUp)) {
            navigate(-1);
        }
        if (mInput->pressed(Action::NavDown)) {
            navigate(1);
        }
        if (mInput->pressed(Action::Confirm) && !mMenu.empty()) {
            const auto& entry = mMenu[static_cast<usize>(mSelected)];
            if (entry.enabled) {
                play(Sfx::UiSelect);
                activate(entry.id);
            }
        }
        if (mInput->pressed(Action::Cancel) || mInput->pressed(Action::Pause)) {
            play(Sfx::UiBack);
            if (mScreen == Screen::Map) {
                set_screen(Screen::Expedition);
            } else if (mScreen == Screen::Pause) {
                set_screen(Screen::Expedition);
            } else if (mScreen != Screen::MainMenu) {
                activate(kIdBack);
            }
        }
        // Left/right adjust the selected slider on the settings screen.
        if (mScreen == Screen::Settings && !mMenu.empty()) {
            const int dir = mInput->pressed(Action::NavRight) ? 1 : mInput->pressed(Action::NavLeft) ? -1 : 0;
            if (dir != 0) {
                const int id = mMenu[static_cast<usize>(mSelected)].id;
                const float step = 0.1f * static_cast<float>(dir);
                switch (id) {
                    case kIdVolumeUp:
                        mSettings.audio.master = saturate(mSettings.audio.master + step);
                        break;
                    case kIdShakeUp:
                        mSettings.access.screenShake = saturate(mSettings.access.screenShake + step);
                        break;
                    case kIdParticlesUp:
                        mSettings.access.particleDensity = saturate(mSettings.access.particleDensity + step);
                        break;
                    case kIdTextScaleUp:
                        mSettings.access.textScale = clamp(mSettings.access.textScale + step, 0.7f, 2.0f);
                        break;
                    case kIdAssistFuelUp:
                        mSettings.assists.fuelDrainScale =
                            clamp(mSettings.assists.fuelDrainScale + step, 0.2f, 1.5f);
                        break;
                    case kIdAssistDamageUp:
                        mSettings.assists.damageScale =
                            clamp(mSettings.assists.damageScale + step, 0.0f, 1.5f);
                        break;
                    case kIdAssistHeatUp:
                        mSettings.assists.heatScale = clamp(mSettings.assists.heatScale + step, 0.2f, 1.5f);
                        break;
                    case kIdAssistMineUp:
                        mSettings.assists.mineTimeScale =
                            clamp(mSettings.assists.mineTimeScale + step, 0.4f, 1.5f);
                        break;
                    default:
                        break;
                }
                mSession->set_assists(mSettings.assists);
                build_menu();
            }
        }
    }
    update_tutorials();
}

} // namespace df

namespace df {

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void Client::render_hud(Draw& d, const RenderSnapshot& snap) {
    const float w = static_cast<float>(mWindow.width());
    const float h = static_cast<float>(mWindow.height());
    const float s = d.scale();
    const float pad = 14.0f * s;
    const float barW = 210.0f * s;
    const float barH = 15.0f * s;
    const float rowH = 22.0f * s;
    const float textSize = 13.0f * mSettings.access.textScale;

    d.panel(pad, pad, barW + 96.0f * s, rowH * 4.0f + pad, palette::kPanel);

    struct Row {
        const char* label;
        float value;
        float max;
        Color color;
    };

    const Row rows[] = {
        {"FUEL", snap.fuel, snap.fuelCapacity, palette::kFuel},
        {"HULL", snap.hull, snap.hullMax, palette::kHull},
        {"HEAT", snap.heat, snap.heatCapacity, palette::kHeat},
        {"CARGO", snap.cargoUsed, snap.cargoCapacity, palette::kCargo},
    };
    float y = pad + 8.0f * s;
    for (const auto& row : rows) {
        const float fraction = row.max > 0.0f ? row.value / row.max : 0.0f;
        // Icon + text + colour: never colour alone (Section 12.2).
        d.text(row.label, pad + 8.0f * s, y, palette::kTextDim, textSize);
        d.bar(pad + 62.0f * s, y - 2.0f * s, barW, barH, fraction,
              fraction < 0.2f ? palette::kDanger : row.color, Color{40, 42, 52}, true);
        d.text(std::format("{:.0f}/{:.0f}", row.value, row.max), pad + 62.0f * s + barW - 6.0f * s, y,
               palette::kText, textSize, TextAlign::Right);
        y += rowH;
    }

    // Depth, biome and credits.
    const auto& biome = mDb.biomes()[static_cast<usize>(snap.biomeIndex)];
    d.panel(w - 260.0f * s - pad, pad, 260.0f * s, rowH * 3.0f, palette::kPanel);
    d.text(std::format("DEPTH {} M", snap.depth), w - pad - 12.0f * s, pad + 8.0f * s, palette::kText,
           textSize, TextAlign::Right);
    d.text(pretty(biome.nameKey), w - pad - 12.0f * s, pad + 8.0f * s + rowH, palette::kAccent, textSize,
           TextAlign::Right);
    d.text(std::format("{} CR   {} INSIGHT", snap.credits, snap.insight), w - pad - 12.0f * s,
           pad + 8.0f * s + rowH * 2.0f, palette::kTextDim, textSize, TextAlign::Right);

    // Contextual mining readout: what is being cut, how hard, how far along.
    if (snap.mining) {
        const auto& mat = mSession->world().material(snap.mineTarget);
        const float hardness = mSession->world().hardness(snap.mineTarget);
        const MineQuote q = mSession->sim().quote(mSession->rig(), snap.mineTarget);
        const float boxW = 300.0f * s;
        const float boxH = 62.0f * s;
        d.panel(w * 0.5f - boxW * 0.5f, h - boxH - pad * 3.0f, boxW, boxH, palette::kPanel);
        d.text(pretty(mat.nameKey), w * 0.5f, h - boxH - pad * 3.0f + 8.0f * s, palette::kText, textSize,
               TextAlign::Center);
        d.text(std::format("HARDNESS {:.1f}  {}  {:.1f}S  {} CR", hardness,
                           std::string(material_category_name(mat.category)), q.seconds, mat.baseValue),
               w * 0.5f, h - boxH - pad * 3.0f + 8.0f * s + 16.0f * s, palette::kTextDim, textSize * 0.9f,
               TextAlign::Center);
        d.bar(w * 0.5f - boxW * 0.4f, h - boxH - pad * 3.0f + 42.0f * s, boxW * 0.8f, 8.0f * s,
              snap.mineProgress, palette::kAccent, Color{40, 42, 52});
    }

    // Active contracts.
    const auto& contracts = mSession->progress().contracts;
    if (!contracts.empty()) {
        float cy = pad + rowH * 5.0f;
        d.text("OBJECTIVES", pad + 8.0f * s, cy, palette::kTextDim, textSize * 0.9f);
        cy += 16.0f * s;
        for (const auto& c : contracts) {
            const auto& t = mDb.contracts()[static_cast<usize>(c.templateIndex)];
            std::string line = pretty(t.nameKey);
            if (c.targetMaterial >= 0) {
                line += std::format(" {}/{}", c.progress, c.targetAmount);
            } else if (c.targetDepth > 0) {
                line += std::format(" {}/{} m", snap.depth, c.targetDepth);
            }
            d.text(line, pad + 8.0f * s, cy, c.completed ? palette::kGood : palette::kText, textSize * 0.9f);
            cy += 14.0f * s;
        }
    }

    // Toasts and captions.
    float ty = h - pad;
    for (auto it = mToasts.rbegin(); it != mToasts.rend(); ++it) {
        ty -= 18.0f * s;
        const u8 alpha = static_cast<u8>(clamp(it->life / 1.5f, 0.0f, 1.0f) * 255.0f);
        d.text(it->text, pad, ty, it->color.with_alpha(alpha), textSize);
    }

    if (!mActiveTutorial.empty()) {
        const float boxW = 520.0f * s;
        d.panel(w * 0.5f - boxW * 0.5f, pad + rowH * 4.5f, boxW, 46.0f * s, Color{22, 30, 44, 230},
                palette::kAccent);
        d.text(mActiveTutorial.substr(0, 92), w * 0.5f, pad + rowH * 4.5f + 10.0f * s, palette::kText,
               textSize * 0.95f, TextAlign::Center);
        if (mActiveTutorial.size() > 92) {
            d.text(mActiveTutorial.substr(92, 92), w * 0.5f, pad + rowH * 4.5f + 26.0f * s, palette::kText,
                   textSize * 0.95f, TextAlign::Center);
        }
    }
}

void Client::render_menu(Draw& d, std::string_view title, std::string_view subtitle) {
    const float w = static_cast<float>(mWindow.width());
    const float h = static_cast<float>(mWindow.height());
    const float s = d.scale();
    const float textSize = 14.0f * mSettings.access.textScale;

    d.rect(0.0f, 0.0f, w, h, Color{10, 11, 16, static_cast<u8>(mScreen == Screen::Pause ? 205 : 255)});
    const float panelW = std::min(w - 80.0f * s, 900.0f * s);
    const float panelX = (w - panelW) * 0.5f;
    d.panel(panelX, 40.0f * s, panelW, h - 80.0f * s);

    d.text(title, w * 0.5f, 58.0f * s, palette::kAccent, 24.0f * mSettings.access.textScale,
           TextAlign::Center);
    if (!subtitle.empty()) {
        d.text(subtitle, w * 0.5f, 88.0f * s, palette::kTextDim, textSize * 0.95f, TextAlign::Center);
    }

    const float rowH = 26.0f * s * mSettings.access.textScale;
    const float listTop = 116.0f * s;
    const float listBottom = h - 70.0f * s;
    const int visible = std::max(1, static_cast<int>((listBottom - listTop) / rowH));
    if (mSelected < mScroll) {
        mScroll = mSelected;
    }
    if (mSelected >= mScroll + visible) {
        mScroll = mSelected - visible + 1;
    }
    for (int i = 0; i < visible; ++i) {
        const int index = mScroll + i;
        if (index >= static_cast<int>(mMenu.size())) {
            break;
        }
        const auto& entry = mMenu[static_cast<usize>(index)];
        const float y = listTop + static_cast<float>(i) * rowH;
        const bool selected = index == mSelected;
        if (selected) {
            d.rect(panelX + 10.0f * s, y - 3.0f * s, panelW - 20.0f * s, rowH - 3.0f * s,
                   Color{48, 62, 84, 220});
            d.text(">", panelX + 18.0f * s, y, palette::kAccent, textSize);
        }
        const Color c = entry.enabled ? entry.accent : palette::kTextDim.with_alpha(150);
        // Reserve a label column and a detail column so long strings truncate
        // instead of overlapping.
        const float labelX = panelX + 34.0f * s;
        const float detailRight = panelX + panelW - 18.0f * s;
        const float labelWidth = (panelW - 52.0f * s) * 0.34f;
        const float detailWidth = (panelW - 52.0f * s) * 0.62f;
        const float labelCharW = d.text_width("M", textSize);
        const float detailCharW = d.text_width("M", textSize * 0.9f);
        std::string label = entry.label;
        if (labelCharW > 0.0f) {
            const usize maxChars = static_cast<usize>(labelWidth / labelCharW);
            if (label.size() > maxChars && maxChars > 1) {
                label = label.substr(0, maxChars - 1) + ".";
            }
        }
        d.text(label, labelX, y, c, textSize);
        if (!entry.detail.empty() && detailCharW > 0.0f) {
            std::string detail = entry.detail;
            const usize maxChars = static_cast<usize>(detailWidth / detailCharW);
            if (detail.size() > maxChars && maxChars > 1) {
                detail = detail.substr(0, maxChars - 1) + ".";
            }
            d.text(detail, detailRight, y, palette::kTextDim, textSize * 0.9f, TextAlign::Right);
        }
    }
    if (static_cast<int>(mMenu.size()) > visible) {
        d.text(std::format("{}-{} of {}", mScroll + 1,
                           std::min<int>(mScroll + visible, static_cast<int>(mMenu.size())), mMenu.size()),
               panelX + panelW - 18.0f * s, listBottom + 6.0f * s, palette::kTextDim, textSize * 0.85f,
               TextAlign::Right);
    }
    d.text("MOVE: UP/DOWN   SELECT: ENTER OR A   BACK: BACKSPACE OR B", w * 0.5f, h - 62.0f * s,
           palette::kTextDim, textSize * 0.85f, TextAlign::Center);

    float ty = h - 26.0f * s;
    for (auto it = mToasts.rbegin(); it != mToasts.rend(); ++it) {
        const u8 alpha = static_cast<u8>(clamp(it->life / 1.5f, 0.0f, 1.0f) * 255.0f);
        d.text(it->text, 20.0f * s, ty, it->color.with_alpha(alpha), textSize * 0.9f);
        ty -= 16.0f * s;
    }
}

void Client::render_map(Draw& d) {
    const float w = static_cast<float>(mWindow.width());
    const float h = static_cast<float>(mWindow.height());
    const float s = d.scale();
    d.rect(0.0f, 0.0f, w, h, Color{8, 9, 14, 240});
    d.text("EXPEDITION MAP", w * 0.5f, 30.0f * s, palette::kAccent, 22.0f, TextAlign::Center);

    const auto snap = mSession->snapshot(1.0f);
    const float mapW = w - 120.0f * s;
    const float mapH = h - 140.0f * s;
    const float ox = 60.0f * s;
    const float oy = 80.0f * s;
    d.panel(ox, oy, mapW, mapH);

    // Two tiles per pixel keeps a whole biome legible on screen.
    const int step = 2;
    const i64 halfX = static_cast<i64>(mapW / 2.0f) * step / 2;
    const i64 halfY = static_cast<i64>(mapH / 2.0f) * step / 2;
    const TileCoord centre = mSession->rig().tile();
    for (float py = 0; py < mapH; py += 2.0f) {
        for (float px = 0; px < mapW; px += 2.0f) {
            const i64 tx = centre.x - halfX + static_cast<i64>(px * static_cast<float>(step));
            const i64 ty = centre.y - halfY + static_cast<i64>(py * static_cast<float>(step));
            const TileCoord t{tx, ty};
            if (!mSession->world().revealed(t)) {
                continue;
            }
            const auto& m = mSession->world().material(t);
            if (m.category == MaterialCategory::Empty) {
                d.rect(ox + px, oy + py, 2.0f, 2.0f, Color{30, 32, 40});
                continue;
            }
            Color c = WorldRenderer::material_color(m, mSettings);
            if (m.category == MaterialCategory::Rock) {
                c = c.scaled(0.6f);
            }
            d.rect(ox + px, oy + py, 2.0f, 2.0f, c);
        }
    }
    d.rect(ox + mapW * 0.5f - 3.0f, oy + mapH * 0.5f - 3.0f, 6.0f, 6.0f, palette::kAccent);
    d.text(std::format("DEPTH {} M   SEED {}", snap.depth, mSession->world_seed()), w * 0.5f, h - 44.0f * s,
           palette::kTextDim, 13.0f, TextAlign::Center);
    d.text("BACK: M OR B", w * 0.5f, h - 26.0f * s, palette::kTextDim, 12.0f, TextAlign::Center);
}

void Client::render_debug(Draw& d) {
    const float s = d.scale();
    const auto& stats = mSession->world().stats();
    const auto snap = mSession->snapshot(1.0f);
    const char* lines[10];
    std::string buf[10];
    buf[0] =
        std::format("frame {:.2f} ms   tick {:.2f} ms   frames {}", mFrameTimeMs, mTickTimeMs, mFrameCount);
    buf[1] = std::format("chunks resident {}   deltas {}   generated {}   evicted {}", stats.residentChunks,
                         stats.deltaChunks, stats.chunksGenerated, stats.chunksEvicted);
    buf[2] =
        std::format("chunk textures {}   draw calls {}", mRenderer.cached_chunks(), mRenderer.draw_calls());
    buf[3] = std::format("rig ({:.2f}, {:.2f}) vel ({:.2f}, {:.2f}) ground {} anchored {}",
                         mSession->rig().position.x, mSession->rig().position.y, mSession->rig().velocity.x,
                         mSession->rig().velocity.y, mSession->rig().onGround, mSession->rig().anchored);
    buf[4] = std::format("state {}   seed {}   biome {}", std::string(game_state_name(mSession->state())),
                         mSession->world_seed(), snap.biomeIndex);
    buf[5] = std::format("drill {:.2f}  thrust {:.2f}  cooling {:.2f}  scan {:.1f}",
                         mSession->sim().stats().drillPower, mSession->sim().stats().thrust,
                         mSession->sim().stats().cooling, mSession->sim().stats().scannerRange);
    buf[6] = std::format("play {:.1f}s   tiles {}   expeditions {}", mSession->play_seconds(),
                         mSession->progress().stats.tilesMined, mSession->progress().stats.expeditions);
    const int count = 7;
    d.panel(10.0f * s, static_cast<float>(mWindow.height()) - 130.0f * s, 620.0f * s, 120.0f * s,
            Color{0, 0, 0, 200});
    for (int i = 0; i < count; ++i) {
        lines[i] = buf[i].c_str();
        d.text(lines[i], 18.0f * s,
               static_cast<float>(mWindow.height()) - 122.0f * s + static_cast<float>(i) * 15.0f * s,
               palette::kGood, 12.0f);
    }
}

void Client::render(float alpha) {
    Draw d(mWindow.renderer());
    const float uiScale = mSettings.video.uiScale > 0.0f
                              ? mSettings.video.uiScale
                              : std::max(1.0f, static_cast<float>(mWindow.height()) / 900.0f);
    d.set_scale(uiScale);

    const float w = static_cast<float>(mWindow.width());
    const float h = static_cast<float>(mWindow.height());

    if (mScreen == Screen::Expedition || mScreen == Screen::Pause || mScreen == Screen::Map) {
        const auto snap = mSession->snapshot(alpha);
        mCamera.pixelsPerTile = kBaseTilePixels * uiScale * mSettings.video.zoom;
        const Vec2 lookAhead = mSettings.access.reduceCameraMotion ? Vec2{} : snap.rigVelocity * 0.16f;
        const Vec2 desired = snap.rigPosition + lookAhead;
        // Damped follow keeps the rig readable without swimming.
        mCamera.centre.x = approach(mCamera.centre.x, desired.x, 9.0f, 1.0f / 60.0f);
        mCamera.centre.y = approach(mCamera.centre.y, desired.y, 9.0f, 1.0f / 60.0f);
        mCamera.shakeOffset = mRenderer.shake_offset();
        mRenderer.render(d, *mSession, snap, mCamera, mSettings, w, h);
        if (mScreen != Screen::Map) {
            render_hud(d, snap);
        }
    } else {
        d.clear(palette::kBackground);
    }

    switch (mScreen) {
        case Screen::Expedition:
            break;
        case Screen::Map:
            render_map(d);
            break;
        default:
            render_menu(d, mMenuTitle, mMenuSubtitle);
            break;
    }

    if (mDebugOverlay) {
        render_debug(d);
    }
    SDL_RenderPresent(mWindow.renderer());
}

int Client::run() {
    u64 last = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    while (mRunning) {
        const u64 now = SDL_GetPerformanceCounter();
        const double frameSeconds = std::min(0.25, static_cast<double>(now - last) / freq);
        last = now;
        mFrameTimeMs = frameSeconds * 1000.0;
        ++mFrameCount;

        poll_events();
        update(static_cast<float>(frameSeconds));
        if (mAudio) {
            mAudio->update();
        }
        const float alpha = static_cast<float>(mAccumulator / kFixedDeltaSeconds);
        render(clamp(alpha, 0.0f, 1.0f));
    }
    if (mSession->state() == GameStateId::SurfaceHub || mSession->state() == GameStateId::Expedition) {
        (void)mSession->save_game(0);
    }
    (void)save_settings(mUserDir / "settings.json", mSettings);
    return 0;
}

Status Client::capture_demo(const std::filesystem::path& outFile, u64 seed, int ticks, Screen screen) {
    NewGameOptions options;
    options.seed = seed;
    options.randomSeed = false;
    options.assists = mSettings.assists;
    mSession->new_game(options);
    mRenderer.invalidate_all();

    if (screen == Screen::Expedition || screen == Screen::Map) {
        AutopilotConfig cfg;
        cfg.targetDepth = 90;
        cfg.maxTicks = ticks;
        // Drive a real expedition so the capture shows played terrain, not a
        // pristine slice of generation.
        mSession->start_expedition();
        bool returning = false;
        for (int i = 0; i < ticks && mSession->state() == GameStateId::Expedition; ++i) {
            const auto here = mSession->rig().tile();
            TickCommands cmd;
            if (!returning && (here.y >= cfg.targetDepth || mSession->rig().fuel < 45.0f)) {
                returning = true;
            }
            if (returning) {
                const bool blocked = mSession->world().is_solid(TileCoord{here.x, here.y - 1});
                cmd.mine = blocked ? MineDirection::Up : MineDirection::None;
                cmd.thrust = !blocked;
            } else if (mSession->world().is_solid(TileCoord{here.x, here.y + 1})) {
                cmd.mine = MineDirection::Down;
            } else if (i % 90 < 30) {
                cmd.mine = MineDirection::Right;
                cmd.moveX = 0.6f;
            }
            mSession->tick(cmd);
            drain_domain_events();
            mRenderer.update_effects(static_cast<float>(kFixedDeltaSeconds));
        }
        mCamera.centre = mSession->rig().position;
    }
    set_screen(screen);
    poll_events();
    render(1.0f);
    render(1.0f);

    SDL_Surface* surface = SDL_RenderReadPixels(mWindow.renderer(), nullptr);
    if (surface == nullptr) {
        return fail(Error{std::string("could not read pixels: ") + SDL_GetError(), {}, {}, {}, {}});
    }
    const bool ok = SDL_SaveBMP(surface, outFile.string().c_str());
    SDL_DestroySurface(surface);
    if (!ok) {
        return fail(
            Error{std::string("could not save screenshot: ") + SDL_GetError(), outFile.string(), {}, {}, {}});
    }
    DF_LOG_INFO("qa", "wrote screenshot {}", outFile.string());
    return {};
}

int Client::run_headless_frames(int frames) {
    for (int i = 0; i < frames && mRunning; ++i) {
        poll_events();
        update(1.0f / 60.0f);
        render(1.0f);
    }
    return 0;
}

} // namespace df
