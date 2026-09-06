#pragma once

#include "core/Math.hpp"
#include "core/Types.hpp"
#include "domain/Commands.hpp"
#include "domain/Inventory.hpp"

namespace df {

/// Rig collision box, in tiles. Deliberately smaller than one tile so that
/// mining a single tile always produces a passable opening.
inline constexpr float kRigWidth = 0.86f;
inline constexpr float kRigHeight = 0.86f;

enum class RigMode : u8 { Surface, Expedition, Rescued, Destroyed };

struct RigState {
    Vec2 position{0.0f, 0.0f}; ///< Tile-space centre; +y is down.
    Vec2 velocity{};
    float fuel = 100.0f;
    float hull = 100.0f;
    float heat = 0.0f;
    bool onGround = false;
    float coyoteTimer = 0.0f; ///< Input tolerance after leaving ground.
    MineDirection facing = MineDirection::Down;
    MineDirection activeMine = MineDirection::None;
    TileCoord mineTarget{};
    float mineProgress = 0.0f;
    float sinceDamage = 0.0f;
    float scanCooldown = 0.0f;
    float lastFallSpeed = 0.0f;
    bool anchored = false; ///< Braced against rock while drilling.
    i64 deepestThisRun = 0;
    RigMode mode = RigMode::Surface;

    TileCoord tile() const noexcept {
        return TileCoord{ifloor(position.x + 0.5f), ifloor(position.y + 0.5f)};
    }

    Rect bounds() const noexcept {
        return Rect{position.x + 0.5f - kRigWidth * 0.5f, position.y + 0.5f - kRigHeight * 0.5f, kRigWidth,
                    kRigHeight};
    }

    i64 depth_tiles() const noexcept { return static_cast<i64>(position.y); }
};

} // namespace df
