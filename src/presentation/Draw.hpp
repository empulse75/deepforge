// Drawing primitives for the instrument-panel interface.
//
// Everything the UI is made of lives here: proportional bitmap text, machined
// panels, inset wells, segmented gauges and a procedural icon set. There are no
// image assets - every mark is drawn from rectangles, which keeps the whole
// look crisp at any integer UI scale and keeps the repository asset-free.
#pragma once

#include "core/Math.hpp"
#include "core/Types.hpp"
#include "presentation/Theme.hpp"

#include <string>
#include <string_view>

struct SDL_Renderer;

namespace df {

enum class TextAlign { Left, Center, Right };

enum class Face : u8 { Ui, Micro };

/// A named typographic style. `scale` multiplies the UI scale, so text is always
/// rendered at an integer pixel scale and stays sharp.
struct TextStyle {
    Face face = Face::Ui;
    int scale = 1;
    float tracking = 1.0f; ///< Extra pixels between glyphs, at 1x.
    bool bold = false;
    bool tabular = false; ///< Fixed advance, so changing numbers do not jitter.
};

namespace text_style {
inline constexpr TextStyle kDisplay{Face::Ui, 3, 2.0f, true, false};
inline constexpr TextStyle kTitle{Face::Ui, 2, 2.0f, true, false};
inline constexpr TextStyle kHeading{Face::Ui, 1, 1.0f, true, false};
inline constexpr TextStyle kBody{Face::Ui, 1, 1.0f, false, false};
inline constexpr TextStyle kNumeric{Face::Ui, 1, 1.0f, false, true};
inline constexpr TextStyle kNumericBig{Face::Ui, 2, 1.0f, true, true};
inline constexpr TextStyle kCaption{Face::Micro, 1, 1.0f, false, false};
inline constexpr TextStyle kCaptionNum{Face::Micro, 1, 1.0f, false, true};
} // namespace text_style

/// Panel treatments. The difference between them is where the light comes from.
enum class Surface : u8 {
    Raised, ///< Housing that sits above the board: light top-left.
    Sunken, ///< A well cut into the housing: light bottom-right.
    Flat,   ///< A plain filled area with a hairline.
    Glass,  ///< Translucent overlay for modals.
};

/// Procedural icons. Each is drawn inside a square box from rectangles.
enum class Icon : u8 {
    None,
    Fuel,
    Hull,
    Heat,
    Cargo,
    Credit,
    Insight,
    Depth,
    Drill,
    Garage,
    Module,
    Skill,
    Research,
    Refinery,
    Contract,
    Facility,
    Codex,
    Map,
    Save,
    Settings,
    Controls,
    Lock,
    Check,
    Warning,
    Chevron,
    ChevronDown,
    Descend,
    Lift,
    Scanner,
    Landmark,
    Star,
    Cross,
    Plus,
    Minus,
    Count
};

/// Thin wrapper over SDL_Renderer. All coordinates are in physical pixels; the
/// UI scale is applied by callers through `scale()`.
class Draw {
public:
    explicit Draw(SDL_Renderer* renderer) : mRenderer(renderer) {}

    SDL_Renderer* renderer() const noexcept { return mRenderer; }

    void set_scale(float s) noexcept { mScale = s; }

    float scale() const noexcept { return mScale; }

    /// Converts a token-space length into physical pixels.
    float px(float logical) const noexcept { return std::round(logical * mScale); }

    // --- primitives --------------------------------------------------------
    void clear(Color c);
    void rect(float x, float y, float w, float h, Color c);

    void rect(const Rect& r, Color c) { rect(r.x, r.y, r.w, r.h, c); }

    void outline(const Rect& r, Color c, float thickness = 1.0f);

    void outline_xywh(float x, float y, float w, float h, Color c, float thickness = 1.0f) {
        outline(Rect{x, y, w, h}, c, thickness);
    }

    void hline(float x, float y, float w, Color c, float thickness = 1.0f);
    void vline(float x, float y, float h, Color c, float thickness = 1.0f);
    void line(float x0, float y0, float x1, float y1, Color c);

    /// Vertical gradient approximated with horizontal bands. Bands are cheap and
    /// suit the dithered, machined look better than a smooth ramp would.
    void vgradient(const Rect& r, Color top, Color bottom, int bands = 10);

    /// A machined bevel: light on the top and left, dark on the bottom and right
    /// (or the reverse for an inset).
    void bevel(const Rect& r, bool inset, float thickness = 1.0f);

    /// Panel body plus bevel plus optional corner notches.
    void panel(const Rect& r, Surface surface, bool notches = true);

    /// Small diagonal marks at the corners of a panel. Purely decorative, but
    /// they are what makes a rectangle read as a machined housing.
    void corner_notches(const Rect& r, Color c, float length);

    /// Faint horizontal texture, as on a brushed metal face.
    void brushed(const Rect& r, Color c, float spacing = 3.0f);

    // --- text --------------------------------------------------------------
    /// Draws text with `y` as the top of the cell. Returns the advance width.
    float text(std::string_view s, float x, float y, Color c, const TextStyle& style = text_style::kBody,
               TextAlign align = TextAlign::Left);

    float measure(std::string_view s, const TextStyle& style = text_style::kBody) const;

    /// Height of one line in physical pixels, including descender space.
    float line_height(const TextStyle& style) const;

    /// Distance from the top of the cell to the baseline.
    float baseline(const TextStyle& style) const;

    /// Truncates with an ellipsis so it fits `maxWidth`.
    std::string elide(std::string_view s, float maxWidth, const TextStyle& style) const;

    // --- composite widgets -------------------------------------------------
    /// A segmented bar gauge. Segments read as discrete notches, which makes a
    /// level legible at a glance without reading the number.
    void gauge(const Rect& r, float fraction, Color fill, Color ground, int segments = 20,
               float danger = -1.0f);

    /// A thin progress bar with no segmentation, for mining progress.
    void progress(const Rect& r, float fraction, Color fill, Color ground);

    /// Draws an icon inside a square of `size` pixels with its top-left at x, y.
    void icon(Icon id, float x, float y, float size, Color c);

    /// A small pill used for tags, deltas and status chips.
    void chip(const Rect& r, std::string_view label, Color fg, Color bg, const TextStyle& style);

private:
    void glyph_runs(u8 bits, int width, float x, float y, float px_, Color c);

    SDL_Renderer* mRenderer = nullptr;
    float mScale = 1.0f;
};

} // namespace df
