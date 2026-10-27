// The token machinery, tested as behaviour (TEST-01 seam: theme-token usage —
// "Screens compose tokens; they do not invent colours"). What is pinned here
// is the machinery's contract: the colour-vision swap actually swaps, the mix
// and shade helpers land where the art direction says they land, and the
// spacing scale is the scale the layout arithmetic assumes. Individual token
// values move (the accessibility cards will retoken them); these invariants
// are what the designers lean on when they do.
#include "presentation/Theme.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace df;

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
