// The pure decision core of the interface.
//
// One focus model: a row is active if the pointer is over it *or* the focus
// index points at it, and it commits on click or on confirm (ARCHITECTURE,
// "The interface is drawn, not loaded"). The decisions live here, not in the
// painter, so they are the single source for both: `Ui` calls into this pass
// and draws what it returns, and the test suite drives it directly. No SDL,
// no drawing, no animation state — inputs are plain data, so the whole pass
// links and runs headless on every preset.
#pragma once

#include "presentation/Ui.hpp"

#include <vector>

namespace df::focus {

/// Geometry the pass needs. The caller computes it from the draw surface
/// (`px()` at the active UI scale) so the decisions themselves stay
/// arithmetic: the row list lives at `view`, `row_height` pixels tall,
/// scrolled by `scroll`.
struct Geometry {
    Rect view;
    float row_height = 0.0f;
    float scale = 1.0f; ///< For slider-track metrics.
};

/// One row's decision, handed back to the painter so the visual state and the
/// interaction decision cannot drift apart.
struct RowDecision {
    int index = -1;
    Rect rect;
    bool hovered = false; ///< Pointer is over it and it is not inert.
    bool focused = false; ///< The focus index (possibly moved this frame).
    bool pressed = false; ///< Pointer over it and the button is held.
};

/// The frame's decisions: the interaction outcome plus the per-row states the
/// painter animates. `selected` and `scroll` are mutated exactly as the list
/// screen has always mutated them: hover moves the focus index, the wheel
/// scrolls, the focused row is kept inside the view.
struct Decisions {
    MenuResult result;
    std::vector<RowDecision> rows;
    float max_scroll = 0.0f; ///< What the scrollbar draws against.
};

Decisions decide(const MenuScreen& screen, const PointerState& pointer, int& selected, float& scroll,
                 bool confirmPressed, const Geometry& geom);

/// Track geometry for a slider row. Shared by the painter and the drag test
/// so a click always lands exactly where the knob is drawn.
Rect slider_track(const Rect& row, float scale);

} // namespace df::focus
