// The action-state layer, tested at the seam ARCHITECTURE provides (TEST-01):
// bindings plus a device snapshot go in, TickCommands come out — and the
// accessibility hold/toggle options are honoured, not decorative (Section 12.3
// "Every one of these is honoured"). InputSystem is the SDL glue; this drives
// the production core with synthetic snapshots, no SDL runtime, no display.
// Bindings are set from the data the core reads, so the test follows a
// rebinding instead of hard-coding keys.
#include "platform/InputCore.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace df;
using namespace df::input_core;

namespace {

Settings settings_with(Action action, Binding binding) {
    Settings s;
    s.bindings[static_cast<usize>(action)].push_back(binding);
    return s;
}

constexpr Binding kb(i32 scancode) {
    return Binding{BindingDevice::Keyboard, scancode, 0};
}

} // namespace

TEST_CASE("platform/InputCore edge states are press, hold, release", "[platform][input]") {
    auto settings = settings_with(Action::Thrust, kb(40));
    ActionMapper mapper;
    DeviceSnapshot snap;

    snap.keys[40] = true;
    mapper.update(settings, snap);
    CHECK(mapper.pressed(Action::Thrust)); // went down this frame
    CHECK(mapper.down(Action::Thrust));

    mapper.update(settings, snap);
    CHECK_FALSE(mapper.pressed(Action::Thrust)); // held, not a new press
    CHECK(mapper.down(Action::Thrust));

    snap.keys[40] = false;
    mapper.update(settings, snap);
    CHECK(mapper.action(Action::Thrust).released);
    CHECK_FALSE(mapper.down(Action::Thrust));
}

TEST_CASE("platform/InputCore hold mode follows the key; toggle mode latches it", "[platform][input]") {
    auto settings = settings_with(Action::Thrust, kb(40));

    // Default: hold to thrust — the command mirrors the key.
    ActionMapper hold;
    DeviceSnapshot down;
    down.keys[40] = true;
    DeviceSnapshot up;
    hold.update(settings, down);
    CHECK(hold.to_commands(settings).thrust);
    hold.update(settings, up);
    CHECK_FALSE(hold.to_commands(settings).thrust);

    // The accessibility option: a press toggles, and the state survives the
    // release. That is the whole point of turning hold-to-thrust off.
    settings.access.holdToThrust = false;
    ActionMapper toggle;
    toggle.update(settings, down);
    CHECK(toggle.to_commands(settings).thrust); // latched on by the press edge
    toggle.update(settings, up);                // no edge, nothing flips
    CHECK(toggle.to_commands(settings).thrust);
    toggle.update(settings, down); // second press toggles off
    CHECK_FALSE(toggle.to_commands(settings).thrust);
}

TEST_CASE("platform/InputCore mining toggles to the pressed direction and cancels on the same one",
          "[platform][input]") {
    Settings settings;
    settings.access.holdToMine = false; // the mining toggle option
    settings.bindings[static_cast<usize>(Action::MineDown)].push_back(kb(50));
    settings.bindings[static_cast<usize>(Action::MineLeft)].push_back(kb(51));
    ActionMapper mapper;

    DeviceSnapshot pressDown;
    pressDown.keys[50] = true;
    mapper.update(settings, pressDown);
    CHECK(mapper.to_commands(settings).mine == MineDirection::Down);

    // Key gone; the toggle keeps mining down.
    DeviceSnapshot none;
    mapper.update(settings, none);
    CHECK(mapper.to_commands(settings).mine == MineDirection::Down);

    // A different direction latches over.
    DeviceSnapshot pressLeft;
    pressLeft.keys[51] = true;
    mapper.update(settings, pressLeft);
    CHECK(mapper.to_commands(settings).mine == MineDirection::Left);

    // The same direction pressed again — a real edge, so release first —
    // cancels: the documented exit from the toggle.
    mapper.update(settings, none);
    mapper.update(settings, pressLeft);
    CHECK(mapper.to_commands(settings).mine == MineDirection::None);
}

TEST_CASE("platform/InputCore the keyboard axis keeps left and right mutually exclusive",
          "[platform][input]") {
    Settings settings;
    settings.bindings[static_cast<usize>(Action::MoveLeft)].push_back(kb(30));
    settings.bindings[static_cast<usize>(Action::MoveRight)].push_back(kb(31));
    ActionMapper mapper;

    DeviceSnapshot left;
    left.keys[30] = true;
    mapper.update(settings, left);
    CHECK(mapper.move_axis() == -1.0f);

    DeviceSnapshot both;
    both.keys[30] = true;
    both.keys[31] = true;
    mapper.update(settings, both);
    CHECK(mapper.move_axis() == 0.0f); // neither direction wins, so the rig holds

    DeviceSnapshot right;
    right.keys[31] = true;
    mapper.update(settings, right);
    CHECK(mapper.move_axis() == 1.0f);
}

TEST_CASE("platform/InputCore the stick dead zone zeroes small moves and rescales the rest",
          "[platform][input]") {
    // The axis is bound to an action that is not a Move action, so the digital
    // keyboard override in move_axis() cannot mask what the analogue rescale
    // produced.
    Settings settings;
    settings.bindings[static_cast<usize>(Action::Brake)].push_back(
        Binding{BindingDevice::GamepadAxis, 5, -1});
    ActionMapper mapper;

    DeviceSnapshot snap;
    snap.gamepad_connected = true;

    // Inside the zone: nothing moves, no binding fires.
    snap.axes[5] = -0.1f;
    snap.left_stick_x = 0.2f;
    mapper.update(settings, snap);
    CHECK_FALSE(mapper.down(Action::Brake));
    CHECK(mapper.move_axis() == 0.0f);

    // Past the zone: the binding is down, and the axis rescales so the first
    // movement past the threshold is not a jump. At -0.61 the answer is
    // exactly half: -(0.61 - 0.22) / (1 - 0.22) = -0.5.
    snap.axes[5] = -0.5f;
    snap.left_stick_x = -0.61f;
    mapper.update(settings, snap);
    CHECK(mapper.down(Action::Brake)); // sign -1 binding, negative deflection
    CHECK(mapper.move_axis() == Catch::Approx(-0.5f).margin(1e-5f));
}

TEST_CASE("platform/InputCore a hand-edited binding outside the code tables reads as up",
          "[platform][input]") {
    // The input trust boundary reaches here too: a scancode from a
    // hand-edited settings file must not read a device state the table does
    // not have.
    auto settings = settings_with(Action::Interact, kb(512));
    ActionMapper mapper;
    DeviceSnapshot snap;
    for (usize i = 0; i < snap.keys.size(); ++i) {
        snap.keys[i] = true; // everything real is down
    }
    mapper.update(settings, snap);
    CHECK_FALSE(mapper.down(Action::Interact));
}
