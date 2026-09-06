// Player intent for one simulation tick. Input is sampled by the platform layer
// and converted into this structure before the simulation runs (Section 15.3).
#pragma once

#include "core/Types.hpp"

namespace df {

enum class MineDirection : u8 { None, Left, Right, Up, Down };

struct TickCommands {
    float moveX = 0.0f;  ///< -1..1
    bool thrust = false; ///< Vertical thrust / hover.
    bool brake = false;
    MineDirection mine = MineDirection::None;
    bool interact = false; ///< Use landmark / activate transit node.
    bool scan = false;
    bool unstuck = false; ///< Deterministic safety action (Section 6.1).
    bool deployBeacon = false;
};

} // namespace df
