// Input sampling. Converts SDL events and device state into action states,
// then into a TickCommands buffer for the simulation. The decisions themselves
// (edges, dead zone, accessibility toggles) live in the SDL-free core,
// InputCore.hpp; this file is the SDL glue: device sampling and event capture.
#pragma once

#include "platform/InputCore.hpp"

#include <array>
#include <string>

struct SDL_Gamepad;
struct SDL_Renderer;
union SDL_Event;

namespace df {

class InputSystem {
public:
    InputSystem();
    ~InputSystem();

    InputSystem(const InputSystem&) = delete;
    InputSystem& operator=(const InputSystem&) = delete;

    void handle_event(const SDL_Event& e);

    /// Recomputes edge states. Call once per rendered frame.
    void update(const Settings& settings);

    const ActionState& action(Action a) const { return mMapper.action(a); }

    bool down(Action a) const { return mMapper.down(a); }

    bool pressed(Action a) const { return mMapper.pressed(a); }

    /// Analogue horizontal axis, -1..1, with dead zone applied.
    float move_axis() const noexcept { return mMapper.move_axis(); }

    // --- pointer -----------------------------------------------------------
    /// Mouse position in render pixels. `present` stays false until the mouse
    /// actually moves, so a controller player never sees a stray hover.
    Vec2 mouse_position() const noexcept { return mMousePosition; }

    bool mouse_present() const noexcept { return mMousePresent; }

    bool mouse_down() const noexcept { return mMouseDown; }

    bool mouse_pressed() const noexcept { return mMousePressed; }

    bool mouse_released() const noexcept { return mMouseReleased; }

    float wheel() const noexcept { return mWheel; }

    /// Called by the client each frame with the renderer, so window coordinates
    /// can be converted to render pixels under high-DPI scaling.
    void sample_pointer(SDL_Renderer* renderer);

    /// Converts current action state into simulation intent.
    TickCommands to_commands(const Settings& settings);

    bool quit_requested() const noexcept { return mQuit; }

    bool gamepad_connected() const noexcept { return mGamepad != nullptr; }

    /// Rebinding capture: the next input received becomes the binding.
    void begin_capture() {
        mCapturing = true;
        mCaptured = false;
    }

    bool capture_ready() const noexcept { return mCaptured; }

    Binding captured_binding() const noexcept { return mCapturedBinding; }

    void cancel_capture() {
        mCapturing = false;
        mCaptured = false;
    }

private:
    input_core::ActionMapper mMapper; // the single source for action state
    SDL_Gamepad* mGamepad = nullptr;
    bool mQuit = false;
    bool mCapturing = false;
    bool mCaptured = false;
    Binding mCapturedBinding{};

    // Pointer state stays here because it is sampled through SDL window
    // coordinates; the click/release edges belong to the pointer, not to the
    // binding map.
    Vec2 mMousePosition{};
    bool mMousePresent = false;
    bool mMouseDown = false;
    bool mMousePressed = false;
    bool mMouseReleased = false;
    bool mMouseWasDown = false;
    float mWheel = 0.0f;
    float mWheelAccum = 0.0f;
};

} // namespace df
