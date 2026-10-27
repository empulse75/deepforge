// The pure decision core of the interface — see FocusModel.hpp. The logic
// here is the exact pass Ui::menu_screen used to run inline before the
// extraction; it was moved, not rewritten.

#include "presentation/FocusModel.hpp"

#include "core/Math.hpp"

#include <algorithm>

namespace df::focus {
namespace {

bool hit(const PointerState& pointer, const Rect& r) {
    return pointer.present && pointer.position.x >= r.x && pointer.position.x < r.x + r.w &&
           pointer.position.y >= r.y && pointer.position.y < r.y + r.h;
}

/// Pixel rounding for a logical length, matching Draw::px().
float px(float logical, float scale) {
    return std::round(logical * scale);
}

} // namespace

Rect slider_track(const Rect& row, float scale) {
    const float pad = px(theme::kSpace4, scale);
    // The numeric column is a fixed width so the track never shifts as the
    // reading goes from "9%" to "100%".
    const float valueW = px(46.0f, scale);
    const float trackW = std::min(px(150.0f, scale), row.w * 0.30f);
    const float trackH = px(5.0f, scale);
    return Rect{row.x + row.w - pad - valueW - trackW, std::round(row.y + (row.h - trackH) * 0.5f), trackW,
                trackH};
}

Decisions decide(const MenuScreen& screen, const PointerState& pointer, int& selected, float& scroll,
                 bool confirmPressed, const Geometry& geom) {
    Decisions decisions;
    MenuResult& result = decisions.result;
    const Rect& view = geom.view;
    const float rowH = geom.row_height;
    const int count = static_cast<int>(screen.items.size());
    const float contentH = static_cast<float>(count) * rowH;
    const float maxScroll = std::max(0.0f, contentH - view.h);
    decisions.max_scroll = maxScroll;

    // Pointer wheel scrolls the list.
    if (hit(pointer, view) && pointer.wheel != 0.0f) {
        scroll = clamp(scroll - pointer.wheel * rowH * 2.0f, 0.0f, maxScroll);
        result.scrolled = true;
    }
    // Keep the focused row inside the view.
    if (selected >= 0 && count > 0) {
        const float top = static_cast<float>(selected) * rowH;
        scroll = clamp(scroll, top + rowH - view.h, top);
        scroll = clamp(scroll, 0.0f, maxScroll);
    }

    for (int i = 0; i < count; ++i) {
        const float y = view.y + static_cast<float>(i) * rowH - scroll;
        if (y + rowH < view.y - rowH || y > view.y + view.h + rowH) {
            continue;
        }
        const Rect rowRect{view.x, y, view.w, rowH};
        // Clip: skip rows that would spill outside the well.
        if (y < view.y - 1.0f || y + rowH > view.y + view.h + 1.0f) {
            continue;
        }
        const auto& item = screen.items[static_cast<usize>(i)];
        const bool hovered = !item.inert && hit(pointer, rowRect);
        if (hovered) {
            result.hovered = i;
            if (selected != i) {
                selected = i;
            }
        }
        const bool focused = selected == i;
        RowDecision rowDecision;
        rowDecision.index = i;
        rowDecision.rect = rowRect;
        rowDecision.hovered = hovered;
        rowDecision.focused = focused;
        rowDecision.pressed = hovered && pointer.down;
        decisions.rows.push_back(rowDecision);

        if (item.slider >= 0.0f && hovered && pointer.down) {
            const Rect track = slider_track(rowRect, geom.scale);
            result.dragged = i;
            result.dragValue = clamp((pointer.position.x - track.x) / track.w, 0.0f, 1.0f);
        } else if (hovered && pointer.released) {
            (item.enabled ? result.activated : result.rejected) = i;
        }
    }

    if (confirmPressed && selected >= 0 && selected < count) {
        const auto& sel = screen.items[static_cast<usize>(selected)];
        if (!sel.inert && sel.slider < 0.0f) {
            (sel.enabled ? result.activated : result.rejected) = selected;
        }
    }
    return decisions;
}

} // namespace df::focus
