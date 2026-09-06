#include "domain/Expedition.hpp"

#include "core/Log.hpp"
#include "domain/Loadout.hpp"

#include <algorithm>
#include <cmath>

namespace df {
namespace {

constexpr float kGroundAccel = 26.0f;
constexpr float kAirAccel = 15.0f;
constexpr float kMaxRunSpeed = 8.2f;
constexpr float kGroundFriction = 18.0f;
constexpr float kAirDrag = 1.6f;
constexpr float kThrustAccel = 46.0f;
constexpr float kMaxRiseSpeed = 9.0f;
constexpr float kCoyoteSeconds = 0.09f;
constexpr float kLowFuelFraction = 0.18f;
constexpr float kScanCooldownSeconds = 2.5f;

} // namespace

ExpeditionSim::ExpeditionSim(const ContentDatabase& db, WorldMap& map, PlayerProgress& progress)
    : mDb(&db), mMap(&map), mProgress(&progress) {
    refresh_stats();
}

void ExpeditionSim::refresh_stats() {
    mStats = compute_stats(*mDb, *mProgress);
}

TileCoord ExpeditionSim::target_tile(const RigState& rig, MineDirection dir) {
    const TileCoord c = rig.tile();
    switch (dir) {
        case MineDirection::Left:
            return TileCoord{c.x - 1, c.y};
        case MineDirection::Right:
            return TileCoord{c.x + 1, c.y};
        case MineDirection::Up:
            return TileCoord{c.x, c.y - 1};
        case MineDirection::Down:
            return TileCoord{c.x, c.y + 1};
        case MineDirection::None:
            break;
    }
    return c;
}

void ExpeditionSim::begin_expedition(RigState& rig, Inventory& cargo, Inventory& samples) {
    refresh_stats();
    rig.mode = RigMode::Expedition;
    rig.velocity = Vec2{};
    rig.mineProgress = 0.0f;
    rig.activeMine = MineDirection::None;
    cargo.set_capacity(mStats.cargoCapacity);
    samples.set_capacity(static_cast<float>(mStats.protectedSampleSlots));
    mRuntime = ExpeditionRuntime{};
    mRuntime.startDepth = rig.depth_tiles();
    mRuntime.entryDepth = std::max<i64>(0, rig.depth_tiles());
    mRuntime.creditsAtStart = mProgress->credits;
    mRuntime.biomeIndex = mMap->generator().biome_index_at(std::max<i64>(0, rig.depth_tiles()));
    mRuntime.lastBiomeIndex = mRuntime.biomeIndex;
}

MineQuote ExpeditionSim::quote(const RigState& rig, TileCoord target) const {
    MineQuote q;
    q.material = mMap->material_index(target);
    q.mineable = mMap->is_mineable(target);
    if (!q.mineable) {
        return q;
    }
    const auto& econ = mDb->economy();
    q.hardness = mMap->hardness(target);

    // Heat degrades drill efficiency smoothly rather than at a cliff.
    const float heatFraction = saturate(rig.heat / std::max(1.0f, mStats.heatCapacity));
    const float heatEfficiency = lerp(1.0f, 0.62f, heatFraction * heatFraction);
    const float power = std::max(0.05f, mStats.drillPower * heatEfficiency);

    q.seconds = clamp(econ.baseMineSeconds * std::pow(q.hardness / power, econ.mineHardnessExponent),
                      econ.minMineSeconds, econ.maxMineSeconds) *
                mAssists.mineTimeScale;
    // Section 10.3: fuel is charged per mining action. Lower drillEfficiency
    // means cheaper drilling, so it scales the cost directly.
    q.fuelPerTile =
        econ.mineFuelBase * std::pow(q.hardness, econ.mineFuelHardnessExponent) * mStats.drillEfficiency;
    q.fuelPerSecond = q.fuelPerTile / std::max(0.01f, q.seconds);
    return q;
}

bool ExpeditionSim::solid_at(float x, float y) const {
    return mMap->is_solid(TileCoord{ifloor(x), ifloor(y)});
}

bool ExpeditionSim::box_blocked(Rect r) const {
    const i64 x0 = ifloor(r.left());
    const i64 x1 = ifloor(r.right() - 1e-4f);
    const i64 y0 = ifloor(r.top());
    const i64 y1 = ifloor(r.bottom() - 1e-4f);
    for (i64 y = y0; y <= y1; ++y) {
        for (i64 x = x0; x <= x1; ++x) {
            if (mMap->is_solid(TileCoord{x, y})) {
                return true;
            }
        }
    }
    return false;
}

void ExpeditionSim::damage(RigState& rig, float amount, EventQueue& events, EventKind kind, i32 source) {
    if (amount <= 0.0f) {
        return;
    }
    const float applied = amount * mAssists.damageScale;
    rig.hull -= applied;
    rig.sinceDamage = 0.0f;
    events.push(DomainEvent{kind, source, 0, applied, rig.tile(), {}});
    if (rig.hull <= 0.0f) {
        rig.hull = 0.0f;
        mRuntime.rescuePending = true;
        events.push(DomainEvent{EventKind::RescueTriggered, 0, 0, 0.0f, rig.tile(), "hull"});
    }
}

bool ExpeditionSim::drill_anchored(const RigState& rig, const TickCommands& cmd) const {
    if (cmd.mine == MineDirection::None) {
        return false;
    }
    const TileCoord target = target_tile(rig, cmd.mine);
    if (!mMap->is_mineable(target)) {
        return false;
    }
    // The rig braces on the tile it is cutting. This is what makes drilling
    // upward a genuine alternative to burning thrust, and it keeps mining
    // interaction predictable (Section 6.2).
    return true;
}

void ExpeditionSim::integrate_movement(RigState& rig, const TickCommands& cmd, float dt, EventQueue& events,
                                       float cargoLoadFraction, bool anchored) {
    const auto& econ = mDb->economy();
    const Vec2 previous = rig.position;
    rig.anchored = anchored;
    if (anchored) {
        // Braced: no gravity, no drift, no thrust cost. Only the drill draws
        // fuel, which update_mining charges.
        rig.velocity = Vec2{};
        rig.lastFallSpeed = 0.0f;
        rig.fuel = std::max(0.0f, rig.fuel - econ.idleFuelPerSecond * dt * mAssists.fuelDrainScale);
        resolve_collision(rig, previous, events);
        return;
    }

    // Cargo mass costs acceleration, never control.
    const float massPenalty = 1.0f - econ.cargoMassAccelPenalty * cargoLoadFraction * mStats.cargoMass;
    const float accelScale = clamp(massPenalty, 0.55f, 1.0f);

    const bool hasFuel = rig.fuel > 0.0f;
    float fuelSpent = 0.0f;

    // Horizontal.
    const float accel = (rig.onGround ? kGroundAccel : kAirAccel) * mStats.thrust * accelScale;
    if (std::fabs(cmd.moveX) > 0.05f) {
        rig.velocity.x += accel * cmd.moveX * dt;
        if (!rig.onGround) {
            fuelSpent += econ.lateralFuelPerSecond * std::fabs(cmd.moveX) * dt;
        }
    } else {
        const float friction = (rig.onGround ? kGroundFriction : kAirDrag) * mStats.brake;
        rig.velocity.x = move_towards(rig.velocity.x, 0.0f, friction * dt);
    }
    if (cmd.brake) {
        const bool freeBrake = mStats.flags.has(RigFlag::BrakeWithoutFuel);
        if (freeBrake || hasFuel) {
            rig.velocity.x = move_towards(rig.velocity.x, 0.0f, kGroundFriction * 1.6f * mStats.brake * dt);
            rig.velocity.y = move_towards(rig.velocity.y, 0.0f, kGroundFriction * 1.1f * mStats.brake * dt);
            if (!freeBrake) {
                fuelSpent += econ.lateralFuelPerSecond * 0.6f * dt;
            }
        }
    }
    rig.velocity.x = clamp(rig.velocity.x, -kMaxRunSpeed * mStats.thrust, kMaxRunSpeed * mStats.thrust);

    // Vertical: gravity, then thrust. Ascending is where fuel pressure lives.
    rig.velocity.y += econ.gravity * dt;
    if (cmd.thrust && hasFuel) {
        rig.velocity.y -= kThrustAccel * mStats.thrust * accelScale * dt;
        fuelSpent += econ.thrustFuelPerSecond * mStats.hoverFuel * dt;
    }
    rig.velocity.y = clamp(rig.velocity.y, -kMaxRiseSpeed * mStats.thrust, econ.maxFallSpeed);

    fuelSpent += econ.idleFuelPerSecond * dt;
    fuelSpent *= mAssists.fuelDrainScale;
    const float fuelBefore = rig.fuel;
    rig.fuel = std::max(0.0f, rig.fuel - fuelSpent);
    if (fuelBefore > mStats.fuelCapacity * kLowFuelFraction &&
        rig.fuel <= mStats.fuelCapacity * kLowFuelFraction) {
        events.push(DomainEvent{EventKind::FuelLow, 0, 0, rig.fuel, rig.tile(), {}});
    }
    if (fuelBefore > 0.0f && rig.fuel <= 0.0f) {
        events.push(DomainEvent{EventKind::FuelEmpty, 0, 0, 0.0f, rig.tile(), {}});
    }

    rig.lastFallSpeed = rig.velocity.y;
    rig.position += rig.velocity * dt;
    resolve_collision(rig, previous, events);
}

void ExpeditionSim::resolve_collision(RigState& rig, Vec2 previous, EventQueue& events) {
    // Axis-separated resolution. Horizontal first so that a rig sliding along a
    // floor never catches on tile seams.
    Rect box = rig.bounds();
    Rect test = box;
    test.y = previous.y + 0.5f - kRigHeight * 0.5f;
    if (box_blocked(test)) {
        rig.position.x = previous.x;
        rig.velocity.x = 0.0f;
    }

    box = rig.bounds();
    if (box_blocked(box)) {
        const bool movingDown = rig.position.y > previous.y;
        rig.position.y = previous.y;
        if (movingDown) {
            const float impact = std::fabs(rig.lastFallSpeed);
            const auto& econ = mDb->economy();
            if (impact > econ.impactDamageThreshold) {
                const float dmg =
                    (impact - econ.impactDamageThreshold) * econ.impactDamagePerUnit * mStats.impactDamage;
                damage(rig, dmg, events, EventKind::VehicleDamaged, 0);
            }
            rig.onGround = true;
            rig.coyoteTimer = kCoyoteSeconds;
        }
        rig.velocity.y = 0.0f;
    }

    // Ground probe just below the box.
    Rect probe = rig.bounds();
    probe.y += 0.06f;
    const bool grounded = box_blocked(probe);
    // Lane alignment: a grounded rig settles into the column it is mostly over.
    // Without this the box can straddle two columns and "mine down" would
    // target a column the rig is not actually above, which reads as the drill
    // doing nothing (Section 6.1: mining interaction must stay predictable).
    if (grounded && std::fabs(rig.velocity.x) < 0.35f) {
        const float lane = std::round(rig.position.x);
        const float aligned =
            move_towards(rig.position.x, lane, 6.0f * static_cast<float>(kFixedDeltaSeconds));
        const float saved = rig.position.x;
        rig.position.x = aligned;
        if (box_blocked(rig.bounds())) {
            rig.position.x = saved;
        }
    }
    if (grounded) {
        rig.onGround = true;
        rig.coyoteTimer = kCoyoteSeconds;
    } else {
        rig.onGround = false;
    }

    // Never allow a rig to end a tick embedded in terrain.
    if (box_blocked(rig.bounds())) {
        rig.position = previous;
        if (box_blocked(rig.bounds())) {
            perform_unstuck(rig, events);
        }
    }
}

bool ExpeditionSim::perform_unstuck(RigState& rig, EventQueue& events) {
    // Deterministic spiral search for the nearest free position.
    const TileCoord origin = rig.tile();
    for (i32 radius = 0; radius <= 24; ++radius) {
        for (i32 dy = -radius; dy <= radius; ++dy) {
            for (i32 dx = -radius; dx <= radius; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != radius) {
                    continue;
                }
                const TileCoord t{origin.x + dx, origin.y + dy};
                RigState probe = rig;
                probe.position = Vec2{static_cast<float>(t.x), static_cast<float>(t.y)};
                if (!box_blocked(probe.bounds())) {
                    rig.position = probe.position;
                    rig.velocity = Vec2{};
                    const bool free = mStats.flags.has(RigFlag::FreeUnstuck);
                    if (!free) {
                        rig.fuel = std::max(0.0f, rig.fuel - 3.0f);
                        rig.hull = std::max(1.0f, rig.hull - 2.0f);
                    }
                    events.push(DomainEvent{EventKind::Unstuck, radius, 0, free ? 0.0f : 3.0f, t,
                                            "unstuck: nearest free position"});
                    DF_LOG_WARN("sim", "unstuck at radius {} from ({},{})", radius, origin.x, origin.y);
                    return true;
                }
            }
        }
    }
    return false;
}

void ExpeditionSim::update_mining(RigState& rig, Inventory& cargo, Inventory& samples,
                                  const TickCommands& cmd, float dt, EventQueue& events) {
    if (cmd.mine == MineDirection::None) {
        rig.activeMine = MineDirection::None;
        return;
    }
    const TileCoord target = target_tile(rig, cmd.mine);
    if (rig.activeMine != cmd.mine || !(rig.mineTarget == target)) {
        // Switching target keeps the previous tile's stored progress; the world
        // map owns it, so interrupting never destroys work (Section 6.2).
        if (rig.activeMine != MineDirection::None && rig.mineProgress > 0.0f) {
            mMap->set_mining_progress(rig.mineTarget, rig.mineProgress);
        }
        rig.activeMine = cmd.mine;
        rig.mineTarget = target;
        rig.facing = cmd.mine;
        rig.mineProgress = mMap->mining_progress(target);
    }

    const MineQuote q = quote(rig, target);
    if (!q.mineable) {
        events.push(DomainEvent{EventKind::MineBlocked, q.material, 0, 0.0f, target, {}});
        return;
    }
    if (rig.fuel <= 0.0f) {
        events.push(DomainEvent{EventKind::FuelEmpty, 0, 0, 0.0f, target, {}});
        return;
    }

    rig.fuel = std::max(0.0f, rig.fuel - q.fuelPerSecond * dt * mAssists.fuelDrainScale);
    rig.mineProgress += dt / std::max(0.01f, q.seconds);
    mMap->set_mining_progress(target, rig.mineProgress);

    const auto& mat = mMap->material(target);
    rig.heat += mat.heatOnMine * dt * mStats.heatGeneration * mAssists.heatScale;

    if (rig.mineProgress >= 1.0f) {
        break_tile(rig, cargo, samples, target, events);
        rig.mineProgress = 0.0f;
        mMap->clear_mining_progress(target);
        // Drill advance: the rig climbs into the space it just cut. This is what
        // makes drilling a real alternative to burning thrust, and it keeps the
        // rig from paying hover fuel to hold itself against a ceiling.
        if (cmd.mine == MineDirection::Up || cmd.mine == MineDirection::Left ||
            cmd.mine == MineDirection::Right) {
            RigState probe = rig;
            probe.position = Vec2{static_cast<float>(target.x), static_cast<float>(target.y)};
            if (!box_blocked(probe.bounds())) {
                rig.position = probe.position;
                rig.velocity = Vec2{};
            }
        }
    }
}

void ExpeditionSim::break_tile(RigState& rig, Inventory& cargo, Inventory& samples, TileCoord t,
                               EventQueue& events) {
    const i32 materialIndex = mMap->material_index(t);
    if (materialIndex < 0) {
        return;
    }
    const auto& mat = mDb->materials()[static_cast<usize>(materialIndex)];

    // Volatile material punishes careless extraction unless stabilised.
    if (mat.volatileDamage > 0.0f) {
        const float mitigated = mat.volatileDamage * (1.0f - saturate(mStats.volatileStability));
        if (mitigated > 0.1f) {
            damage(rig, mitigated, events, EventKind::VehicleDamaged, materialIndex);
        }
    }

    mMap->set_material(t, mDb->airMaterial.index);
    ++mRuntime.tilesMined;
    ++mProgress->stats.tilesMined;
    events.push(DomainEvent{EventKind::TileMined, materialIndex, 0, 0.0f, t, {}});

    if (mat.cargoUnits > 0 &&
        (mat.baseValue > 0 || mat.protectedSample || mat.category == MaterialCategory::Strategic)) {
        Rng yieldRng{hash_coords(derive_seed(mMap->generator().seed(), RngStream::Loot), t.x, t.y)};
        i32 amount = yieldRng.range(mat.yieldMin, mat.yieldMax);
        amount = std::max(1, static_cast<i32>(std::lround(static_cast<float>(amount) * mStats.yieldMul)));

        Inventory& destination = mat.protectedSample ? samples : cargo;
        const i32 stored = destination.add(*mDb, materialIndex, amount);
        if (stored > 0) {
            events.push(DomainEvent{mat.protectedSample ? EventKind::SampleFound : EventKind::CargoAdded,
                                    materialIndex,
                                    stored,
                                    0.0f,
                                    t,
                                    {}});
            auto& entry = mProgress->codex[materialIndex];
            if (entry.material < 0) {
                entry.material = materialIndex;
                entry.firstDepth = t.y;
            }
            entry.minedCount += stored;
        }
        if (stored < amount) {
            events.push(DomainEvent{EventKind::CargoFull, materialIndex, amount - stored, 0.0f, t, {}});
        }
    }

    // Milestone drills shatter neighbouring brittle tiles in the same action.
    if (mStats.mineRadius > 0) {
        for (i32 dy = -mStats.mineRadius; dy <= mStats.mineRadius; ++dy) {
            for (i32 dx = -mStats.mineRadius; dx <= mStats.mineRadius; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                const TileCoord n{t.x + dx, t.y + dy};
                if (!mMap->is_mineable(n)) {
                    continue;
                }
                const auto& nm = mMap->material(n);
                if (nm.hardness <= 1.2f && nm.category == MaterialCategory::Rock) {
                    mMap->set_material(n, mDb->airMaterial.index);
                    events.push(DomainEvent{EventKind::TileMined, mMap->material_index(n), 0, 0.0f, n, {}});
                }
            }
        }
    }
}

void ExpeditionSim::update_heat(RigState& rig, const TickCommands& cmd, float dt, EventQueue& events) {
    const auto& econ = mDb->economy();
    const i32 bi = mMap->generator().biome_index_at(std::max<i64>(0, rig.depth_tiles()));
    const auto& biome = mDb->biomes()[static_cast<usize>(bi)];

    float gain = biome.ambientHeat * mStats.depthHeat;
    if (cmd.thrust) {
        gain += 2.0f;
    }
    gain *= mStats.heatGeneration * mAssists.heatScale;
    rig.heat += gain * dt;
    rig.heat -= econ.heatDecayPerSecond * mStats.cooling * dt;
    rig.heat = std::max(0.0f, rig.heat);

    // Thermal reclaimers convert excess heat into fuel.
    const float fraction = rig.heat / std::max(1.0f, mStats.heatCapacity);
    if (mStats.heatToFuelRate > 0.0f && fraction >= mStats.heatToFuelThreshold) {
        const float converted = mStats.heatToFuelRate * dt;
        rig.fuel = std::min(mStats.fuelCapacity, rig.fuel + converted);
        rig.heat = std::max(0.0f, rig.heat - converted * 6.0f);
    }

    if (fraction >= 1.0f) {
        if (mStats.flags.has(RigFlag::SurviveHeatSpike) && rig.heat < mStats.heatCapacity * 1.15f) {
            // One spike absorbed before damage begins.
        } else {
            damage(rig, econ.heatDamagePerSecond * dt, events, EventKind::HeatCritical, 0);
        }
    } else if (fraction >= 0.85f) {
        events.push(DomainEvent{EventKind::HeatCritical, 1, 0, fraction, rig.tile(), {}});
    }
}

void ExpeditionSim::update_hazards(RigState& rig, float dt, EventQueue& events) {
    const TileCoord centre = rig.tile();
    const ChunkCoord c0 = chunk_of(TileCoord{centre.x - 8, centre.y - 8});
    const ChunkCoord c1 = chunk_of(TileCoord{centre.x + 8, centre.y + 8});
    for (i64 cy = c0.y; cy <= c1.y; ++cy) {
        for (i64 cx = c0.x; cx <= c1.x; ++cx) {
            Chunk& ch = mMap->chunk(ChunkCoord{cx, cy});
            for (auto& hz : ch.hazards()) {
                if (hz.definition < 0) {
                    continue;
                }
                const auto& def = mDb->hazard_def(HazardHandle{hz.definition});
                const float dx = static_cast<float>(hz.tile.x) - rig.position.x;
                const float dy = static_cast<float>(hz.tile.y) - rig.position.y;
                const float distSq = dx * dx + dy * dy;
                const float radius = def.radius;
                if (distSq > radius * radius) {
                    hz.triggered = false;
                    hz.cooldown = 0.0f;
                    continue;
                }
                if (def.counteredBy != RigFlag::Count && mStats.flags.has(def.counteredBy)) {
                    continue;
                }
                // Telegraph: hazards always warn before they can hurt you.
                hz.cooldown += dt;
                if (hz.cooldown < def.telegraphSeconds) {
                    continue;
                }
                const float dps = (def.areaDamage > 0.0f ? def.areaDamage : def.contactDamage * 0.5f);
                damage(rig, dps * mStats.hazardDamage * dt, events, EventKind::VehicleDamaged, hz.definition);
                rig.heat += def.heatPerSecond * dt * mStats.depthHeat * mAssists.heatScale;
                if (!hz.triggered && distSq < 1.4f * 1.4f) {
                    hz.triggered = true;
                    damage(rig, def.contactDamage * mStats.hazardDamage, events, EventKind::VehicleDamaged,
                           hz.definition);
                }
            }
        }
    }
}

void ExpeditionSim::update_discovery(RigState& rig, const TickCommands& cmd, float dt, EventQueue& events) {
    const TileCoord centre = rig.tile();
    mMap->reveal_radius(centre, 7);
    if (mStats.scannerRange > 0.0f) {
        rig.scanCooldown = std::max(0.0f, rig.scanCooldown - dt);
        if (cmd.scan && rig.scanCooldown <= 0.0f) {
            rig.scanCooldown = kScanCooldownSeconds;
            mMap->reveal_radius(centre, static_cast<i32>(mStats.scannerRange), true);
            events.push(DomainEvent{EventKind::ScanPulse, 0, 0, mStats.scannerRange, centre, {}});
        }
    }

    for (auto* lm : mMap->landmarks_near(centre, 14)) {
        if (mProgress->discoveredLandmarks.count(static_cast<i64>(lm->stableId)) == 0) {
            mProgress->discoveredLandmarks.insert(static_cast<i64>(lm->stableId));
            events.push(DomainEvent{EventKind::LandmarkDiscovered, lm->definition, 0, 0.0f, lm->origin,
                                    mDb->landmarks()[static_cast<usize>(lm->definition)].nameKey});
        }
        const auto& def = mDb->landmarks()[static_cast<usize>(lm->definition)];
        if (!cmd.interact || lm->activated) {
            continue;
        }
        const i64 dx = lm->origin.x + lm->width / 2 - centre.x;
        const i64 dy = lm->origin.y + lm->height / 2 - centre.y;
        if (std::llabs(dx) > lm->width || std::llabs(dy) > lm->height) {
            continue;
        }
        lm->activated = true;
        mMap->set_landmark_state(chunk_of(lm->origin), lm->stableId, true, lm->looted);
        events.push(
            DomainEvent{EventKind::LandmarkActivated, lm->definition, 0, 0.0f, lm->origin, def.nameKey});
        switch (def.kind) {
            case LandmarkKind::RepairStation:
                rig.hull = std::min(mStats.hullMax, rig.hull + mStats.hullMax * 0.5f);
                rig.fuel = std::min(mStats.fuelCapacity, rig.fuel + mStats.fuelCapacity * 0.35f);
                break;
            case LandmarkKind::TransitNode:
                // Activating a node makes it a permanent entry point, which is
                // what keeps deep biomes reachable without a full climb.
                mProgress->add_transit_node(lm->stableId, lm->origin.x + lm->width / 2,
                                            lm->origin.y + lm->height / 2);
                break;
            case LandmarkKind::CoolingPocket:
                rig.heat = 0.0f;
                break;
            default:
                break;
        }
    }
}

void ExpeditionSim::tick(RigState& rig, Inventory& cargo, Inventory& samples, const TickCommands& cmd,
                         float dt, EventQueue& events) {
    if (rig.mode != RigMode::Expedition) {
        return;
    }
    ++mRuntime.tickCount;
    mRuntime.elapsedSeconds += dt;
    rig.sinceDamage += dt;
    rig.coyoteTimer = std::max(0.0f, rig.coyoteTimer - dt);

    if (cmd.unstuck) {
        perform_unstuck(rig, events);
    }

    const float loadFraction =
        cargo.capacity() > 0.0f ? saturate(cargo.used_units(*mDb) / cargo.capacity()) : 0.0f;

    const bool anchored = drill_anchored(rig, cmd);
    integrate_movement(rig, cmd, dt, events, loadFraction, anchored);
    update_mining(rig, cargo, samples, cmd, dt, events);
    update_heat(rig, cmd, dt, events);
    update_hazards(rig, dt, events);
    update_discovery(rig, cmd, dt, events);
    mMap->tick_mining_decay(dt, rig.tile(), 24);

    // Auto-collector picks up loose ore around the rig.
    if (mStats.autoCollectRadius > 0.5f) {
        const i32 r = static_cast<i32>(mStats.autoCollectRadius);
        const TileCoord c = rig.tile();
        for (i32 dy = -r; dy <= r; ++dy) {
            for (i32 dx = -r; dx <= r; ++dx) {
                const TileCoord t{c.x + dx, c.y + dy};
                if (!mMap->is_mineable(t)) {
                    continue;
                }
                const auto& m = mMap->material(t);
                if (m.hardness <= 0.35f && m.baseValue > 0) {
                    break_tile(rig, cargo, samples, t, events);
                }
            }
        }
    }

    const i64 depth = rig.depth_tiles();
    if (depth > rig.deepestThisRun) {
        rig.deepestThisRun = depth;
    }
    if (depth > mProgress->stats.deepestDepth) {
        // Announce records at readable milestones, not once per metre.
        const i64 previousMilestone = mProgress->stats.deepestDepth / 25;
        mProgress->stats.deepestDepth = depth;
        if (depth / 25 > previousMilestone) {
            events.push(DomainEvent{EventKind::DepthRecord, 0, 0, static_cast<float>(depth), rig.tile(), {}});
        }
    }
    const i32 bi = mMap->generator().biome_index_at(std::max<i64>(0, depth));
    if (bi != mRuntime.lastBiomeIndex) {
        mRuntime.lastBiomeIndex = bi;
        mRuntime.biomeIndex = bi;
        events.push(DomainEvent{EventKind::BiomeEntered, bi, 0, 0.0f, rig.tile(),
                                mDb->biomes()[static_cast<usize>(bi)].nameKey});
    }

    // Stranded: no fuel, on the ground, nothing left to do.
    if (rig.fuel <= 0.0f && rig.onGround && depth > 4 && rig.velocity.x == 0.0f) {
        mRuntime.rescuePending = true;
    }
    if (rig.hull <= 0.0f) {
        mRuntime.rescuePending = true;
    }
    if (depth > mRuntime.entryDepth + 6) {
        mRuntime.hasDescended = true;
    }
    // Climbing back to the entry point ends the expedition. Launching from an
    // activated transit node therefore also means extracting through it, which
    // is what makes deep expeditions a round trip rather than a one-way climb.
    if (depth <= mRuntime.entryDepth && mRuntime.hasDescended && !mRuntime.rescuePending) {
        mRuntime.returnedToSurface = true;
    }
}

} // namespace df
