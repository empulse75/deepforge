// The one focus model, tested at the seam ARCHITECTURE provides (TEST-01):
// a row is active if the pointer is over it *or* the focus index points at
// it, and it commits on click or on confirm. Mouse and keyboard/controller
// are two inputs into the same model, not two code paths — which is why no
// screen may be reachable by only one device. The decisions run headless in
// FocusModel; this drives the production pass directly with plain data.
#include "presentation/FocusModel.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace df;

namespace {

MenuItem item(int id, bool enabled = true, bool inert = false, float slider = -1.0f) {
    MenuItem i;
    i.id = id;
    i.enabled = enabled;
    i.inert = inert;
    i.slider = slider;
    i.label = "row " + std::to_string(id);
    return i;
}

MenuScreen fixture_screen() {
    MenuScreen s;
    s.title = "focus_model_test";
    s.inspector = false;
    s.items = {item(0), item(1), item(2, false), item(3, true, true), item(4, true, false, 0.5f), item(5)};
    return s;
}

// Scale 1: px() is the identity, so the test can state geometry in tokens.
focus::Geometry fixture_geometry(float viewH = 270.0f) {
    return focus::Geometry{Rect{100.0f, 100.0f, 600.0f, viewH}, theme::kRowHeight, 1.0f};
}

// The centre of row `i` at scroll 0, from the pass's own layout arithmetic.
Vec2 row_centre(const focus::Geometry& geom, int i, float scroll = 0.0f) {
    const float y = geom.view.y + static_cast<float>(i) * geom.row_height - scroll;
    return Vec2{geom.view.x + geom.view.w * 0.5f, y + geom.row_height * 0.5f};
}

PointerState pointer_over(const focus::Geometry& geom, int i, bool released = false, bool down = false,
                          float scroll = 0.0f) {
    PointerState p;
    p.present = true;
    p.position = row_centre(geom, i, scroll);
    p.released = released;
    p.down = down;
    return p;
}

} // namespace

TEST_CASE("presentation/FocusModel pointer over a row takes focus and release commits",
          "[presentation][focus]") {
    auto screen = fixture_screen();
    const auto geom = fixture_geometry();
    int selected = 0;
    float scroll = 0.0f;

    const auto d =
        focus::decide(screen, pointer_over(geom, 1, /*released=*/true), selected, scroll, false, geom);
    CHECK(d.result.hovered == 1);
    CHECK(d.result.activated == 1);
    // The focus index follows the pointer — the single-model property.
    CHECK(selected == 1);
    CHECK(d.result.rejected == -1);
}

TEST_CASE("presentation/FocusModel a click on a disabled row is refused but takes focus",
          "[presentation][focus]") {
    // An action that is merely unavailable is NOT inert: it stays focusable so
    // the inspector can say why. The refusal is reported, not swallowed.
    auto screen = fixture_screen();
    const auto geom = fixture_geometry();
    int selected = 0;
    float scroll = 0.0f;

    const auto d =
        focus::decide(screen, pointer_over(geom, 2, /*released=*/true), selected, scroll, false, geom);
    CHECK(d.result.rejected == 2);
    CHECK(d.result.activated == -1);
    CHECK(selected == 2);
}

TEST_CASE("presentation/FocusModel confirm commits through the focus index with no pointer",
          "[presentation][focus]") {
    // The gamepad path: nothing was ever hovered; confirm commits the row the
    // focus index already points at.
    auto screen = fixture_screen();
    const auto geom = fixture_geometry();
    int selected = 1;
    float scroll = 0.0f;

    PointerState p; // present = false
    const auto d = focus::decide(screen, p, selected, scroll, /*confirmPressed=*/true, geom);
    CHECK(d.result.hovered == -1);
    CHECK(d.result.activated == 1);
    CHECK(selected == 1);
}

TEST_CASE("presentation/FocusModel inert rows never take focus", "[presentation][focus]") {
    // Hovering an inert row must not move the focus index; confirm still
    // commits the focused row, so the readout cannot eat the commit.
    auto screen = fixture_screen();
    const auto geom = fixture_geometry();
    int selected = 0;
    float scroll = 0.0f;

    const auto d =
        focus::decide(screen, pointer_over(geom, 3, /*released=*/true), selected, scroll, false, geom);
    CHECK(d.result.hovered == -1);
    CHECK(d.result.activated == -1);
    CHECK(d.result.rejected == -1);
    CHECK(selected == 0);

    PointerState none;
    const auto d2 = focus::decide(screen, none, selected, scroll, /*confirmPressed=*/true, geom);
    CHECK(d2.result.activated == 0);
}

TEST_CASE("presentation/FocusModel a slider row drags on press and never click-commits",
          "[presentation][focus]") {
    auto screen = fixture_screen();
    const auto geom = fixture_geometry();
    int selected = 0;
    float scroll = 0.0f;

    // Press at the middle of the track geometry — the painter draws the knob
    // exactly here, which is what the shared slider_track exists for.
    PointerState p;
    p.present = true;
    p.down = true;
    const Rect rowRect{geom.view.x, geom.view.y + 4.0f * geom.row_height, geom.view.w, geom.row_height};
    const Rect track = focus::slider_track(rowRect, 1.0f);
    p.position = Vec2{track.x + track.w * 0.5f, rowRect.y + rowRect.h * 0.5f};

    const auto d = focus::decide(screen, p, selected, scroll, /*confirmPressed=*/false, geom);
    REQUIRE(d.result.dragged == 4);
    CHECK(d.result.dragValue == 0.5f);
    // A drag is the commit gesture; a plain click on a slider is not a commit.
    CHECK(d.result.activated == -1);

    // And confirm does not commit a slider row either — same frame decision.
    int selSlider = 4;
    float scroll2 = 0.0f;
    PointerState none;
    const auto d2 = focus::decide(screen, none, selSlider, scroll2, /*confirmPressed=*/true, geom);
    CHECK(d2.result.activated == -1);
    CHECK(d2.result.rejected == -1);
}

TEST_CASE("presentation/FocusModel the wheel scrolls inside the view and clamps", "[presentation][focus]") {
    // Ten rows in a five-row view: contentH 270 over view 135 -> max scroll 135.
    auto screen = fixture_screen();
    screen.items = {item(0), item(1), item(2), item(3), item(4), item(5), item(6), item(7), item(8), item(9)};
    const auto geom = fixture_geometry(135.0f);
    int selected = -1;
    float scroll = 0.0f;

    PointerState p;
    p.present = true;
    p.position = Vec2{300.0f, 150.0f}; // inside the view
    p.wheel = -1.0f;
    auto d = focus::decide(screen, p, selected, scroll, false, geom);
    CHECK(d.result.scrolled);
    CHECK(scroll == 54.0f);           // one notch is two rows
    p.position = row_centre(geom, 0); // park the pointer, no click
    p.wheel = 1.0f;                   // back past the top
    const auto d2 = focus::decide(screen, p, selected, scroll, false, geom);
    CHECK(d2.result.scrolled);
    CHECK(scroll == 0.0f); // clamped at the top
}

TEST_CASE("presentation/FocusModel the focused row is kept inside the view", "[presentation][focus]") {
    auto screen = fixture_screen();
    screen.items = {item(0), item(1), item(2), item(3), item(4), item(5), item(6), item(7), item(8), item(9)};
    const auto geom = fixture_geometry(135.0f);
    int selected = 9; // the last row, below the fold
    float scroll = 0.0f;

    PointerState none;
    const auto d = focus::decide(screen, none, selected, scroll, false, geom);
    CHECK(scroll == 135.0f); // the pass scrolls until the focused row fits
    CHECK(d.max_scroll == 135.0f);
}
