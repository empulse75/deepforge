// Thin immediate-mode drawing helpers over SDL_Renderer.
//
// Presentation observes simulation state; it never owns it (Section 15.1).
#pragma once

#include "core/Math.hpp"
#include "core/Types.hpp"

#include <string>
#include <string_view>

struct SDL_Renderer;

namespace df {

struct Color {
    u8 r = 255, g = 255, b = 255, a = 255;

    constexpr Color() = default;

    constexpr Color(u8 ir, u8 ig, u8 ib, u8 ia = 255) : r(ir), g(ig), b(ib), a(ia) {}

    constexpr Color with_alpha(u8 na) const { return Color{r, g, b, na}; }

    constexpr Color scaled(float f) const {
        return Color{static_cast<u8>(clamp(static_cast<float>(r) * f, 0.0f, 255.0f)),
                     static_cast<u8>(clamp(static_cast<float>(g) * f, 0.0f, 255.0f)),
                     static_cast<u8>(clamp(static_cast<float>(b) * f, 0.0f, 255.0f)), a};
    }
};

namespace palette {
inline constexpr Color kBackground{14, 14, 20};
inline constexpr Color kPanel{26, 28, 38, 235};
inline constexpr Color kPanelEdge{72, 80, 102};
inline constexpr Color kText{226, 230, 240};
inline constexpr Color kTextDim{150, 158, 176};
inline constexpr Color kAccent{120, 200, 230};
inline constexpr Color kGood{130, 214, 140};
inline constexpr Color kWarn{242, 196, 96};
inline constexpr Color kDanger{238, 106, 96};
inline constexpr Color kFuel{126, 196, 226};
inline constexpr Color kHull{160, 200, 150};
inline constexpr Color kHeat{240, 140, 80};
inline constexpr Color kCargo{206, 186, 120};
} // namespace palette

enum class TextAlign { Left, Center, Right };

/// Wraps a renderer with the primitives the UI needs. Deliberately small: the
/// shipping UI is our own, ImGui is developer-only.
class Draw {
public:
    explicit Draw(SDL_Renderer* renderer) : mRenderer(renderer) {}

    SDL_Renderer* renderer() const noexcept { return mRenderer; }

    void set_scale(float s) noexcept { mScale = s; }

    float scale() const noexcept { return mScale; }

    void clear(Color c);
    void rect(float x, float y, float w, float h, Color c);
    void rect_outline(float x, float y, float w, float h, Color c, float thickness = 1.0f);
    void line(float x0, float y0, float x1, float y1, Color c);
    void panel(float x, float y, float w, float h, Color fill = palette::kPanel,
               Color edge = palette::kPanelEdge);

    /// Draws text with the built-in 5x7 font. `size` is the pixel height of a
    /// glyph row; the font scales in integer steps for crispness.
    float text(std::string_view s, float x, float y, Color c, float size = 14.0f,
               TextAlign align = TextAlign::Left);

    float text_width(std::string_view s, float size = 14.0f) const;

    void bar(float x, float y, float w, float h, float fraction, Color fill, Color back,
             bool showTicks = false);

private:
    SDL_Renderer* mRenderer = nullptr;
    float mScale = 1.0f;
};

} // namespace df
