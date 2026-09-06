#include "presentation/Draw.hpp"

#include "presentation/FontData.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace df {
namespace {

constexpr float kGlyphAdvance = static_cast<float>(kGlyphWidth) + 1.0f;

}

void Draw::clear(Color c) {
    SDL_SetRenderDrawColor(mRenderer, c.r, c.g, c.b, c.a);
    SDL_RenderClear(mRenderer);
}

void Draw::rect(float x, float y, float w, float h, Color c) {
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(mRenderer, c.r, c.g, c.b, c.a);
    const SDL_FRect r{x, y, w, h};
    SDL_RenderFillRect(mRenderer, &r);
}

void Draw::rect_outline(float x, float y, float w, float h, Color c, float thickness) {
    rect(x, y, w, thickness, c);
    rect(x, y + h - thickness, w, thickness, c);
    rect(x, y, thickness, h, c);
    rect(x + w - thickness, y, thickness, h, c);
}

void Draw::line(float x0, float y0, float x1, float y1, Color c) {
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(mRenderer, c.r, c.g, c.b, c.a);
    SDL_RenderLine(mRenderer, x0, y0, x1, y1);
}

void Draw::panel(float x, float y, float w, float h, Color fill, Color edge) {
    rect(x, y, w, h, fill);
    rect_outline(x, y, w, h, edge, std::max(1.0f, std::round(mScale)));
}

float Draw::text_width(std::string_view s, float size) const {
    const float px = std::max(1.0f, std::round(size * mScale / static_cast<float>(kGlyphHeight)));
    return static_cast<float>(s.size()) * kGlyphAdvance * px;
}

float Draw::text(std::string_view s, float x, float y, Color c, float size, TextAlign align) {
    const float px = std::max(1.0f, std::round(size * mScale / static_cast<float>(kGlyphHeight)));
    const float width = static_cast<float>(s.size()) * kGlyphAdvance * px;
    float cursor = x;
    if (align == TextAlign::Center) {
        cursor = x - width * 0.5f;
    } else if (align == TextAlign::Right) {
        cursor = x - width;
    }
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(mRenderer, c.r, c.g, c.b, c.a);
    for (const char ch : s) {
        const int code = static_cast<unsigned char>(ch);
        if (code >= kGlyphFirst && code < kGlyphFirst + kGlyphCount) {
            const auto& rows = kFontRows[code - kGlyphFirst];
            for (int row = 0; row < kGlyphHeight; ++row) {
                const u8 bits = rows[row];
                if (bits == 0) {
                    continue;
                }
                int runStart = -1;
                for (int col = 0; col <= kGlyphWidth; ++col) {
                    const bool on = col < kGlyphWidth && (bits & (1u << (kGlyphWidth - 1 - col))) != 0;
                    if (on && runStart < 0) {
                        runStart = col;
                    } else if (!on && runStart >= 0) {
                        // Draw contiguous pixel runs as one rect: far fewer calls
                        // than one per pixel.
                        const SDL_FRect r{cursor + static_cast<float>(runStart) * px,
                                          y + static_cast<float>(row) * px,
                                          static_cast<float>(col - runStart) * px, px};
                        SDL_RenderFillRect(mRenderer, &r);
                        runStart = -1;
                    }
                }
            }
        }
        cursor += kGlyphAdvance * px;
    }
    return width;
}

void Draw::bar(float x, float y, float w, float h, float fraction, Color fill, Color back, bool showTicks) {
    rect(x, y, w, h, back);
    const float f = clamp(fraction, 0.0f, 1.0f);
    rect(x, y, w * f, h, fill);
    rect_outline(x, y, w, h, palette::kPanelEdge, std::max(1.0f, std::round(mScale)));
    if (showTicks) {
        // Quarter ticks give a colour-independent read of the level.
        for (int i = 1; i < 4; ++i) {
            const float tx = x + w * (static_cast<float>(i) / 4.0f);
            rect(tx, y, std::max(1.0f, mScale), h, palette::kPanelEdge);
        }
    }
}

} // namespace df
