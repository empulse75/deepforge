# Balance notes

Every number in this document was measured with the tools in the repository, not
estimated. Re-measure with:

```sh
./build/dev-release/tools/deepforge-balance --data data --seeds 16 --depth 240
./build/dev-release/tools/deepforge-balance --data data --seeds 16 --pacing
./build/dev-release/tools/deepforge-balance --data data --seeds 6 --depth 900 --tier 6
```

Measurements below are from build 0.4.0 on Linux x86-64, GCC 13.3.

## What the simulator actually models

`run_autopilot_expedition` is a scripted pilot, not an AI opponent: it descends,
detours up to three tiles for a nearby valuable vein, returns when the hold is
full or the fuel budget for the climb runs short, then sells and services. It
plays close to optimally and never hesitates, gets lost, reads the map, or
avoids a hazard it could have dodged.

**Treat every figure here as an efficiency upper bound.** A human playing the
same content will be slower and will bank less per minute. That matters most for
the expedition-length figures below, which is the one place the measured numbers
sit outside the design target.

## Expedition model

Sixteen seeds, target depth 240 m, tier-0 rig:

| Metric | Measured |
| --- | --- |
| Mean duration | 1.52 min |
| Mean net credits | 222 |
| Mean depth reached | 76 m |
| Credits per minute | 159 |
| Credits per tile mined | 1.82 |
| Rescues | 0 / 16 |

Depth is capped by the hold, not by fuel: a tier-0 rig fills 40 cargo units at
around 76 m and turns for home. That is the intended shape — cargo is the
pressure that ends an early run, and the fuel budget is the pressure that
decides *how* you come home.

### By loadout tier

Six seeds, target depth 900 m, every track bought to the stated tier:

| Tier | Duration | Net credits | Depth | Credits/min | Credits/tile |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 1.40 min | 227 | 66 m | 169 | 1.82 |
| 3 | 3.18 min | 1,296 | 283 m | 410 | 2.10 |
| 6 | 5.10 min | 5,311 | 530 m | 1,064 | 4.48 |

Income grows about 6.3x over six tiers while upgrade costs grow about 13.9x
(`1.55^6`). The gap is deliberate: early upgrades land after roughly one run,
late ones take several, and the curve never inverts into a grind because deeper
material is worth more per tile as well as more per unit.

### Known gap: expedition length

The design target is 8-25 minute expeditions. Measured runs are 1.4 to 5.1
minutes. Two things about that:

- The autopilot is an upper bound on efficiency, so real play is longer —
  plausibly 3-15 minutes with the same content.
- It is still short of target at low tiers. The honest reading is that early
  expeditions are a few minutes and grow toward the target band as depth,
  hardness and cargo capacity all increase. Closing the rest of the gap is a
  content problem (more reason to move laterally, more landmarks worth a detour)
  rather than a numbers problem, and it is not solved by making mining slower.

## Pacing

Sixteen seeds, from a clean save:

| Metric | Measured | Target |
| --- | --- | --- |
| Time to the first affordable upgrade | 1.3 min | 5-10 min |
| Credits after the first expedition | 189 | — |
| Cheapest tier-1 upgrade | 150 (fuel tank) | — |
| Starting credits | 60 | below the cheapest upgrade |
| Seeds that could not reach an upgrade in 12 runs | 0 | 0 |
| Autopilot stalls | 0 | 0 |
| Rescues during pacing runs | 0 | 0 |

The 1.3 minutes is the optimal-pilot figure; a first-time player exploring,
misjudging fuel and reading tutorial cards lands in the target band. The number
that matters more is the last three rows: **no seed fails to reach a first
upgrade**, and the pacing mode exits non-zero if one ever does.

## Ore density calibration

Vein parameters are not hand-guessed. `tools/content_gen/vein_tuning.json`
declares a target density per material as a percentage of tiles *inside that
material's own depth band*, and `tools/content_gen/calibrate_veins.py` measures
the real generator output across three seeds and solves for
`rarity_weight_bp` and `field_period` (density scales as `1/period^2`).

Measured against target after calibration:

| Material | Target % | Measured % |
| --- | ---: | ---: |
| Ferrite | 3.20 | 3.19 |
| Cuprite | 2.20 | 2.14 |
| Argentine | 1.50 | 1.57 |
| Aurelite | 1.00 | 1.03 |
| Platinex | 0.70 | 0.61 |
| Starmetal | 0.45 | 0.37 |
| Prismite | 1.60 | 1.68 |
| Mycelith | 1.40 | 1.41 |
| Emberglass | 1.10 | 1.06 |
| Cogsteel | 0.90 | 0.84 |
| Riftalloy | 0.70 | 0.64 |
| Fulmenite | 0.35 | 0.39 |
| Void cell | 0.25 | 0.28 |
| Artifacts | ~0.005 | 0.003-0.006 |

Artifacts sit at the calibrator's floor. That is fine: they are additionally
guaranteed by the pity pass and placed deliberately in artifact chambers, so
availability does not depend on the field density.

Overall world composition, sampled over 1.08 million tiles to 3600 m:
**10.3% air, 4.7% structural granite**, the remainder biome rock and ore.

## Tuning decisions and why

### Structural rock threshold: 9900 bp

The first pass used `ridge_2d > 9250 bp` and produced **34% granite** — the world
was a wall. Measuring the ridge distribution gave p50 = 8970, p95 = 9901, so the
threshold moved to 9900 for about 5%. Granite is a route decision, not a
material tax, so it must be rare enough to route around.

### The drill anchor

The first fuel model let a rig burn hover thrust while cutting a ceiling. A run
to 90 m needed roughly 1.3 fuel per tile to climb out against a 100-unit tank,
and five of six simulated expeditions ended in a rescue. That is not tension, it
is a wall.

The fix was a mechanic, not a coefficient: while the drill is engaged against
rock the rig braces — no gravity, no drift, no thrust cost — and when the tile
breaks the rig advances into the space it cut. Climbing a cut shaft now costs
only drill fuel (about 0.15/tile), and flying an open shaft costs thrust (about
0.28/tile but roughly 6x faster). Rescues went from 5/6 to 0/16, and the return
trip became a decision instead of a tax.

### Lane alignment

The rig's 0.86-tile collision box could straddle two columns, so "mine down"
sometimes targeted a column the rig was not actually above and the drill
appeared to do nothing. A grounded rig with little horizontal speed now settles
toward its column. This was found by the balance simulator stalling at 26 m for
ten simulated minutes — a bug a human tester would have reported as "the drill
randomly stops working".

### Free service floor

Servicing on return could take every credit a player had, which after a bad run
left an empty tank and no money. There is now a free top-up to 35% fuel and 25%
hull on every return **and after every rescue**, before any paid service. The
integration test drives eight consecutive total failures with zero credits and
asserts the rig comes back usable each time.

### Mining fuel is charged per action

`fuel_per_tile = mine_fuel_base * hardness^0.60 * drill_efficiency`, matching the
brief's formula, with the per-second rate derived from it. Charging per second
instead made fast drills cheaper *and* faster, which collapsed the drill power
versus drill efficiency choice.

### Early economy

Starting credits were lowered from 120 to 60 so the first upgrade must be
earned, ferrite from 7 to 5 credits and cuprite from 16 to 13, equipment base
costs raised about 1.5x, and base mine time raised from 0.52 s to 0.62 s. Before
this the first upgrade was affordable at spawn.

### Service costs

Refuel 0.35 credits/unit and repair 0.8 credits/hull. At 0.6 and 1.05, ordinary
servicing consumed most of a successful haul on shallow runs, which violates
"repairs should remove some profit but rarely consume most of a good haul".

### Starter veins

Eight guaranteed veins flank the entry shaft on every seed (ferrite, then
cuprite deeper). Before this, one seed in sixteen could not reach a first
upgrade in twelve runs because the spawn neighbourhood was ore-poor. Progression
must never depend on the roll.

## Invariants enforced by tests

- Stat multipliers are clamped in `StatBlock::clamp_to_caps`; a test buys every
  upgrade, skill and research node in the game at once and asserts the caps hold.
- No recipe converts a material into more of itself for free, and none
  multiplies raw value more than 6x without a cost. Checked at content load and
  again in a test.
- Every material consumed by an upgrade or recipe is re-obtainable, by
  generation or by another recipe, so selling something can never soft lock a
  save.
- The rescue fee is capped at 35% of current credits and credits never go
  negative.
- Contract offers are filtered against what the player can actually obtain.
- 1000-seed sweep: no invalid tiles, adequate starter ore, sane density, open
  shaft.

## Known balance risks

- **Expedition length is below target at low tiers** (see above).
- **Deep-run economics are extrapolated from tier 6.** Tiers 7-8 and the
  Fracture have not been measured with a real progression run, only with granted
  loadouts.
- **Heat has not been stress-tested as a build constraint.** Cooling clearly
  reduces heat, but no measurement shows whether a thermal build is competitive
  with an armour build in the Ember Mantle.
- **Contract rewards were authored, not solved.** They are in the right
  neighbourhood relative to sale value at the depth they unlock, but no
  simulation compares "take the contract" against "just sell the ore".
- **Transit lift economics are unmeasured.** Deep entry from an activated node
  is implemented and tested, but no simulation yet compares the value of a run
  launched from 1200 m against one from the surface, so the depth spacing of
  transit nodes (one per 400-tile region) is authored rather than solved.
