#include "platform/Settings.hpp"

#include "core/JsonFile.hpp"
#include "core/Log.hpp"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <fstream>

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

Binding decode_binding(const json& j) {
    Binding b;
    b.device = static_cast<BindingDevice>(j.value("device", 0));
    b.code = j.value("code", 0);
    b.axisSign = j.value("sign", 0);
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

Result<Settings> load_settings(const std::filesystem::path& file) {
    // The file being absent and the file being corrupt are different problems
    // with different remedies, so they must stay different errors: a missing
    // file means "there is nothing to load yet", a malformed one means "delete
    // it to restore defaults". read_json_file() reports both as a read/parse
    // failure, so the existence check comes first.
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) {
        return fail(Error{"settings file not found", file.string(), {}, {}, {}});
    }
    auto parsed = read_json_file(file);
    if (!parsed) {
        return fail(
            Error{"settings file is not valid JSON", file.string(), {}, {}, "delete it to restore defaults"});
    }
    json j = std::move(*parsed);
    Settings s = default_settings();
    if (j.contains("video")) {
        const auto& v = j["video"];
        s.video.windowWidth = v.value("width", s.video.windowWidth);
        s.video.windowHeight = v.value("height", s.video.windowHeight);
        s.video.fullscreen = v.value("fullscreen", s.video.fullscreen);
        s.video.vsync = v.value("vsync", s.video.vsync);
        s.video.uiScale = v.value("ui_scale", s.video.uiScale);
        s.video.zoom = v.value("zoom", s.video.zoom);
    }
    if (j.contains("audio")) {
        const auto& a = j["audio"];
        s.audio.master = a.value("master", s.audio.master);
        s.audio.sfx = a.value("sfx", s.audio.sfx);
        s.audio.music = a.value("music", s.audio.music);
        s.audio.subtitles = a.value("subtitles", s.audio.subtitles);
    }
    if (j.contains("accessibility")) {
        const auto& a = j["accessibility"];
        s.access.screenShake = a.value("screen_shake", s.access.screenShake);
        s.access.flashIntensity = a.value("flash_intensity", s.access.flashIntensity);
        s.access.particleDensity = a.value("particle_density", s.access.particleDensity);
        s.access.colorBlindPalette = a.value("color_blind_palette", s.access.colorBlindPalette);
        s.access.materialLabels = a.value("material_labels", s.access.materialLabels);
        s.access.holdToMine = a.value("hold_to_mine", s.access.holdToMine);
        s.access.holdToThrust = a.value("hold_to_thrust", s.access.holdToThrust);
        s.access.textScale = a.value("text_scale", s.access.textScale);
        s.access.reduceCameraMotion = a.value("reduce_camera_motion", s.access.reduceCameraMotion);
    }
    if (j.contains("assists")) {
        const auto& a = j["assists"];
        s.assists.fuelDrainScale = a.value("fuel_drain", s.assists.fuelDrainScale);
        s.assists.damageScale = a.value("damage", s.assists.damageScale);
        s.assists.heatScale = a.value("heat", s.assists.heatScale);
        s.assists.mineTimeScale = a.value("mine_time", s.assists.mineTimeScale);
    }
    if (j.contains("bindings")) {
        for (usize i = 0; i < s.bindings.size(); ++i) {
            const std::string name{action_name(static_cast<Action>(i))};
            if (!j["bindings"].contains(name)) {
                continue;
            }
            s.bindings[i].clear();
            for (const auto& b : j["bindings"][name]) {
                s.bindings[i].push_back(decode_binding(b));
            }
        }
    }
    return s;
}

} // namespace df
