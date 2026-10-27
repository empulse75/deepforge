// Player settings, accessibility options and control bindings.
#pragma once

#include "core/Result.hpp"
#include "core/Types.hpp"
#include "domain/Expedition.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace df {

/// Every action the game can be told to do. Bindings are fully remappable.
enum class Action : u16 {
    MoveLeft,
    MoveRight,
    Thrust,
    Brake,
    MineLeft,
    MineRight,
    MineUp,
    MineDown,
    Interact,
    Scan,
    Unstuck,
    Map,
    Pause,
    Confirm,
    Cancel,
    NavUp,
    NavDown,
    NavLeft,
    NavRight,
    TabNext,
    TabPrev,
    DebugOverlay,
    Count
};

std::string_view action_name(Action a);
std::string_view action_label(Action a);

enum class BindingDevice : u8 { Keyboard, GamepadButton, GamepadAxis, Mouse };

struct Binding {
    BindingDevice device = BindingDevice::Keyboard;
    i32 code = 0;     ///< SDL scancode / gamepad button / axis index.
    i32 axisSign = 0; ///< For axes: +1 or -1.

    friend bool operator==(const Binding&, const Binding&) = default;
};

struct VideoSettings {
    int windowWidth = 1600;
    int windowHeight = 900;
    bool fullscreen = false;
    bool vsync = true;
    float uiScale = 0.0f; ///< 0 means automatic from display DPI.
    float zoom = 1.0f;
};

struct AudioSettings {
    float master = 0.8f;
    float sfx = 0.9f;
    float music = 0.5f;
    bool subtitles = true;
};

/// Accessibility options (Section 12.3). Every one of these is honoured.
struct AccessibilitySettings {
    float screenShake = 1.0f;    ///< 0 disables shake entirely.
    float flashIntensity = 1.0f; ///< 0 disables flashes.
    float particleDensity = 1.0f;
    bool colorBlindPalette = false;
    bool materialLabels = true; ///< Text/pattern in addition to colour.
    bool holdToMine = true;     ///< False makes mining a toggle.
    bool holdToThrust = true;
    float textScale = 1.0f;
    bool reduceCameraMotion = false;
};

struct Settings {
    VideoSettings video;
    AudioSettings audio;
    AccessibilitySettings access;
    AssistSettings assists;
    std::array<std::vector<Binding>, static_cast<usize>(Action::Count)> bindings;

    void reset_bindings_to_defaults();

    /// Replaces every binding for `action`.
    void rebind(Action action, Binding binding);

    /// Removes a binding wherever it appears, so no code is bound twice.
    void clear_conflicts(Binding binding, Action except);

    std::string describe_binding(Action action) const;
};

Settings default_settings();

Status save_settings(const std::filesystem::path& file, const Settings& s);

/// Load the user-editable settings file. A wrong type is a named failure carrying
/// the documented remedy; a value outside its documented domain falls back to the
/// documented default and records a warning through `warnings` (the existing
/// Error machinery — the loader logs each one too). The default keeps existing
/// callers unchanged.
Result<Settings> load_settings(const std::filesystem::path& file, ErrorList* warnings = nullptr);

} // namespace df
