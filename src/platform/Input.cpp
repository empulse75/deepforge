#include "platform/Input.hpp"

#include "core/Log.hpp"
#include "core/Math.hpp"

#include <SDL3/SDL.h>

#include <cmath>

namespace df {

InputSystem::InputSystem() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids != nullptr) {
        if (count > 0) {
            mGamepad = SDL_OpenGamepad(ids[0]);
            if (mGamepad) {
                DF_LOG_INFO("input", "gamepad connected: {}", SDL_GetGamepadName(mGamepad));
            }
        }
        SDL_free(ids);
    }
}

InputSystem::~InputSystem() {
    if (mGamepad) {
        SDL_CloseGamepad(mGamepad);
    }
}

void InputSystem::handle_event(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_QUIT:
            mQuit = true;
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!mGamepad) {
                mGamepad = SDL_OpenGamepad(e.gdevice.which);
            }
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            if (mGamepad && SDL_GetGamepadID(mGamepad) == e.gdevice.which) {
                SDL_CloseGamepad(mGamepad);
                mGamepad = nullptr;
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (mCapturing && !e.key.repeat) {
                mCapturedBinding = Binding{BindingDevice::Keyboard, static_cast<i32>(e.key.scancode), 0};
                mCaptured = true;
                mCapturing = false;
            }
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (mCapturing) {
                mCapturedBinding =
                    Binding{BindingDevice::GamepadButton, static_cast<i32>(e.gbutton.button), 0};
                mCaptured = true;
                mCapturing = false;
            }
            break;
        default:
            break;
    }
}

bool InputSystem::binding_down(const Binding& b) const {
    switch (b.device) {
        case BindingDevice::Keyboard: {
            const bool* keys = SDL_GetKeyboardState(nullptr);
            return keys != nullptr && keys[b.code];
        }
        case BindingDevice::GamepadButton:
            return mGamepad != nullptr &&
                   SDL_GetGamepadButton(mGamepad, static_cast<SDL_GamepadButton>(b.code));
        case BindingDevice::GamepadAxis: {
            if (mGamepad == nullptr) {
                return false;
            }
            const float v =
                static_cast<float>(SDL_GetGamepadAxis(mGamepad, static_cast<SDL_GamepadAxis>(b.code))) /
                32767.0f;
            return b.axisSign < 0 ? v < -dead_zone : v > dead_zone;
        }
        case BindingDevice::Mouse: {
            const auto buttons = SDL_GetMouseState(nullptr, nullptr);
            return (buttons & SDL_BUTTON_MASK(b.code)) != 0;
        }
    }
    return false;
}

void InputSystem::update(const Settings& settings) {
    for (usize i = 0; i < mActions.size(); ++i) {
        bool down = false;
        for (const auto& b : settings.bindings[i]) {
            if (binding_down(b)) {
                down = true;
                break;
            }
        }
        auto& state = mActions[i];
        state.pressed = down && !state.down;
        state.released = !down && state.down;
        state.down = down;
    }

    mMoveAxis = 0.0f;
    if (mGamepad != nullptr) {
        const float raw = static_cast<float>(SDL_GetGamepadAxis(mGamepad, SDL_GAMEPAD_AXIS_LEFTX)) / 32767.0f;
        if (std::fabs(raw) > dead_zone) {
            // Rescale past the dead zone so the first movement is not a jump.
            const float sign = raw < 0.0f ? -1.0f : 1.0f;
            mMoveAxis = sign * (std::fabs(raw) - dead_zone) / (1.0f - dead_zone);
        }
    }
    if (down(Action::MoveLeft) && !down(Action::MoveRight)) {
        mMoveAxis = -1.0f;
    } else if (down(Action::MoveRight) && !down(Action::MoveLeft)) {
        mMoveAxis = 1.0f;
    }
}

TickCommands InputSystem::to_commands(const Settings& settings) {
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

} // namespace df
