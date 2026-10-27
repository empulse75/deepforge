// The pure action-state layer — see InputCore.hpp. The bodies here are the
// rules InputSystem used to run against live SDL state before the extraction;
// they were moved, not rewritten (the mouse-button bit rule is SDL3's
// documented SDL_BUTTON_MASK semantics, spelled out here to stay SDL-free).

#include "platform/InputCore.hpp"

#include "core/Math.hpp"

#include <cmath>

namespace df::input_core {
namespace {

constexpr usize kGamepadCodeCount = 64;

bool key_down(const DeviceSnapshot& snapshot, i32 code) {
    return code >= 0 && static_cast<usize>(code) < snapshot.keys.size() &&
           snapshot.keys[static_cast<usize>(code)];
}

} // namespace

void ActionMapper::update(const Settings& settings, const DeviceSnapshot& snapshot) {
    for (usize i = 0; i < mStates.size(); ++i) {
        bool down = false;
        for (const auto& b : settings.bindings[i]) {
            if (binding_down(b, snapshot)) {
                down = true;
                break;
            }
        }
        auto& state = mStates[i];
        state.pressed = down && !state.down;
        state.released = !down && state.down;
        state.down = down;
    }

    mMoveAxis = 0.0f;
    if (snapshot.gamepad_connected) {
        const float raw = snapshot.left_stick_x;
        if (std::fabs(raw) > kDeadZone) {
            // Rescale past the dead zone so the first movement is not a jump.
            const float sign = raw < 0.0f ? -1.0f : 1.0f;
            mMoveAxis = sign * (std::fabs(raw) - kDeadZone) / (1.0f - kDeadZone);
        }
    }
    if (down(Action::MoveLeft) && !down(Action::MoveRight)) {
        mMoveAxis = -1.0f;
    } else if (down(Action::MoveRight) && !down(Action::MoveLeft)) {
        mMoveAxis = 1.0f;
    }
}

TickCommands ActionMapper::to_commands(const Settings& settings) {
    TickCommands cmd;
    cmd.moveX = clamp(mMoveAxis, -1.0f, 1.0f);

    if (settings.access.holdToThrust) {
        cmd.thrust = down(Action::Thrust);
    } else {
        if (pressed(Action::Thrust)) {
            mThrustToggle = !mThrustToggle;
        }
        cmd.thrust = mThrustToggle;
    }

    MineDirection dir = MineDirection::None;
    if (down(Action::MineDown)) {
        dir = MineDirection::Down;
    } else if (down(Action::MineLeft)) {
        dir = MineDirection::Left;
    } else if (down(Action::MineRight)) {
        dir = MineDirection::Right;
    } else if (down(Action::MineUp)) {
        dir = MineDirection::Up;
    }
    if (settings.access.holdToMine) {
        cmd.mine = dir;
    } else {
        if (dir != MineDirection::None && (pressed(Action::MineDown) || pressed(Action::MineLeft) ||
                                           pressed(Action::MineRight) || pressed(Action::MineUp))) {
            if (mMineToggle && mToggleDirection == dir) {
                mMineToggle = false;
            } else {
                mMineToggle = true;
                mToggleDirection = dir;
            }
        }
        cmd.mine = mMineToggle ? mToggleDirection : MineDirection::None;
    }

    cmd.brake = down(Action::Brake);
    cmd.interact = down(Action::Interact);
    cmd.scan = pressed(Action::Scan);
    cmd.unstuck = pressed(Action::Unstuck);
    return cmd;
}

bool ActionMapper::binding_down(const Binding& b, const DeviceSnapshot& snapshot) const {
    switch (b.device) {
        case BindingDevice::Keyboard:
            return key_down(snapshot, b.code);
        case BindingDevice::GamepadButton:
            return snapshot.gamepad_connected && b.code >= 0 &&
                   static_cast<usize>(b.code) < kGamepadCodeCount &&
                   snapshot.gamepad_buttons[static_cast<usize>(b.code)];
        case BindingDevice::GamepadAxis: {
            if (!snapshot.gamepad_connected || b.code < 0 ||
                static_cast<usize>(b.code) >= snapshot.axes.size()) {
                return false;
            }
            const float v = snapshot.axes[static_cast<usize>(b.code)];
            return b.axisSign < 0 ? v < -kDeadZone : v > kDeadZone;
        }
        case BindingDevice::Mouse:
            return b.code >= 1 && static_cast<u32>(b.code) < 33 &&
                   (snapshot.mouse_buttons_mask & (1u << (static_cast<u32>(b.code) - 1))) != 0;
    }
    return false;
}

} // namespace df::input_core
