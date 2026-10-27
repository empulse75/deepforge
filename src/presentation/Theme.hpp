// Design tokens for the interface.
//
// The art direction is an industrial instrument panel: machined slate housings,
// inset readouts, an amber primary accent for anything the player acts on and a
// cyan secondary for anything the instruments report. Colour is never the only
// carrier of meaning - every state also has a shape, an icon or a label
// (Section 12.2).
//
// Everything here is a token. Screens compose tokens; they do not invent
// colours or spacings of their own.
#pragma once

#include "core/Math.hpp"
#include "core/Types.hpp"

namespace df {

struct Color {
    u8 r = 255, g = 255, b = 255, a = 255;

    constexpr Color() = default;

    constexpr Color(u8 ir, u8 ig, u8 ib, u8 ia = 255) : r(ir), g(ig), b(ib), a(ia) {}

    constexpr Color with_alpha(u8 na) const { return Color{r, g, b, na}; }

    constexpr Color with_alpha_f(float f) const {
        return Color{r, g, b, static_cast<u8>(clamp(f, 0.0f, 1.0f) * 255.0f)};
    }

    /// Multiplies brightness, keeping alpha. Used for bevels and shading.
    constexpr Color shade(float f) const {
        return Color{static_cast<u8>(clamp(static_cast<float>(r) * f, 0.0f, 255.0f)),
                     static_cast<u8>(clamp(static_cast<float>(g) * f, 0.0f, 255.0f)),
                     static_cast<u8>(clamp(static_cast<float>(b) * f, 0.0f, 255.0f)), a};
    }

    friend constexpr bool operator==(const Color&, const Color&) = default;
};

constexpr Color mix(Color a, Color b, float t) {
    const float u = clamp(t, 0.0f, 1.0f);
    return Color{
        static_cast<u8>(static_cast<float>(a.r) + (static_cast<float>(b.r) - static_cast<float>(a.r)) * u),
        static_cast<u8>(static_cast<float>(a.g) + (static_cast<float>(b.g) - static_cast<float>(a.g)) * u),
        static_cast<u8>(static_cast<float>(a.b) + (static_cast<float>(b.b) - static_cast<float>(a.b)) * u),
        static_cast<u8>(static_cast<float>(a.a) + (static_cast<float>(b.a) - static_cast<float>(a.a)) * u)};
}

namespace theme {

// --- grounds ---------------------------------------------------------------
inline constexpr Color kVoid{8, 9, 13};          ///< Behind everything.
inline constexpr Color kHousing{20, 22, 28};     ///< Main panel body.
inline constexpr Color kHousingHigh{27, 30, 38}; ///< Raised panel body.
inline constexpr Color kWell{12, 13, 18};        ///< Inset well / readout ground.
inline constexpr Color kWellDeep{9, 10, 14};

// --- machined edges --------------------------------------------------------
inline constexpr Color kBevelLight{54, 61, 76}; ///< Top and left of a raised edge.
inline constexpr Color kBevelDark{4, 5, 8};     ///< Bottom and right.
inline constexpr Color kRule{40, 45, 57};       ///< Dividers and hairlines.
inline constexpr Color kRuleSoft{30, 34, 43};

// --- text ------------------------------------------------------------------
// Values are the first on their hue that clear WCAG AA on every ground each
// token is drawn on (A11Y-03; the arithmetic is a test — the audit's spreadsheet
// is not the enforcement). kTextDisabled answers to the 3.0 bar, as WCAG
// applies it to inactive components.
inline constexpr Color kText{216, 222, 233};
inline constexpr Color kTextMuted{140, 151, 170};
inline constexpr Color kTextFaint{124, 134, 152};
inline constexpr Color kTextDisabled{96, 104, 120};

// --- accents ---------------------------------------------------------------
inline constexpr Color kAmber{240, 168, 60}; ///< Primary: focus, action, selection.
inline constexpr Color kAmberBright{255, 200, 116};
inline constexpr Color kAmberDeep{120, 78, 22};
inline constexpr Color kCyan{88, 199, 224}; ///< Secondary: instrument readings.
inline constexpr Color kCyanDeep{28, 78, 94};

// --- semantic --------------------------------------------------------------
inline constexpr Color kGood{124, 196, 106};
inline constexpr Color kWarn{232, 163, 61};
inline constexpr Color kDanger{224, 91, 76};
inline constexpr Color kDangerDeep{92, 30, 26};

// --- gauges ----------------------------------------------------------------
inline constexpr Color kFuel{88, 199, 224};
inline constexpr Color kHull{124, 196, 106};
inline constexpr Color kHeat{236, 135, 60};
inline constexpr Color kCargo{206, 176, 108};

/// Colour-vision-friendly gauge set: separates by blue/yellow/white rather than
/// red/green, which is the axis most affected by common colour blindness.
inline constexpr Color kFuelCB{110, 178, 240};
inline constexpr Color kHullCB{236, 236, 236};
inline constexpr Color kHeatCB{244, 190, 60};
inline constexpr Color kCargoCB{176, 150, 230};

// --- spacing scale (logical px at 1x, multiplied by the UI scale) ----------
inline constexpr float kSpace1 = 2.0f;
inline constexpr float kSpace2 = 4.0f;
inline constexpr float kSpace3 = 7.0f;
inline constexpr float kSpace4 = 11.0f;
inline constexpr float kSpace5 = 16.0f;
inline constexpr float kSpace6 = 24.0f;
inline constexpr float kSpace7 = 36.0f;

inline constexpr float kRowHeight = 27.0f; ///< List row, before UI scale.
inline constexpr float kHeaderHeight = 18.0f;
inline constexpr float kNotch = 6.0f; ///< Machined corner mark length.

/// Text scale (A11Y-01). The stored setting stays continuous (the file is
/// user-editable), but the applied factor snaps to these steps so every glyph
/// cell lands on the integer grid (ADR 6) and the bitmap faces stay whole.
/// Nearest step; exact ties resolve downward.
inline constexpr float kTextScaleSteps[] = {1.0f, 1.5f, 2.0f};

constexpr float snap_text_scale(float v) {
    float best = kTextScaleSteps[0];
    for (const float s : kTextScaleSteps) {
        if (std::fabs(s - v) < std::fabs(best - v) || (std::fabs(s - v) == std::fabs(best - v) && s < best)) {
            best = s;
        }
    }
    return best;
}

/// The keyboard/gamepad step on the text-size slider: the nearest step in the
/// pressed direction from the snapped position, clamped to the step set (a
/// tenth-of-track nudge could never leave the first step once snapping is in
/// force).
constexpr float next_text_scale_step(float current, int dir) {
    const float base = snap_text_scale(current);
    if (dir == 0) {
        return base;
    }
    float best = base;
    bool found = false;
    for (const float s : kTextScaleSteps) {
        if (!((dir > 0 && s > base) || (dir < 0 && s < base))) {
            continue;
        }
        if (!found || std::fabs(s - base) < std::fabs(best - base)) {
            best = s;
            found = true;
        }
    }
    return best;
}

/// The most-used style must not be the smallest text in the game (A11Y-02):
/// the Micro face's effective scale never falls below this.
inline constexpr int kCaptionMinScale = 2;

/// Motion. Every duration here is also gated by the reduce-motion setting.
inline constexpr float kHoverRate = 16.0f; ///< Exponential approach rate.
inline constexpr float kFocusRate = 22.0f;
inline constexpr float kPanelRate = 14.0f;
inline constexpr float kGaugeRate = 9.0f;

} // namespace theme

/// Gauge colours resolved against the colour-vision setting.
struct GaugePalette {
    Color fuel = theme::kFuel;
    Color hull = theme::kHull;
    Color heat = theme::kHeat;
    Color cargo = theme::kCargo;

    static GaugePalette resolve(bool colorBlind) {
        GaugePalette p;
        if (colorBlind) {
            p.fuel = theme::kFuelCB;
            p.hull = theme::kHullCB;
            p.heat = theme::kHeatCB;
            p.cargo = theme::kCargoCB;
        }
        return p;
    }
};

} // namespace df
