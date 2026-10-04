# Spec: Project Deepforge — deep progression, presentation and non-P2W monetisation

Status: proposed (spec-driven development, phase 1). This spec extends an
existing, working game; it is not a greenfield plan. Verified baseline
2026-09-16: **Project Deepforge 0.4.0 (Release)** builds, is installed at
`~/.local/bin/deepforge`, passes **76/76 ctest (210,301 assertions)**,
cross-compiles to a PE32+ Windows `.exe`, packages a 2.3 MiB CPack zip, and
runs `--headless-frames 30` to exit 0 under `SDL_VIDEODRIVER=dummy`.

The repo root holding `CMakeLists.txt` is **`/home/gonzo/deepforge/deepforge`**;
this file lives there. The parent `/home/gonzo/deepforge` is only a delivery
wrapper (`START-HERE.md`, screenshots, baseline copy).

---

## Objective

Deepforge is a Motherload-like 2D mining and progression game, already
playable end to end, that this spec pushes further on three axes the operator
named: beautiful 2D presentation, a deeper upgrade + research game, and an
ethical monetisation layer that can never bend the simulation.

The loop in one paragraph: from the **surface hub** you descend a carved shaft
into a seeded, chunked world, cutting through rock while managing four
interacting pressures — **fuel** (range), **cargo** (when the run must end),
**hull** (mistake absorber), **heat** (tactical meter) — then **extract**,
**sell** the haul at depth-priced rates, spend the proceeds on **equipment
tiers** and **modules**, bank samples and insight into **research**, and take
the changed rig back down. The tension that makes it compelling is
**greed-versus-fuel**: the climb home is the expensive half of every run, and
the drill-anchor rule (cutting upward costs only drill fuel ≈ 0.15/tile; flying
an open shaft costs thrust ≈ 0.28/tile but is ~6x faster) turns fuel from a
timer into a route plan, so "one more vein" is always weighed against "will the
tank still cover the way back". This is an original game, not a clone: the
anchor mechanic, the four-pressure model with heat, transit lifts, a three-lane
proficiency tree with mutually exclusive keystones, original world, lore,
names (Weathered Shelf, prismite, the Fracture), art drawn from primitives, and
synthesised audio are all this project's own. No third-party game asset is or
will be reproduced.

Success is a player who, unprompted, starts "one more run" — measured, not
asserted, by the pacing and session targets in *Depth & progression model* and
*Success criteria*.

## Assumptions

1. The operator's direction (Motherload-like, beautiful 2D, deep
   upgrade/research, addictive loop, non-P2W monetisation) **supersedes any
   earlier in-repo brief**, including the original `goal.md` brief wherever the
   two disagree. `goal.md` is gitignored and not present in this working copy;
   per `START-HERE.md` it is the original build brief. No superseded sentence
   could be quoted from it here; when it surfaces, reconcile against this spec
   and record any conflict in a new ADR.
2. The host is headless (Debian 13, RTX 3090, no display server). Interactive
   rendering **cannot** be verified locally. Offscreen/headless verification is
   therefore the **primary local proof** everywhere in this spec; an operator
   eyeball pass on a desktop machine remains a release-time gate only.
3. Monetisation stays offline and storefront-agnostic until the operator names
   a platform. This spec designs the catalog, entitlement plumbing and proof
   harness; it assumes no SDK integration in any slice.
4. Cosmetics remain within the existing asset-free pipeline: procedural and
   data-driven art (palettes, decal patterns, particle profiles), no external
   art or audio files, per ADR 6/ADR 8 and `THIRD_PARTY_NOTICES.md`.
5. Existing multiplayer absence is a decision, not a gap (single-player,
   offline per `GAME_DESIGN.md`); the operator's direction does not ask for it.

## Tech Stack

Pinned and fetched by CMake `FetchContent`; nothing vendored
(`docs/THIRD_PARTY_NOTICES.md`):

| Component | Version | Role |
| --- | --- | --- |
| C++23 | GCC 13+ (verified GCC 13.3), Clang 17+, MSVC 19.38+ | `std::expected` errors, `std::format`; `<print>` unused (GCC 13 lacks it) |
| SDL3 | `release-3.4.16` | Window, input, gamepads, audio, timing, 2D renderer |
| nlohmann/json | 3.11.3 (SHA-256 pinned) | Content and save DTOs |
| Catch2 | v3.7.1 | Tests (`[tags]`, `TEST_CASE`, `CHECK`/`REQUIRE`) |
| CMake ≥ 3.28 + Ninja | presets in `CMakePresets.json` | Build orchestration |

No new runtime dependency is admitted by this spec. Cosmetic and store data
reuse nlohmann/json and the existing content loader; no image or sprite
library, no UI toolkit (ADR 6), no engine.

## Commands

Only presets that exist in `CMakePresets.json` are referenced:
`dev-debug`, `dev-release`, `release`, `headless`, `asan`, `ubsan`, `tsan`,
`ci`. (Windows cross-builds are configured outside this file's scope; the
operator-verified PE32+ build is a release-gate fact, not a daily command.)

```sh
# Inner loop (no display needed): build headless, run all tests
cmake --preset headless && cmake --build --preset headless && ctest --preset headless

# Full client build + suite
cmake --preset dev-release && cmake --build --preset dev-release && ctest --preset dev-release
cmake --preset dev-debug   && cmake --build --preset dev-debug   && ctest --preset dev-debug

# The whole gate before any commit: dev-release + ci + headless + asan + ubsan,
# 76+ tests under sanitizers, headless client smoke test; prints == all green
tools/verify.sh            # or: tools/verify.sh --quick (inner loop only)

# Sanitizer builds of the headless targets
cmake --preset asan  && cmake --build --preset asan  && ctest --preset asan
cmake --preset ubsan && cmake --build --preset ubsan && ctest --preset ubsan
cmake --preset tsan  && cmake --build --preset tsan  && ctest --preset tsan

# Warnings-as-errors build (what CI builds)
cmake --preset ci && cmake --build --preset ci

# Content validation without launching
./build/dev-release/tools/deepforge-validate --data data

# Economy/pacing measurement (pacing exits non-zero if any seed misses the gate)
./build/dev-release/tools/deepforge-balance --data data --seeds 16 --depth 240
./build/dev-release/tools/deepforge-balance --data data --seeds 16 --pacing

# Headless smoke proof of the real client loop (primary local render proof)
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  ./build/dev-release/src/deepforge --data data --user-dir /tmp/dfuser --headless-frames 30

# Deterministic visual regression — works with no display (software renderer)
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  ./build/dev-release/src/deepforge --data data --user-dir /tmp/dfuser \
  --screenshot /tmp/hub.bmp --screenshot-screen hub --screenshot-seed 3407 --screenshot-ticks 2400

# Package (2.3 MiB zip expected)
cmake --build --preset release --target package
```

`--screenshot-screen` accepts exactly the screens main.cpp registers today:
`expedition, menu, hub, map, garage, modules, skills, research, refinery,
contracts, facilities, codex, pause, results, failure, settings, bindings,
credits`.

## Project Structure

Layered CMake targets; each layer links only layers above it in this table,
enforced by target linkage (`docs/ARCHITECTURE.md`):

```
src/core/           deepforge_core        ids, Math, Rng (splitmix64), Noise, Result, Log — std only
src/content/        deepforge_content     ContentDatabase/Loader, Definitions, Effects registry
src/world/          deepforge_world       Chunk (32x32), WorldGenerator, WorldMap; integer-only, deterministic
src/domain/         deepforge_domain      ExpeditionSim, Economy, Inventory, Loadout, PlayerProgress, RigState,
                                          Commands/Events — no SDL, no clock, no filesystem
src/app/            deepforge_app         GameSession (state machine), SaveFile (atomic, checksummed), Autopilot
src/platform/       deepforge_platform    SDL Window/Input/Audio, Settings, OS paths
src/presentation/   deepforge_presentation+SDL   Client (composition root), Ui/Draw/Theme, WorldRenderer,
                                          FontData (generated — never hand-edit)
src/main.cpp        deepforge_game        executable: flag parsing, run_client
tests/unit|simulation|integration|generation/   Catch2 suites, globbed on configure
tools/              deepforge-validate, deepforge-balance, content_gen (vein calibrator, font gen), verify.sh
data/*.json         shipped content, schema-validated at load (see disposition below)
docs/               architecture, design, balance, testing, ADRs (docs/decisions/, append-only)
```

Additions this spec introduces (no layer violations; the store never links
into domain/world/content/core — see *Monetisation*):

- `data/cosmetics.json`, `data/store_catalog.json` — new content, loaded by
  `ContentLoader`, validated by `deepforge-validate`.
- `src/presentation/Cosmetics.hpp/.cpp` — cosmetic state applied at draw time
  only (livery palette swaps, trail/particle profiles, hub decor).
- `src/presentation/StoreFront.*` — catalog UI, ownership ledger; talks to
  platform purchase hooks behind an interface that no-ops offline.
- `tests/unit/StoreContract.test.cpp` — the monetisation invariant test.

**Content data disposition** — every shipped `data/*.json`, what this spec
does with it:

| File | Disposition | Notes |
| --- | --- | --- |
| `biomes.json` | **Reused as-is** | six depth bands, hazards, gates, sky/fog colours already authored |
| `contracts.json` | **Reused as-is** | 37 templates, eligibility-checked offers stay |
| `economy.json` | **Reused as-is** | all loop constants cited in this spec come from here |
| `equipment.json` | **Reused as-is** | 9 tracks, 7–9 tiers, milestone capabilities |
| `facilities.json` | **Reused as-is** | 6 facilities × 4 tiers incl. research lab (timeline hook in slice 2 reads lab tier) |
| `factions.json` | **Reused as-is** | reputation is not spendable — stays |
| `hazards.json` | **Reused as-is** | damage/telegraph/counterplay data cited by the pressure table |
| `landmarks.json` | **Reused as-is** | insight sources, transit nodes |
| `materials.json` | **Reused as-is** | 30 materials; sell values/hardness cited verbatim |
| `modules.json` | **Reused as-is** | 31 modules, power budget, tradeoffs |
| `recipes.json` | **Reused as-is** | refining stays the component tap |
| `research.json` | **Extended** | schema 1 → 2: add `lab_expeditions`; nodes, insight, samples unchanged |
| `skills.json` | **Reused as-is** | proficiency tree untouched |
| `strings.en.json` | **Extended** | grows only as new content (research rows, cosmetics) lands; no key churn |
| `cosmetics.json` | **New (slice 2)** | cosmetic palettes/trails/decor |
| `store_catalog.json` | **New (slice 3)** | entitlements, `kind == "cosmetic"` only |

## Code Style

Real conventions, from `CONTRIBUTING.md` and the existing suites (this snippet
is the shape a new test in this spec must take):

```cpp
// tests/unit/StoreContract.test.cpp
#include "TestSupport.hpp"

#include "content/ContentDatabase.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace df;

TEST_CASE("no purchasable entitlement is readable by the simulation", "[store][p2w]") {
    const auto& db = dft::shipped_content();
    for (const auto& ent : db.store_catalog().entitlements) {
        INFO("entitlement " << ent.id);
        CHECK(ent.kind == EntitlementKind::Cosmetic);
        CHECK(ent.simulation_reads == 0);   // loader-counted: definition must
                                            // carry zero sim-visible effects
    }
}
```

- C++23; `std::expected` for errors, `std::format` for text. Comments say
  **why**, not what.
- Layering downward only: core → content → world → domain → app →
  platform/presentation → game. CMake linkage is the enforcement mechanism.
- The simulation is integer-only and deterministic (ADR 3): a float in world
  generation is a bug. Cosmetic RNG lives on separate streams
  (`WorldRenderer::mCosmeticState`) that can never move ore.
- Formatting is `.clang-format`, not opinion; `.clang-tidy` fixes go into the
  code, not the suppression list. Never format `FontData.hpp` (generated;
  `tools/content_gen/gen_font.py` regenerates it).
- Every player-facing string is authored in `data/strings.en.json`; the loader
  and a test reject missing *and* orphaned keys. Content ids are stable
  strings (`material.ferrite`); runtime uses typed handles.
- Anything an effect can modify is derived at the point of use, never cached in
  `PlayerProgress` (ADR 9).
- Loud failures: a missing string, flag typo or stale cache is a named error at
  load/build time, never silent degradation.

## 2D presentation

**Backend.** Keep the existing SDL3 2D renderer path
(`SDL_CreateRenderer` in `platform/Window.cpp`): it selects the GPU-accelerated
2D backend on desktop and degrades to software automatically when no display
exists — which is exactly the headless CI path, already proven by the
`--screenshot` and `--headless-frames` flows. A migration to the SDL3
`SDL_GPU` API is **out of scope**: it would replace the primitives-based
`Draw`/`Ui` stack (ADR 6) wholesale for no player-visible gain on this host.

**Tile strategy.** Keep the per-chunk texture cache in `WorldRenderer`
(`CachedChunk`): each 32×32 chunk is composited once into an `SDL_Texture` at
the current `pixelsPerTile`, invalidated only when a tile changes or the scale
changes (`invalidate_chunk`/`invalidate_all`), so a frame costs roughly a dozen
quads. Tile art upgrade path: replace the procedural per-tile rectangle
patterns with a **tileset sprite atlas** (one atlas per material category,
32 px tiles, authored in the existing procedural/pattern pipeline) — the chunk
cache then composites *atlas* tiles instead of drawn rectangles. Mechanism
unchanged; art source upgraded.

**Depth layers (new).** Add a parallax background stack behind the tilemap in
`WorldRenderer`: (L0) biome sky/fog gradient from `sky_color`/`fog_color`;
(L1–L3) rock-silhouette bands at parallax factors 0.2× / 0.4× / 0.65× of camera
scroll; rig-attached headlamp glow plus point lights for lava pockets, scanner
pulses and emissive crystals drawn as an additive overlay above the tilemap;
fog colorising toward `fog_color` with depth. All background/light layers are
cosmetic-only — they read the `RenderSnapshot` and never feed back into the
simulation.

**'Beautiful' in testable terms.**
- One art scale: the world renders at 32 px/tile (Camera default), with integer
  zoom steps 1×/2× and letterboxed fit; no fractional scaling, no shimmer.
- Palette per biome depth band, sourced from `biomes.json`
  (`sky_color`, `fog_color`, material `color`), blended across the authored
  40-tile transition bands — palette changes are data, not code.
- Readable silhouettes: verified at 1:1 by cropping and magnifying captured BMP
  screenshots (the ADR 8 rule), not by eyeballing scaled-down images.
- 60 FPS budget on the dev machine (RTX 3090 host): frame time p95 ≤ 16.6 ms at
  2× zoom in the deepest band with particles on, measured via the F3 overlay
  (`draw_calls`, chunk texture count) and asserted in the headless frame run.

**Fallback (headless CI path).** `SDL_VIDEODRIVER=dummy` exercises the real
client loop with a null driver (`--headless-frames N`, exit 0); the software
renderer under `SDL_VIDEODRIVER=offscreen` produces deterministic BMP captures
at a fixed seed and tick count. The screenshot flow is the **primary visual
proof**; visual regression = capture per screen, then compare against a
committed baseline at 1:1 (region mean-abs-diff ≤ 2%, plus the 8× crop read for
silhouette/contrast checks). Baselines update only via reviewed commit. Exact
pixel equality is not required; determinism of seed + tick makes comparisons
stable on identical software.

## Depth & progression model

Depth bands are the six authored biomes in `data/biomes.json` (reused as-is;
depths, hazards and gates already exist). Each band introduces a material
family, a hazard class and a new pressure:

| Band (biome id) | Depth (m) | Introduces | Pressure twist |
| --- | --- | --- | --- |
| `biome.weathered_shelf` | 0–250 | ferrite 5cr, cuprite 13cr; supply caches | tight tier-0 tank (100 fuel); weak collapses |
| `biome.prism_faults` | 250–650 | argentine 34cr, aurelite 72cr, prismite (research sample) | hardness 1.25×, arc hazards; scanner play pays |
| `biome.mycelial_deeps` | 650–1050 | mycelith; bio-modules | spores/corrosion; research-gated entry |
| `biome.ember_mantle` | 1050–1650 | emberglass, platinex 148cr | ambient heat 1.6+, `lava_pocket` (16 contact dmg); cooling builds |
| `biome.machine_strata` | 1650–2400 | cogsteel; ancient components | turrets/pistons; transit network, artifacts |
| `biome.the_fracture` | 2400+ | riftalloy, starmetal 300cr, void cells | mixed extremes; endgame, finale content LATER |

**Fuel vs depth (existing, keep).** Base tank 100.0 units
(`economy.json`); tank tiers add +42 each: 100 → 142 → 184 → 226 → 268 → 310
(tier 5 also −15% hover fuel) → 352 (tier 6, costs 1 refined plate). Climb
costs: drill-anchored ≈ 0.15 fuel/tile vs flying ≈ 0.28/tile at ~6× speed —
the return-plan decision. Thrust 2.5 fuel/s, lateral 0.5/s, idle 0.02/s.

**Hull damage sources (existing, keep).** Fall impact above 13 m/s at 2.4 dmg
per m/s over; hazard contact/area damage (`hazards.json`, e.g. lava pocket 16
contact + 3.0/s area + 26 heat/s); heat above capacity at 6 hull/s; careless
volatile extraction. Zero hull or empty tank triggers rescue: fee capped at
35% of current credits (base 55 + 0.15/depth), first rescue free, ≥45% of the
run's cargo kept (base loss 55%), permanent progression always kept.

**Cargo & sell values (existing, keep).** Base hold 40 units (fills at ~76 m
mean, the intended early-run ender); tiers add +22 each (→ 62 → 84 → 106 →
130 + auto-mark …). Cargo mass cuts acceleration up to 22%, never control.
Sell price = `base_value × (1 + depth_bonus)`, depth bonus +1.5% per 100 m
reached, further shaped by skills (e.g. `skill.assayer` +6%) and refining
recipes. Free service floor on every hub return: 35% fuel / 25% hull.

**First-hour curve (targets the spec holds the build to).**
- Time-to-first-reward: first sale within the first expedition (~1.5 min
  autopilot; ~3–5 min first-time human).
- Time-to-first-upgrade: fuel tank tier 1 (150 cr) reachable in **5–10 min**
  from a clean save on **every seed** — already gated by
  `deepforge-balance --pacing` exiting non-zero otherwise; that gate becomes a
  required Success Criterion.
- Target session: 20–40 min, composed of expeditions growing from ~3–5 min
  (tier 0–1) toward the 8–25 min band by mid-game; the measured early-run
  length gap (`BALANCE_NOTES.md`) is closed with content (lateral landmarks,
  contract detours), never by slowing mining.
- Cadence: scanner + first module + refinery + first contract by ~25 min;
  proficiency branch choice, first facility and research samples by ~60 min.
- The ~30-second decision set during a run: (1) one more vein or turn home —
  fuel against depth; (2) dig a clean return shaft or wander and climb slowly;
  (3) route around granite spurs/hazards or spend hull; (4) cool now or push
  the heat meter; (5) spend a scan pulse or save it; (6) bank this pocket or
  detour for the contract target. On the surface: buy, install, research,
  contract, descend.

## Upgrades and research — two systems, two currencies

**Upgrades = money-shaped, per-run power.** Nine permanent equipment tracks
(`equipment.json`, reused): drill power, drill efficiency, engine, fuel tank,
cargo bay, hull, cooling, scanner, power core — 7–9 tiers each, cost
`round_to_5(base × 1.50–1.63^tier)`, late tiers also costing refined
components. Milestone tiers grant **capabilities**, not percentages (drill
tier 3 shatters brittle clusters; scanner tier 6 sees through rock; engine
tier 7 hover assist). Modules (31, `modules.json`, reused) are swappable
buildcraft on a shared utility-power budget, each strong module carrying a
stated tradeoff. Bought with **credits** — earned by hauling.

**Research = exploration-shaped, capability-unlocking meta-progression.**
Eighteen nodes (`research.json`, **extended**, schema 1 → 2). Currency is
**insight**, earned only from genuinely new discoveries: first material
discovery, landmark first-activation, insight-reward contracts, biome first
entry, depth records. Insight is never purchasable and never farmable by
repetition. A node requires: (a) `insight_cost`; (b) its `prerequisites`
completed (DAG, three lanes: Survey / Materials / Deep Operations, ending in
the capstone); (c) where authored, a **sample** (`requires_sample`, e.g.
`material.echo_shard`) delivered in protected storage and consumed — so
research is paid in risk-taking, not money.

**New in this spec (schema 2 addition): lab timelines.** Each research node
also carries `lab_expeditions: N` (1–3). Progress advances by exactly one per
**completed expedition that returns to the hub** while the node is assigned to
the lab (one active node). No wall-clock, no offline timers, no purchasable
speed-up — consistent with the existing no-waiting-timers design. Effect: a
second, slower cadence layered on the loop (return → research ticks → new
capability soon), so research pace is governed by play, and the two curves
cannot collapse into one: money buys power now, exploration+returns buy
capabilities soon.

**Unlock gating rule (unchanged, restated as law):** biome access beyond the
second band, module purchasability, recipes and passive effects are gated by
**research only**. Credits alone can never open a biome; insight alone can
never raise a stat.

## Interface

The interface stays the instrument-panel system of ADR 6: builders describe
rows in `Client.cpp` screen builders; `Ui.cpp` widgets own all geometry;
`Theme.hpp` owns every colour/spacing token; `Draw.cpp` renders bevelled
panels, segmented gauges and procedural icons. Screens never invent a colour.

- **HUD (expedition screen):** left column — fuel and heat segmented gauges;
  right column — hull and cargo gauges (cargo shows units/capacity and mass
  effect); centre-top — depth readout and currency; contextual drill-anchor
  indicator whenever the anchor braces; event captions (hazard telegraphs,
  discovery, heat spike) appear as labelled toasts at the screen edge —
  **never** over the rig or the drill controls. All gauges are colour +
  icon + label; loss of colour never loses information (colour-vision palette
  variant already exists and stays).
- **Garage/shop UI:** the hub destinations menu opens `Screen::Garage` and
  `Screen::Modules`; each track row shows current tier, next-tier cost, and a
  live **before/after stat preview** in the inspector column; unaffordable rows
  stay focusable and state the reason (never silently skipped).
- **Research screen** (`Screen::Research`): the DAG as a focusable list of
  lanes; each row shows insight cost, sample requirement, lab-timeline
  progress; inspector explains exactly what unlocks and why a node is locked.
- **Input:** keyboard + mouse + SDL gamepad, one shared focus model, fully
  remappable in `Screen::Bindings`; every screen completable by controller
  alone. Existing map retained (W/A/S/D move+drill, E interact, Q scan, U
  emergency reposition, M map, Esc pause; gamepad equivalents per README).
- **Accessibility/usability rules (testable):** every state carries an icon,
  shape or label in addition to colour; text scale and hold-vs-toggle options
  persist; any screen that is not the expedition pauses the simulation
  (pause-safe menus); **no modal may occlude the active control surface during
  play** — in-expedition prompts (rescue preview, pause) are full-screen state
  transitions, not overlays, honouring agents.md delivery §19 (never block
  the operator's input).
- **Verification:** every screen change ships a screenshot capture (the
  `--screenshot-screen` name list above) plus an 8× crop of a text region and a
  gauge region; contrast is checked against `Theme.hpp` tokens, not eyeballed.

## Monetisation — non-P2W, testable

**Phasing (operator direction, 2026-09-16): cosmetics ship first; everything else is designed-for but deliberately not built.**

- **Phase 1 — cosmetics only.** The first release sells nothing but item 1 below. This is the whole monetisation scope of the initial launch, and it is deliberate: cosmetics are the only category that is *structurally incapable* of affecting the simulation, so shipping them first means the store cannot become a balance problem while the game loop is still being tuned. It also means the non-P2W invariant test below is trivially satisfiable at launch and stays as a permanent guard rather than a retrofit.
- **Phase 2 — expansion content packs (approved as a revenue line, see Resolved Decisions).** Not in the first release. Requires the save worldgen-version and content-pack-id fields to be in place and proven, which is why those fields are mandated from slice 1.
- **Phase 3 — non-simulation convenience.** Last, and only if it earns its place. Most "convenience" ideas fail the invariant on inspection; the ones that survive are usually worth giving away free instead.

Keep the later phases in mind while building — specifically: the entitlement layer must be able to express a non-cosmetic `kind` without a schema migration, and the store/simulation separation must already hold — but do not build the Phase 2/3 surfaces now.

**May be sold:**
1. **Cosmetics** — rig liveries/palettes, drill/trail particle profiles, hub
   decor kits. Data-driven, applied in `presentation/` only; may reuse existing
   content ids for palettes but may not alter any material, stat or hazard.
2. **Expansion content** (operator may veto) — standalone content packs (new
   biome band + materials + contracts + research nodes) that add to the world
   for the buyer while the base game remains a complete, finishable game; no
   base-game system may be hollowed out to make the pack necessary.
3. **Convenience that touches no simulation state** — e.g. menu/page
   preferences, extra visual filter presets. Anything that changes a rate, a
   resource, a timer or an outcome is **not** convenience and is forbidden.

**May never be sold:** credits, fuel, hull points, cargo capacity, refined
components, insight, skill points or reputation; depth skips (pre-activated
transit nodes); research or lab-timeline acceleration; any progression-rate
purchase that outpaces play; loot boxes or any randomised paid reward;
time-limited FOMO rotations; anything gameplay-affecting at all.

**The invariant (assertion):** *no purchasable entitlement may appear as an
input to the simulation tick or to any economy formula. The simulation layer
(domain/world/content/core) is closed against the store: it contains zero
references to store/entitlement identifiers, and `data/store_catalog.json`
may define only `kind == "cosmetic"` entitlements whose effect data is empty by
construction.*

**Required test** (`tests/unit/StoreContract.test.cpp`, tagged `[store][p2w]`,
registered in the headless suite so sanitizers run it too):
1. Enumerate every entitlement in `store_catalog.json`; assert `kind ==
   "cosmetic"` and no effect entries readable by domain/world.
2. `grep -r "store_catalog\|entitlement" src/domain src/world src/content
   src/core` → zero hits (hard CI check; CMake additionally does not link the
   store target into `deepforge_domain`/`deepforge_world`).
3. Cross-check: every cosmetic id referenced by the store exists in
   `cosmetics.json`, and every cosmetic is reachable through the normal
   cosmetic palette/particle systems in presentation only.
A paid cosmetic that altered, say, drill heat would fail (1) at the effect
check and fail (2) the moment anyone tried to read it in `domain/`.

## Testing Strategy

**Deterministic, headless first** (existing doctrine,
`docs/TESTING.md`). Five Catch2 suites (`tests/unit`, `tests/simulation`,
`tests/integration`, `tests/generation`, content validator as CTest), no wall
clock, no machine random state, scratch dirs via `dft::temp_dir()`. New work
lands as:

| Concern | Where | Proof |
| --- | --- | --- |
| Dig/fuel/economy math changes | `tests/unit/Economy.test.cpp`, `tests/simulation/Simulation.test.cpp` | formula-level cases like the existing sell/clamp/rescue tests |
| Deterministic-run regression | `tests/simulation` | fixed seed **3407** per host policy: same command stream ⇒ bit-identical final rig/session state hash, two runs, plus ragged-batch equivalence |
| Monetisation invariant | `tests/unit/StoreContract.test.cpp` | table above |
| Visual regression | screenshot captures committed as baselines | offscreen software renderer, no display |
| Pacing/addictiveness gates | `deepforge-balance --pacing` + integration | first upgrade ≤ 12 runs on every seed; expeditions-per-session shape within targets |
| Whole gate | `tools/verify.sh` | `== all green`, 76+ cases, sanitizers clean |

Required commands are the ones listed in the *Commands* section — no invented
presets, no new tooling beyond what exists.

## Scope Ledger

Slice numbering: **slice 1 hardens and re-proves the existing vertical loop**
(the game already ships it); slices 2–3 deliver the operator's new asks. Slice
1 exit is the gate for everything else.

| Feature | Slice | Notes |
| --- | --- | --- |
| Core loop: descend → pressures → extract → sell → upgrade → deeper | **IN SLICE 1** | exists (0.4.0); re-proven by gates |
| One biome band playable (Weathered Shelf 0–250 m) with starter materials (ferrite, cuprite) | **IN SLICE 1** | exists; pacing gate proves it per seed |
| Upgrade tracks with 2–3 reachable tiers (fuel tank, drill power, cargo bay) | **IN SLICE 1** | exists; pacing gate |
| One research branch spendable (Survey lane, insight + sample) | **IN SLICE 1** | exists; covered by economy tests |
| Deterministic-run test at seed 3407 | **IN SLICE 1** | new, small |
| Pacing gate as release blocker | **IN SLICE 1** | exists as tool; made mandatory |
| Headless visual-regression baselines (hub, expedition, garage) | **IN SLICE 1** | new captures off existing renderer |
| Remaining bands 2–6 as authored (Prism Faults → Fracture) | LATER | slice 2+; already generated, needs per-band proof passes |
| Research lab timelines (`research.json` schema 2) | LATER | slice 2 |
| Parallax background stack + lighting/emissive overlay | LATER | slice 2 |
| Tileset sprite atlas art pass (replace procedural tiles) | LATER | slice 2–3 |
| Cosmetics system (`cosmetics.json`, livery/trail/hub decor) | LATER | slice 2 |
| Store catalog + entitlement plumbing + StoreContract test | LATER | slice 3, after cosmetics exist to sell |
| Storefront/payment integration | LATER | blocked on operator choice of platform |
| Expedition-length content fix (lateral landmarks, detours) | LATER | closes the measured 1.4–5 min vs 8–25 min gap |
| Endgame content: finale expedition, artifact chain payoff, New Game+ | LATER | authored content, not systems |
| macOS user-data path fix | LATER | known limitation, small |
| Localisation system beyond `strings.en.json` | LATER | strings table is ready for it |
| Multiplayer / co-op / PvP / trading between players | **OUT** | the entire architecture is single-writer deterministic simulation with one save owner (`GameSession`); networked play would require re-authoring determinism, save ownership, economy sinks and the non-P2W contract. The operator's direction contains no multiplayer ask; time goes to depth and presentation instead |

## Success Criteria

Every criterion is a runnable command or an observable condition:

1. `tools/verify.sh` prints `== all green` (dev-release + ci + headless + asan
   + ubsan; the existing 76-case suite passes, extended by the new tests).
2. `ctest --preset headless` exits 0 with **zero skipped tests**.
3. New dig/fuel/economy unit tests pass:
   `./build/headless/tests/deepforge_tests "[economy]" "[simulation]"` exit 0.
4. Determinism: `./build/headless/tests/deepforge_tests "seed 3407"` passes —
   bit-identical session state across two runs and across ragged batching.
5. `SDL_VIDEODRIVER=dummy ./build/dev-release/src/deepforge --data data
   --user-dir /tmp/dfuser --headless-frames 30` exits 0 (primary local render
   proof).
6. `SDL_VIDEODRIVER=offscreen … --screenshot /tmp/hub.bmp --screenshot-screen
   hub --screenshot-seed 3407` writes a BMP whose region diff vs the committed
   baseline is ≤ 2% mean-abs at 1:1; the 8× crop of a gauge and a text row is
   human-readable at 1:1 (observable condition, recorded in the PR).
7. `./build/dev-release/tools/deepforge-balance --data data --seeds 16
   --pacing` exits 0 — first upgrade reachable on every seed within 12 runs;
   measured first-upgrade time for a human-plausible pilot stays within the
   5–10 min target.
8. `./build/dev-release/tools/deepforge-validate --data data` exits 0,
   including the new `cosmetics.json`/`store_catalog.json` schemas when they
   land.
9. `grep -r "store_catalog\|entitlement" src/domain src/world src/content
   src/core` returns no matches (monetisation invariant, structural half).
10. F3 overlay on the dev machine shows frame time p95 ≤ 16.6 ms during a deep
    band with particles (60 FPS budget) — observable condition.
11. No modal overlaps the rig/drill control region during an active expedition
    (observable condition; verified by capture of a rescue/pause moment).

## Boundaries

**Always:**
- Run `tools/verify.sh --quick` before any commit, full `tools/verify.sh`
  before anything merge-worthy.
- Keep simulation deterministic and displayless-testable; new systems get
  headless proofs (`--headless-frames`, screenshots) in the same change.
- Extend content through `data/*.json` + `strings.en.json` and validate with
  `deepforge-validate`; stable ids only; no orphaned or missing strings.
- Record consequential decisions as append-only ADRs in `docs/decisions/`.

**Ask first:**
- Any new third-party dependency, any change to pinned versions, any new
  CMake option or preset.
- Any schema-version bump of a save or content file (migration plan required).
- Touching the research/economy balance constants in `economy.json` beyond
  what a measurement justifies (re-run `deepforge-balance`, cite numbers).
- Any store/pricing/platform decision (operator-owned).
- Adding fonts, art sources or audio sources outside the existing generators.

**Never:**
- Ship any purchasable that the simulation can read — the monetisation
  invariant and its test are permanent, and the `domain`/`world` layer closure
  against store data is never relaxed.
- Introduce loot boxes, randomised paid rewards, purchasable resources,
  depth skips, research acceleration, energy timers, daily-streak FOMO or
  pay-to-skip mechanics.
- Reproduce third-party game assets (names, art, audio, maps) — original
  content only.
- Break simulation determinism: no wall clock, `std::random_device`,
  float world-gen, or frame-timing dependence in domain/world.
- Format `FontData.hpp` by hand; edit generated files; delete or weaken a
  failing test without replacing it with a better one.
- Block the player's control surface with an occluding overlay during active
  play (agents.md §19).

## Resolved Decisions (operator approval, 2026-09-16)

The three questions this spec raised are decided below, so implementation is not blocked on them.

1. **Distribution: direct/self-hosted first, itch.io second, Steam later. Expansion packs ARE an approved revenue line.**
   The project already produces exactly what direct distribution needs — a CPack zip and a cross-compiled Windows `.exe` — so self-hosted download plus key redemption ships with zero platform dependency, zero revenue cut, and no store review gate. itch.io follows as the low-friction storefront. Steam is deliberately last: Steamworks is a real integration surface (SDK, depots, build pipeline, review) that would pull effort out of the game loop, and nothing in slice 1 needs it.
   Expansion content packs are approved **because content-for-money is not advantage-for-money**. The binding constraint: a pack may add biomes, materials, equipment models, research branches, and cosmetics, but it may **never** gate a balance fix, a quality-of-life improvement, an interface affordance, or an existing-content buff behind purchase. Balance and QoL ship free to everyone, always. A pack that makes the base game worse in order to sell the fix is the failure mode this rule exists to prevent.
2. **Cosmetics stay strictly owner-local.** No shared cosmetic previews, no author-visible rigs, no cosmetic data in any shared or serialized-for-others surface. Screenshot/photo mode renders the owner's own cosmetics only. Rationale: shared previews only earn their complexity once other players can see them, and multiplayer is marked OUT in the Scope Ledger — building the shared-rig plumbing now would be speculative complexity serving a decision nobody has made. This is revisited only if and when multiplayer is adopted.
3. **Expansions extend the world DOWNWARD as deeper strata; separate hub-reachable regions are the later pattern.** Downward extension reuses the existing worldgen and the depth-band progression that is already this game's spine, so the first pack is additive rather than structural. Separate regions are deferred until save/worldgen versioning has been proven in practice by at least one shipped downward extension.
   **Forward-compatibility requirement, binding from slice 1 (not deferred):** every save MUST carry an explicit worldgen version and a content-pack identifier set, and the loader MUST refuse — clearly and without corrupting the file — to load a save that references content the running build does not have. This is required *now* because retrofitting version fields onto saves that already exist in players' hands is the migration nobody can do cleanly. A save written by slice 1 must remain loadable, or explicitly rejectable, by every later build.

## Open Questions

None blocking. This spec is approved for slice-1 implementation as written; further questions are raised against the card that hits them rather than parked here.
