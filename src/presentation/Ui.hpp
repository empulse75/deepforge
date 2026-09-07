// Widget kit for the instrument-panel interface.
//
// Immediate mode with a small persistent store for per-item animation, so hover
// and focus can ease rather than snap. Every widget is driven by the same
// pointer/keyboard/controller state, which is what keeps mouse support from
// forking the interaction model: a row is "active" if the pointer is over it or
// the focus index points at it, and it activates on click or on confirm.
#pragma once

#include "presentation/Draw.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace df {

/// Pointer state for one frame, in render pixels.
struct PointerState {
    Vec2 position{};
    bool present = false; ///< False until the mouse actually moves.
    bool down = false;
    bool pressed = false;
    bool released = false;
    float wheel = 0.0f;
};

/// A before/after statistic shown in the inspector when a row is highlighted.
struct StatDelta {
    std::string label;
    float before = 0.0f;
    float after = 0.0f;
    bool lowerIsBetter = false;
    int decimals = 1;
};

/// One row in a list screen, plus everything the inspector needs to describe it.
struct MenuItem {
    Icon icon = Icon::None;
    std::string label;
    std::string value;  ///< Right-aligned primary figure.
    std::string detail; ///< Secondary line beneath the label.
    std::string status; ///< Short state chip, e.g. "installed".
    std::string body;   ///< Inspector paragraph.
    std::string note;   ///< Why the row is unavailable, if it is.
    std::vector<StatDelta> deltas;
    std::vector<std::pair<std::string, std::string>> facts;
    Color accent = theme::kText;
    Color statusColor = theme::kCyan;
    Color valueColor = theme::kText; ///< Lets a price read as unaffordable at a glance.
    bool enabled = true;             ///< Can be committed to right now.
    /// Pure readout: never takes focus. An action that is merely unavailable is
    /// NOT inert - it stays focusable so the inspector can say why.
    bool inert = false;
    bool marked = false; ///< Owned / taken / complete: draws a check.
    bool locked = false;
    float progress = -1.0f;
    float slider = -1.0f; ///< 0..1 when the row is a slider; negative when it is not.
    int id = 0;
};

/// A whole list screen.
struct MenuScreen {
    std::string title;
    std::string subtitle;
    std::string hint;
    std::vector<MenuItem> items;
    /// Shown centred in the list well when nothing but navigation rows remain,
    /// so an empty screen explains itself instead of reading as a blank panel.
    std::string emptyText;
    bool inspector = true;
    Icon titleIcon = Icon::None;
};

/// Result of drawing a list screen.
struct MenuResult {
    int hovered = -1;
    int activated = -1; ///< Index of the row the player committed to.
    int rejected = -1;  ///< Index of a row the player tried but cannot use yet.
    bool scrolled = false;
    int dragged = -1;       ///< Index of a slider row being dragged this frame.
    float dragValue = 0.0f; ///< Its new 0..1 fraction.
};

/// Persistent per-widget animation, keyed by a stable id.
struct WidgetAnim {
    float hover = 0.0f;
    float focus = 0.0f;
    float press = 0.0f;
    float touched = 0.0f;
};

class Ui {
public:
    Ui() = default;

    void begin_frame(Draw& draw, const PointerState& pointer, float dt, bool reduceMotion);

    Draw& draw() { return *mDraw; }

    const PointerState& pointer() const noexcept { return mPointer; }

    float dt() const noexcept { return mDt; }

    bool reduce_motion() const noexcept { return mReduceMotion; }

    /// Eased 0..1 for a widget, so hover and focus fade instead of snapping.
    float anim(u64 key, bool hovered, bool focused, bool pressed);

    bool hit(const Rect& r) const;

    // --- composed screens --------------------------------------------------
    /// Top status strip shown on every screen.
    void status_bar(const Rect& r, std::string_view context, i64 credits, i64 insight, int proficiency,
                    std::string_view right);

    /// Draws a full list screen and returns hover/activation.
    MenuResult menu_screen(const Rect& area, const MenuScreen& screen, int& selected, float& scroll,
                           bool confirmPressed);

    /// Title block with an optional icon and a rule.
    void screen_header(const Rect& r, std::string_view title, std::string_view subtitle, Icon icon);

    /// Bottom hint strip listing the controls that apply here.
    void hint_bar(const Rect& r, std::string_view hint);

    /// Draws word-wrapped text inside `box`'s width and returns the y after the
    /// last line. `box.h` is unused - the caller decides how much room it has.
    float wrap_text(std::string_view text, const Rect& box, Color c, const TextStyle& style);

    /// Inspector panel body for one item.
    void inspector(const Rect& r, const MenuItem& item);

    /// Labelled segmented gauge with an icon and a numeric readout.
    void meter(const Rect& r, Icon icon, std::string_view label, float value, float max, Color fill,
               float danger = -1.0f);

    /// Vertical depth tape: biome bands with the rig's position marked.
    void depth_tape(const Rect& r, i64 depth, i64 maxDepth, const std::vector<std::pair<i64, Color>>& bands,
                    std::string_view biome);

private:
    void list_row(const Rect& r, const MenuItem& item, bool hovered, float t);

    /// Track geometry for a slider row. Shared by the painter and the drag test
    /// so a click always lands exactly where the knob is drawn.
    Rect slider_track(const Rect& row) const;

    Draw* mDraw = nullptr;
    PointerState mPointer;
    float mDt = 0.0f;
    bool mReduceMotion = false;
    std::unordered_map<u64, WidgetAnim> mAnim;
    float mFrameTime = 0.0f;
};

} // namespace df
