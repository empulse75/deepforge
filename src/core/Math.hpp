// Small project-owned math types. We deliberately maintain exactly one math
// system (Section 14 of the design brief).
#pragma once

#include "core/Types.hpp"

#include <algorithm>
#include <cmath>

namespace df {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() = default;

    constexpr Vec2(float ix, float iy) : x(ix), y(iy) {}

    constexpr Vec2 operator+(Vec2 o) const noexcept { return {x + o.x, y + o.y}; }

    constexpr Vec2 operator-(Vec2 o) const noexcept { return {x - o.x, y - o.y}; }

    constexpr Vec2 operator*(float s) const noexcept { return {x * s, y * s}; }

    constexpr Vec2 operator/(float s) const noexcept { return {x / s, y / s}; }

    constexpr Vec2& operator+=(Vec2 o) noexcept {
        x += o.x;
        y += o.y;
        return *this;
    }

    constexpr Vec2& operator-=(Vec2 o) noexcept {
        x -= o.x;
        y -= o.y;
        return *this;
    }

    constexpr Vec2& operator*=(float s) noexcept {
        x *= s;
        y *= s;
        return *this;
    }

    friend constexpr bool operator==(const Vec2&, const Vec2&) = default;
};

struct Vec2i {
    i32 x = 0;
    i32 y = 0;

    friend constexpr bool operator==(const Vec2i&, const Vec2i&) = default;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    constexpr float left() const noexcept { return x; }

    constexpr float right() const noexcept { return x + w; }

    constexpr float top() const noexcept { return y; }

    constexpr float bottom() const noexcept { return y + h; }

    constexpr bool overlaps(const Rect& o) const noexcept {
        return left() < o.right() && right() > o.left() && top() < o.bottom() && bottom() > o.top();
    }
};

constexpr float length_sq(Vec2 v) noexcept {
    return v.x * v.x + v.y * v.y;
}

inline float length(Vec2 v) noexcept {
    return std::sqrt(length_sq(v));
}

inline Vec2 normalized(Vec2 v) noexcept {
    const float l = length(v);
    return l > 1e-6f ? v / l : Vec2{};
}

template <typename T>
constexpr T clamp(T v, T lo, T hi) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
}

constexpr float lerp(float a, float b, float t) noexcept {
    return a + (b - a) * t;
}

constexpr float saturate(float v) noexcept {
    return clamp(v, 0.0f, 1.0f);
}

/// Frame-rate independent exponential approach; deterministic for a fixed dt.
inline float approach(float current, float target, float rate, float dt) noexcept {
    const float t = 1.0f - std::exp(-rate * dt);
    return current + (target - current) * t;
}

constexpr float move_towards(float current, float target, float maxDelta) noexcept {
    const float d = target - current;
    if (d > maxDelta) {
        return current + maxDelta;
    }
    if (d < -maxDelta) {
        return current - maxDelta;
    }
    return target;
}

constexpr i64 ifloor(float v) noexcept {
    const i64 i = static_cast<i64>(v);
    return (v < 0.0f && static_cast<float>(i) != v) ? i - 1 : i;
}

} // namespace df
