#include "presentation/Client.hpp"

#include "app/Autopilot.hpp"
#include "core/Log.hpp"
#include "core/Version.hpp"
#include "domain/Loadout.hpp"
#include "presentation/Pretty.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>

namespace df {
namespace {

constexpr float kBaseTilePixels = 34.0f;

// Menu action ids. Screen-specific list entries start at kIdDynamicBase.
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
    kIdOpenMap,
    kIdAbandonRun,
    kIdConfirmRescue,
    kIdResultsOk,
    kIdRespec,
    kIdResetBindings,
    kIdToggleFullscreen,
    kIdToggleVsync,
    kIdFlashToggle,
    kIdPaletteToggle,
    kIdHoldMineToggle,
    kIdHoldThrustToggle,
    kIdReduceMotionToggle,
    kIdSubtitlesToggle,
    kIdVolume,
    kIdShake,
    kIdParticles,
    kIdTextScale,
    kIdAssistFuel,
    kIdAssistDamage,
    kIdAssistHeat,
    kIdAssistMine,
    kIdRecoverBackup,
    kIdDynamicBase = 10000,
};

/// Every continuous setting and its range, in one place. The row's track, the
/// keyboard step and the pointer drag all read the range from here, so they can
/// never disagree about what "full" means.
struct SliderSpec {
    int id;
    float min;
    float max;
    bool percent; ///< Shown as a percentage rather than a bare multiplier.
};

inline constexpr SliderSpec kSliders[] = {
    {kIdVolume, 0.0f, 1.0f, true},     {kIdShake, 0.0f, 1.0f, true},      {kIdParticles, 0.0f, 1.0f, true},
    {kIdTextScale, 0.7f, 2.0f, true},  {kIdAssistFuel, 0.2f, 1.5f, true}, {kIdAssistDamage, 0.0f, 1.5f, true},
    {kIdAssistHeat, 0.2f, 1.5f, true}, {kIdAssistMine, 0.4f, 1.5f, true},
};

const SliderSpec* slider_spec(int id) {
    for (const auto& spec : kSliders) {
        if (spec.id == id) {
            return &spec;
        }
    }
    return nullptr;
}

std::string thousands(i64 v) {
    std::string s = std::to_string(v < 0 ? -v : v);
    std::string out;
    int count = 0;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        out.push_back(*it);
        if (++count % 3 == 0 && it + 1 != s.rend()) {
            out.push_back(',');
        }
    }
    if (v < 0) {
        out.push_back('-');
    }
    std::reverse(out.begin(), out.end());
    return out;
}

Icon slot_icon(ModuleSlot slot) {
    switch (slot) {
        case ModuleSlot::Drill:
            return Icon::Drill;
        case ModuleSlot::Mobility:
            return Icon::Descend;
        case ModuleSlot::Survival:
            return Icon::Hull;
        case ModuleSlot::Survey:
            return Icon::Scanner;
        default:
            return Icon::Module;
    }
}

Icon track_icon(const std::string& track) {
    if (track == "drill_power" || track == "drill_efficiency") {
        return Icon::Drill;
    }
    if (track == "fuel_tank") {
        return Icon::Fuel;
    }
    if (track == "cargo_bay") {
        return Icon::Cargo;
    }
    if (track == "hull") {
        return Icon::Hull;
    }
    if (track == "cooling") {
        return Icon::Heat;
    }
    if (track == "scanner") {
        return Icon::Scanner;
    }
    if (track == "engine") {
        return Icon::Descend;
    }
    return Icon::Module;
}

Icon material_icon(MaterialCategory c) {
    switch (c) {
        case MaterialCategory::Commodity:
            return Icon::Credit;
        case MaterialCategory::Strategic:
            return Icon::Module;
        case MaterialCategory::Volatile:
            return Icon::Warning;
        case MaterialCategory::Artifact:
            return Icon::Star;
        default:
            return Icon::Cargo;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

Client::Client(ContentDatabase db, std::filesystem::path userDir)
    : mDb(std::move(db)), mUserDir(std::move(userDir)) {
    auto loaded = load_settings(mUserDir / "settings.json");
    mSettings = loaded ? *loaded : default_settings();
    mSession = std::make_unique<GameSession>(mDb, mUserDir / "saves");
    mTutorials = {
        {"move", "Piloting",
         "A and D steer. W or Space fires the lift thrusters. They burn fuel, so climbing back out "
         "is the expensive half of every run.",
         Icon::Descend, false},
        {"mine", "Drilling",
         "Hold S to cut downward, J and L for the sides, I for overhead. The rig braces while the "
         "drill is engaged, so cutting upward costs no thrust.",
         Icon::Drill, false},
        {"cargo", "Cargo",
         "Ore fills the hold. When it is full, head for the surface. Unsold cargo is the only thing "
         "a failed run can cost you.",
         Icon::Cargo, false},
        {"fuel", "Fuel",
         "Read the fuel bar against your depth, not on its own. Cutting a shaft home is cheap but "
         "slow; flying up an open shaft is fast but thirsty.",
         Icon::Fuel, false},
        {"upgrade", "The Garage",
         "Sell your haul, then spend credits in the Garage. The first tank or drill tier changes "
         "how deep a single run can reach.",
         Icon::Garage, false},
        {"heat", "Heat",
         "Below the Ember Mantle the rock itself is hot. Cooling and heat sinks turn that from a "
         "wall into a route choice.",
         Icon::Heat, false},
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

float Client::ui_scale() const {
    if (mSettings.video.uiScale > 0.0f) {
        return mSettings.video.uiScale;
    }
    // Integer scaling only: the interface is a bitmap face and stays sharp only
    // at whole multiples.
    return clamp(std::round(static_cast<float>(mWindow.height()) / 520.0f), 1.0f, 8.0f);
}

void Client::set_screen(Screen s) {
    if (mScreen != s) {
        mPreviousScreen = mScreen;
        mScreenIntro = 0.0f;
    }
    mScreen = s;
    mSelected = 0;
    mScroll = 0.0f;
    build_screen();
}

void Client::push_toast(std::string text, Color c, Icon icon) {
    mToasts.push_back(Toast{std::move(text), icon, 4.2f, 0.0f, c});
    while (mToasts.size() > 5) {
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
            push_toast(std::string(caption), theme::kTextFaint);
        }
    }
}

void Client::navigate(int delta) {
    if (mMenu.items.empty()) {
        return;
    }
    const int n = static_cast<int>(mMenu.items.size());
    for (int step = 0; step < n; ++step) {
        mSelected = (mSelected + delta + n) % n;
        if (!mMenu.items[static_cast<usize>(mSelected)].inert || step == n - 1) {
            break;
        }
    }
    play(Sfx::UiMove);
}

} // namespace df

namespace df {

// ---------------------------------------------------------------------------
// Screen builders
// ---------------------------------------------------------------------------

std::string Client::name_of(std::string_view key) const {
    const auto t = mDb.text(key);
    return t.empty() ? pretty_key(key) : std::string(t);
}

std::string Client::id_name(std::string_view id) const {
    return name_of(std::string(id) + ".name");
}

std::string Client::humanise(std::string_view reason) const {
    // Domain reasons name content by id, because the domain has no business
    // knowing display names. The translation happens here, at the one layer that
    // does: "requires research research.field_metallurgy" becomes
    // "Requires research Field Metallurgy".
    static constexpr std::string_view kPrefixes[] = {"material.",  "research.", "skill.",    "module.",
                                                     "equipment.", "recipe.",   "facility.", "contract.",
                                                     "landmark.",  "hazard.",   "biome.",    "faction."};

    std::string out;
    std::string token;
    auto flush = [&]() {
        if (token.empty()) {
            return;
        }
        bool isId = false;
        for (const std::string_view prefix : kPrefixes) {
            if (token.starts_with(prefix)) {
                isId = true;
                break;
            }
        }
        out += isId ? id_name(token) : token;
        token.clear();
    };
    for (const char ch : reason) {
        if (ch == ' ') {
            flush();
            out.push_back(ch);
        } else {
            token.push_back(ch);
        }
    }
    flush();
    if (!out.empty()) {
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    }
    return out;
}

std::string Client::body_of(std::string_view key) const {
    return std::string(mDb.text(key));
}

void Client::build_screen() {
    mMenu = MenuScreen{};
    switch (mScreen) {
        case Screen::MainMenu:
            build_main_menu();
            break;
        case Screen::NewGame:
            build_new_game();
            break;
        case Screen::LoadGame:
            build_load_game();
            break;
        case Screen::Hub:
            build_hub();
            break;
        case Screen::Garage:
            build_garage();
            break;
        case Screen::Modules:
            build_modules();
            break;
        case Screen::Skills:
            build_skills();
            break;
        case Screen::Research:
            build_research();
            break;
        case Screen::Refinery:
            build_refinery();
            break;
        case Screen::Contracts:
            build_contracts();
            break;
        case Screen::Facilities:
            build_facilities();
            break;
        case Screen::Codex:
            build_codex();
            break;
        case Screen::Pause:
            build_pause();
            break;
        case Screen::Results:
            build_results();
            break;
        case Screen::Failure:
            build_failure();
            break;
        case Screen::Settings:
            build_settings();
            break;
        case Screen::Bindings:
            build_bindings();
            break;
        case Screen::Credits:
            build_credits();
            break;
        case Screen::Error:
            build_error();
            break;
        default:
            break;
    }
    mSelected = clamp(mSelected, 0, std::max(0, static_cast<int>(mMenu.items.size()) - 1));
    // Never leave the cursor parked on a pure readout. Rows that are actions but
    // currently unavailable stay selectable: the inspector explains why.
    if (!mMenu.items.empty() && mMenu.items[static_cast<usize>(mSelected)].inert) {
        for (usize i = 0; i < mMenu.items.size(); ++i) {
            if (!mMenu.items[i].inert) {
                mSelected = static_cast<int>(i);
                break;
            }
        }
    }
}

void Client::build_main_menu() {
    mMenu.title = "";
    mMenu.subtitle = "";
    mMenu.hint = "Up / Down to move   Enter or A to select";
    mMenu.inspector = false;
    const auto slots = mSession->saves().list_slots(mDb, 1);
    if (slots[0].exists && !slots[0].damaged) {
        MenuItem cont;
        cont.icon = Icon::Descend;
        cont.label = "Continue";
        cont.detail = slots[0].summary;
        cont.accent = theme::kAmber;
        cont.id = kIdContinue;
        mMenu.items.push_back(std::move(cont));
    }
    auto add = [&](Icon icon, const char* label, const char* detail, int id) {
        MenuItem item;
        item.icon = icon;
        item.label = label;
        item.detail = detail;
        item.id = id;
        mMenu.items.push_back(std::move(item));
    };
    add(Icon::Star, "New expedition", "Start a new world from a seed", kIdNewGame);
    add(Icon::Save, "Load slot", "Choose a save slot", kIdLoadGame);
    add(Icon::Settings, "Settings", "Video, audio, accessibility, assists", kIdSettings);
    add(Icon::Controls, "Controls", "Remap keyboard and controller", kIdBindings);
    add(Icon::Codex, "Credits", "", kIdCredits);
    add(Icon::Cross, "Quit", "", kIdQuit);
}

void Client::build_new_game() {
    mMenu.title = "New World";
    mMenu.titleIcon = Icon::Star;
    mMenu.subtitle = "The seed decides the geology. The same seed always builds the same world.";
    mMenu.hint = "Enter to change   Backspace to go back";

    MenuItem seed;
    seed.icon = Icon::Scanner;
    seed.label = "Seed source";
    seed.value = mUseRandomSeed ? "Random" : thousands(static_cast<i64>(mSeedInput));
    seed.detail = mUseRandomSeed ? "A fresh world every time" : "Reproducible: share it with anyone";
    seed.body = "World generation is deterministic. A given seed produces exactly the same "
                "geology, veins and landmarks on any machine.";
    seed.id = kIdToggleRandomSeed;
    mMenu.items.push_back(std::move(seed));

    if (!mUseRandomSeed) {
        MenuItem up;
        up.icon = Icon::Plus;
        up.label = "Seed + 1000";
        up.value = thousands(static_cast<i64>(mSeedInput + 1000));
        up.id = kIdSeedUp;
        mMenu.items.push_back(std::move(up));
        MenuItem down;
        down.icon = Icon::Minus;
        down.label = "Seed - 1000";
        down.value = thousands(static_cast<i64>(mSeedInput > 1000 ? mSeedInput - 1000 : 0));
        down.id = kIdSeedDown;
        mMenu.items.push_back(std::move(down));
    }

    MenuItem begin;
    begin.icon = Icon::Descend;
    begin.label = "Begin";
    begin.detail = "Descend for the first time";
    begin.accent = theme::kAmber;
    begin.body = "You start with a weak drill, a small tank and a hold that fills fast. Everything "
                 "else is earned.";
    begin.facts = {{"Starting credits", thousands(mDb.economy().startingCredits)},
                   {"Assists", "Adjustable in Settings"}};
    begin.id = kIdStartWorld;
    mMenu.items.push_back(std::move(begin));

    MenuItem back;
    back.icon = Icon::Chevron;
    back.label = "Back";
    back.id = kIdBack;
    mMenu.items.push_back(std::move(back));
}

void Client::build_load_game() {
    mMenu.title = "Load";
    mMenu.titleIcon = Icon::Save;
    mMenu.subtitle = "Every slot keeps a rotating backup.";
    mMenu.hint = "Enter to load   Backspace to go back";
    const auto slots = mSession->saves().list_slots(mDb, 4);
    for (const auto& s : slots) {
        MenuItem item;
        item.icon = Icon::Save;
        item.label = s.slot == 0 ? "Autosave" : std::format("Slot {}", s.slot);
        item.enabled = s.exists;
        item.id = kIdDynamicBase + s.slot;
        if (!s.exists) {
            item.detail = "Empty";
            item.body = "Nothing saved in this slot yet.";
        } else if (s.damaged) {
            item.detail = "Damaged";
            item.status = "damaged";
            item.statusColor = theme::kDanger;
            item.accent = theme::kDanger;
            item.note = s.summary;
            item.body = "The main file failed validation. The damaged file has been left untouched "
                        "so nothing is lost.";
            item.facts = {{"Backup", s.backupAvailable ? "Available" : "None"}};
        } else {
            item.detail = s.summary;
            item.value = std::format("{:.0f} min", s.playSeconds / 60.0);
            item.body = "Loading replaces the current session.";
            item.facts = {{"Credits", thousands(s.credits)},
                          {"Deepest", std::format("{} m", s.deepestDepth)},
                          {"Build", s.buildVersion.empty() ? "-" : s.buildVersion},
                          {"Save schema", std::format("v{}", s.schemaVersion)}};
        }
        mMenu.items.push_back(std::move(item));
    }
    MenuItem restore;
    restore.icon = Icon::Warning;
    restore.label = "Restore backup of selected slot";
    restore.detail = "Use if a save is damaged";
    restore.accent = theme::kWarn;
    restore.id = kIdRecoverBackup;
    mMenu.items.push_back(std::move(restore));

    MenuItem back;
    back.icon = Icon::Chevron;
    back.label = "Back";
    back.id = kIdBack;
    mMenu.items.push_back(std::move(back));
}

void Client::build_hub() {
    const auto& p = mSession->progress();
    mMenu.title = "Surface Hub";
    mMenu.titleIcon = Icon::Facility;
    mMenu.subtitle = std::format("Deepest {} m   {} expeditions   {} tiles cut", p.stats.deepestDepth,
                                 p.stats.expeditions, thousands(p.stats.tilesMined));
    mMenu.hint = "Up / Down to move   Enter or A to select   Esc for the main menu";

    MenuItem descend;
    descend.icon = Icon::Descend;
    descend.label = "Descend";
    descend.detail = "Enter the mine through the surface shaft";
    descend.accent = theme::kAmber;
    descend.body = "Cut down, read the rock, and turn for home while the fuel still covers the "
                   "climb.";
    const auto& stats = mSession->sim().stats();
    descend.facts = {{"Fuel", std::format("{:.0f}", stats.fuelCapacity)},
                     {"Hold", std::format("{:.0f}", stats.cargoCapacity)},
                     {"Hull", std::format("{:.0f}", stats.hullMax)},
                     {"Drill", std::format("{:.2f}", stats.drillPower)}};
    descend.id = kIdDescend;
    mMenu.items.push_back(std::move(descend));

    // Activated transit nodes, deepest first: permanent launch points.
    for (usize i = 0; i < p.transitNodes.size(); ++i) {
        const auto& node = p.transitNodes[p.transitNodes.size() - 1 - i];
        const i32 biome = mSession->world().generator().biome_index_at(node.y);
        MenuItem lift;
        lift.icon = Icon::Lift;
        lift.label = std::format("Transit lift to {} m", node.y);
        lift.detail = name_of(mDb.biomes()[static_cast<usize>(biome)].nameKey);
        lift.accent = theme::kCyan;
        lift.body = "Launch from this node and extract through it: climbing back to this depth "
                    "ends the run.";
        lift.id = kIdDynamicBase + 900 + static_cast<int>(p.transitNodes.size() - 1 - i);
        mMenu.items.push_back(std::move(lift));
    }

    struct Dest {
        Icon icon;
        const char* label;
        std::string detail;
        std::string body;
        int id;
    };

    const Dest destinations[] = {
        {Icon::Garage, "Garage", "Rig equipment tiers",
         "Nine permanent upgrade tracks. Milestone tiers grant a new capability, not just a bigger "
         "number.",
         kIdGarage},
        {Icon::Module, "Modules",
         std::format("{} / {} power used", p.used_utility_power(mDb), max_utility_power(mDb, p)),
         "Swappable buildcraft on a shared power budget. Anything strong carries a tradeoff.", kIdModules},
        {Icon::Skill, "Proficiency", std::format("{} unspent", p.skill_points_available()),
         "Three branches ending in mutually exclusive keystones. Respec is always available.", kIdSkills},
        {Icon::Research, "Research lab", std::format("{} insight", p.insight),
         "Converts samples and insight into new mechanics rather than into another shop.", kIdResearch},
        {Icon::Refinery, "Refinery", "Convert raw ore into components",
         "Strategic ore becomes the components that advanced equipment and facilities demand.", kIdRefinery},
        {Icon::Contract, "Contract terminal", std::format("{} active", p.contracts.size()),
         "Alternate objectives for a run. The board never offers something you cannot obtain.", kIdContracts},
        {Icon::Facility, "Facilities", "Upgrade the base",
         "Facilities change what actions are available. No timers, nothing to babysit.", kIdFacilities},
        {Icon::Codex, "Codex", std::format("{} materials logged", p.codex.size()),
         "Materials, landmarks, records and every tutorial card you have seen.", kIdCodex},
    };
    for (const auto& dest : destinations) {
        MenuItem item;
        item.icon = dest.icon;
        item.label = dest.label;
        item.detail = dest.detail;
        item.body = dest.body;
        item.id = dest.id;
        mMenu.items.push_back(std::move(item));
    }

    MenuItem save;
    save.icon = Icon::Save;
    save.label = "Save";
    save.detail = "Write to the autosave slot";
    save.id = kIdSaveGame;
    mMenu.items.push_back(std::move(save));

    MenuItem menu;
    menu.icon = Icon::Chevron;
    menu.label = "Main menu";
    menu.detail = "Saves first";
    menu.id = kIdReturnToMenu;
    mMenu.items.push_back(std::move(menu));
}

void Client::build_garage() {
    const auto& p = mSession->progress();
    mMenu.title = "Garage";
    mMenu.titleIcon = Icon::Garage;
    mMenu.subtitle = "Permanent rig upgrades. Highlight a track to see exactly what changes.";
    mMenu.hint = "Enter to buy   Backspace to go back";

    const StatBlock before = compute_stats(mDb, p);
    for (usize i = 0; i < mDb.equipment().size(); ++i) {
        const auto& track = mDb.equipment()[i];
        const i32 tier = p.equipmentTier[i];
        const bool maxed = static_cast<usize>(tier + 1) >= track.tiers.size();
        std::string reason;
        const bool can = mSession->economy().can_buy_equipment(static_cast<i32>(i), &reason);

        MenuItem item;
        item.icon = track_icon(track.track);
        item.label = name_of(track.nameKey);
        item.id = kIdDynamicBase + static_cast<int>(i);
        item.enabled = !maxed && can;
        item.status = std::format("T{}", tier);
        item.statusColor = tier > 0 ? theme::kCyan : theme::kTextFaint;

        if (maxed) {
            item.detail = "Maximum tier reached";
            item.marked = true;
            item.body = body_of(track.descriptionKey);
            item.facts.emplace_back("Fitted", name_of(track.tiers.back().nameKey));
        } else {
            const auto& next = track.tiers[static_cast<usize>(tier + 1)];
            const StatBlock after = preview_with_equipment(mDb, p, static_cast<i32>(i), tier + 1);
            item.value = std::format("{} cr", thousands(next.costCredits));
            item.valueColor = p.credits >= next.costCredits ? theme::kText : theme::kDanger;
            item.detail = std::format("Tier {} to {}", tier, tier + 1);
            item.body = body_of(track.descriptionKey);
            item.facts.emplace_back(name_of(next.nameKey), body_of(next.summaryKey));

            auto add = [&](const char* label, float b, float a, bool lower = false, int dec = 1) {
                if (std::fabs(a - b) > 0.0005f) {
                    item.deltas.push_back(StatDelta{label, b, a, lower, dec});
                }
            };
            add("Fuel capacity", before.fuelCapacity, after.fuelCapacity, false, 0);
            add("Cargo capacity", before.cargoCapacity, after.cargoCapacity, false, 0);
            add("Hull", before.hullMax, after.hullMax, false, 0);
            add("Drill power", before.drillPower, after.drillPower, false, 2);
            add("Drill fuel use", before.drillEfficiency, after.drillEfficiency, true, 2);
            add("Thrust", before.thrust, after.thrust, false, 2);
            add("Cooling", before.cooling, after.cooling, false, 2);
            add("Scanner range", before.scannerRange, after.scannerRange, false, 0);
            add("Impact damage", before.impactDamage, after.impactDamage, true, 2);

            if (after.flags.bits() != before.flags.bits()) {
                for (u32 f = 0; f < static_cast<u32>(RigFlag::Count); ++f) {
                    const RigFlag flag = static_cast<RigFlag>(f);
                    if (after.flags.has(flag) && !before.flags.has(flag)) {
                        item.facts.emplace_back("New capability",
                                                pretty_key(std::string(rig_flag_name(flag))));
                    }
                }
                item.statusColor = theme::kAmber;
            }
            if (!next.costComponents.empty()) {
                std::string parts;
                for (const auto& [mid, amount] : next.costComponents) {
                    if (!parts.empty()) {
                        parts += ", ";
                    }
                    const auto h = mDb.material(mid.text());
                    parts += std::format("{} x{} ({})", id_name(mid.text()), amount,
                                         h.valid() ? p.vault.count(h.index) : 0);
                }
                item.facts.emplace_back("Components", parts);
            }
            if (!can) {
                item.note = humanise(reason);
            }
        }
        mMenu.items.push_back(std::move(item));
    }

    MenuItem back;
    back.icon = Icon::Chevron;
    back.label = "Back";
    back.id = kIdBack;
    mMenu.items.push_back(std::move(back));
}

void Client::build_modules() {
    const auto& p = mSession->progress();
    mMenu.title = "Modules";
    mMenu.titleIcon = Icon::Module;
    mMenu.subtitle = std::format("Utility power {} of {} used   {} cr", p.used_utility_power(mDb),
                                 max_utility_power(mDb, p), thousands(p.credits));
    mMenu.hint = "Enter to buy, install or remove   Backspace to go back";

    const StatBlock before = compute_stats(mDb, p);
    for (usize i = 0; i < mDb.modules().size(); ++i) {
        const auto& m = mDb.modules()[i];
        const bool owned = p.ownedModules.count(static_cast<i32>(i)) > 0;
        const bool installed = p.module_installed(static_cast<i32>(i));
        const bool unlocked = mSession->economy().module_unlocked(static_cast<i32>(i));

        MenuItem item;
        item.icon = slot_icon(m.slot);
        item.label = name_of(m.nameKey);
        item.id = kIdDynamicBase + static_cast<int>(i);
        item.enabled = unlocked;
        item.locked = !unlocked;
        item.marked = installed;
        item.body = body_of(m.descriptionKey);
        item.detail =
            std::format("{} slot   {} power", pretty_key(std::string(module_slot_name(m.slot))), m.powerCost);

        if (!unlocked) {
            item.status = "locked";
            item.statusColor = theme::kTextDisabled;
            item.note = m.unlockResearch.empty()
                            ? "Requires faction reputation"
                            : std::format("Requires research: {}", id_name(m.unlockResearch.text()));
        } else if (installed) {
            item.status = "installed";
            item.statusColor = theme::kGood;
            item.accent = theme::kGood;
            item.value = "remove";
        } else if (owned) {
            item.status = "owned";
            item.statusColor = theme::kCyan;
            item.value = "install";
        } else {
            item.value = std::format("{} cr", thousands(m.costCredits));
        }

        const StatBlock after = preview_with_module(mDb, p, static_cast<i32>(i), !installed);
        auto add = [&](const char* label, float b, float a, bool lower = false, int dec = 2) {
            if (std::fabs(a - b) > 0.0005f) {
                item.deltas.push_back(StatDelta{label, b, a, lower, dec});
            }
        };
        add("Drill power", before.drillPower, after.drillPower);
        add("Thrust", before.thrust, after.thrust);
        add("Cooling", before.cooling, after.cooling);
        add("Heat generated", before.heatGeneration, after.heatGeneration, true);
        add("Hull", before.hullMax, after.hullMax, false, 0);
        add("Cargo capacity", before.cargoCapacity, after.cargoCapacity, false, 0);
        add("Scanner range", before.scannerRange, after.scannerRange, false, 0);
        add("Yield", before.yieldMul, after.yieldMul);
        add("Hazard damage", before.hazardDamage, after.hazardDamage, true);
        add("Sale value", before.saleValue, after.saleValue);

        if (!m.tags.empty()) {
            std::string tags;
            for (const auto& t : m.tags) {
                if (!tags.empty()) {
                    tags += ", ";
                }
                tags += t.text();
            }
            item.facts.emplace_back("Tags", tags);
        }
        mMenu.items.push_back(std::move(item));
    }

    MenuItem back;
    back.icon = Icon::Chevron;
    back.label = "Back";
    back.id = kIdBack;
    mMenu.items.push_back(std::move(back));
}

void Client::build_skills() {
    const auto& p = mSession->progress();
    mMenu.title = "Proficiency";
    mMenu.titleIcon = Icon::Skill;
    mMenu.subtitle = std::format("{} points available   respec costs {} cr", p.skill_points_available(),
                                 thousands(mSession->economy().respec_cost()));
    mMenu.hint = "Enter to unlock   Backspace to go back";

    const StatBlock before = compute_stats(mDb, p);
    for (usize i = 0; i < mDb.skills().size(); ++i) {
        const auto& s = mDb.skills()[i];
        const bool taken = p.unlockedSkills.count(static_cast<i32>(i)) > 0;

        MenuItem item;
        item.icon = Icon::Skill;
        item.label = name_of(s.nameKey);
        item.id = kIdDynamicBase + static_cast<int>(i);
        item.enabled = !taken;
        item.marked = taken;
        item.value = std::format("{} pt", s.cost);
        // The three trees are interleaved in one list, so every row names its own
        // branch: "Row 2" alone is ambiguous the moment you are past the first tree.
        item.detail = std::format("{}   row {}", pretty_key(std::string(skill_branch_name(s.branch))), s.row);
        if (s.keystone) {
            item.status = "keystone";
            item.statusColor = theme::kAmber;
            item.accent = theme::kAmber;
            item.facts.emplace_back("Keystone", "Only one keystone per branch; respec to change your mind");
        }
        if (taken) {
            item.status = "taken";
            item.statusColor = theme::kGood;
        }

        item.body = body_of(s.descriptionKey);
        const StatBlock after = preview_with_skill(mDb, p, static_cast<i32>(i));
        auto add = [&](const char* label, float b, float a, bool lower = false, int dec = 2) {
            if (std::fabs(a - b) > 0.0005f) {
                item.deltas.push_back(StatDelta{label, b, a, lower, dec});
            }
        };
        add("Drill power", before.drillPower, after.drillPower);
        add("Yield", before.yieldMul, after.yieldMul);
        add("Sale value", before.saleValue, after.saleValue);
        add("Scanner range", before.scannerRange, after.scannerRange, false, 0);
        add("Cooling", before.cooling, after.cooling);
        add("Thrust", before.thrust, after.thrust);
        add("Fuel capacity", before.fuelCapacity, after.fuelCapacity, false, 0);
        add("Hazard damage", before.hazardDamage, after.hazardDamage, true);
        add("Rescue fee", before.rescueFee, after.rescueFee, true);

        if (!s.prerequisites.empty()) {
            std::string prereq;
            for (const auto& pr : s.prerequisites) {
                if (!prereq.empty()) {
                    prereq += ", ";
                }
                prereq += id_name(pr.text());
            }
            item.facts.emplace_back("Requires", prereq);
        }
        if (!taken && p.skill_points_available() < s.cost) {
            item.note = "Not enough proficiency points";
            item.enabled = false;
        }
        mMenu.items.push_back(std::move(item));
    }

    MenuItem respec;
    respec.icon = Icon::Cross;
    respec.label = "Respec";
    respec.detail = "Refund every point for a fee";
    respec.value = std::format("{} cr", thousands(mSession->economy().respec_cost()));
    respec.accent = theme::kWarn;
    respec.id = kIdRespec;
    mMenu.items.push_back(std::move(respec));

    MenuItem back;
    back.icon = Icon::Chevron;
    back.label = "Back";
    back.id = kIdBack;
    mMenu.items.push_back(std::move(back));
}

void Client::build_research() {
    const auto& p = mSession->progress();
    mMenu.title = "Research Lab";
    mMenu.titleIcon = Icon::Research;
    mMenu.subtitle =
        std::format("{} insight   {} sample types in storage", p.insight, p.samples.stacks().size());
    mMenu.hint = "Enter to research   Backspace to go back";

    for (usize i = 0; i < mDb.research().size(); ++i) {
        const auto& r = mDb.research()[i];
        const bool done = p.has_research(static_cast<i32>(i));
        std::string reason;
        const bool ok = mSession->economy().research_available(static_cast<i32>(i), &reason);

        MenuItem item;
        item.icon = r.capstone ? Icon::Star : Icon::Research;
        item.label = name_of(r.nameKey);
        item.id = kIdDynamicBase + static_cast<int>(i);
        item.enabled = ok;
        item.marked = done;
        item.value = done ? "" : std::format("{} insight", r.insightCost);
        item.valueColor = p.insight >= r.insightCost ? theme::kText : theme::kDanger;
        if (done) {
            item.status = "complete";
            item.statusColor = theme::kGood;
        } else if (r.capstone) {
            item.status = "capstone";
            item.statusColor = theme::kAmber;
            item.accent = theme::kAmber;
        }
        item.body = body_of(r.descriptionKey);

        if (!r.requiresSample.empty()) {
            const auto h = mDb.material(r.requiresSample.text());
            item.detail = std::format("Sample: {}", id_name(r.requiresSample.text()));
            item.facts.emplace_back(
                "Sample held",
                std::format("{} / {}", h.valid() ? p.samples.count(h.index) : 0, r.requiresSampleAmount));
        }
        if (!r.unlocksModules.empty()) {
            std::string mods;
            for (const auto& mid : r.unlocksModules) {
                if (!mods.empty()) {
                    mods += ", ";
                }
                mods += id_name(mid.text());
            }
            item.facts.emplace_back("Unlocks modules", mods);
        }
        if (!r.unlocksBiomes.empty()) {
            std::string biomes;
            for (const auto& bid : r.unlocksBiomes) {
                if (!biomes.empty()) {
                    biomes += ", ";
                }
                biomes += id_name(bid.text());
            }
            item.facts.emplace_back("Opens biome", biomes);
        }
        if (!r.unlocksRecipes.empty()) {
            std::string recipes;
            for (const auto& rid : r.unlocksRecipes) {
                if (!recipes.empty()) {
                    recipes += ", ";
                }
                recipes += id_name(rid.text());
            }
            item.facts.emplace_back("Unlocks recipes", recipes);
        }
        if (!done && !ok) {
            item.note = humanise(reason);
        }
        mMenu.items.push_back(std::move(item));
    }

    MenuItem back;
    back.icon = Icon::Chevron;
    back.label = "Back";
    back.id = kIdBack;
    mMenu.items.push_back(std::move(back));
}

void Client::build_refinery() {
    const auto& p = mSession->progress();
    mMenu.title = "Refinery";
    mMenu.titleIcon = Icon::Refinery;
    mMenu.subtitle = "Recipes draw from the surface vault.";
    mMenu.hint = "Enter to run one batch   Backspace to go back";

    for (usize i = 0; i < mDb.recipes().size(); ++i) {
        const auto& r = mDb.recipes()[i];
        std::string reason;
        const bool ok = mSession->economy().recipe_available(static_cast<i32>(i), &reason);

        MenuItem item;
        item.icon = Icon::Refinery;
        item.label = name_of(r.nameKey);
        item.id = kIdDynamicBase + static_cast<int>(i);
        item.enabled = ok;
        item.value = r.creditCost > 0 ? std::format("{} cr", r.creditCost) : "";

        std::string inputs;
        bool haveAll = true;
        for (const auto& in : r.inputs) {
            const auto h = mDb.material(in.materialId.text());
            const i32 held = h.valid() ? p.vault.count(h.index) : 0;
            if (held < in.amount) {
                haveAll = false;
            }
            if (!inputs.empty()) {
                inputs += " + ";
            }
            inputs += std::format("{} x{}", id_name(in.materialId.text()), in.amount);
            item.facts.emplace_back(id_name(in.materialId.text()), std::format("{} / {}", held, in.amount));
        }
        std::string outputs;
        for (const auto& out : r.outputs) {
            if (!outputs.empty()) {
                outputs += " + ";
            }
            outputs += std::format("{} x{}", id_name(out.materialId.text()), out.amount);
        }
        // Inputs alone on the row: the full recipe never fits on one line, and
        // the inspector below already lists what each batch produces.
        item.detail = inputs;
        item.facts.emplace_back("Produces", outputs);
        item.body = body_of(r.descriptionKey);
        if (!ok) {
            item.note = humanise(reason);
        } else if (!haveAll) {
            item.note = "Not enough input material in the vault";
            item.enabled = false;
        }
        mMenu.items.push_back(std::move(item));
    }

    MenuItem back;
    back.icon = Icon::Chevron;
    back.label = "Back";
    back.id = kIdBack;
    mMenu.items.push_back(std::move(back));
}

void Client::build_contracts() {
    const auto& p = mSession->progress();
    mMenu.title = "Contract Terminal";
    mMenu.titleIcon = Icon::Contract;
    mMenu.subtitle = std::format("{} of 4 accepted", p.contracts.size());
    mMenu.hint = "Enter to accept, or to abandon an active contract";

    for (usize i = 0; i < p.contracts.size(); ++i) {
        const auto& c = p.contracts[i];
        const auto& t = mDb.contracts()[static_cast<usize>(c.templateIndex)];
        MenuItem item;
        item.icon = Icon::Contract;
        item.label = name_of(t.nameKey);
        item.id = kIdDynamicBase + 500 + static_cast<int>(i);
        item.accent = theme::kGood;
        item.status = c.completed ? "complete" : "active";
        item.statusColor = c.completed ? theme::kGood : theme::kCyan;
        item.value = "abandon";
        if (c.targetMaterial >= 0) {
            item.detail = std::format("Deliver {} x {}", c.targetAmount,
                                      name_of(mDb.materials()[static_cast<usize>(c.targetMaterial)].nameKey));
            item.progress = c.targetAmount > 0
                                ? static_cast<float>(c.progress) / static_cast<float>(c.targetAmount)
                                : 0.0f;
            item.facts.emplace_back("Progress", std::format("{} / {}", c.progress, c.targetAmount));
        } else if (c.targetDepth > 0) {
            item.detail = std::format("Reach {} m", c.targetDepth);
            item.progress = clamp(static_cast<float>(p.stats.deepestDepth) /
                                      static_cast<float>(std::max(1, c.targetDepth)),
                                  0.0f, 1.0f);
        } else {
            item.detail = pretty_key(std::string(contract_kind_name(t.kind)));
        }
        item.facts.emplace_back("Reward", std::format("{} cr", thousands(t.rewardCredits)));
        if (t.rewardInsight > 0) {
            item.facts.emplace_back("Insight", std::to_string(t.rewardInsight));
        }
        item.body = body_of(mDb.contracts()[static_cast<usize>(c.templateIndex)].descriptionKey);
        mMenu.items.push_back(std::move(item));
    }

    const auto& offers = mSession->economy().offers();
    for (usize i = 0; i < offers.size(); ++i) {
        const auto& c = offers[i];
        const auto& t = mDb.contracts()[static_cast<usize>(c.templateIndex)];
        MenuItem item;
        item.icon = t.authored ? Icon::Star : Icon::Contract;
        item.label = name_of(t.nameKey);
        item.id = kIdDynamicBase + static_cast<int>(i);
        item.enabled = p.contracts.size() < 4;
        if (t.authored) {
            item.status = "story";
            item.statusColor = theme::kAmber;
            item.accent = theme::kAmber;
        }
        if (c.targetMaterial >= 0) {
            item.detail = std::format("Deliver {} x {}", c.targetAmount,
                                      name_of(mDb.materials()[static_cast<usize>(c.targetMaterial)].nameKey));
        } else if (c.targetDepth > 0) {
            item.detail = std::format("Reach {} m", c.targetDepth);
        } else {
            item.detail = pretty_key(std::string(contract_kind_name(t.kind)));
        }
        i64 credits = t.rewardCredits;
        if (t.rewardCreditsPerUnit > 0 && c.targetAmount > 0) {
            credits += static_cast<i64>(t.rewardCreditsPerUnit) * c.targetAmount;
        }
        item.value = std::format("{} cr", thousands(credits));
        item.facts.emplace_back("Credits", thousands(credits));
        if (t.rewardInsight > 0) {
            item.facts.emplace_back("Insight", std::to_string(t.rewardInsight));
        }
        if (t.rewardReputation > 0 && !t.factionId.empty()) {
            item.facts.emplace_back(id_name(t.factionId.text()), std::format("+{} rep", t.rewardReputation));
        }
        item.body = body_of(t.descriptionKey);
        if (!item.enabled) {
            item.note = "Contract board is full";
        }
        mMenu.items.push_back(std::move(item));
    }

    MenuItem back;
    back.icon = Icon::Chevron;
    back.label = "Back";
    back.id = kIdBack;
    mMenu.items.push_back(std::move(back));
}

void Client::build_facilities() {
    const auto& p = mSession->progress();
    mMenu.title = "Base Facilities";
    mMenu.titleIcon = Icon::Facility;
    mMenu.subtitle = "Facilities change what the hub can do.";
    mMenu.hint = "Enter to upgrade   Backspace to go back";

    for (usize i = 0; i < mDb.facilities().size(); ++i) {
        const auto& f = mDb.facilities()[i];
        const i32 tier = p.facilityTier[i];
        const bool maxed = static_cast<usize>(tier + 1) >= f.tiers.size();

        MenuItem item;
        item.icon = Icon::Facility;
        item.label = name_of(f.nameKey);
        item.id = kIdDynamicBase + static_cast<int>(i);
        item.enabled = !maxed;
        item.marked = maxed;
        item.status = std::format("T{}", tier);
        item.statusColor = tier > 0 ? theme::kCyan : theme::kTextFaint;
        if (maxed) {
            item.detail = "Maximum tier reached";
        } else {
            const auto& next = f.tiers[static_cast<usize>(tier + 1)];
            item.detail = std::format("Tier {} to {}", tier, tier + 1);
            item.value = std::format("{} cr", thousands(next.costCredits));
            item.valueColor = p.credits >= next.costCredits ? theme::kText : theme::kDanger;
            if (!next.costComponents.empty()) {
                std::string parts;
                for (const auto& ing : next.costComponents) {
                    if (!parts.empty()) {
                        parts += ", ";
                    }
                    const auto h = mDb.material(ing.materialId.text());
                    parts += std::format("{} x{} ({})", id_name(ing.materialId.text()), ing.amount,
                                         h.valid() ? p.vault.count(h.index) : 0);
                }
                item.facts.emplace_back("Components", parts);
            }
            if (!next.unlocksRecipes.empty()) {
                std::string recipes;
                for (const auto& rid : next.unlocksRecipes) {
                    if (!recipes.empty()) {
                        recipes += ", ";
                    }
                    recipes += id_name(rid.text());
                }
                item.facts.emplace_back("Unlocks", recipes);
            }
            if (p.credits < next.costCredits) {
                item.note = "Not enough credits";
                item.enabled = false;
            }
        }
        mMenu.items.push_back(std::move(item));
    }

    MenuItem back;
    back.icon = Icon::Chevron;
    back.label = "Back";
    back.id = kIdBack;
    mMenu.items.push_back(std::move(back));
}

void Client::build_codex() {
    const auto& p = mSession->progress();
    mMenu.title = "Codex";
    mMenu.titleIcon = Icon::Codex;
    mMenu.subtitle = std::format("{} materials   {} landmarks   {} expeditions   {} rescues", p.codex.size(),
                                 p.discoveredLandmarks.size(), p.stats.expeditions, p.stats.rescues);
    mMenu.hint = "Backspace to go back";

    std::vector<std::pair<i32, CodexEntry>> entries(p.codex.begin(), p.codex.end());
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.second.firstDepth < b.second.firstDepth; });
    for (const auto& [mi, entry] : entries) {
        const auto& m = mDb.materials()[static_cast<usize>(mi)];
        MenuItem item;
        item.icon = material_icon(m.category);
        item.label = name_of(m.nameKey);
        item.detail = std::format("First seen at {} m   {} category", entry.firstDepth,
                                  pretty_key(std::string(material_category_name(m.category))));
        item.value = std::format("{}", entry.minedCount);
        item.enabled = false;
        item.inert = true;
        item.accent = Color{m.colorR, m.colorG, m.colorB};
        item.facts = {{"Value", std::format("{} cr", m.baseValue)},
                      {"Hardness", std::format("{:.1f}", m.hardness)},
                      {"Cargo units", std::to_string(m.cargoUnits)},
                      {"Mined", thousands(entry.minedCount)}};
        item.body = body_of(m.descriptionKey);
        if (m.protectedSample) {
            item.facts.emplace_back("Storage",
                                    "Protected sample space - a failed run never costs a discovery");
        }
        mMenu.items.push_back(std::move(item));
    }
    for (const auto& t : mTutorials) {
        if (!t.shown) {
            continue;
        }
        MenuItem item;
        item.icon = t.icon;
        item.label = t.title;
        item.detail = "Tutorial";
        item.body = t.body;
        item.accent = theme::kCyan;
        item.enabled = false;
        item.inert = true;
        mMenu.items.push_back(std::move(item));
    }

    if (mMenu.items.empty()) {
        mMenu.emptyText = "Nothing logged yet. Every material you cut and every landmark you "
                          "reach is recorded here, with what it is worth and where it was found.";
    }

    MenuItem back;
    back.icon = Icon::Chevron;
    back.label = "Back";
    back.enabled = true;
    back.id = kIdBack;
    mMenu.items.push_back(std::move(back));
}

void Client::build_pause() {
    mMenu.title = "Paused";
    mMenu.titleIcon = Icon::Settings;
    mMenu.subtitle = std::format("Depth {} m   {} tiles cut this run", mSession->rig().deepestThisRun,
                                 mSession->sim().runtime().tilesMined);
    mMenu.hint = "Esc to resume";
    mMenu.inspector = false;

    auto add = [&](Icon icon, const char* label, const char* detail, int id, Color accent = theme::kText) {
        MenuItem item;
        item.icon = icon;
        item.label = label;
        item.detail = detail;
        item.accent = accent;
        item.id = id;
        mMenu.items.push_back(std::move(item));
    };
    add(Icon::Descend, "Resume", "", kIdResume, theme::kAmber);
    add(Icon::Map, "Map", "Review what you have surveyed", kIdOpenMap);
    add(Icon::Settings, "Settings", "", kIdSettings);
    add(Icon::Controls, "Controls", "", kIdBindings);
    add(Icon::Warning, "Abandon run", "Call for extraction; unsold cargo is at risk", kIdAbandonRun,
        theme::kWarn);
    add(Icon::Save, "Save and quit to menu", "", kIdReturnToMenu);
}

void Client::build_results() {
    const auto& s = mSession->last_summary();
    mMenu.title = s.rescued ? "Extracted" : "Expedition Complete";
    mMenu.titleIcon = s.rescued ? Icon::Warning : Icon::Check;
    mMenu.subtitle = std::format("{} m reached   {:.1f} min   {} tiles cut", s.depthReached,
                                 s.seconds / 60.0f, s.tilesMined);
    mMenu.hint = "Enter to continue";
    mMenu.inspector = false;

    for (const auto& line : s.sold) {
        const auto& m = mDb.materials()[static_cast<usize>(line.material)];
        MenuItem item;
        item.icon = material_icon(m.category);
        item.label = name_of(m.nameKey);
        item.detail = std::format("{} units", line.amount);
        item.value = std::format("+{} cr", thousands(line.credits));
        item.accent = Color{m.colorR, m.colorG, m.colorB};
        item.enabled = false;
        item.inert = true;
        mMenu.items.push_back(std::move(item));
    }
    if (s.sold.empty()) {
        MenuItem item;
        item.icon = Icon::Cargo;
        item.label = "Nothing sold";
        item.detail = "The hold came back empty";
        item.enabled = false;
        item.inert = true;
        mMenu.items.push_back(std::move(item));
    }
    for (const i32 t : s.contractsCompleted) {
        MenuItem item;
        item.icon = Icon::Check;
        item.label = name_of(mDb.contracts()[static_cast<usize>(t)].nameKey);
        item.detail = "Contract complete";
        item.accent = theme::kGood;
        item.enabled = false;
        item.inert = true;
        mMenu.items.push_back(std::move(item));
    }

    MenuItem total;
    total.icon = Icon::Credit;
    total.label = "Total earned";
    total.value = std::format("{} cr", thousands(s.creditsEarned));
    total.accent = theme::kAmber;
    total.enabled = false;
    total.inert = true;
    mMenu.items.push_back(std::move(total));

    MenuItem ok;
    ok.icon = Icon::Chevron;
    ok.label = "Continue";
    ok.accent = theme::kAmber;
    ok.id = kIdResultsOk;
    mMenu.items.push_back(std::move(ok));
}

void Client::build_failure() {
    const auto& r = mSession->pending_rescue();
    mMenu.title = "Rescue Beacon";
    mMenu.titleIcon = Icon::Warning;
    mMenu.subtitle = r.freeRescue ? "Your first extraction is covered by the cooperative."
                                  : "The fee is capped, so a failure can never bury you.";
    mMenu.hint = "Enter to accept extraction";
    mMenu.inspector = false;

    MenuItem fee;
    fee.icon = Icon::Credit;
    fee.label = "Extraction fee";
    fee.value = std::format("{} cr", thousands(r.fee));
    fee.detail = r.freeRescue ? "Waived" : "Capped at 35% of your credits";
    fee.accent = r.fee > 0 ? theme::kWarn : theme::kGood;
    fee.enabled = false;
    fee.inert = true;
    mMenu.items.push_back(std::move(fee));

    MenuItem cargo;
    cargo.icon = Icon::Cargo;
    cargo.label = "Cargo at risk";
    cargo.value = std::format("{} units", r.cargoUnitsLost);
    cargo.detail = std::format("About {} cr of unsold ore", thousands(r.cargoValueLost));
    cargo.accent = theme::kWarn;
    cargo.enabled = false;
    cargo.inert = true;
    mMenu.items.push_back(std::move(cargo));

    MenuItem kept;
    kept.icon = Icon::Check;
    kept.label = "Kept in full";
    kept.detail = "Equipment, modules, research, proficiency, samples, base upgrades";
    kept.accent = theme::kGood;
    kept.enabled = false;
    kept.inert = true;
    mMenu.items.push_back(std::move(kept));

    MenuItem accept;
    accept.icon = Icon::Lift;
    accept.label = "Accept extraction";
    accept.accent = theme::kAmber;
    accept.id = kIdConfirmRescue;
    mMenu.items.push_back(std::move(accept));
}

void Client::build_settings() {
    mMenu.title = "Settings";
    mMenu.titleIcon = Icon::Settings;
    mMenu.subtitle = "Left and right adjust. Everything applies immediately.";
    mMenu.hint = "Left / Right to adjust   Enter to toggle   Backspace to go back";
    mMenu.inspector = false;

    auto toggle = [&](Icon icon, const char* label, bool on, const char* detail, int id) {
        MenuItem item;
        item.icon = icon;
        item.label = label;
        item.value = on ? "on" : "off";
        item.detail = detail;
        item.statusColor = on ? theme::kGood : theme::kTextFaint;
        item.accent = on ? theme::kText : theme::kTextMuted;
        item.id = id;
        mMenu.items.push_back(std::move(item));
    };
    auto slider = [&](Icon icon, const char* label, float value, const char* detail, int id) {
        MenuItem item;
        item.icon = icon;
        item.label = label;
        item.value = std::format("{:.0f}%", value * 100.0f);
        item.detail = detail;
        item.slider = slider_fraction(id); ///< Normalised against this id's own range.
        item.id = id;
        mMenu.items.push_back(std::move(item));
    };

    toggle(Icon::Map, "Fullscreen", mSettings.video.fullscreen, "", kIdToggleFullscreen);
    toggle(Icon::Settings, "VSync", mSettings.video.vsync, "", kIdToggleVsync);
    slider(Icon::Settings, "Master volume", mSettings.audio.master, "", kIdVolume);
    toggle(Icon::Codex, "Subtitles and captions", mSettings.audio.subtitles,
           "Meaningful audio cues also appear as text", kIdSubtitlesToggle);
    slider(Icon::Warning, "Screen shake", mSettings.access.screenShake, "Zero disables it entirely",
           kIdShake);
    toggle(Icon::Star, "Flashes", mSettings.access.flashIntensity > 0.0f, "", kIdFlashToggle);
    slider(Icon::Star, "Particle density", mSettings.access.particleDensity, "", kIdParticles);
    toggle(Icon::Scanner, "Colour-vision palette", mSettings.access.colorBlindPalette,
           "Separates materials by blue, yellow and white", kIdPaletteToggle);
    toggle(Icon::Descend, "Reduce camera motion", mSettings.access.reduceCameraMotion,
           "Disables look-ahead and eases", kIdReduceMotionToggle);
    slider(Icon::Codex, "Text size", mSettings.access.textScale, "", kIdTextScale);
    toggle(Icon::Drill, "Hold to drill", mSettings.access.holdToMine, "Off makes drilling a toggle",
           kIdHoldMineToggle);
    toggle(Icon::Fuel, "Hold to thrust", mSettings.access.holdToThrust, "Off makes thrust a toggle",
           kIdHoldThrustToggle);
    slider(Icon::Fuel, "Assist: fuel drain", mSettings.assists.fuelDrainScale, "", kIdAssistFuel);
    slider(Icon::Hull, "Assist: damage taken", mSettings.assists.damageScale, "", kIdAssistDamage);
    slider(Icon::Heat, "Assist: heat build-up", mSettings.assists.heatScale, "", kIdAssistHeat);
    slider(Icon::Drill, "Assist: drilling time", mSettings.assists.mineTimeScale, "", kIdAssistMine);

    MenuItem back;
    back.icon = Icon::Chevron;
    back.label = "Back";
    back.id = kIdBack;
    mMenu.items.push_back(std::move(back));
}

void Client::build_bindings() {
    mMenu.title = "Controls";
    mMenu.titleIcon = Icon::Controls;
    mMenu.subtitle = mAwaitingRebind ? "Press any key or gamepad button..."
                                     : "Select an action, then press the new input.";
    mMenu.hint = "Enter to rebind   Backspace to go back";
    mMenu.inspector = false;

    for (int i = 0; i < static_cast<int>(Action::Count); ++i) {
        const Action a = static_cast<Action>(i);
        MenuItem item;
        item.icon = Icon::Controls;
        item.label = std::string(action_label(a));
        item.value = mSettings.describe_binding(a);
        item.id = kIdDynamicBase + i;
        if (mAwaitingRebind && mRebindAction == a) {
            item.status = "listening";
            item.statusColor = theme::kAmber;
            item.accent = theme::kAmber;
        }
        mMenu.items.push_back(std::move(item));
    }

    MenuItem reset;
    reset.icon = Icon::Cross;
    reset.label = "Reset to defaults";
    reset.accent = theme::kWarn;
    reset.id = kIdResetBindings;
    mMenu.items.push_back(std::move(reset));

    MenuItem back;
    back.icon = Icon::Chevron;
    back.label = "Back";
    back.id = kIdBack;
    mMenu.items.push_back(std::move(back));
}

void Client::build_credits() {
    mMenu.title = "Credits";
    mMenu.titleIcon = Icon::Codex;
    mMenu.subtitle = std::format("Project Deepforge {}  -  built {}", kBuildVersion, kBuildDate);
    mMenu.hint = "Backspace to go back";
    mMenu.inspector = false;

    auto line = [&](Icon icon, const char* label, const char* detail) {
        MenuItem item;
        item.icon = icon;
        item.label = label;
        item.detail = detail;
        item.enabled = false;
        item.inert = true;
        mMenu.items.push_back(std::move(item));
    };
    line(Icon::Star, "Design, code and content", "Original work for this project");
    line(Icon::Settings, "SDL3", "Window, input, gamepads, audio - zlib licence");
    line(Icon::Codex, "nlohmann/json", "Content and save data - MIT licence");
    line(Icon::Check, "Catch2", "Tests, build-time only - BSL-1.0");
    line(Icon::Module, "Art and audio", "Drawn and synthesised at runtime; no asset files");

    MenuItem back;
    back.icon = Icon::Chevron;
    back.label = "Back";
    back.enabled = true;
    back.id = kIdBack;
    mMenu.items.push_back(std::move(back));
}

void Client::build_error() {
    mMenu.title = "Problem";
    mMenu.titleIcon = Icon::Warning;
    mMenu.subtitle = mErrorText;
    mMenu.hint = "";
    mMenu.inspector = false;

    MenuItem backup;
    backup.icon = Icon::Save;
    backup.label = "Try the backup save";
    backup.accent = theme::kAmber;
    backup.id = kIdRecoverBackup;
    mMenu.items.push_back(std::move(backup));

    MenuItem menu;
    menu.icon = Icon::Chevron;
    menu.label = "Back to menu";
    menu.id = kIdReturnToMenu;
    mMenu.items.push_back(std::move(menu));
}

} // namespace df

namespace df {

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

float Client::slider_fraction(int id) const {
    const SliderSpec* spec = slider_spec(id);
    if (spec == nullptr) {
        return 0.0f;
    }
    return clamp((slider_value(id) - spec->min) / std::max(0.0001f, spec->max - spec->min), 0.0f, 1.0f);
}

float Client::slider_value(int id) const {
    switch (id) {
        case kIdVolume:
            return mSettings.audio.master;
        case kIdShake:
            return mSettings.access.screenShake;
        case kIdParticles:
            return mSettings.access.particleDensity;
        case kIdTextScale:
            return mSettings.access.textScale;
        case kIdAssistFuel:
            return mSettings.assists.fuelDrainScale;
        case kIdAssistDamage:
            return mSettings.assists.damageScale;
        case kIdAssistHeat:
            return mSettings.assists.heatScale;
        case kIdAssistMine:
            return mSettings.assists.mineTimeScale;
        default:
            return 0.0f;
    }
}

void Client::set_slider(int id, float fraction) {
    const SliderSpec* spec = slider_spec(id);
    if (spec == nullptr) {
        return;
    }
    const float v = spec->min + clamp(fraction, 0.0f, 1.0f) * (spec->max - spec->min);
    switch (id) {
        case kIdVolume:
            mSettings.audio.master = v;
            if (mAudio) {
                mAudio->set_volumes(v, mSettings.audio.sfx);
            }
            break;
        case kIdShake:
            mSettings.access.screenShake = v;
            break;
        case kIdParticles:
            mSettings.access.particleDensity = v;
            break;
        case kIdTextScale:
            mSettings.access.textScale = v;
            break;
        case kIdAssistFuel:
            mSettings.assists.fuelDrainScale = v;
            break;
        case kIdAssistDamage:
            mSettings.assists.damageScale = v;
            break;
        case kIdAssistHeat:
            mSettings.assists.heatScale = v;
            break;
        case kIdAssistMine:
            mSettings.assists.mineTimeScale = v;
            break;
        default:
            return;
    }
    mSession->set_assists(mSettings.assists);
    build_screen();
}

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
            return;
        }
        case kIdNewGame:
            set_screen(Screen::NewGame);
            return;
        case kIdLoadGame:
            set_screen(Screen::LoadGame);
            return;
        case kIdSettings:
            set_screen(Screen::Settings);
            return;
        case kIdBindings:
            set_screen(Screen::Bindings);
            return;
        case kIdCredits:
            set_screen(Screen::Credits);
            return;
        case kIdQuit:
            mRunning = false;
            return;
        case kIdBack:
            set_screen(mPreviousScreen == mScreen ? Screen::MainMenu : mPreviousScreen);
            return;
        case kIdToggleRandomSeed:
            mUseRandomSeed = !mUseRandomSeed;
            if (!mUseRandomSeed && mSeedInput == 0) {
                mSeedInput = 20260907;
            }
            build_screen();
            return;
        case kIdSeedUp:
            mSeedInput += 1000;
            build_screen();
            return;
        case kIdSeedDown:
            mSeedInput = mSeedInput > 1000 ? mSeedInput - 1000 : 0;
            build_screen();
            return;
        case kIdStartWorld: {
            NewGameOptions options;
            options.randomSeed = mUseRandomSeed;
            options.seed = mSeedInput;
            options.assists = mSettings.assists;
            mSession->new_game(options);
            mRenderer.invalidate_all();
            set_screen(Screen::Hub);
            push_toast(std::format("World seed {}", mSession->world_seed()), theme::kCyan, Icon::Scanner);
            return;
        }
        case kIdDescend:
            mSession->start_expedition();
            mRenderer.invalidate_all();
            mCamera.centre = mSession->rig().position;
            set_screen(Screen::Expedition);
            return;
        case kIdGarage:
            set_screen(Screen::Garage);
            return;
        case kIdModules:
            set_screen(Screen::Modules);
            return;
        case kIdSkills:
            set_screen(Screen::Skills);
            return;
        case kIdResearch:
            set_screen(Screen::Research);
            return;
        case kIdRefinery:
            set_screen(Screen::Refinery);
            return;
        case kIdContracts:
            set_screen(Screen::Contracts);
            return;
        case kIdFacilities:
            set_screen(Screen::Facilities);
            return;
        case kIdCodex:
            set_screen(Screen::Codex);
            return;
        case kIdSaveGame: {
            auto r = mSession->save_game(0);
            push_toast(r ? "Saved." : "Save failed: " + r.error().message, r ? theme::kGood : theme::kDanger,
                       r ? Icon::Check : Icon::Warning);
            return;
        }
        case kIdReturnToMenu:
            if (mSession->state() != GameStateId::MainMenu) {
                (void)mSession->save_game(0);
            }
            set_screen(Screen::MainMenu);
            return;
        case kIdResume:
            set_screen(Screen::Expedition);
            return;
        case kIdOpenMap:
            set_screen(Screen::Map);
            return;
        case kIdAbandonRun:
            mSession->return_to_hub();
            mRenderer.invalidate_all();
            play(Sfx::Return);
            set_screen(Screen::Results);
            return;
        case kIdConfirmRescue:
            mSession->resolve_failure();
            mRenderer.invalidate_all();
            set_screen(Screen::Hub);
            push_toast("Extracted. Progression intact.", theme::kGood, Icon::Check);
            return;
        case kIdResultsOk:
            set_screen(Screen::Hub);
            return;
        case kIdRespec: {
            auto r = econ.respec_skills();
            push_toast(r ? "Proficiency refunded." : r.error().message, r ? theme::kGood : theme::kDanger,
                       r ? Icon::Check : Icon::Warning);
            mSession->refresh();
            build_screen();
            return;
        }
        case kIdResetBindings:
            mSettings.reset_bindings_to_defaults();
            build_screen();
            return;
        case kIdToggleFullscreen:
            mSettings.video.fullscreen = !mSettings.video.fullscreen;
            mWindow.set_fullscreen(mSettings.video.fullscreen);
            mRenderer.invalidate_all();
            build_screen();
            return;
        case kIdToggleVsync:
            mSettings.video.vsync = !mSettings.video.vsync;
            mWindow.set_vsync(mSettings.video.vsync);
            build_screen();
            return;
        case kIdFlashToggle:
            mSettings.access.flashIntensity = mSettings.access.flashIntensity > 0.0f ? 0.0f : 1.0f;
            build_screen();
            return;
        case kIdPaletteToggle:
            mSettings.access.colorBlindPalette = !mSettings.access.colorBlindPalette;
            mRenderer.invalidate_all();
            build_screen();
            return;
        case kIdHoldMineToggle:
            mSettings.access.holdToMine = !mSettings.access.holdToMine;
            build_screen();
            return;
        case kIdHoldThrustToggle:
            mSettings.access.holdToThrust = !mSettings.access.holdToThrust;
            build_screen();
            return;
        case kIdReduceMotionToggle:
            mSettings.access.reduceCameraMotion = !mSettings.access.reduceCameraMotion;
            build_screen();
            return;
        case kIdSubtitlesToggle:
            mSettings.audio.subtitles = !mSettings.audio.subtitles;
            build_screen();
            return;
        case kIdRecoverBackup: {
            auto r = mSession->load_game(mLoadSlot, true);
            if (!r) {
                mErrorText = r.error().to_string();
                set_screen(Screen::Error);
            } else {
                mRenderer.invalidate_all();
                push_toast("Restored from backup.", theme::kGood, Icon::Check);
                set_screen(Screen::Hub);
            }
            return;
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
        case Screen::Hub:
            if (index >= 900) {
                mSession->start_expedition(index - 900);
                mRenderer.invalidate_all();
                mCamera.centre = mSession->rig().position;
                set_screen(Screen::Expedition);
            }
            break;
        case Screen::Garage: {
            auto r = econ.buy_equipment(index);
            if (r) {
                play(Sfx::Upgrade);
                push_toast("Upgrade installed.", theme::kGood, Icon::Check);
                mSession->refresh();
                (void)mSession->autosave();
            } else {
                push_toast(r.error().message, theme::kDanger, Icon::Warning);
            }
            build_screen();
            break;
        }
        case Screen::Modules: {
            if (p.ownedModules.count(index) == 0) {
                auto r = econ.buy_module(index);
                push_toast(r ? "Module purchased." : r.error().message, r ? theme::kGood : theme::kDanger,
                           r ? Icon::Check : Icon::Warning);
                if (r) {
                    play(Sfx::Purchase);
                }
            } else if (p.module_installed(index)) {
                (void)econ.uninstall_module(index);
                push_toast("Module removed.", theme::kTextMuted, Icon::Minus);
            } else {
                auto r = econ.install_module(index);
                push_toast(r ? "Module installed." : r.error().message, r ? theme::kGood : theme::kDanger,
                           r ? Icon::Check : Icon::Warning);
            }
            mSession->refresh();
            (void)mSession->autosave();
            build_screen();
            break;
        }
        case Screen::Skills: {
            auto r = econ.unlock_skill(index);
            push_toast(r ? "Proficiency unlocked." : r.error().message, r ? theme::kGood : theme::kDanger,
                       r ? Icon::Check : Icon::Warning);
            if (r) {
                play(Sfx::Upgrade);
            }
            mSession->refresh();
            (void)mSession->autosave();
            build_screen();
            break;
        }
        case Screen::Research: {
            auto r = econ.complete_research(index);
            push_toast(r ? "Research complete." : r.error().message, r ? theme::kGood : theme::kDanger,
                       r ? Icon::Check : Icon::Warning);
            if (r) {
                play(Sfx::Discovery);
            }
            mSession->refresh();
            (void)mSession->autosave();
            build_screen();
            break;
        }
        case Screen::Refinery: {
            auto r = econ.run_recipe(index, 1, mSession->sim().stats());
            push_toast(r ? "Batch refined." : r.error().message, r ? theme::kGood : theme::kDanger,
                       r ? Icon::Check : Icon::Warning);
            if (r) {
                play(Sfx::Purchase);
            }
            (void)mSession->autosave();
            build_screen();
            break;
        }
        case Screen::Contracts: {
            if (index >= 500) {
                const usize activeIndex = static_cast<usize>(index - 500);
                if (activeIndex < p.contracts.size()) {
                    (void)econ.abandon_contract(p.contracts[activeIndex].instanceId);
                    push_toast("Contract abandoned.", theme::kTextMuted, Icon::Minus);
                }
            } else {
                auto r = econ.accept_contract(static_cast<usize>(index));
                push_toast(r ? "Contract accepted." : r.error().message, r ? theme::kGood : theme::kDanger,
                           r ? Icon::Check : Icon::Warning);
            }
            (void)mSession->autosave();
            build_screen();
            break;
        }
        case Screen::Facilities: {
            auto r = econ.upgrade_facility(index);
            push_toast(r ? "Facility upgraded." : r.error().message, r ? theme::kGood : theme::kDanger,
                       r ? Icon::Check : Icon::Warning);
            if (r) {
                play(Sfx::Upgrade);
            }
            mSession->refresh();
            (void)mSession->autosave();
            build_screen();
            break;
        }
        case Screen::Bindings:
            mRebindAction = static_cast<Action>(index);
            mAwaitingRebind = true;
            mInput->begin_capture();
            build_screen();
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
            mRenderer.invalidate_all();
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
                    push_toast(std::format("Sample recovered: {}", name_of(m.nameKey)), theme::kAmber,
                               Icon::Star);
                }
                break;
            }
            case EventKind::CargoFull:
                push_toast("Cargo hold full - return to the surface", theme::kWarn, Icon::Cargo);
                play(Sfx::CargoFull);
                break;
            case EventKind::FuelLow:
                push_toast("Fuel low", theme::kWarn, Icon::Fuel);
                play(Sfx::LowFuel);
                break;
            case EventKind::FuelEmpty:
                push_toast("Out of fuel", theme::kDanger, Icon::Fuel);
                break;
            case EventKind::VehicleDamaged:
                mRenderer.add_shake(0.25f, mSettings);
                if (mAudio) {
                    mAudio->play(Sfx::HullDamage);
                }
                break;
            case EventKind::HeatCritical:
                if (e.a == 1) {
                    push_toast("Heat critical", theme::kDanger, Icon::Heat);
                }
                play(Sfx::HeatWarning);
                break;
            case EventKind::LandmarkDiscovered:
                push_toast(std::format("Discovered {}", id_name(e.text)), theme::kCyan, Icon::Landmark);
                play(Sfx::Discovery);
                break;
            case EventKind::LandmarkActivated:
                push_toast(std::format("Activated {}", id_name(e.text)), theme::kGood, Icon::Check);
                play(Sfx::Upgrade);
                break;
            case EventKind::BiomeEntered:
                push_toast(std::format("Entering {}", id_name(e.text)), theme::kAmber, Icon::Depth);
                break;
            case EventKind::DepthRecord:
                push_toast(std::format("New depth record: {:.0f} m", e.value), theme::kGood, Icon::Depth);
                break;
            case EventKind::Unstuck:
                push_toast("Emergency reposition used", theme::kWarn, Icon::Warning);
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
                mTutorialTitle = t.title;
                mTutorialBody = t.body;
                mTutorialLife = 11.0f;
                play(Sfx::UiSelect);
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
    mTime += frameSeconds;
    mInput->update(mSettings);
    mInput->sample_pointer(mWindow.renderer());
    if (mAudio) {
        mAudio->set_volumes(mSettings.audio.master, mSettings.audio.sfx);
    }
    mScreenIntro = mSettings.access.reduceCameraMotion
                       ? 1.0f
                       : approach(mScreenIntro, 1.0f, theme::kPanelRate, frameSeconds);

    if (mAwaitingRebind) {
        if (mInput->capture_ready()) {
            mSettings.rebind(mRebindAction, mInput->captured_binding());
            mAwaitingRebind = false;
            build_screen();
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
        t.age += frameSeconds;
    }
    while (!mToasts.empty() && mToasts.front().life <= 0.0f) {
        mToasts.pop_front();
    }
    mTutorialLife = std::max(0.0f, mTutorialLife - frameSeconds);
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
        if (mInput->pressed(Action::Cancel) || mInput->pressed(Action::Pause)) {
            play(Sfx::UiBack);
            if (mScreen == Screen::Map || mScreen == Screen::Pause) {
                set_screen(Screen::Expedition);
            } else if (mScreen != Screen::MainMenu) {
                activate(kIdBack);
            }
        }
        // Left and right adjust the highlighted slider on the settings screen.
        // The step is a tenth of the track, whatever the underlying range is.
        if (mScreen == Screen::Settings && !mMenu.items.empty()) {
            const int dir = mInput->pressed(Action::NavRight) ? 1 : mInput->pressed(Action::NavLeft) ? -1 : 0;
            const int id = mMenu.items[static_cast<usize>(mSelected)].id;
            if (dir != 0 && slider_spec(id) != nullptr) {
                set_slider(id, slider_fraction(id) + 0.1f * static_cast<float>(dir));
            }
        }
    }
    update_tutorials();
}

} // namespace df

namespace df {

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void Client::paint_toasts(Draw& d, float x, float bottom, bool overWorld) {
    float y = bottom;
    for (auto it = mToasts.rbegin(); it != mToasts.rend(); ++it) {
        const float h = d.line_height(text_style::kBody) + d.px(theme::kSpace3);
        y -= h + d.px(theme::kSpace1);
        // Fade in quickly, hold, then fade out.
        const float in = clamp(it->age / 0.18f, 0.0f, 1.0f);
        const float out = clamp(it->life / 0.9f, 0.0f, 1.0f);
        const float alpha = std::min(in, out);
        if (alpha <= 0.01f) {
            continue;
        }
        const float slide = (1.0f - in) * d.px(8.0f);
        const float iconSize = d.px(9.0f);
        const float w = d.measure(it->text, text_style::kBody) + d.px(theme::kSpace4) * 2.0f +
                        (it->icon != Icon::None ? iconSize + d.px(theme::kSpace2) : 0.0f);
        const Rect box{x - slide, y, w, h};
        if (overWorld) {
            d.rect(box, Color{10, 12, 17, static_cast<u8>(190.0f * alpha)});
        } else {
            d.rect(box, theme::kHousingHigh.with_alpha_f(0.85f * alpha));
        }
        d.rect(box.x, box.y, d.px(2.0f), box.h, it->color.with_alpha_f(alpha));
        float tx = box.x + d.px(theme::kSpace4);
        if (it->icon != Icon::None) {
            d.icon(it->icon, tx, box.y + (box.h - iconSize) * 0.5f, iconSize, it->color.with_alpha_f(alpha));
            tx += iconSize + d.px(theme::kSpace2);
        }
        d.text(it->text, tx, box.y + (box.h - d.line_height(text_style::kBody)) * 0.5f,
               it->color.with_alpha_f(alpha), text_style::kBody);
    }
}

void Client::paint_tutorial_card(Draw& d, const Rect& area) {
    if (mTutorialLife <= 0.0f || mTutorialTitle.empty()) {
        return;
    }
    const float alpha = clamp(mTutorialLife / 1.2f, 0.0f, 1.0f);
    const float w = std::min(area.w * 0.52f, d.px(300.0f));
    const float pad = d.px(theme::kSpace4);

    // Wrap the body to the card width first, so the card is exactly tall enough.
    std::vector<std::string> lines;
    std::string line;
    std::string word;
    const float textW = w - pad * 2.0f - d.px(16.0f);
    for (usize i = 0; i <= mTutorialBody.size(); ++i) {
        const char ch = i < mTutorialBody.size() ? mTutorialBody[i] : ' ';
        if (ch == ' ') {
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (d.measure(candidate, text_style::kBody) > textW) {
                lines.push_back(line);
                line = word;
            } else {
                line = candidate;
            }
            word.clear();
        } else {
            word.push_back(ch);
        }
    }
    if (!line.empty()) {
        lines.push_back(line);
    }

    const float h = pad * 2.0f + d.line_height(text_style::kHeading) + d.px(theme::kSpace2) +
                    static_cast<float>(lines.size()) * (d.line_height(text_style::kBody) + d.px(2.0f));
    const Rect card{area.x + area.w * 0.5f - w * 0.5f, area.y + d.px(theme::kSpace6), w, h};

    d.rect(card, Color{18, 24, 33, static_cast<u8>(238.0f * alpha)});
    d.rect(card.x, card.y, d.px(2.5f), card.h, theme::kCyan.with_alpha_f(alpha));
    d.bevel(card, false, std::max(1.0f, d.scale()));

    const float iconSize = d.px(13.0f);
    d.icon(Icon::Scanner, card.x + pad, card.y + pad, iconSize, theme::kCyan.with_alpha_f(alpha));
    d.text(mTutorialTitle, card.x + pad + iconSize + d.px(theme::kSpace3), card.y + pad,
           theme::kText.with_alpha_f(alpha), text_style::kHeading);
    float y = card.y + pad + d.line_height(text_style::kHeading) + d.px(theme::kSpace2);
    for (const auto& l : lines) {
        d.text(l, card.x + pad + iconSize + d.px(theme::kSpace3), y, theme::kTextMuted.with_alpha_f(alpha),
               text_style::kBody);
        y += d.line_height(text_style::kBody) + d.px(2.0f);
    }
}

void Client::paint_hud(Draw& d, const RenderSnapshot& snap) {
    const float w = static_cast<float>(mWindow.width());
    const float h = static_cast<float>(mWindow.height());
    const float pad = d.px(theme::kSpace5);
    const GaugePalette gauges = GaugePalette::resolve(mSettings.access.colorBlindPalette);

    // --- rig instrument cluster, top left ----------------------------------
    const float clusterW = std::min(w * 0.33f, d.px(230.0f));
    const float meterH = d.line_height(text_style::kCaption) + d.px(8.0f);
    const float clusterH = meterH * 4.0f + d.px(theme::kSpace3) * 3.0f + d.px(theme::kSpace4) * 2.0f;
    const Rect cluster{pad, pad, clusterW, clusterH};
    d.panel(cluster, Surface::Raised);

    const Rect inner{cluster.x + d.px(theme::kSpace4), cluster.y + d.px(theme::kSpace4),
                     cluster.w - d.px(theme::kSpace4) * 2.0f, meterH};

    struct Row {
        Icon icon;
        const char* label;
        float value;
        float max;
        Color color;
        float danger;
    };

    const Row rows[] = {
        {Icon::Fuel, "FUEL", mFuelShown, snap.fuelCapacity, gauges.fuel, -1.0f},
        {Icon::Hull, "HULL", mHullShown, snap.hullMax, gauges.hull, -1.0f},
        {Icon::Heat, "HEAT", mHeatShown, snap.heatCapacity, gauges.heat, 0.85f},
        {Icon::Cargo, "CARGO", mCargoShown, snap.cargoCapacity, gauges.cargo, -1.0f},
    };
    for (int i = 0; i < 4; ++i) {
        Rect r = inner;
        r.y += static_cast<float>(i) * (meterH + d.px(theme::kSpace3));
        mUi.meter(r, rows[i].icon, rows[i].label, rows[i].value, rows[i].max, rows[i].color, rows[i].danger);
    }

    // --- depth and biome, top right ----------------------------------------
    const auto& biome = mDb.biomes()[static_cast<usize>(snap.biomeIndex)];
    const float infoW = std::min(w * 0.26f, d.px(180.0f));
    const float infoH = d.line_height(text_style::kNumericBig) + d.line_height(text_style::kCaption) * 2.0f +
                        d.px(theme::kSpace4) * 2.0f + d.px(theme::kSpace2) * 2.0f;
    const Rect info{w - infoW - pad, pad, infoW, infoH};
    d.panel(info, Surface::Raised);
    float iy = info.y + d.px(theme::kSpace4);
    d.text("DEPTH", info.x + d.px(theme::kSpace4), iy, theme::kTextFaint, text_style::kCaption);
    d.text(std::format("{}m", snap.depth), info.x + info.w - d.px(theme::kSpace4), iy - d.px(3.0f),
           theme::kText, text_style::kNumericBig, TextAlign::Right);
    iy += d.line_height(text_style::kNumericBig) + d.px(theme::kSpace2);
    d.text(name_of(biome.nameKey), info.x + d.px(theme::kSpace4), iy, theme::kCyan, text_style::kCaption);
    iy += d.line_height(text_style::kCaption) + d.px(theme::kSpace2);
    d.text(std::format("{} CR", thousands(snap.credits)), info.x + d.px(theme::kSpace4), iy, theme::kAmber,
           text_style::kCaptionNum);

    // --- depth tape down the right edge ------------------------------------
    std::vector<std::pair<i64, Color>> bands;
    const Color bandColors[] = {Color{96, 78, 58},  Color{78, 82, 120},   Color{92, 140, 96},
                                Color{160, 88, 52}, Color{110, 116, 132}, Color{120, 84, 160}};
    for (usize i = 0; i < mDb.biomes().size(); ++i) {
        bands.emplace_back(mDb.biomes()[i].minDepth, bandColors[i % 6]);
    }
    const float tapeW = d.px(24.0f);
    const Rect tape{w - tapeW - pad, info.y + info.h + d.px(theme::kSpace6), tapeW, h * 0.46f};
    const i64 maxDepth = mDb.biomes().empty() ? 4200 : mDb.biomes().back().maxDepth;
    mUi.depth_tape(tape, std::max<i64>(0, snap.depth), maxDepth, bands, "");

    // --- mining target readout, bottom centre -------------------------------
    if (snap.mining) {
        const auto& mat = mSession->world().material(snap.mineTarget);
        const float hardness = mSession->world().hardness(snap.mineTarget);
        const MineQuote q = mSession->sim().quote(mSession->rig(), snap.mineTarget);
        const float boxW = std::min(w * 0.42f, d.px(280.0f));
        const float boxH = d.line_height(text_style::kHeading) + d.line_height(text_style::kCaption) +
                           d.px(theme::kSpace4) * 2.0f + d.px(6.0f);
        const Rect box{w * 0.5f - boxW * 0.5f, h - boxH - pad * 2.2f, boxW, boxH};
        d.panel(box, Surface::Raised);

        const float sw = d.px(12.0f);
        const Color mc{mat.colorR, mat.colorG, mat.colorB};
        d.rect(box.x + d.px(theme::kSpace4), box.y + d.px(theme::kSpace4), sw, sw, mc);
        d.outline(Rect{box.x + d.px(theme::kSpace4), box.y + d.px(theme::kSpace4), sw, sw},
                  theme::kBevelLight, std::max(1.0f, d.scale()));
        const float tx = box.x + d.px(theme::kSpace4) + sw + d.px(theme::kSpace3);
        d.text(name_of(mat.nameKey), tx, box.y + d.px(theme::kSpace4), theme::kText, text_style::kHeading);
        d.text(std::format("{}   hardness {:.1f}   {:.1f}s   {} cr",
                           pretty_key(std::string(material_category_name(mat.category))), hardness, q.seconds,
                           mat.baseValue),
               tx, box.y + d.px(theme::kSpace4) + d.line_height(text_style::kHeading) + d.px(1.0f),
               theme::kTextMuted, text_style::kCaption);
        d.progress(Rect{box.x + d.px(theme::kSpace4), box.y + box.h - d.px(theme::kSpace4) - d.px(4.0f),
                        box.w - d.px(theme::kSpace4) * 2.0f, d.px(4.0f)},
                   snap.mineProgress, theme::kAmber, theme::kWellDeep);
    }

    // --- objectives, under the cluster --------------------------------------
    const auto& contracts = mSession->progress().contracts;
    if (!contracts.empty()) {
        float cy = cluster.y + cluster.h + d.px(theme::kSpace4);
        d.text("OBJECTIVES", pad + d.px(theme::kSpace2), cy, theme::kTextFaint, text_style::kCaption);
        cy += d.line_height(text_style::kCaption) + d.px(theme::kSpace2);
        for (const auto& c : contracts) {
            const auto& t = mDb.contracts()[static_cast<usize>(c.templateIndex)];
            std::string line = name_of(t.nameKey);
            float progress = -1.0f;
            if (c.targetMaterial >= 0) {
                line += std::format("  {}/{}", c.progress, c.targetAmount);
                progress = c.targetAmount > 0
                               ? static_cast<float>(c.progress) / static_cast<float>(c.targetAmount)
                               : 0.0f;
            } else if (c.targetDepth > 0) {
                line += std::format("  {}/{}m", snap.depth, c.targetDepth);
                progress =
                    clamp(static_cast<float>(snap.depth) / static_cast<float>(std::max(1, c.targetDepth)),
                          0.0f, 1.0f);
            }
            const Color c2 = c.completed ? theme::kGood : theme::kText;
            const float ic = d.px(8.0f);
            d.icon(c.completed ? Icon::Check : Icon::Contract, pad + d.px(theme::kSpace2), cy, ic, c2);
            d.text(line, pad + d.px(theme::kSpace2) + ic + d.px(theme::kSpace2), cy, c2,
                   text_style::kCaption);
            cy += d.line_height(text_style::kCaption) + d.px(1.0f);
            if (progress >= 0.0f) {
                d.progress(
                    Rect{pad + d.px(theme::kSpace2) + ic + d.px(theme::kSpace2), cy, d.px(90.0f), d.px(2.0f)},
                    progress, c2, theme::kWellDeep);
                cy += d.px(theme::kSpace3);
            }
            cy += d.px(theme::kSpace2);
        }
    }

    // --- warning banner -----------------------------------------------------
    const float fuelFrac = snap.fuelCapacity > 0 ? snap.fuel / snap.fuelCapacity : 1.0f;
    const float hullFrac = snap.hullMax > 0 ? snap.hull / snap.hullMax : 1.0f;
    const float heatFrac = snap.heatCapacity > 0 ? snap.heat / snap.heatCapacity : 0.0f;
    const char* warning = nullptr;
    Icon warnIcon = Icon::Warning;
    if (hullFrac < 0.2f) {
        warning = "HULL CRITICAL";
        warnIcon = Icon::Hull;
    } else if (heatFrac > 0.9f) {
        warning = "HEAT CRITICAL";
        warnIcon = Icon::Heat;
    } else if (fuelFrac < 0.15f) {
        warning = "FUEL RESERVE";
        warnIcon = Icon::Fuel;
    }
    if (warning != nullptr) {
        // Pulse, but only as far as the flash setting allows.
        const float pulse = 0.55f + 0.45f * std::sin(mTime * 6.0f) * mSettings.access.flashIntensity;
        const float bw = d.measure(warning, text_style::kHeading) + d.px(theme::kSpace6) * 2.0f;
        const float bh = d.line_height(text_style::kHeading) + d.px(theme::kSpace4);
        const Rect banner{w * 0.5f - bw * 0.5f, pad, bw, bh};
        d.rect(banner, theme::kDangerDeep.with_alpha_f(0.55f + 0.35f * pulse));
        d.outline(banner, theme::kDanger.with_alpha_f(pulse), std::max(1.0f, d.scale()));
        const float ic = d.px(10.0f);
        d.icon(warnIcon, banner.x + d.px(theme::kSpace4), banner.y + (banner.h - ic) * 0.5f, ic,
               theme::kDanger);
        d.text(warning, banner.x + banner.w * 0.5f + ic * 0.5f,
               banner.y + (banner.h - d.line_height(text_style::kHeading)) * 0.5f, theme::kDanger,
               text_style::kHeading, TextAlign::Center);
    }

    paint_tutorial_card(d, Rect{0.0f, cluster.y + cluster.h, w, h});
    paint_toasts(d, pad, h - pad, true);
}

void Client::paint_main_menu(Draw& d) {
    const float w = static_cast<float>(mWindow.width());
    const float h = static_cast<float>(mWindow.height());
    d.clear(theme::kVoid);

    // Backdrop: strata receding into the dark, drifting slowly downward. It is
    // the game's subject rendered as decoration.
    const int layers = 9;
    for (int i = layers - 1; i >= 0; --i) {
        const float t = static_cast<float>(i) / static_cast<float>(layers - 1);
        const float bandH = h * (0.05f + 0.02f * t);
        const float drift = std::fmod(mTime * (2.0f + 5.0f * (1.0f - t)), bandH * 2.0f);
        const Color c = mix(Color{34, 30, 28}, Color{10, 11, 15}, t);
        for (float y = h * 0.42f + static_cast<float>(i) * bandH - drift; y < h; y += bandH * 2.0f) {
            d.rect(0.0f, y, w, bandH, c);
        }
    }
    // A warm glow at the horizon, where the surface would be.
    d.vgradient(Rect{0.0f, h * 0.28f, w, h * 0.22f}, Color{10, 11, 15, 0}, Color{46, 30, 18, 190}, 12);
    d.hline(0.0f, h * 0.5f, w, theme::kAmberDeep.with_alpha(120), std::max(1.0f, d.scale()));

    // Title lockup.
    const float titleY = h * 0.17f;
    const float markW = d.px(56.0f);
    const float markX = w * 0.5f - markW * 0.5f;
    for (int i = 0; i < 5; ++i) {
        const float bh = d.px(5.0f);
        d.rect(markX + static_cast<float>(i) * d.px(3.0f), titleY + static_cast<float>(i) * bh * 1.35f,
               markW - static_cast<float>(i) * d.px(6.0f), bh,
               mix(theme::kAmberBright, theme::kAmberDeep, static_cast<float>(i) / 4.0f));
    }
    const float wordY = titleY + d.px(46.0f);
    d.text("DEEPFORGE", w * 0.5f, wordY, theme::kText, text_style::kDisplay, TextAlign::Center);
    const float ruleW = d.measure("DEEPFORGE", text_style::kDisplay);
    d.hline(w * 0.5f - ruleW * 0.5f, wordY + d.line_height(text_style::kDisplay) + d.px(theme::kSpace3),
            ruleW, theme::kAmber, std::max(1.0f, d.scale()));
    d.text("A mining progression game", w * 0.5f,
           wordY + d.line_height(text_style::kDisplay) + d.px(theme::kSpace5), theme::kTextMuted,
           text_style::kBody, TextAlign::Center);

    // The menu itself: a housing centred below the title, drawn through the same
    // widget as every other screen so hover, focus and motion match exactly.
    const float listW = std::min(w * 0.5f, d.px(340.0f));
    const float rowH = d.px(theme::kRowHeight);
    const float unit = std::max(1.0f, std::round(d.scale()));
    // The well adds two units of padding on each edge; size the area to the rows
    // exactly so nothing is clipped and no dead space opens under the last one.
    const float bodyH = static_cast<float>(mMenu.items.size()) * rowH + unit * 4.0f;
    const float hintH = d.line_height(text_style::kCaption) + d.px(theme::kSpace5);
    const float areaH = bodyH + d.px(theme::kSpace3) + hintH;
    const float slide = (1.0f - mScreenIntro) * d.px(14.0f);
    const Rect area{w * 0.5f - listW * 0.5f, std::round(h * 0.60f - areaH * 0.5f) + slide, listW, areaH};

    MenuScreen screen = mMenu;
    screen.inspector = false;
    int selected = mSelected;
    const MenuResult result =
        mUi.menu_screen(area, screen, selected, mScroll, mInput->pressed(Action::Confirm));
    mSelected = selected;
    if (result.activated >= 0) {
        play(Sfx::UiSelect);
        activate(screen.items[static_cast<usize>(result.activated)].id);
    } else if (result.rejected >= 0) {
        const auto& item = screen.items[static_cast<usize>(result.rejected)];
        play(Sfx::UiBack, -0.35f);
        push_toast(item.note.empty() ? "Not available yet" : item.note, theme::kWarn, Icon::Warning);
    }

    d.text(std::format("build {}  -  {}", kBuildVersion, kBuildDate), w * 0.5f, h - d.px(24.0f),
           theme::kTextFaint, text_style::kCaption, TextAlign::Center);
    paint_toasts(d, d.px(theme::kSpace5), h - d.px(theme::kSpace5), false);
}

void Client::paint_menu_screen(Draw& d) {
    const float w = static_cast<float>(mWindow.width());
    const float h = static_cast<float>(mWindow.height());
    const bool overlay = mScreen == Screen::Pause;

    if (!overlay) {
        d.clear(theme::kVoid);
        // Faint strata behind every hub screen ties the UI to the world.
        for (int i = 0; i < 7; ++i) {
            const float t = static_cast<float>(i) / 6.0f;
            d.rect(0.0f, h * (0.12f + t * 0.13f), w, h * 0.035f, mix(Color{20, 18, 17}, theme::kVoid, t));
        }
    } else {
        d.rect(0.0f, 0.0f, w, h, Color{8, 9, 13, 210});
    }

    const float pad = d.px(theme::kSpace6);
    const float barH = d.line_height(text_style::kHeading) + d.px(theme::kSpace5);
    const Rect bar{0.0f, 0.0f, w, barH};
    const auto& p = mSession->progress();
    const char* context = "";
    switch (mScreen) {
        case Screen::Hub:
            context = "SURFACE HUB";
            break;
        case Screen::Garage:
            context = "GARAGE";
            break;
        case Screen::Modules:
            context = "MODULES";
            break;
        case Screen::Skills:
            context = "PROFICIENCY";
            break;
        case Screen::Research:
            context = "RESEARCH";
            break;
        case Screen::Refinery:
            context = "REFINERY";
            break;
        case Screen::Contracts:
            context = "CONTRACTS";
            break;
        case Screen::Facilities:
            context = "FACILITIES";
            break;
        case Screen::Codex:
            context = "CODEX";
            break;
        case Screen::Settings:
            context = "SETTINGS";
            break;
        case Screen::Bindings:
            context = "CONTROLS";
            break;
        case Screen::Results:
            context = "DEBRIEF";
            break;
        case Screen::Failure:
            context = "RESCUE";
            break;
        case Screen::Pause:
            context = "PAUSED";
            break;
        case Screen::NewGame:
            context = "NEW WORLD";
            break;
        case Screen::LoadGame:
            context = "LOAD";
            break;
        case Screen::Credits:
            context = "CREDITS";
            break;
        default:
            context = "";
            break;
    }
    const bool inWorld = mSession->state() != GameStateId::MainMenu;
    mUi.status_bar(bar, context, inWorld ? p.credits : 0, inWorld ? p.insight : 0,
                   inWorld ? p.skill_points_available() : 0,
                   inWorld ? std::format("seed {}", mSession->world_seed()) : std::string{});

    const float slide = (1.0f - mScreenIntro) * d.px(10.0f);
    const Rect area{pad, barH + pad * 0.6f + slide, w - pad * 2.0f, h - barH - pad * 1.6f};

    int selected = mSelected;
    MenuScreen screen = mMenu;
    const MenuResult result =
        mUi.menu_screen(area, screen, selected, mScroll, mInput->pressed(Action::Confirm));
    if (selected != mSelected) {
        mSelected = selected;
    }

    // Settings sliders report their drag through the widget, which owns the
    // track geometry; nothing here re-derives a row rect.
    if (mScreen == Screen::Settings && result.dragged >= 0) {
        set_slider(screen.items[static_cast<usize>(result.dragged)].id, result.dragValue);
    }

    if (result.activated >= 0) {
        play(Sfx::UiSelect);
        activate(screen.items[static_cast<usize>(result.activated)].id);
    } else if (result.rejected >= 0) {
        // Never fail silently: say what is missing rather than ignoring the press.
        const auto& item = screen.items[static_cast<usize>(result.rejected)];
        play(Sfx::UiBack, -0.35f);
        push_toast(item.note.empty() ? "Not available yet" : item.note, theme::kWarn, Icon::Warning);
    }
    paint_toasts(d, d.px(theme::kSpace6), h - d.px(theme::kSpace4), false);
}

void Client::paint_map(Draw& d) {
    const float w = static_cast<float>(mWindow.width());
    const float h = static_cast<float>(mWindow.height());
    d.rect(0.0f, 0.0f, w, h, Color{8, 9, 13, 244});

    const float pad = d.px(theme::kSpace6);
    const float barH = d.line_height(text_style::kHeading) + d.px(theme::kSpace5);
    mUi.status_bar(Rect{0.0f, 0.0f, w, barH}, "SURVEY MAP", mSession->progress().credits,
                   mSession->progress().insight, mSession->progress().skill_points_available(),
                   std::format("seed {}", mSession->world_seed()));

    const Rect frame{pad, barH + pad * 0.6f, w - pad * 2.0f, h - barH - pad * 1.8f};
    d.panel(frame, Surface::Sunken);
    const float unit = std::max(1.0f, std::round(d.scale()));
    const Rect view{frame.x + unit * 2.0f, frame.y + unit * 2.0f, frame.w - unit * 4.0f,
                    frame.h - unit * 4.0f};

    const auto snap = mSession->snapshot(1.0f);
    const int step = 2;
    const i64 halfX = static_cast<i64>(view.w / 2.0f) * step / 2;
    const i64 halfY = static_cast<i64>(view.h / 2.0f) * step / 2;
    const TileCoord centre = mSession->rig().tile();
    for (float py = 0; py < view.h; py += 2.0f) {
        for (float px = 0; px < view.w; px += 2.0f) {
            const TileCoord t{centre.x - halfX + static_cast<i64>(px * static_cast<float>(step)),
                              centre.y - halfY + static_cast<i64>(py * static_cast<float>(step))};
            if (!mSession->world().revealed(t)) {
                continue;
            }
            const auto& m = mSession->world().material(t);
            if (m.category == MaterialCategory::Empty) {
                d.rect(view.x + px, view.y + py, 2.0f, 2.0f, Color{30, 33, 42});
                continue;
            }
            Color c = WorldRenderer::material_color(m, mSettings);
            if (m.category == MaterialCategory::Rock) {
                c = c.shade(0.55f);
            }
            d.rect(view.x + px, view.y + py, 2.0f, 2.0f, c);
        }
    }
    // The rig, marked with a bracket rather than a dot so it is findable.
    const float cx = view.x + view.w * 0.5f;
    const float cy = view.y + view.h * 0.5f;
    const float arm = d.px(7.0f);
    d.rect(cx - arm, cy - unit, arm * 0.5f, unit * 2.0f, theme::kAmber);
    d.rect(cx + arm * 0.5f, cy - unit, arm * 0.5f, unit * 2.0f, theme::kAmber);
    d.rect(cx - unit, cy - arm, unit * 2.0f, arm * 0.5f, theme::kAmber);
    d.rect(cx - unit, cy + arm * 0.5f, unit * 2.0f, arm * 0.5f, theme::kAmber);

    // Legend, so the colours mean something.
    float lx = frame.x + d.px(theme::kSpace5);
    const float ly = frame.y + frame.h - d.px(theme::kSpace6);

    struct Legend {
        const char* label;
        Color color;
    };

    const GaugePalette gp = GaugePalette::resolve(mSettings.access.colorBlindPalette);
    const Legend legend[] = {{"Commodity", gp.cargo},
                             {"Strategic", theme::kCyan},
                             {"Volatile", theme::kDanger},
                             {"Open", Color{30, 33, 42}}};
    for (const auto& item : legend) {
        d.rect(lx, ly, d.px(7.0f), d.px(7.0f), item.color);
        lx += d.px(10.0f);
        lx += d.text(item.label, lx, ly - d.px(1.0f), theme::kTextMuted, text_style::kCaption);
        lx += d.px(theme::kSpace5);
    }

    d.text(std::format("DEPTH {} M", snap.depth), frame.x + frame.w - d.px(theme::kSpace5), ly, theme::kAmber,
           text_style::kCaptionNum, TextAlign::Right);
    mUi.hint_bar(Rect{pad, h - d.px(theme::kSpace6), w - pad * 2.0f, d.px(theme::kSpace5)},
                 "M or B to return");
}

void Client::paint_debug(Draw& d) {
    const float s = d.scale();
    const auto& stats = mSession->world().stats();
    const auto snap = mSession->snapshot(1.0f);
    std::string buf[7];
    buf[0] =
        std::format("frame {:.2f} ms   tick {:.2f} ms   frames {}", mFrameTimeMs, mTickTimeMs, mFrameCount);
    buf[1] = std::format("chunks resident {}   deltas {}   generated {}   evicted {}", stats.residentChunks,
                         stats.deltaChunks, stats.chunksGenerated, stats.chunksEvicted);
    buf[2] = std::format("chunk textures {}   draw calls {}   ui scale {:.0f}", mRenderer.cached_chunks(),
                         mRenderer.draw_calls(), s);
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

    const float lineH = d.line_height(text_style::kCaption) + d.px(2.0f);
    const Rect box{d.px(theme::kSpace4), static_cast<float>(mWindow.height()) - lineH * 7.0f - d.px(20.0f),
                   d.px(320.0f), lineH * 7.0f + d.px(14.0f)};
    d.rect(box, Color{0, 0, 0, 205});
    d.outline(box, theme::kGood.with_alpha(90), std::max(1.0f, s));
    for (int i = 0; i < 7; ++i) {
        d.text(buf[i], box.x + d.px(theme::kSpace3),
               box.y + d.px(theme::kSpace3) + static_cast<float>(i) * lineH, theme::kGood,
               text_style::kCaption);
    }
}

void Client::render(float alpha) {
    Draw d(mWindow.renderer());
    d.set_scale(ui_scale());

    const float w = static_cast<float>(mWindow.width());
    const float h = static_cast<float>(mWindow.height());

    PointerState pointer;
    pointer.position = mInput->mouse_position();
    pointer.present = mInput->mouse_present();
    pointer.down = mInput->mouse_down();
    pointer.pressed = mInput->mouse_pressed();
    pointer.released = mInput->mouse_released();
    pointer.wheel = mInput->wheel();
    mUi.begin_frame(d, pointer, static_cast<float>(mFrameTimeMs / 1000.0),
                    mSettings.access.reduceCameraMotion);

    if (mScreen == Screen::Expedition || mScreen == Screen::Pause || mScreen == Screen::Map) {
        const auto snap = mSession->snapshot(alpha);
        // Instrument readings ease toward the truth so needles settle.
        const float dt = static_cast<float>(mFrameTimeMs / 1000.0);
        const bool instant = mSettings.access.reduceCameraMotion;
        mFuelShown = instant ? snap.fuel : approach(mFuelShown, snap.fuel, theme::kGaugeRate, dt);
        mHullShown = instant ? snap.hull : approach(mHullShown, snap.hull, theme::kGaugeRate, dt);
        mHeatShown = instant ? snap.heat : approach(mHeatShown, snap.heat, theme::kGaugeRate, dt);
        mCargoShown = instant ? snap.cargoUsed : approach(mCargoShown, snap.cargoUsed, theme::kGaugeRate, dt);

        mCamera.pixelsPerTile = kBaseTilePixels * ui_scale() * mSettings.video.zoom;
        const Vec2 lookAhead = mSettings.access.reduceCameraMotion ? Vec2{} : snap.rigVelocity * 0.16f;
        const Vec2 desired = snap.rigPosition + lookAhead;
        mCamera.centre.x = approach(mCamera.centre.x, desired.x, 9.0f, 1.0f / 60.0f);
        mCamera.centre.y = approach(mCamera.centre.y, desired.y, 9.0f, 1.0f / 60.0f);
        mCamera.shakeOffset = mRenderer.shake_offset();
        mRenderer.render(d, *mSession, snap, mCamera, mSettings, w, h);

        // A vignette pushes the eye to the centre and darkens the HUD margins.
        const int steps = 8;
        for (int i = 0; i < steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const u8 a = static_cast<u8>(16.0f * (1.0f - t));
            const float band = h * 0.02f;
            d.rect(0.0f, static_cast<float>(i) * band, w, band, Color{0, 0, 0, a});
            d.rect(0.0f, h - static_cast<float>(i + 1) * band, w, band, Color{0, 0, 0, a});
        }
        if (mScreen == Screen::Expedition) {
            paint_hud(d, snap);
        }
    }

    switch (mScreen) {
        case Screen::Expedition:
            break;
        case Screen::MainMenu:
            paint_main_menu(d);
            break;
        case Screen::Map:
            paint_map(d);
            break;
        default:
            paint_menu_screen(d);
            break;
    }

    if (mDebugOverlay) {
        paint_debug(d);
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
        mSession->start_expedition();
        bool returning = false;
        for (int i = 0; i < ticks && mSession->state() == GameStateId::Expedition; ++i) {
            const auto here = mSession->rig().tile();
            TickCommands cmd;
            if (!returning && (here.y >= 90 || mSession->rig().fuel < 45.0f)) {
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
        const auto snap = mSession->snapshot(1.0f);
        mFuelShown = snap.fuel;
        mHullShown = snap.hull;
        mHeatShown = snap.heat;
        mCargoShown = snap.cargoUsed;
    }
    set_screen(screen);
    mScreenIntro = 1.0f;
    mTutorialLife = 0.0f;
    mToasts.clear();
    poll_events();
    mFrameTimeMs = 16.0;
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
        mFrameTimeMs = 16.0;
        update(1.0f / 60.0f);
        render(1.0f);
    }
    return 0;
}

} // namespace df
