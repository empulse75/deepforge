// The token machinery, tested as behaviour (TEST-01 seam: theme-token usage —
// "Screens compose tokens; they do not invent colours"). What is pinned here
// is the machinery's contract: the colour-vision swap actually swaps, the mix
// and shade helpers land where the art direction says they land, and the
// spacing scale is the scale the layout arithmetic assumes. Individual token
// values move (the accessibility cards will retoken them); these invariants
// are what the designers lean on when they do.
#include "presentation/Draw.hpp"
#include "presentation/Theme.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace df;

namespace {

// WCAG 2.x relative luminance and contrast ratio, computed over the tokens as
// SPEC:414 mandates ("contrast is checked against Theme.hpp tokens, not
// eyeballed"). Alpha is composited as the code performs it.
double luminance(const Color& c) {
    const auto lin = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin(c.r / 255.0) + 0.7152 * lin(c.g / 255.0) + 0.0722 * lin(c.b / 255.0);
}

double contrast(const Color& a, const Color& b) {
    const double la = luminance(a);
    const double lb = luminance(b);
    const double hi = std::max(la, lb);
    const double lo = std::min(la, lb);
    return (hi + 0.05) / (lo + 0.05);
}

Color over(const Color& src, const Color& dst) {
    const double a = src.a / 255.0;
    return Color{static_cast<u8>(src.r * a + dst.r * (1.0 - a)),
                 static_cast<u8>(src.g * a + dst.g * (1.0 - a)),
                 static_cast<u8>(src.b * a + dst.b * (1.0 - a)), 255};
}

constexpr double kBarNormal = 4.5; ///< AA, normal text.
constexpr double kBarLarge = 3.0;  ///< AA, large text — and the bar WCAG applies to inactive components.

} // namespace

// The pairs the UI audit measured (docs/audit/evidence/contrast-tokens.txt):
// every text token on every ground it is actually drawn on, at the bar that
// governs the pair — 4.5 for normal text, 3.0 for disabled text (WCAG exempts
// inactive UI components from the text criterion; 1.4.11 asks 3.0) and for the
// tiny bitmap chips (the audit's AA-large classification). Caption-on-plate
// and focused-row grounds are the caption card's (P1-G), not listed here.
TEST_CASE("presentation/Theme text tokens clear WCAG AA on the grounds they are drawn on",
          "[presentation][theme][a11y]") {
    // The retoken this card landed (A11Y-03): the first values on the current
    // hue that clear their bars on every ground recorded.
    CHECK(theme::kTextFaint == Color{124, 134, 152});
    CHECK(theme::kTextDisabled == Color{96, 104, 120});

    const Color grounds[] = {theme::kHousing, theme::kHousingHigh, theme::kWell, theme::kVoid};
    for (const Color& g : grounds) {
        CHECK(contrast(theme::kText, g) >= kBarNormal);
        CHECK(contrast(theme::kTextMuted, g) >= kBarNormal);
        CHECK(contrast(theme::kTextFaint, g) >= kBarNormal);
    }

    CHECK(contrast(theme::kTextDisabled, theme::kHousing) >= kBarLarge);
    CHECK(contrast(theme::kTextDisabled, theme::kVoid) >= kBarLarge);

    CHECK(contrast(theme::kAmber, theme::kHousing) >= kBarNormal);
    CHECK(contrast(theme::kAmber, theme::kVoid) >= kBarNormal);
    CHECK(contrast(theme::kAmberBright, theme::kHousing) >= kBarNormal);
    CHECK(contrast(theme::kCyan, theme::kWell) >= kBarNormal);
    CHECK(contrast(theme::kCyan, theme::kHousing) >= kBarNormal);
    CHECK(contrast(theme::kCyan, theme::kVoid) >= kBarNormal);
    CHECK(contrast(theme::kGood, theme::kHousing) >= kBarNormal);
    CHECK(contrast(theme::kGood, theme::kWell) >= kBarNormal);
    CHECK(contrast(theme::kWarn, over(theme::kDangerDeep.with_alpha(110), theme::kHousing)) >=
          kBarNormal); // inspector note box
    CHECK(contrast(theme::kWarn, over(Color{14, 16, 21, 236}, theme::kVoid)) >= kBarNormal); // toast plate
    CHECK(contrast(theme::kDanger, theme::kHousing) >= kBarNormal);
    CHECK(contrast(theme::kDanger, theme::kWell) >= kBarNormal);

    // Chip labels: full token over the tinted chip on the raised panel body.
    const Color raised = theme::kHousingHigh;
    CHECK(contrast(theme::kCyan, over(theme::kCyan.with_alpha(34), raised)) >= kBarLarge);
    CHECK(contrast(theme::kAmber, over(theme::kAmber.with_alpha(34), raised)) >= kBarLarge);
    CHECK(contrast(theme::kGood, over(theme::kGood.with_alpha(34), raised)) >= kBarLarge);
    CHECK(contrast(theme::kDanger, over(theme::kDanger.with_alpha(34), raised)) >= kBarLarge);
}

// A11Y-04 (P1-G): the caption pairs the accessibility card owns. Caption text is
// now drawn in kText on an opaque plate (the plates went opaque so the ground is
// the tokens themselves, not a blend of the world showing through); the hairline
// is the kRule token so the plate separates from bright grounds. Same arithmetic
// as the test above - SPEC:414.
TEST_CASE("presentation/Theme captions clear AA on their opaque plates", "[presentation][theme][a11y]") {
    CHECK(theme::kPlateEdge == theme::kRule);        // the hairline is the machined-edge token
    CHECK(theme::kPlateOpaque == Color{10, 12, 17}); // the in-world plate ground, now opaque
    CHECK(theme::kPlateOpaque.a == 255);

    CHECK(contrast(theme::kText, theme::kPlateOpaque) >= kBarNormal); // in-world captions
    CHECK(contrast(theme::kText, theme::kHousingHigh) >= kBarNormal); // menu-plate captions

    // Separation from bright backgrounds (WCAG 1.4.11's 3.0 bar for non-text):
    // the audit's 1.85:1 reading sat over the brightest frequent material,
    // argentine; the opaque plate and its hairline both clear it.
    const Color brightGround{196, 200, 208};
    CHECK(contrast(theme::kPlateOpaque, brightGround) >= kBarLarge);
    CHECK(contrast(theme::kPlateEdge, brightGround) >= kBarLarge);
    CHECK(contrast(theme::kHousingHigh, brightGround) >= kBarLarge); // menu plate reads against it too
}

// A11Y-01: the text-size setting is honoured through the one text-metric path,
// snapped to whole steps so bitmap glyph cells stay whole (ADR 6's integer
// grid). The stored value stays continuous (the settings file is user-editable);
// the applied factor snaps, and the keyboard steps between steps.
TEST_CASE("presentation/Theme the text-size slider snaps to whole steps", "[presentation][theme][a11y]") {
    static_assert(theme::snap_text_scale(0.7f) == 1.0f);
    static_assert(theme::snap_text_scale(1.0f) == 1.0f);
    static_assert(theme::snap_text_scale(1.24f) == 1.0f);
    static_assert(theme::snap_text_scale(1.26f) == 1.5f);
    static_assert(theme::snap_text_scale(1.5f) == 1.5f);
    static_assert(theme::snap_text_scale(1.75f) == 1.5f); // exact ties resolve downward
    static_assert(theme::snap_text_scale(1.8f) == 2.0f);
    static_assert(theme::snap_text_scale(2.0f) == 2.0f);
    static_assert(theme::snap_text_scale(0.0f) == 1.0f); // out of range clamps to the nearest step
    static_assert(theme::snap_text_scale(3.0f) == 2.0f);

    static_assert(theme::next_text_scale_step(1.0f, +1) == 1.5f);
    static_assert(theme::next_text_scale_step(1.2f, +1) == 1.5f);
    static_assert(theme::next_text_scale_step(1.5f, +1) == 2.0f);
    static_assert(theme::next_text_scale_step(2.0f, +1) == 2.0f); // already at the top
    static_assert(theme::next_text_scale_step(1.5f, -1) == 1.0f);
    static_assert(theme::next_text_scale_step(1.0f, -1) == 1.0f); // already at the bottom
    static_assert(theme::next_text_scale_step(2.0f, -1) == 1.5f);
    static_assert(theme::next_text_scale_step(1.5f, 0) == 1.5f);
}

// A11Y-02: the most-used style must not be the smallest text in the game.
// The floor lives in the single metric path, so pin the constants that feed it.
TEST_CASE("presentation/Theme the Micro face honours the caption floor", "[presentation][theme][a11y]") {
    CHECK(theme::kCaptionMinScale >= 2);
    // The floor must bite on the styles that use the Micro face, or it is decoration.
    CHECK(text_style::kCaption.scale < theme::kCaptionMinScale);
    CHECK(text_style::kCaptionNum.scale < theme::kCaptionMinScale);
    CHECK(text_style::kCaption.face == Face::Micro);
    CHECK(text_style::kCaptionNum.face == Face::Micro);
}

TEST_CASE("presentation/Theme the gauge palette resolves per the colour-vision setting",
          "[presentation][theme]") {
    const auto plain = GaugePalette::resolve(false);
    CHECK(plain.fuel == theme::kFuel);
    CHECK(plain.hull == theme::kHull);
    CHECK(plain.heat == theme::kHeat);
    CHECK(plain.cargo == theme::kCargo);

    const auto cb = GaugePalette::resolve(true);
    CHECK(cb.fuel == theme::kFuelCB);
    CHECK(cb.hull == theme::kHullCB);
    CHECK(cb.heat == theme::kHeatCB);
    CHECK(cb.cargo == theme::kCargoCB);

    // The colour-blind set must separate every gauge from its default, or the
    // setting does nothing on the axis it exists for.
    CHECK(cb.fuel != plain.fuel);
    CHECK(cb.hull != plain.hull);
    CHECK(cb.heat != plain.heat);
    CHECK(cb.cargo != plain.cargo);
}

TEST_CASE("presentation/Theme colour helpers honour their documented endpoints", "[presentation][theme]") {
    constexpr Color a{10, 20, 30, 200};
    constexpr Color b{200, 150, 100, 250};

    // t is a fraction: its endpoints are the inputs, and out-of-range input
    // clamps rather than overshooting.
    CHECK(mix(a, b, 0.0f) == a);
    CHECK(mix(a, b, 1.0f) == b);
    CHECK(mix(a, b, -1.0f) == a);
    CHECK(mix(a, b, 2.0f) == b);

    CHECK(a.with_alpha(0) == Color{10, 20, 30, 0});
    CHECK(a.with_alpha_f(0.0f) == Color{10, 20, 30, 0});
    CHECK(a.with_alpha_f(1.0f) == Color{10, 20, 30, 255});
    CHECK(a.with_alpha_f(5.0f) == Color{10, 20, 30, 255}); // clamped
    CHECK(a.with_alpha_f(-2.0f) == Color{10, 20, 30, 0});

    // Shade multiplies brightness and keeps alpha; channels clamp at 255.
    CHECK(a.shade(0.0f) == Color{0, 0, 0, 200});
    CHECK(b.shade(2.0f) == Color{255, 255, 200, 250});
}

TEST_CASE("presentation/Theme the spacing scale runs upward and the layout tokens are sane",
          "[presentation][theme]") {
    CHECK(theme::kSpace1 < theme::kSpace2);
    CHECK(theme::kSpace2 < theme::kSpace3);
    CHECK(theme::kSpace3 < theme::kSpace4);
    CHECK(theme::kSpace4 < theme::kSpace5);
    CHECK(theme::kSpace5 < theme::kSpace6);
    CHECK(theme::kSpace6 < theme::kSpace7);
    CHECK(theme::kRowHeight > 0.0f);
    CHECK(theme::kHeaderHeight > 0.0f);

    // Motion rates are exponential-approach rates; every one must be positive
    // and every one is gated by the reduce-motion setting at the call site.
    CHECK(theme::kHoverRate > 0.0f);
    CHECK(theme::kFocusRate > 0.0f);
    CHECK(theme::kPanelRate > 0.0f);
    CHECK(theme::kGaugeRate > 0.0f);
}
