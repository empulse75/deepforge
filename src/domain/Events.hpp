// Typed domain events. Consumers are explicit; there is no global bus.
#pragma once

#include "core/Types.hpp"

#include <string>
#include <vector>

namespace df {

enum class EventKind : u8 {
    TileMined,
    CargoAdded,
    CargoFull,
    VehicleDamaged,
    HeatCritical,
    FuelLow,
    FuelEmpty,
    LandmarkDiscovered,
    LandmarkActivated,
    SampleFound,
    ContractProgress,
    ContractComplete,
    ExpeditionEnded,
    RescueTriggered,
    Unstuck,
    DepthRecord,
    BiomeEntered,
    ScanPulse,
    MineBlocked,
};

struct DomainEvent {
    EventKind kind = EventKind::TileMined;
    i32 a = 0; ///< Material / definition index, damage source, etc.
    i32 b = 0; ///< Amount.
    float value = 0.0f;
    TileCoord tile{};
    std::string text; ///< Localisation key or preformatted detail.
};

/// A bounded, per-tick queue. Presentation drains it; it never feeds back.
class EventQueue {
public:
    void push(DomainEvent e) {
        if (mEvents.size() < kMaxEvents) {
            mEvents.push_back(std::move(e));
        }
    }

    const std::vector<DomainEvent>& events() const noexcept { return mEvents; }

    void clear() noexcept { mEvents.clear(); }

    bool empty() const noexcept { return mEvents.empty(); }

private:
    static constexpr usize kMaxEvents = 256;
    std::vector<DomainEvent> mEvents;
};

} // namespace df
