#include "platform/Settings.hpp"

#include "core/JsonFile.hpp"
#include "core/Log.hpp"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <string>

namespace df {
namespace {

using json = nlohmann::json;

struct ActionInfo {
    Action action;
    const char* id;
    const char* label;
};

constexpr ActionInfo kActions[] = {
    {Action::MoveLeft, "move_left", "Move left"},
    {Action::MoveRight, "move_right", "Move right"},
    {Action::Thrust, "thrust", "Thrust / ascend"},
    {Action::Brake, "brake", "Brake"},
    {Action::MineLeft, "mine_left", "Drill left"},
    {Action::MineRight, "mine_right", "Drill right"},
    {Action::MineUp, "mine_up", "Drill up"},
    {Action::MineDown, "mine_down", "Drill down"},
    {Action::Interact, "interact", "Interact"},
    {Action::Scan, "scan", "Scan"},
    {Action::Unstuck, "unstuck", "Emergency reposition"},
    {Action::Map, "map", "Map"},
    {Action::Pause, "pause", "Pause"},
    {Action::Confirm, "confirm", "Confirm"},
    {Action::Cancel, "cancel", "Back"},
    {Action::NavUp, "nav_up", "Menu up"},
    {Action::NavDown, "nav_down", "Menu down"},
    {Action::NavLeft, "nav_left", "Menu left"},
    {Action::NavRight, "nav_right", "Menu right"},
    {Action::TabNext, "tab_next", "Next tab"},
    {Action::TabPrev, "tab_prev", "Previous tab"},
    {Action::DebugOverlay, "debug_overlay", "Developer overlay"},
};

json encode_binding(const Binding& b) {
    return json{{"device", static_cast<int>(b.device)}, {"code", b.code}, {"sign", b.axisSign}};
}

/// The documented domain a numeric setting has to stay inside (SEC-04). Outside it
/// the value falls back to the documented default with a warning, because the
/// window call, the render scale and the sliders all assume the range. The ranges
/// mirror the settings-screen slider table in src/presentation/Client.cpp
/// (kSliders) and the integer-step clamp in Client::ui_scale — the platform layer
/// sits below presentation and may not include it, so the table is written down
/// twice and the tests pin that the two agree.
struct Domain {
    double lo;
    double hi;
};

constexpr Domain kWindowWidthDomain{256.0, 7680.0};  // the real display range, 8K at the top
constexpr Domain kWindowHeightDomain{256.0, 4320.0}; // 0 means automatic from display DPI
constexpr Domain kUiScaleDomain{0.0, 8.0};           // 0 is the automatic mode, not a value
constexpr Domain kZoomDomain{1.0, 2.0};              // SPEC: integer zoom steps 1x and 2x
constexpr double kZoomStep1 = 1.0, kZoomStep2 = 2.0; // ... and no fractional scaling
constexpr Domain kVolumeDomain{0.0, 1.0};
constexpr Domain kSliderDomain{0.0, 1.0}; // screen shake, flashes, particle density
constexpr Domain kTextScaleDomain{0.7, 2.0};
constexpr Domain kAssistFuelDomain{0.2, 1.5};
constexpr Domain kAssistDamageDomain{0.0, 1.5};
constexpr Domain kAssistHeatDomain{0.2, 1.5};
constexpr Domain kAssistMineDomain{0.4, 1.5};

constexpr const char* kRemedy = "delete it to restore defaults";

/// The loader's problem list. A wrong type is fatal — this file is officially
/// user-editable, so the diagnosis and the documented remedy are the point, which
/// is exactly the path that used to be unreachable because the process died first.
/// An out-of-domain value is a warning: the documented default stands, and the
/// field is named. Nothing here throws — that is the whole reason this exists.
struct Issues {
    std::string file;
    ErrorList* warnings = nullptr; // may be null; the warning is logged either way
    bool failed = false;
    Error failure{};

    void fatal(const json& found, const char* section, const char* key, FieldType wanted) {
        if (failed) {
            return; // one named failure is enough to stop the load
        }
        failed = true;
        failure = Error{std::string("field must be ") + field_type_label(wanted) + ", but is " +
                            std::string(field_type_name(found)),
                        file, section, key, kRemedy};
    }

    void warn(const json& found, const char* section, const char* key, const Domain& domain,
              const std::string& usedDefault) {
        Error w{std::string("value is outside its documented range (") + std::to_string(domain.lo) + "-" +
                    std::to_string(domain.hi) + "), but is " + std::string(field_type_name(found)) +
                    " — using the default " + usedDefault,
                file,
                section,
                key,
                {}};
        if (warnings != nullptr) {
            warnings->add(w);
        }
        DF_LOG_WARN("settings", "{}", w.to_string());
    }
};

/// The one accessor for a settings section. A section that is absent is normal and
/// silent — every field in it keeps its documented default. A section present as
/// anything other than an object is the planted case that aborted the process
/// inside nlohmann before the app's own advice could reach the player.
const json* sectionOf(const json& root, const char* name, Issues& issues) {
    if (!root.contains(name)) {
        return nullptr;
    }
    const auto& section = root.at(name);
    if (!matches_type(section, FieldType::Object)) {
        issues.fatal(section, name, "", FieldType::Object);
        return nullptr;
    }
    return &section;
}

/// The one accessor for a settings field: missing → the default, silently; wrong
/// type → named failure with the documented remedy; right type outside its
/// documented domain → the default with a warning. The typed readers below all go
/// through this, so the type rule lives in exactly one place.
const json* fieldOf(const json& section, const char* key, FieldType type, Issues& issues,
                    const char* sectionName) {
    if (!section.contains(key) || section.at(key).is_null()) {
        return nullptr; // missing: the documented default stands, silently
    }
    const auto& value = section.at(key);
    if (!matches_type(value, type)) {
        issues.fatal(value, sectionName, key, type);
        return nullptr;
    }
    return &value;
}

int readInt(const json& section, const char* key, int def, const Domain& domain, Issues& issues,
            const char* sectionName) {
    const json* value = fieldOf(section, key, FieldType::Integer, issues, sectionName);
    if (value == nullptr) {
        return def;
    }
    const long long v = value->get<long long>();
    if (v < static_cast<long long>(domain.lo) || v > static_cast<long long>(domain.hi)) {
        issues.warn(*value, sectionName, key, domain, std::to_string(def));
        return def;
    }
    return static_cast<int>(v);
}

float readNumber(const json& section, const char* key, float def, const Domain& domain, Issues& issues,
                 const char* sectionName) {
    const json* value = fieldOf(section, key, FieldType::Number, issues, sectionName);
    if (value == nullptr) {
        return def;
    }
    const double v = value->get<double>();
    if (v < domain.lo || v > domain.hi) {
        issues.warn(*value, sectionName, key, domain, std::to_string(def));
        return def;
    }
    return static_cast<float>(v);
}

bool readFlag(const json& section, const char* key, bool def, Issues& issues, const char* sectionName) {
    const json* value = fieldOf(section, key, FieldType::Boolean, issues, sectionName);
    return value == nullptr ? def : value->get<bool>();
}

Binding decode_binding(const json& j, Issues& issues, const char* actionName) {
    Binding b; // the documented default binding: keyboard, code 0, sign 0
    const json* device = fieldOf(j, "device", FieldType::Integer, issues, actionName);
    if (device != nullptr) {
        const long long d = device->get<long long>();
        if (d < 0 || d > static_cast<long long>(BindingDevice::Mouse)) {
            const Domain enumerators{0.0, static_cast<double>(static_cast<int>(BindingDevice::Mouse))};
            issues.warn(*device, actionName, "device", enumerators, "0");
        } else {
            b.device = static_cast<BindingDevice>(d);
        }
    }
    const json* code = fieldOf(j, "code", FieldType::Integer, issues, actionName);
    if (code != nullptr) {
        b.code = static_cast<i32>(code->get<long long>()); // scancode ranges are SEC-03's
    }
    const json* sign = fieldOf(j, "sign", FieldType::Integer, issues, actionName);
    if (sign != nullptr) {
        b.axisSign = static_cast<i32>(sign->get<long long>());
    }
    return b;
}

} // namespace

std::string_view action_name(Action a) {
    for (const auto& e : kActions) {
        if (e.action == a) {
            return e.id;
        }
    }
    return "unknown";
}

std::string_view action_label(Action a) {
    for (const auto& e : kActions) {
        if (e.action == a) {
            return e.label;
        }
    }
    return "Unknown";
}

void Settings::reset_bindings_to_defaults() {
    for (auto& b : bindings) {
        b.clear();
    }
    auto key = [&](Action a, SDL_Scancode sc) {
        bindings[static_cast<usize>(a)].push_back(Binding{BindingDevice::Keyboard, static_cast<i32>(sc), 0});
    };
    auto pad = [&](Action a, SDL_GamepadButton btn) {
        bindings[static_cast<usize>(a)].push_back(
            Binding{BindingDevice::GamepadButton, static_cast<i32>(btn), 0});
    };
    auto axis = [&](Action a, SDL_GamepadAxis ax, int sign) {
        bindings[static_cast<usize>(a)].push_back(
            Binding{BindingDevice::GamepadAxis, static_cast<i32>(ax), sign});
    };

    key(Action::MoveLeft, SDL_SCANCODE_A);
    key(Action::MoveLeft, SDL_SCANCODE_LEFT);
    key(Action::MoveRight, SDL_SCANCODE_D);
    key(Action::MoveRight, SDL_SCANCODE_RIGHT);
    key(Action::Thrust, SDL_SCANCODE_W);
    key(Action::Thrust, SDL_SCANCODE_UP);
    key(Action::Thrust, SDL_SCANCODE_SPACE);
    key(Action::Brake, SDL_SCANCODE_LSHIFT);
    key(Action::MineDown, SDL_SCANCODE_S);
    key(Action::MineDown, SDL_SCANCODE_DOWN);
    key(Action::MineLeft, SDL_SCANCODE_J);
    key(Action::MineRight, SDL_SCANCODE_L);
    key(Action::MineUp, SDL_SCANCODE_I);
    key(Action::Interact, SDL_SCANCODE_E);
    key(Action::Scan, SDL_SCANCODE_Q);
    key(Action::Unstuck, SDL_SCANCODE_U);
    key(Action::Map, SDL_SCANCODE_M);
    key(Action::Pause, SDL_SCANCODE_ESCAPE);
    key(Action::Confirm, SDL_SCANCODE_RETURN);
    key(Action::Confirm, SDL_SCANCODE_E);
    key(Action::Cancel, SDL_SCANCODE_BACKSPACE);
    key(Action::NavUp, SDL_SCANCODE_UP);
    key(Action::NavUp, SDL_SCANCODE_W);
    key(Action::NavDown, SDL_SCANCODE_DOWN);
    key(Action::NavDown, SDL_SCANCODE_S);
    key(Action::NavLeft, SDL_SCANCODE_LEFT);
    key(Action::NavLeft, SDL_SCANCODE_A);
    key(Action::NavRight, SDL_SCANCODE_RIGHT);
    key(Action::NavRight, SDL_SCANCODE_D);
    key(Action::TabNext, SDL_SCANCODE_TAB);
    key(Action::TabPrev, SDL_SCANCODE_GRAVE);
    key(Action::DebugOverlay, SDL_SCANCODE_F3);

    pad(Action::Thrust, SDL_GAMEPAD_BUTTON_SOUTH);
    pad(Action::Brake, SDL_GAMEPAD_BUTTON_EAST);
    pad(Action::Interact, SDL_GAMEPAD_BUTTON_WEST);
    pad(Action::Scan, SDL_GAMEPAD_BUTTON_NORTH);
    pad(Action::Pause, SDL_GAMEPAD_BUTTON_START);
    pad(Action::Map, SDL_GAMEPAD_BUTTON_BACK);
    pad(Action::Confirm, SDL_GAMEPAD_BUTTON_SOUTH);
    pad(Action::Cancel, SDL_GAMEPAD_BUTTON_EAST);
    pad(Action::NavUp, SDL_GAMEPAD_BUTTON_DPAD_UP);
    pad(Action::NavDown, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    pad(Action::NavLeft, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    pad(Action::NavRight, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    pad(Action::TabNext, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    pad(Action::TabPrev, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    pad(Action::MineDown, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    pad(Action::MineLeft, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    pad(Action::MineRight, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    pad(Action::MineUp, SDL_GAMEPAD_BUTTON_DPAD_UP);
    axis(Action::MoveLeft, SDL_GAMEPAD_AXIS_LEFTX, -1);
    axis(Action::MoveRight, SDL_GAMEPAD_AXIS_LEFTX, 1);
    axis(Action::MineDown, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1);
}

void Settings::clear_conflicts(Binding binding, Action except) {
    for (usize i = 0; i < bindings.size(); ++i) {
        if (static_cast<Action>(i) == except) {
            continue;
        }
        auto& list = bindings[i];
        list.erase(std::remove(list.begin(), list.end(), binding), list.end());
    }
}

void Settings::rebind(Action action, Binding binding) {
    clear_conflicts(binding, action);
    bindings[static_cast<usize>(action)].clear();
    bindings[static_cast<usize>(action)].push_back(binding);
}

std::string Settings::describe_binding(Action action) const {
    const auto& list = bindings[static_cast<usize>(action)];
    if (list.empty()) {
        return "unbound";
    }
    std::string out;
    for (usize i = 0; i < list.size() && i < 2; ++i) {
        if (i != 0) {
            out += " / ";
        }
        const auto& b = list[i];
        switch (b.device) {
            case BindingDevice::Keyboard: {
                const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(b.code));
                out += (name && *name) ? name : "key";
                break;
            }
            case BindingDevice::GamepadButton: {
                const char* name = SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(b.code));
                out += "pad:";
                out += (name && *name) ? name : "button";
                break;
            }
            case BindingDevice::GamepadAxis: {
                const char* name = SDL_GetGamepadStringForAxis(static_cast<SDL_GamepadAxis>(b.code));
                out += "pad:";
                out += (name && *name) ? name : "axis";
                out += b.axisSign < 0 ? "-" : "+";
                break;
            }
            case BindingDevice::Mouse:
                out += "mouse";
                break;
        }
    }
    return out;
}

Settings default_settings() {
    Settings s;
    s.reset_bindings_to_defaults();
    return s;
}

Status save_settings(const std::filesystem::path& file, const Settings& s) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    json j;
    j["version"] = 1;
    j["video"] = {{"width", s.video.windowWidth},     {"height", s.video.windowHeight},
                  {"fullscreen", s.video.fullscreen}, {"vsync", s.video.vsync},
                  {"ui_scale", s.video.uiScale},      {"zoom", s.video.zoom}};
    j["audio"] = {{"master", s.audio.master},
                  {"sfx", s.audio.sfx},
                  {"music", s.audio.music},
                  {"subtitles", s.audio.subtitles}};
    j["accessibility"] = {{"screen_shake", s.access.screenShake},
                          {"flash_intensity", s.access.flashIntensity},
                          {"particle_density", s.access.particleDensity},
                          {"color_blind_palette", s.access.colorBlindPalette},
                          {"material_labels", s.access.materialLabels},
                          {"hold_to_mine", s.access.holdToMine},
                          {"hold_to_thrust", s.access.holdToThrust},
                          {"text_scale", s.access.textScale},
                          {"reduce_camera_motion", s.access.reduceCameraMotion}};
    j["assists"] = {{"fuel_drain", s.assists.fuelDrainScale},
                    {"damage", s.assists.damageScale},
                    {"heat", s.assists.heatScale},
                    {"mine_time", s.assists.mineTimeScale}};
    json binds = json::object();
    for (usize i = 0; i < s.bindings.size(); ++i) {
        json arr = json::array();
        for (const auto& b : s.bindings[i]) {
            arr.push_back(encode_binding(b));
        }
        binds[std::string(action_name(static_cast<Action>(i)))] = std::move(arr);
    }
    j["bindings"] = std::move(binds);

    const auto temp = file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) {
            return fail(Error{"cannot write settings", temp, {}, {}, {}});
        }
        out << j.dump(1, ' ');
    }
    std::filesystem::rename(temp, file, ec);
    if (ec) {
        return fail(Error{"cannot replace settings file: " + ec.message(), file.string(), {}, {}, {}});
    }
    return {};
}

Result<Settings> load_settings(const std::filesystem::path& file, ErrorList* warnings) {
    // The file being absent and the file being corrupt are different problems
    // with different remedies, so they must stay different errors: a missing
    // file means "there is nothing to load yet", a malformed one means "delete
    // it to restore defaults". read_json_file() reports both as a read/parse
    // failure, so the existence check comes first.
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) {
        return fail(Error{"settings file not found", file.string(), {}, {}, {}});
    }
    auto parsed = read_json_file(file, kIngestCeilingSettingsBytes);
    if (!parsed) {
        if (parsed.error().field == "size") {
            return std::unexpected(parsed.error()); // over the documented ceiling, with its remedy
        }
        return fail(
            Error{"settings file is not valid JSON", file.string(), {}, {}, "delete it to restore defaults"});
    }
    json j = std::move(*parsed);
    Issues issues{file.string(), warnings};
    if (!j.is_object()) {
        issues.fatal(j, "", "", FieldType::Object);
        return std::unexpected(issues.failure);
    }
    Settings s = default_settings();

    if (const json* v = sectionOf(j, "video", issues); v != nullptr) {
        s.video.windowWidth = readInt(*v, "width", s.video.windowWidth, kWindowWidthDomain, issues, "video");
        s.video.windowHeight =
            readInt(*v, "height", s.video.windowHeight, kWindowHeightDomain, issues, "video");
        s.video.fullscreen = readFlag(*v, "fullscreen", s.video.fullscreen, issues, "video");
        s.video.vsync = readFlag(*v, "vsync", s.video.vsync, issues, "video");
        s.video.uiScale = readNumber(*v, "ui_scale", s.video.uiScale, kUiScaleDomain, issues, "video");
        if (const json* z = fieldOf(*v, "zoom", FieldType::Number, issues, "video"); z != nullptr) {
            const double zoom = z->get<double>();
            if (zoom != kZoomStep1 && zoom != kZoomStep2) {
                issues.warn(*z, "video", "zoom", kZoomDomain, std::to_string(s.video.zoom));
            } else {
                s.video.zoom = static_cast<float>(zoom);
            }
        }
    }
    if (const json* a = sectionOf(j, "audio", issues); a != nullptr) {
        s.audio.master = readNumber(*a, "master", s.audio.master, kVolumeDomain, issues, "audio");
        s.audio.sfx = readNumber(*a, "sfx", s.audio.sfx, kVolumeDomain, issues, "audio");
        s.audio.music = readNumber(*a, "music", s.audio.music, kVolumeDomain, issues, "audio");
        s.audio.subtitles = readFlag(*a, "subtitles", s.audio.subtitles, issues, "audio");
    }
    if (const json* a = sectionOf(j, "accessibility", issues); a != nullptr) {
        s.access.screenShake =
            readNumber(*a, "screen_shake", s.access.screenShake, kSliderDomain, issues, "accessibility");
        s.access.flashIntensity = readNumber(*a, "flash_intensity", s.access.flashIntensity, kSliderDomain,
                                             issues, "accessibility");
        s.access.particleDensity = readNumber(*a, "particle_density", s.access.particleDensity, kSliderDomain,
                                              issues, "accessibility");
        s.access.colorBlindPalette =
            readFlag(*a, "color_blind_palette", s.access.colorBlindPalette, issues, "accessibility");
        s.access.materialLabels =
            readFlag(*a, "material_labels", s.access.materialLabels, issues, "accessibility");
        s.access.holdToMine = readFlag(*a, "hold_to_mine", s.access.holdToMine, issues, "accessibility");
        s.access.holdToThrust =
            readFlag(*a, "hold_to_thrust", s.access.holdToThrust, issues, "accessibility");
        s.access.textScale =
            readNumber(*a, "text_scale", s.access.textScale, kTextScaleDomain, issues, "accessibility");
        s.access.reduceCameraMotion =
            readFlag(*a, "reduce_camera_motion", s.access.reduceCameraMotion, issues, "accessibility");
    }
    if (const json* a = sectionOf(j, "assists", issues); a != nullptr) {
        s.assists.fuelDrainScale =
            readNumber(*a, "fuel_drain", s.assists.fuelDrainScale, kAssistFuelDomain, issues, "assists");
        s.assists.damageScale =
            readNumber(*a, "damage", s.assists.damageScale, kAssistDamageDomain, issues, "assists");
        s.assists.heatScale =
            readNumber(*a, "heat", s.assists.heatScale, kAssistHeatDomain, issues, "assists");
        s.assists.mineTimeScale =
            readNumber(*a, "mine_time", s.assists.mineTimeScale, kAssistMineDomain, issues, "assists");
    }
    if (const json* b = sectionOf(j, "bindings", issues); b != nullptr && !issues.failed) {
        for (usize i = 0; i < s.bindings.size(); ++i) {
            const std::string name{action_name(static_cast<Action>(i))};
            if (!b->contains(name)) {
                continue;
            }
            s.bindings[i].clear();
            for (const auto& e : (*b)[name]) {
                if (!e.is_object()) {
                    issues.fatal(e, name.c_str(), "", FieldType::Object);
                    break;
                }
                s.bindings[i].push_back(decode_binding(e, issues, name.c_str()));
            }
        }
    }

    if (issues.failed) {
        return std::unexpected(issues.failure);
    }
    return s;
}

} // namespace df
