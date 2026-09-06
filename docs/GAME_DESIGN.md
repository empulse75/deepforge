# Game design

This document describes what is **implemented**, not what is planned. Anything
still on paper is called out as such.

## Fantasy

You pilot a compact prospecting rig above an old, unstable world. The rig starts
improvised: a weak drill, a small tank, a hold that fills in ninety seconds. Over
time it becomes a specific machine — a surveyor that sees veins through rock, a
thermal engineer that turns waste heat back into fuel, an armoured deep explorer
that shrugs off the Ember Mantle, a hauler that empties a layer in one trip.

## The loop

1. **At the surface hub**: sell the haul, refine components, buy an equipment
   tier or a module, spend proficiency, take a contract, look at what the last
   run revealed.
2. **Descend** through the shaft the world starts with, or through the tunnels
   your previous runs cut.
3. **Read the rock**: veins clump into fields; the category marker on a tile
   tells you what it is without relying on colour; the scanner reveals what
   sight has not.
4. **Manage four pressures** that each do a different job (below).
5. **Decide**: one more vein, or turn for home while the fuel still covers the
   climb.
6. **Return**, sell, settle contracts, bank strategic material, hand samples to
   the lab.
7. **Change something about the rig**, and the next descent is a different
   problem.

### The decision that makes the loop work

The drill anchors. While it is engaged against rock the rig braces and pays no
thrust, so cutting a shaft upward costs only drill fuel — slow, cheap, safe.
Flying up an open shaft costs thrust — fast, expensive, and it needs a shaft to
exist. Every descent is therefore also a question about the route home: do you
dig a clean vertical shaft you can fly back up, or do you wander after ore and
accept a slow climb?

That single mechanic is what turns fuel from a timer into a plan.

## The four pressures

| Pressure | Job | How it is felt |
| --- | --- | --- |
| **Fuel** | Long-term range budget | Thrust, lateral flight and drilling draw it. Depth is the multiplier: the climb home is the expensive half. |
| **Cargo** | Forces prioritisation and return decisions | A full hold ends a run. Mass costs acceleration (capped at 22%), never control. |
| **Hull** | Absorbs mistakes | Falls above 13 m/s, hazards, and mishandled volatiles. Zero hull calls a rescue. |
| **Heat** | Short-term tactical meter | Rises with ambient depth heat, drilling hot rock and thrusting; falls with cooling. It creates pauses, route choices and cooling builds, and it degrades drill efficiency smoothly rather than at a cliff. |

There is no oxygen meter. It would have been a second fuel bar.

## Failure

When the hull reaches zero, or the rig is stranded without fuel, a rescue beacon
fires. The failure screen previews the consequences before you accept:

- **Kept, always**: equipment, modules, research, proficiency, discoveries, base
  facilities, protected samples, and everything already banked.
- **At risk**: unsold cargo from this run. Recovery technology reduces the loss.
- **Fee**: capped at 35% of your current credits, so a failure can never create a
  debt you cannot climb out of. The first rescue is free.

An autosave is written before resolution and again after, so save-scumming
cannot corrupt state.

There is also an anti-soft-lock floor: every return to the surface tops the rig
up to 35% fuel and 25% hull for free before any paid service. No sequence of bad
runs can strand a player with an empty tank and no credits.

## Progression systems

### Equipment (9 tracks, 7-9 tiers each)

Drill power, drill efficiency, engine, fuel tank, cargo bay, hull, cooling,
scanner, utility power. Costs follow `round_to_5(base * growth^tier)` with growth
between 1.50 and 1.63 depending on the track.

Tiers are not uniform percentages. Milestone tiers grant a **capability**:

| Track | Milestone | What it unlocks |
| --- | --- | --- |
| Drill power | 3 | Shatters brittle clusters in one action |
| Drill power | 6 | Widens the cut to neighbouring soft tiles |
| Drill efficiency | 4 | Shows vein direction |
| Engine | 2 | Brake without spending fuel |
| Engine | 7 | Hover assist |
| Fuel tank | 5 | Cheaper hover |
| Cargo bay | 4 | Auto-marks valuable cargo |
| Cargo bay | 7 | Cuts effective cargo mass |
| Hull | 5 | Free emergency reposition |
| Cooling | 3 | Survive one heat spike |
| Scanner | 2 | Partial vein reveal |
| Scanner | 6 | Scan through rock |

### Modules (31, five slot categories, shared power budget)

Drill, mobility, survival, survey and utility modules, freely swappable at the
surface, competing for a single utility power budget rather than arbitrary
inventory friction. Every module powerful enough to matter carries a stated
tradeoff — the pulse drill is a 75% power gain that generates 40% more heat; the
wide fracture head widens the cut but drills slower and dirtier; the resonance
array sees through rock at the cost of hull.

Six or more archetypes are viable from the current set: focused precision miner,
wide-cut hauler, thermal engineer, armoured deep explorer, survey/contract
specialist, and volatile extractor.

### Proficiency (36 nodes, three branches)

Prospector (scanning, yield, contracts, samples), Engineer (heat, repairs, power,
refining), Pathfinder (mobility, hazards, recovery, endurance). Small numeric
nodes connect the tree; the mastery and keystone nodes change behaviour. Each
branch ends in two **mutually exclusive keystones** — Lodefinder trades yield for
sight, Highgrader trades sight for value; Overclock trades heat for power,
Thermal Symbiosis turns heat into fuel; Deep Diver trades speed for depth
tolerance, Sprinter trades hull for speed. Respec is available for a bounded fee
and never requires starting over.

Points are granted for depth milestones and completed contracts, not for
repetition.

### Research (18 nodes)

Research converts samples and insight into capability, and it gates mechanics
rather than acting as a second shop. Artifact samples occupy protected storage
rather than normal cargo, so a failed run never costs you a discovery. Research
unlocks module purchasability, recipes, biome access and passive effects, and
ends at a capstone.

### Surface base (6 facilities, 4 tiers each)

Garage, refinery, research lab, contract terminal, recovery bay, logistics lift.
Facilities change what actions are available and improve rates. There are no
waiting timers and nothing to babysit.

### Contracts and factions

37 contract definitions across delivery, depth, scan, cache recovery, volatile
extraction, infrastructure activation, rescue, and no-module-class challenge
runs, plus five authored story contracts. Contracts are generated only from
templates whose content the player can actually obtain — the eligibility check
verifies the target material is generated at a reachable depth or producible by
an unlocked recipe, so the board never asks for something impossible.

Three factions gate themed modules by reputation. Reputation is not a spendable
currency.

### Codex and mastery

Materials log their first-seen depth and running total. Landmarks and biome
observations are recorded. Tutorial cards are reviewable from the codex.

## Unlock cadence

| Progress | Introduced |
| --- | --- |
| First minutes | Move, drill, fuel, cargo, return, sell, first upgrade |
| First ~25 min | Scanner, first module, refinery, first contract |
| First hour | Proficiency branches, first facility choice, second biome, research samples |
| Hours 1-3 | Factions, landmarks, transit nodes, synergistic modules |
| Hours 3-6 | Advanced refining, keystones, deep hazards, artifact chain |

Tutorials are contextual, short, dismissible, and reviewable in the codex. They
fire on the situation (first descent, half a hold of cargo, half a tank of fuel,
first 900 m) rather than on a timer.

## Economy

Three progression currencies and no more: **credits** (repairs, fuel, common
upgrades, modules), **refined components** (crafted from specific ores for
advanced equipment and facilities), and **insight** (from genuinely new
discoveries and major objectives, spent on research). Faction reputation is not
spendable. Individual material stacks stay ingredients.

Every material has an explicit role. Nothing becomes permanently useless: early
ores remain refinery inputs and contract targets long after they stop being the
best thing to sell.

Formulas are centralised in `data/economy.json`:

```
effective_drill_power = base * equipment * module * perk * heat_efficiency
mine_seconds  = clamp(base_mine_seconds * (hardness / power)^0.85, min, max)
fuel_per_tile = mine_fuel_base * hardness^0.60 * drill_efficiency
upgrade_cost(tier) = round_to_5(base * growth^tier)
```

Every multiplicative modifier is clamped in `StatBlock::clamp_to_caps`, and a
test asserts the caps hold with everything in the game bought at once.

## Biomes

| Biome | Depth (m) | New pressure | New opportunity |
| --- | ---: | --- | --- |
| Weathered Shelf | 0-250 | Weak collapses, tight fuel | Basic metals, caches, the tutorial-safe layer |
| Prism Faults | 250-650 | Hard crystal, arcs | Scanner play, prismite, branching veins |
| Mycelial Deeps | 650-1050 | Spores, corrosion | Bio-materials, symbiotic modules |
| Ember Mantle | 1050-1650 | High ambient heat, lava, volatiles | Thermal energy, alloys, cooling builds |
| Machine Strata | 1650-2400 | Turrets, pistons, magnetic disruption | Ancient components, transit network, artifacts |
| The Fracture | 2400+ | Mixed extremes | Endgame resources, riftalloy, void cells |

Transitions blend over a 40-tile band with visible mixed rock rather than a hard
invisible line, and depth gates are backed by a research requirement so access
means demonstrating the previous mechanic rather than paying a fee.

## Generation fairness

- Every seed's entry shaft is pre-carved and open — asserted over 60 seeds.
- The first 96 m near the shaft is a tutorial-safe template: no hazards, no
  structural walls, no volatiles.
- Eight starter veins flank the shaft on every seed, so the first expedition
  always produces a sale.
- A pity pass guarantees every commodity, strategic, volatile and artifact
  material at least once per 224-tile region within its depth band.
- The world is bounded by indestructible bedrock at ±420 tiles and 4200 m.
- A 1000-seed sweep asserts no invalid tiles, adequate starter ore, and sane
  air/solid density.

## Engagement

Rewards land on several horizons: vein continuation and drill feedback in
seconds, a pocket or cache in a couple of minutes, a sale and an upgrade per
expedition, a biome or mechanic per session, a build identity over hours.

There are no loot boxes, premium currencies, energy timers, daily streaks,
artificial waits, FOMO or pay-to-skip mechanics, and there will not be. The game
is offline, single-player, and finished when you close it.
