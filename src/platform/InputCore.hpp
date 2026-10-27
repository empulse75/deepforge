// The pure action-state layer: bindings plus a device snapshot in, action
// states and TickCommands out. No SDL (ARCHITECTURE, "Events and commands").
//
// `InputSystem` samples the real devices and calls into this; the test suite
// drives it with synthetic snapshots. The edge, dead-zone and accessibility
// toggle rules live here and only here, so the shape the tests can exercise is
// the shape production runs.
#pragma once

#include "core/Math.hpp"
#include "core/Types.hpp"
#include "domain/Commands.hpp"
#include "platform/Settings.hpp"

#include <array>

namespace df {

/// Action edge state for one frame. Lives with the pure core that computes it.
struct ActionState {
    bool down = false;
    bool pressed = false; ///< Went down this frame.
    bool released = false;
};

} // namespace df

namespace df::input_core {

/// Which code is currently down, in the same code spaces `Binding::code` uses:
/// scancodes for keyboard bindings, button and axis indices for gamepad
/// bindings, button numbers for mouse bindings. Codes outside the tables read
/// as up, which is safer than what this replaced on a hand-edited binding.
struct DeviceSnapshot {
    std::array<bool, 512> keys{};
    std::array<bool, 64> gamepad_buttons{};
    std::array<float, 64> axes{}; ///< Normalised -1..1 by the sampler.
    u32 mouse_buttons_mask = 0;   ///< SDL_BUTTON_MASK semantics: bit (n-1) for button n.
    bool gamepad_connected = false;
    /// The raw left-stick x the original implementation hard-coded into
    /// `move_axis()`, normalised by the sampler.
    float left_stick_x = 0.0f;
};

/// Persistent per-action state with edge detection, mirroring what
/// InputSystem did inline before the extraction.
class ActionMapper {
public:
    static constexpr float kDeadZone = 0.22f;

    /// Recomputes edge states from the bindings and the snapshot. Call once
    /// per rendered frame with the live settings.
    void update(const Settings& settings, const DeviceSnapshot& snapshot);

    const ActionState& action(Action a) const { return mStates[static_cast<usize>(a)]; }

    bool down(Action a) const { return action(a).down; }

    bool pressed(Action a) const { return action(a).pressed; }

    /// Analogue horizontal axis, -1..1, with the dead zone applied.
    float move_axis() const noexcept { return mMoveAxis; }

    /// Converts current action state into simulation intent. Mutates the
    /// hold/toggle state, so it is not const.
    TickCommands to_commands(const Settings& settings);

private:
    bool binding_down(const Binding& b, const DeviceSnapshot& snapshot) const;

    std::array<ActionState, static_cast<usize>(Action::Count)> mStates{};
    float mMoveAxis = 0.0f;

    // Toggle state for accessibility hold/toggle options.
    bool mMineToggle = false;
    MineDirection mToggleDirection = MineDirection::None;
    bool mThrustToggle = false;
};

} // namespace df::input_core
