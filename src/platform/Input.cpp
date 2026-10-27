#include "platform/Input.hpp"

#include "core/Log.hpp"
#include "core/Math.hpp"

#include <SDL3/SDL.h>

#include <cmath>

namespace df {
namespace {

/// Fill the SDL-backed snapshot the pure core reads. Sampling the whole code
/// range is a few dozen calls a frame and keeps every decision in InputCore.
input_core::DeviceSnapshot sample_device_state(SDL_Gamepad* gamepad) {
    input_core::DeviceSnapshot snapshot;
    const bool* keys = SDL_GetKeyboardState(nullptr);
    if (keys != nullptr) {
        for (usize i = 0; i < snapshot.keys.size(); ++i) {
            snapshot.keys[i] = keys[i];
        }
    }
    if (gamepad != nullptr) {
        snapshot.gamepad_connected = true;
        for (usize i = 0; i < snapshot.gamepad_buttons.size(); ++i) {
            snapshot.gamepad_buttons[i] = SDL_GetGamepadButton(gamepad, static_cast<SDL_GamepadButton>(i));
        }
        for (usize i = 0; i < snapshot.axes.size(); ++i) {
            snapshot.axes[i] =
                static_cast<float>(SDL_GetGamepadAxis(gamepad, static_cast<SDL_GamepadAxis>(i))) / 32767.0f;
        }
        snapshot.left_stick_x = snapshot.axes[SDL_GAMEPAD_AXIS_LEFTX];
    }
    snapshot.mouse_buttons_mask = SDL_GetMouseState(nullptr, nullptr);
    return snapshot;
}

} // namespace

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
        case SDL_EVENT_MOUSE_MOTION:
            mMousePresent = true;
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            mWheelAccum += e.wheel.y;
            mMousePresent = true;
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

void InputSystem::update(const Settings& settings) {
    mMapper.update(settings, sample_device_state(mGamepad));
}

void InputSystem::sample_pointer(SDL_Renderer* renderer) {
    float wx = 0.0f;
    float wy = 0.0f;
    const auto buttons = SDL_GetMouseState(&wx, &wy);
    float rx = wx;
    float ry = wy;
    if (renderer != nullptr) {
        // Window coordinates are logical; the UI works in render pixels, which
        // differ on a high-DPI display.
        SDL_RenderCoordinatesFromWindow(renderer, wx, wy, &rx, &ry);
    }
    mMousePosition = Vec2{rx, ry};
    const bool down = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
    mMousePressed = down && !mMouseWasDown;
    mMouseReleased = !down && mMouseWasDown;
    mMouseDown = down;
    mMouseWasDown = down;
    if (down) {
        mMousePresent = true;
    }
    mWheel = mWheelAccum;
    mWheelAccum = 0.0f;
}

TickCommands InputSystem::to_commands(const Settings& settings) {
    return mMapper.to_commands(settings);
}

} // namespace df
