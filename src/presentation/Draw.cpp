#include "presentation/Draw.hpp"

#include "presentation/FontData.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace df {
namespace {

struct FaceInfo {
    const u8* rows;
    const u8* advance;
    int width;
    int height;
    int stride;
    int baseline;
};

/// Bold is a separate generated table rather than a runtime double-strike: at
/// this size a mechanical smear closes the one-pixel counters in 'W' and 'm',
/// so the generator widens each glyph and hand-corrects the few it spoils.
FaceInfo face_info(Face f, bool bold) {
    if (f == Face::Micro) {
        return FaceInfo{bold ? &kMicroBoldRows[0][0] : &kMicroRows[0][0],
                        bold ? kMicroBoldAdvance : kMicroAdvance,
                        kMicroWidth,
                        kMicroHeight,
                        kMicroHeight,
                        kMicroHeight - 1};
    }
    return FaceInfo{bold ? &kUiBoldRows[0][0] : &kUiRows[0][0],
                    bold ? kUiBoldAdvance : kUiAdvance,
                    kUiWidth,
                    kUiHeight,
                    kUiHeight,
                    kUiBaseline};
}

int glyph_index(char ch) {
    const int code = static_cast<unsigned char>(ch);
    if (code < 32 || code > 126) {
        return 0;
    }
    return code - 32;
}

/// Widest digit advance, so tabular text lines up in columns.
int digit_advance(const FaceInfo& f) {
    int widest = 0;
    for (char d = '0'; d <= '9'; ++d) {
        widest = std::max<int>(widest, f.advance[glyph_index(d)]);
    }
    return widest;
}

} // namespace

// ---------------------------------------------------------------------------
// Primitives
// ---------------------------------------------------------------------------

void Draw::clear(Color c) {
    SDL_SetRenderDrawColor(mRenderer, c.r, c.g, c.b, 255);
    SDL_RenderClear(mRenderer);
}

void Draw::rect(float x, float y, float w, float h, Color c) {
    if (w <= 0.0f || h <= 0.0f || c.a == 0) {
        return;
    }
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(mRenderer, c.r, c.g, c.b, c.a);
    const SDL_FRect r{x, y, w, h};
    SDL_RenderFillRect(mRenderer, &r);
}

void Draw::hline(float x, float y, float w, Color c, float thickness) {
    rect(x, y, w, std::max(1.0f, thickness), c);
}

void Draw::vline(float x, float y, float h, Color c, float thickness) {
    rect(x, y, std::max(1.0f, thickness), h, c);
}

void Draw::outline(const Rect& r, Color c, float thickness) {
    const float t = std::max(1.0f, thickness);
    rect(r.x, r.y, r.w, t, c);
    rect(r.x, r.y + r.h - t, r.w, t, c);
    rect(r.x, r.y, t, r.h, c);
    rect(r.x + r.w - t, r.y, t, r.h, c);
}

void Draw::line(float x0, float y0, float x1, float y1, Color c) {
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(mRenderer, c.r, c.g, c.b, c.a);
    SDL_RenderLine(mRenderer, x0, y0, x1, y1);
}

void Draw::vgradient(const Rect& r, Color top, Color bottom, int bands) {
    if (bands < 1) {
        bands = 1;
    }
    const float bandH = r.h / static_cast<float>(bands);
    for (int i = 0; i < bands; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(std::max(1, bands - 1));
        rect(r.x, r.y + static_cast<float>(i) * bandH, r.w, std::ceil(bandH) + 0.5f, mix(top, bottom, t));
    }
}

void Draw::bevel(const Rect& r, bool inset, float thickness) {
    const float t = std::max(1.0f, thickness);
    const Color hi = inset ? theme::kBevelDark : theme::kBevelLight;
    const Color lo = inset ? theme::kBevelLight.shade(0.75f) : theme::kBevelDark;
    rect(r.x, r.y, r.w, t, hi);
    rect(r.x, r.y, t, r.h, hi);
    rect(r.x, r.y + r.h - t, r.w, t, lo);
    rect(r.x + r.w - t, r.y, t, r.h, lo);
}

void Draw::corner_notches(const Rect& r, Color c, float length) {
    const float t = std::max(1.0f, std::round(mScale));
    const float l = std::max(length, t * 2.0f);
    // Four L-shaped marks, inset slightly so they read as machined detail
    // rather than as a second border.
    const float in = t;
    rect(r.x + in, r.y + in, l, t, c);
    rect(r.x + in, r.y + in, t, l, c);
    rect(r.x + r.w - in - l, r.y + in, l, t, c);
    rect(r.x + r.w - in - t, r.y + in, t, l, c);
    rect(r.x + in, r.y + r.h - in - t, l, t, c);
    rect(r.x + in, r.y + r.h - in - l, t, l, c);
    rect(r.x + r.w - in - l, r.y + r.h - in - t, l, t, c);
    rect(r.x + r.w - in - t, r.y + r.h - in - l, t, l, c);
}

void Draw::brushed(const Rect& r, Color c, float spacing) {
    const float step = std::max(2.0f, px(spacing));
    for (float y = r.y + step; y < r.y + r.h - 1.0f; y += step) {
        rect(r.x + 1.0f, y, r.w - 2.0f, 1.0f, c);
    }
}

void Draw::panel(const Rect& r, Surface surface, bool notches) {
    const float t = std::max(1.0f, std::round(mScale));
    switch (surface) {
        case Surface::Raised:
            vgradient(r, theme::kHousingHigh, theme::kHousing, 8);
            brushed(r, Color{255, 255, 255, 5});
            bevel(r, false, t);
            break;
        case Surface::Sunken:
            vgradient(r, theme::kWellDeep, theme::kWell, 6);
            bevel(r, true, t);
            break;
        case Surface::Flat:
            rect(r, theme::kHousing);
            outline(r, theme::kRuleSoft, t);
            break;
        case Surface::Glass:
            rect(r, Color{14, 16, 21, 236});
            bevel(r, false, t);
            break;
    }
    if (notches && surface != Surface::Flat) {
        corner_notches(r, theme::kBevelLight.with_alpha(surface == Surface::Sunken ? 90 : 150),
                       px(theme::kNotch));
    }
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

float Draw::glyph_px(const TextStyle& style) const {
    int styleScale = style.scale;
    if (style.face == Face::Micro) {
        styleScale = std::max(styleScale, theme::kCaptionMinScale);
    }
    const float ui = std::max(1.0f, std::round(mScale));
    return std::round(ui * static_cast<float>(styleScale) * theme::snap_text_scale(mTextScale));
}

float Draw::line_height(const TextStyle& style) const {
    const FaceInfo f = face_info(style.face, style.bold);
    return std::round(static_cast<float>(f.height) * glyph_px(style));
}

float Draw::baseline(const TextStyle& style) const {
    const FaceInfo f = face_info(style.face, style.bold);
    return std::round(static_cast<float>(f.baseline) * glyph_px(style));
}

float Draw::measure(std::string_view s, const TextStyle& style) const {
    const FaceInfo f = face_info(style.face, style.bold);
    const float p = glyph_px(style);
    const int tab = style.tabular ? digit_advance(f) : 0;
    float total = 0.0f;
    for (const char ch : s) {
        const int gi = glyph_index(ch);
        int adv = f.advance[gi];
        if (style.tabular && ch >= '0' && ch <= '9') {
            adv = tab;
        }
        total += static_cast<float>(adv) * p + style.tracking * p;
    }
    return total > 0.0f ? total - style.tracking * p : 0.0f;
}

std::string Draw::elide(std::string_view s, float maxWidth, const TextStyle& style) const {
    if (measure(s, style) <= maxWidth) {
        return std::string{s};
    }
    std::string out{s};
    while (!out.empty() && measure(out + "…", style) > maxWidth) {
        out.pop_back();
    }
    // The face has no ellipsis glyph, so use a period run.
    while (!out.empty() && measure(out + "...", style) > maxWidth) {
        out.pop_back();
    }
    return out.empty() ? std::string{} : out + "...";
}

void Draw::glyph_runs(u8 bits, int width, float x, float y, float p, Color c) {
    if (bits == 0) {
        return;
    }
    int runStart = -1;
    for (int col = 0; col <= width; ++col) {
        const bool on = col < width && (bits & (1u << (width - 1 - col))) != 0;
        if (on && runStart < 0) {
            runStart = col;
        } else if (!on && runStart >= 0) {
            // Contiguous horizontal runs become one rectangle, which is far
            // fewer draw calls than one per pixel.
            const SDL_FRect r{x + static_cast<float>(runStart) * p, y, static_cast<float>(col - runStart) * p,
                              p};
            SDL_SetRenderDrawColor(mRenderer, c.r, c.g, c.b, c.a);
            SDL_RenderFillRect(mRenderer, &r);
            runStart = -1;
        }
    }
}

float Draw::text(std::string_view s, float x, float y, Color c, const TextStyle& style, TextAlign align) {
    if (s.empty() || c.a == 0) {
        return 0.0f;
    }
    const FaceInfo f = face_info(style.face, style.bold);
    const float p = glyph_px(style);
    const float width = measure(s, style);
    float cursor = x;
    if (align == TextAlign::Center) {
        cursor = std::round(x - width * 0.5f);
    } else if (align == TextAlign::Right) {
        cursor = std::round(x - width);
    }
    const float top = std::round(y);

    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_BLEND);
    const int tab = style.tabular ? digit_advance(f) : 0;
    for (const char ch : s) {
        const int gi = glyph_index(ch);
        const u8* rows = f.rows + static_cast<usize>(gi) * static_cast<usize>(f.stride);
        for (int row = 0; row < f.height; ++row) {
            const float ry = top + static_cast<float>(row) * p;
            glyph_runs(rows[row], f.width, cursor, ry, p, c);
        }
        int adv = f.advance[gi];
        if (style.tabular && ch >= '0' && ch <= '9') {
            adv = tab;
        }
        cursor += static_cast<float>(adv) * p + style.tracking * p;
    }
    return width;
}

// ---------------------------------------------------------------------------
// Composite widgets
// ---------------------------------------------------------------------------

void Draw::gauge(const Rect& r, float fraction, Color fill, Color ground, int segments, float danger) {
    panel(r, Surface::Sunken, false);
    const float t = std::max(1.0f, std::round(mScale));
    const Rect inner{r.x + t, r.y + t, r.w - t * 2.0f, r.h - t * 2.0f};
    rect(inner, ground);

    if (segments < 1) {
        segments = 1;
    }
    const float gap = std::max(1.0f, std::round(mScale * 0.5f));
    const float segW = (inner.w - gap * static_cast<float>(segments - 1)) / static_cast<float>(segments);
    const float f = clamp(fraction, 0.0f, 1.0f);
    const float lit = f * static_cast<float>(segments);

    for (int i = 0; i < segments; ++i) {
        const float x = inner.x + static_cast<float>(i) * (segW + gap);
        const float coverage = clamp(lit - static_cast<float>(i), 0.0f, 1.0f);
        if (coverage <= 0.0f) {
            // Unlit segments keep a faint ghost so the scale is always readable.
            rect(x, inner.y, segW, inner.h, fill.with_alpha(20));
            continue;
        }
        // Segments brighten toward the leading edge, which gives the gauge a
        // direction to read.
        const float tt = static_cast<float>(i) / static_cast<float>(std::max(1, segments - 1));
        Color c = mix(fill.shade(0.78f), fill, tt);
        if (coverage < 1.0f) {
            c = c.with_alpha_f(0.35f + 0.65f * coverage);
        }
        rect(x, inner.y, segW, inner.h, c);
        rect(x, inner.y, segW, std::max(1.0f, inner.h * 0.22f), c.shade(1.35f).with_alpha(120));
    }

    if (danger >= 0.0f && danger <= 1.0f) {
        // A tick marks the level where this meter starts to hurt, so the
        // threshold is visible without watching the number.
        const float x = inner.x + inner.w * danger;
        rect(x - t * 0.5f, r.y - t, t, r.h + t * 2.0f, theme::kDanger.with_alpha(200));
    }
}

void Draw::progress(const Rect& r, float fraction, Color fill, Color ground) {
    rect(r, ground);
    const float f = clamp(fraction, 0.0f, 1.0f);
    rect(r.x, r.y, r.w * f, r.h, fill);
    if (f > 0.02f) {
        rect(r.x + r.w * f - std::max(1.0f, mScale), r.y, std::max(1.0f, mScale), r.h, fill.shade(1.6f));
    }
    outline(r, theme::kRuleSoft, std::max(1.0f, std::round(mScale)));
}

void Draw::chip(const Rect& r, std::string_view label, Color fg, Color bg, const TextStyle& style) {
    rect(r, bg);
    outline(r, fg.with_alpha(90), std::max(1.0f, std::round(mScale)));
    text(label, r.x + r.w * 0.5f, r.y + (r.h - line_height(style)) * 0.5f, fg, style, TextAlign::Center);
}

// ---------------------------------------------------------------------------
// Icons
// ---------------------------------------------------------------------------

void Draw::icon(Icon id, float x, float y, float size, Color c) {
    // Icons are laid out on a 12x12 grid and drawn from rectangles, so they stay
    // crisp at any UI scale and need no image assets.
    const float u = size / 12.0f;
    auto b = [&](float gx, float gy, float gw, float gh) {
        rect(std::round(x + gx * u), std::round(y + gy * u), std::max(1.0f, std::round(gw * u)),
             std::max(1.0f, std::round(gh * u)), c);
    };
    auto frame = [&](float gx, float gy, float gw, float gh, float t) {
        b(gx, gy, gw, t);
        b(gx, gy + gh - t, gw, t);
        b(gx, gy, t, gh);
        b(gx + gw - t, gy, t, gh);
    };

    switch (id) {
        case Icon::Fuel: // canister with a filled level
            frame(2, 1, 8, 10, 1);
            b(3, 6, 6, 4);
            b(4, 0, 4, 1);
            break;
        case Icon::Hull: // shield
            b(2, 1, 8, 1);
            b(2, 2, 8, 4);
            b(3, 6, 6, 2);
            b(4, 8, 4, 2);
            b(5, 10, 2, 1);
            break;
        case Icon::Heat: // rising flame
            b(5, 1, 2, 3);
            b(4, 3, 4, 3);
            b(3, 5, 6, 4);
            b(4, 9, 4, 2);
            break;
        case Icon::Cargo: // crate with cross banding
            frame(1, 2, 10, 8, 1);
            b(1, 5, 10, 1);
            b(5, 2, 2, 8);
            break;
        case Icon::Credit: // coin with a bar
            frame(2, 2, 8, 8, 1);
            b(4, 5, 4, 2);
            break;
        case Icon::Insight: // faceted gem
            b(5, 1, 2, 1);
            b(3, 2, 6, 2);
            b(1, 4, 10, 2);
            b(3, 6, 6, 2);
            b(5, 8, 2, 2);
            break;
        case Icon::Depth: // downward arrow with rule
            b(5, 1, 2, 6);
            b(3, 6, 6, 1);
            b(4, 7, 4, 1);
            b(5, 8, 2, 1);
            b(2, 10, 8, 1);
            break;
        case Icon::Drill: // bit and shank
            b(5, 1, 2, 5);
            b(3, 6, 6, 1);
            b(4, 7, 4, 2);
            b(5, 9, 2, 2);
            break;
        case Icon::Garage: // rig silhouette under an arch
            b(1, 3, 10, 1);
            b(1, 3, 1, 7);
            b(10, 3, 1, 7);
            b(3, 6, 6, 3);
            b(4, 9, 1, 2);
            b(7, 9, 1, 2);
            break;
        case Icon::Module: // chip with legs
            frame(3, 3, 6, 6, 1);
            b(5, 5, 2, 2);
            b(1, 4, 2, 1);
            b(1, 7, 2, 1);
            b(9, 4, 2, 1);
            b(9, 7, 2, 1);
            break;
        case Icon::Skill: // node tree
            b(5, 1, 2, 2);
            b(6, 3, 1, 3);
            b(2, 6, 9, 1);
            b(2, 6, 1, 3);
            b(9, 6, 1, 3);
            b(1, 9, 3, 2);
            b(8, 9, 3, 2);
            break;
        case Icon::Research: // flask
            b(4, 1, 4, 1);
            b(5, 2, 1, 3);
            b(6, 2, 1, 3);
            b(3, 5, 6, 2);
            b(2, 7, 8, 4);
            break;
        case Icon::Refinery: // furnace with a stack
            b(2, 4, 8, 7);
            b(4, 1, 2, 3);
            b(4, 6, 4, 3);
            break;
        case Icon::Contract: // document with lines
            frame(2, 1, 8, 10, 1);
            b(4, 3, 4, 1);
            b(4, 5, 4, 1);
            b(4, 7, 3, 1);
            break;
        case Icon::Facility: // buildings
            b(1, 5, 4, 6);
            b(6, 2, 5, 9);
            b(2, 7, 2, 1);
            b(7, 4, 3, 1);
            b(7, 7, 3, 1);
            break;
        case Icon::Codex: // open book
            b(1, 2, 5, 8);
            b(6, 2, 5, 8);
            b(5, 1, 2, 10);
            break;
        case Icon::Map: // folded map
            b(1, 2, 3, 8);
            b(4, 3, 4, 8);
            b(8, 2, 3, 8);
            break;
        case Icon::Save: // disk
            frame(1, 1, 10, 10, 1);
            b(4, 2, 4, 3);
            b(3, 7, 6, 3);
            break;
        case Icon::Settings: // slider bank
            b(1, 2, 10, 1);
            b(3, 1, 2, 3);
            b(1, 6, 10, 1);
            b(7, 5, 2, 3);
            b(1, 10, 10, 1);
            b(4, 9, 2, 3);
            break;
        case Icon::Controls: // gamepad
            b(1, 4, 10, 5);
            b(2, 5, 3, 1);
            b(3, 4, 1, 3);
            b(8, 5, 2, 2);
            break;
        case Icon::Lock:
            b(3, 1, 6, 1);
            b(3, 2, 1, 3);
            b(8, 2, 1, 3);
            b(2, 5, 8, 6);
            break;
        case Icon::Check:
            b(1, 6, 2, 2);
            b(3, 8, 2, 2);
            b(5, 6, 2, 2);
            b(7, 4, 2, 2);
            b(9, 2, 2, 2);
            break;
        case Icon::Warning:
            b(5, 1, 2, 2);
            b(4, 3, 4, 2);
            b(3, 5, 6, 2);
            b(2, 7, 8, 2);
            b(5, 10, 2, 1);
            break;
        case Icon::Chevron:
            b(4, 2, 2, 2);
            b(6, 4, 2, 2);
            b(6, 6, 2, 2);
            b(4, 8, 2, 2);
            break;
        case Icon::ChevronDown:
            b(2, 4, 2, 2);
            b(4, 6, 2, 2);
            b(6, 6, 2, 2);
            b(8, 4, 2, 2);
            break;
        case Icon::Descend: // arrow into a shaft
            b(2, 1, 8, 1);
            b(5, 2, 2, 5);
            b(3, 6, 6, 1);
            b(4, 7, 4, 1);
            b(5, 8, 2, 2);
            break;
        case Icon::Lift: // platform on cables
            b(2, 1, 1, 6);
            b(9, 1, 1, 6);
            b(1, 7, 10, 2);
            b(4, 9, 4, 1);
            break;
        case Icon::Scanner: // pulse rings
            b(5, 5, 2, 2);
            frame(3, 3, 6, 6, 1);
            b(1, 1, 2, 1);
            b(1, 1, 1, 2);
            b(9, 1, 2, 1);
            b(10, 1, 1, 2);
            b(1, 10, 2, 1);
            b(1, 9, 1, 2);
            b(9, 10, 2, 1);
            b(10, 9, 1, 2);
            break;
        case Icon::Landmark: // marker pin
            b(4, 1, 4, 1);
            b(3, 2, 6, 4);
            b(4, 6, 4, 1);
            b(5, 7, 2, 4);
            break;
        case Icon::Star:
            b(5, 1, 2, 10);
            b(1, 5, 10, 2);
            b(3, 3, 2, 2);
            b(7, 3, 2, 2);
            b(3, 7, 2, 2);
            b(7, 7, 2, 2);
            break;
        case Icon::Cross:
            b(2, 2, 2, 2);
            b(4, 4, 2, 2);
            b(6, 6, 2, 2);
            b(8, 8, 2, 2);
            b(8, 2, 2, 2);
            b(6, 4, 2, 2);
            b(4, 6, 2, 2);
            b(2, 8, 2, 2);
            break;
        case Icon::Plus:
            b(5, 2, 2, 8);
            b(2, 5, 8, 2);
            break;
        case Icon::Minus:
            b(2, 5, 8, 2);
            break;
        case Icon::None:
        case Icon::Count:
            break;
    }
}

} // namespace df
