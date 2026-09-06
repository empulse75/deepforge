# Goal: Build Project Deepforge

> This document is the authoritative execution brief for Claude Opus 5. Read it completely before changing code. The assignment is to build and continuously validate a playable game, not merely to propose a design.

## 1. Mission

Act as the lead game designer, gameplay programmer, systems architect, build engineer, and QA owner for a polished original 2D mining-progression game with the working title **Project Deepforge**.

The game may take high-level inspiration from the classic loop of descending underground, mining increasingly valuable materials, managing expedition limits, returning to the surface, selling cargo, upgrading the mining vehicle, and reaching ever-deeper layers. It must use original names, lore, visual identity, code, maps, balance, audio, and assets. Do not copy proprietary source code, sprites, characters, dialogue, exact level layouts, UI, sounds, branding, or story content from Motherload or any other game.

The result must feel immediately understandable to a fan of mining games while being its own modern product with deeper buildcraft, stronger replayability, robust save handling, good input and accessibility support, and maintainable architecture.

The primary implementation language is **modern C++23**. Rust is a contingency fallback only under the conditions in Section 29.

## 2. Required Outcome

Produce a repository that a developer can clone, configure, build, test, and run on Windows and Linux. The final repository must contain:

- A playable desktop game executable.
- A complete core loop: launch, descend, mine, manage constraints, return, sell/process resources, upgrade, and descend farther.
- Procedurally generated, seeded, chunked, persistent underground terrain.
- Several progression systems that interact without overwhelming the player.
- Data-driven resources, biomes, items, modules, upgrades, perks, contracts, and economy values.
- Versioned, atomic save/load with migration support and corruption recovery.
- Keyboard/mouse and controller support with remapping.
- Automated tests for deterministic simulation, generation, economy, inventory, upgrades, and persistence.
- Clear developer and player documentation.
- A reproducible CMake-based build with debug, release, sanitizer, and test presets.
- A playable greybox early in development, followed by iterative vertical slices.
- Original placeholder presentation sufficient to make the game readable and satisfying even before final art exists.

Do not end the assignment with only scaffolding, pseudocode, a design document, disconnected systems, or a non-running demo. Every completed milestone must build and remain playable.

## 3. Product Principles and Priority Order

When requirements compete, use this priority order:

1. A fun, responsive, readable mining loop.
2. A buildable and stable game with no save-destroying bugs.
3. Meaningful progression choices and good pacing.
4. Clean, testable, data-driven architecture.
5. Performance, accessibility, and polish.
6. Content breadth.
7. Stretch features.

The core principles are:

- **One more vein:** the player can often see or infer one nearby opportunity worth pursuing.
- **Push your luck:** deeper routes offer better rewards while fuel, cargo, heat, and hull create understandable risk.
- **Every return matters:** a normal expedition should advance at least one visible goal.
- **More than bigger numbers:** progression should unlock new actions, route choices, synergies, and play styles.
- **Readable causality:** the player should know why a block broke slowly, why damage occurred, and what an upgrade changed.
- **Fair failure:** mistakes have consequences, but one failure must not erase hours of progress.
- **Layered discovery:** mechanics and progression systems unlock gradually.
- **Healthy engagement:** aim for compelling play through mastery, anticipation, discovery, buildcraft, and clear goals. Do not add loot boxes, premium currencies, energy timers, daily streak pressure, artificial waiting, FOMO, manipulative notifications, or pay-to-skip design.

## 4. Default Product Decisions

Treat these as approved defaults unless an existing repository clearly establishes compatible alternatives:

| Area | Decision |
| --- | --- |
| Genre | Offline single-player 2D mining, exploration, and progression game |
| Camera | Side view with a surface hub and a very deep underground world |
| Presentation | Crisp stylized 2D, readable at 1080p through 4K; placeholder geometric art is acceptable during development |
| Target platforms | Windows 10/11 and modern x86-64 Linux |
| Input | Keyboard/mouse and common SDL-compatible controllers |
| Simulation | Deterministic fixed update at 60 Hz with interpolated rendering |
| World | Seeded procedural generation, 32 x 32 tile chunks, persistent modifications |
| Session shape | Expeditions normally last 8–25 minutes; surface planning between runs |
| First ending | Approximately 6–10 hours for a new player after balancing |
| Long tail | Optional build experimentation, contracts, discoveries, challenge modifiers, and New Game+ |
| Networking | None required; the game must work completely offline |
| Monetization | Out of scope; never design around monetization |

### Explicit non-goals for version 1

- Multiplayer, accounts, cloud services, live-service events, leaderboards, or a backend.
- A general-purpose engine/editor.
- Fully destructible continuous terrain; use an efficient tile/chunk model.
- Complex colony management.
- Combat becoming more important than mining and route planning.
- Hundreds of low-impact items that differ only by tiny percentage changes.
- Procedural generation that can produce impossible progression.
- Runtime dependence on an internet connection.

## 5. Player Fantasy and Core Loop

The player pilots and gradually rebuilds a compact subterranean prospecting rig above an ancient, unstable world. The rig begins improvised and fragile. Over time it becomes a specialized machine reflecting player choices: efficient surveyor, high-risk speed miner, armored deep explorer, thermal engineer, explosive excavator, cargo hauler, or artifact hunter.

The core loop is:

1. At the surface hub, select an objective, configure modules, refuel, repair, process cargo, and choose upgrades.
2. Enter the mine through the persistent shaft or a discovered access point.
3. Navigate existing tunnels or drill new routes.
4. Read geology, locate veins, mine blocks, collect resources, and discover anomalies.
5. Manage fuel, cargo mass, hull integrity, heat, hazards, and the distance to safety.
6. Decide whether to return safely or push deeper for a higher-value target.
7. Return through normal travel, a costly extraction method, or a discovered transit route.
8. Sell commodities, refine strategic materials, complete contracts, research discoveries, and install upgrades.
9. Unlock a deeper biome or a new way to interact with the world.
10. Repeat with a meaningfully different capability, route, or goal.

### Engagement cadence

Design rewards on several time horizons:

| Horizon | Expected reward |
| --- | --- |
| Seconds | Responsive movement, clear drilling progress, particles, sound, resource pickup, vein continuation |
| 2–5 minutes | A valuable pocket, hazard solution, cache, landmark, or contract step |
| 8–25 minutes | A successful expedition, sale, refinement batch, module, perk point, or facility improvement |
| 30–90 minutes | New biome, new mechanic, build archetype, faction tier, major research, or set-piece discovery |
| Several hours | A specialized build, major landmark chain, ending, challenge layer, or New Game+ modifier |

Do not inflate playtime with slow walking, opaque resource requirements, compulsory repetitive trips, or upgrades whose effect cannot be felt.

## 6. Moment-to-Moment Gameplay

### 6.1 Vehicle movement

- Movement must feel responsive but retain weight.
- Use smooth acceleration, braking, controlled falling, short hover/thrust correction, and collision resolution suitable for a grid world.
- The vehicle may occupy more than one visual tile, but collision and mining interaction must remain predictable.
- Horizontal traversal and descending should be easy at the start. Ascending against gravity should consume more fuel and create the main return-planning pressure.
- Never allow ordinary collision resolution to trap the player permanently inside terrain.
- Provide a deterministic unstuck safety action that moves the rig to the nearest valid position at a modest fuel or durability cost and logs why it occurred.
- Support coyote-time-like input tolerance where it improves controls, but do not make physics nondeterministic.

### 6.2 Mining

- Aim and mine in four cardinal directions at minimum.
- Holding the mine input applies visible progress to the targeted tile.
- Drilling time depends on material hardness, drill power, tool condition/modifiers, heat, and relevant perks.
- The tile must show cracks/progress and an estimated difficulty tier before it breaks.
- Blocks yield configured material amounts and can expose neighboring veins, pockets, caverns, hazards, or support weaknesses.
- Distinguish ordinary rock, sellable ore, strategic crafting material, volatile material, artifact-bearing material, structural blocks, fluid/gas emitters, and indestructible boundary/special tiles.
- Add satisfying but configurable hit-stop, shake, particles, sound pitch variation, light flashes, and pickup feedback. All screen shake and flashes must have accessibility settings.
- Mining must be interruptible without losing accumulated progress immediately. A block may slowly recover progress after a data-driven delay.

### 6.3 Expedition constraints

Use four primary pressures, each with a different purpose:

- **Fuel:** long-term range budget, consumed by thrust, movement assistance, and some tools.
- **Cargo:** forces prioritization and return decisions; mass may affect acceleration slightly but must never make the game miserable.
- **Hull:** absorbs collision and hazard damage; zero hull triggers a rescue/failure flow.
- **Heat:** a short-term tactical meter affected by depth, drilling, lava, boosting, and modules. Heat should create pauses, route choices, or cooling builds rather than act as a second fuel bar.

Do not add oxygen as a default global meter. It may exist later as a biome-specific hazard only if it creates unique decisions.

### 6.4 Failure and recovery

- When hull reaches zero or fuel is exhausted in an unrecoverable situation, offer a rescue.
- Preserve permanent progression, installed equipment, research, skill choices, discoveries, and base upgrades.
- Recover all common quest-critical items automatically.
- Apply a clearly previewed loss to unbanked commodity cargo and a repair/towing fee capped so it cannot create a debt spiral.
- Optionally create a recoverable wreck cache containing part of the lost cargo.
- Provide one early free rescue and make insurance/recovery technology a legitimate build option.
- Autosave before resolving the failure and again after resolution so save scumming cannot corrupt state.

## 7. World Model and Procedural Generation

### 7.1 Coordinates and chunks

- Use signed 64-bit tile coordinates. Positive depth increases downward.
- Default chunk size is 32 x 32 tiles and must be a compile-time or centralized configuration constant.
- Generate chunks deterministically from world seed, chunk coordinate, biome definition, and independent named random streams.
- The same seed and content version must reproduce the same unmodified chunk on every supported platform.
- Store only necessary persistent deltas for generated chunks: mined tiles, placed devices, revealed discoveries, hazard state, containers, and other mutations.
- Use an LRU memory cache. Generate or load nearby chunks asynchronously when safe, then publish immutable/generated results to the main simulation at deterministic boundaries.
- Never mutate active gameplay state from worker threads.
- World generation must have bounded work and never block a frame for an unbounded flood fill.

### 7.2 Generation pipeline

Implement generation as composable, testable passes:

1. Resolve biome and transition weights from depth and world rules.
2. Fill base geology.
3. Carve caves, shafts, and local navigable spaces.
4. Place resource fields and veins using material-specific rules.
5. Place hazards with safe-distance and telegraph constraints.
6. Place authored procedural stamps such as ruins, caches, vents, stations, and transit nodes.
7. Validate local spawn safety and required connectivity.
8. Record deterministic metadata used by scanners and map fog.

Each pass receives an explicit random stream. Adding particles, UI randomness, or an unrelated feature must not change ore placement for an existing seed.

### 7.3 Biome plan

Names are working names and may be improved while preserving their gameplay roles:

| Biome | Approximate depth | New pressure | New opportunity |
| --- | ---: | --- | --- |
| Weathered Shelf | 0–250 m | Weak cave-ins, limited fuel | Basic metals, abandoned caches, tutorial landmarks |
| Prism Faults | 250–650 m | Hard crystal, reflected heat pockets | Scanner gameplay, crystal circuits, branching veins |
| Mycelial Deeps | 650–1,050 m | Spores, living obstructions, corrosive zones | Regrowing paths, bio-materials, symbiotic modules |
| Ember Mantle | 1,050–1,650 m | High ambient heat, lava vents, volatile ore | Thermal energy, high-grade alloys, cooling buildcraft |
| Machine Strata | 1,650–2,400 m | Active defenses, magnetic disruption | Ancient components, transit network, major artifacts |
| The Fracture | 2,400 m and beyond | Mixed extreme anomalies | Endgame resources, final expedition, challenge rules |

Depth values and counts must be data-driven and balanceable. Transitions should be gradual, with visible warning tiles and a landmark gate rather than a hard invisible line.

### 7.4 Generation fairness requirements

- Every world seed must allow access to all mandatory progression resources.
- Critical resources must use pity/guarantee rules after a bounded search radius or number of chunks.
- The first several chunks around the entry shaft must obey a tutorial-safe template.
- No unavoidable instant-damage hazard may be hidden behind an ordinary first-layer block.
- A player must always have a feasible route back using the capabilities required to reach a region.
- Landmark placement must use deterministic minimum/maximum distance constraints.
- Add automated generation sweeps over at least 1,000 seeds checking reachability proxies, resource guarantees, invalid tile references, and extreme density.

### 7.5 World events and landmarks

World interest should come from a mixture of:

- Large visible ore veins.
- Caverns that trade easy movement for fewer mineable resources.
- Collapsed facilities and original environmental storytelling.
- Repair/refuel caches.
- Thermal vents and cooling pockets.
- Ancient transit nodes that become permanent shortcuts after activation.
- One-way crumbly layers that change return routing.
- Optional challenge rooms centered on mining, timing, or hazards.
- Rescue beacons and lost-prospector contracts.
- Artifact chambers with a small authored rule set and procedural placement.
- Biome-scale set pieces that punctuate progression.

Avoid filling the underground with constant combat. Hostile organisms or machines can exist as hazards/actors, but most problems should be solvable by route choice, tools, timing, or engineering.

## 8. Progression Systems

Implement all systems below, but unlock them gradually. Each must provide a distinct decision space.

### 8.1 Permanent equipment tracks

Core rig upgrades are reliable, permanent progression:

- Drill power.
- Drill efficiency.
- Engine thrust and braking.
- Fuel tank capacity.
- Cargo capacity.
- Hull strength.
- Cooling capacity.
- Scanner range/detail.
- Utility power capacity.

Most tracks should have 6–10 meaningful tiers. Do not expose every tier as the same percentage increase. At milestone tiers add qualitative effects, such as mining brittle clusters in one action, seeing vein direction, braking without fuel, surviving one heat spike, or marking high-value cargo automatically.

### 8.2 Swappable modules and buildcraft

Modules create specialization and should be freely swappable at the surface:

- Drill modules: focused cutter, wide fracture head, pulse drill, precision extractor.
- Mobility modules: afterburner, stabilizer, grappling anchor, low-fuel hover.
- Survival modules: heat sink, kinetic buffer, emergency seal, insulated plating.
- Survey modules: directional scanner, material filter, hazard imager, landmark triangulator.
- Utility modules: cargo compactor, auto-collector, deployable beacon, portable refinery.

Use a small number of slot categories or a shared power budget rather than arbitrary inventory friction. Modules should have:

- A stable definition ID.
- Slot/power cost.
- Clear positive effect.
- At least one understandable tradeoff when powerful.
- Tags used for synergies.
- Upgrade path or variant only when it changes play.

Target for version 1: at least 24 meaningfully distinct modules supporting six or more viable build archetypes.

### 8.3 Skill/proficiency tree

Grant proficiency through play and major objectives, not repetitive grinding. Use three broad branches:

- **Prospector:** scanning, vein prediction, yield, contract bonuses, artifact clues.
- **Engineer:** heat, repairs, modules, deployables, refining, fuel efficiency.
- **Pathfinder:** mobility, hazard resistance, recovery, shortcuts, expedition endurance.

Target at least 30 nodes, including several mutually exclusive keystones or limited respec choices. Small numeric nodes may connect the tree, but major nodes must change behavior. Respec must be available for a fair in-game cost and never require starting over.

### 8.4 Research and discovery

Research converts new samples, scans, and artifacts into capabilities:

- The player discovers an unknown item underground.
- It occupies protected sample storage rather than normal cargo when appropriate.
- Returning it unlocks a research entry, experiment, or choice.
- Research can unlock blueprints, biome resistance, scanner modes, processing recipes, lore, or access to a landmark.

Research should gate novel mechanics, not merely serve as another currency shop.

### 8.5 Surface base progression

The hub visibly and mechanically improves:

- Garage: equipment tiers and loadout presets.
- Refinery: converts raw resources into strategic materials with recipes and optional by-products.
- Research lab: analyses discoveries and opens the tech web.
- Contract terminal: objectives, factions, and rewards.
- Recovery bay: repair, insurance, wreck tracking, rescue improvements.
- Logistics lift: deeper entry points and activated transit links.

Base facilities should change available actions and presentation. Keep management compact: no waiting timers and no need to babysit production.

### 8.6 Contracts and faction reputation

Use contracts to give expeditions alternate goals:

- Deliver a material mix.
- Recover a marked cache.
- Scan a biome anomaly.
- Mine a volatile sample without destabilizing it.
- Activate or repair infrastructure.
- Reach a depth and return within a constraint.
- Rescue or locate a missing expedition.
- Complete a route without a particular module class.

Contracts must be generated from validated templates and never request locked, unavailable, or exhausted content. Story/major contracts may be authored. Reputation unlocks themed modules, cosmetics, blueprints, and alternative solutions—not raw mandatory stat dominance.

### 8.7 Collection, codex, and mastery

- Record discovered materials, best depth, landmarks, biome observations, and artifact sets.
- The codex should reveal useful mechanical information, not just lore.
- Add optional mastery goals such as extracting a pristine volatile crystal, completing a no-damage expedition, or mapping a biome.
- Cosmetics and badges may reward mastery without blocking progression.

### 8.8 Endgame and New Game+

Only after the main progression is complete:

- Provide a final multi-stage expedition or set-piece centered on everything learned.
- Allow continued play after the ending.
- Offer optional New Game+ world modifiers, new module variants, alternate biome rules, or a higher-depth anomaly layer.
- Preserve earned cosmetics/codex and offer explicit confirmation before any reset.
- Do not rely on infinite exponential numbers as the only endgame.

## 9. Unlock Cadence

Do not show every system at once. A suggested cadence:

| Player progress | Systems introduced |
| --- | --- |
| First 10 minutes | Movement, mining, fuel, cargo, selling, repair, one obvious upgrade |
| First 25 minutes | Scanner, first module choice, refinery preview, first contract |
| First hour | Skill branches, first base facility choice, second biome, research sample |
| Hours 1–3 | Loadout presets, factions, landmarks, transit node, synergistic modules |
| Hours 3–6 | Advanced refining, mutually exclusive keystones, deep hazards, artifact chain |
| Endgame | Final region, capstone research, challenge modifiers, optional New Game+ |

Every tutorial must be contextual, short, dismissible, reviewable in the codex, and usable with controller input. Prefer one concept plus immediate practice over large text panels.

## 10. Economy and Balance

### 10.1 Resource roles

Every material must have one or more explicit roles:

- Commodity primarily sold for credits.
- Upgrade input.
- Refining ingredient.
- Contract target.
- Research/sample material.
- Volatile/high-risk cargo.
- Artifact component.

Avoid a resource that becomes permanently useless. Late-game sinks, contracts, alternative recipes, or conversion can retain value without making hoarding mandatory.

### 10.2 Currencies

Use at most three global progression currencies:

- **Credits:** repairs, fuel, common upgrades, module purchases.
- **Refined components:** crafted from particular ores for advanced equipment and facilities.
- **Insight:** earned from genuinely new discoveries and major objectives for research/skills.

Faction reputation is not spendable currency. Individual material stacks remain ingredients, not extra abstract currencies.

### 10.3 Baseline formulas

Centralize and data-drive coefficients. Use formulas similar to these as initial baselines, then tune through simulation and playtesting:

~~~text
effective_drill_power =
    base_drill_power
    * equipment_multiplier
    * module_multiplier
    * perk_multiplier
    * heat_efficiency

mine_seconds =
    clamp(base_mine_seconds * pow(tile_hardness / effective_drill_power, 0.85),
          minimum_mine_seconds,
          maximum_mine_seconds)

fuel_cost =
    base_action_cost
    * pow(tile_hardness, 0.60)
    / max(efficiency, epsilon)

upgrade_cost(tier) =
    round_to_5(base_cost * pow(growth_factor, tier))
~~~

Recommended initial growth factors are 1.45–1.75 depending on the track. Qualitative milestone upgrades may cost more. Do not let multiplicative modifiers stack without caps or diminishing returns.

### 10.4 Pacing targets

- The first upgrade should be affordable after one short successful expedition, roughly 5–10 minutes.
- During the first hour, a normal return should complete or materially advance a visible goal.
- A player making reasonable choices should not need to repeat an identical low-risk route more than twice for a mandatory upgrade.
- New biome access should require demonstrating the prior mechanic, not only paying a large fee.
- Ordinary repairs/refueling should remove some profit but rarely consume most of a successful haul.
- Optional aspirational upgrades may require several targeted runs.
- No mandatory upgrade may depend on a single extremely rare random drop without a deterministic fallback.

### 10.5 Economy safety checks

Build a headless balance simulator or deterministic expedition model able to estimate:

- Expected credits and materials by depth and loadout tier.
- Time to first upgrade and biome unlock.
- Repair/fuel expense ratios.
- Value per cargo slot and per minute.
- Contract reward competitiveness.
- Whether any repeatable conversion creates free value.
- Whether selling a progression-critical material can permanently soft-lock a save.

Add assertions/invariants for nonnegative quantities, bounded modifiers, valid recipes, no circular zero-cost recipes, and no integer overflow.

## 11. Content Targets

Use staged targets so quality comes before volume:

| Content | Greybox MVP | Vertical slice | Version 1 target |
| --- | ---: | ---: | ---: |
| Biomes | 1 | 3 | 6 |
| Mineable materials | 6 | 12 | 20+ |
| Equipment tracks | 6 | 8 | 9 |
| Modules | 4 | 12 | 24+ |
| Skill nodes | 0–6 | 18 | 30+ |
| Hazards | 2 | 6 | 10+ |
| Base facilities | 2 | 4 | 6 |
| Contract templates/authored contracts | 3 | 12 | 30+ combined |
| Landmark types | 2 | 8 | 15+ |
| Major artifacts/research discoveries | 1 | 6 | 15+ |
| Major set pieces | 0 | 1 | 3 including finale |

Counts are quality floors, not a reason to create filler. If a content item cannot create a new decision, merge or cut it.

## 12. UI, UX, and Accessibility

### 12.1 Required screens

- Boot/loading with useful error reporting.
- Main menu: continue, new game, load slot, settings, credits, quit.
- Save slot creation with optional seed entry and randomized seed.
- Surface hub screen/world with clearly accessible facilities.
- Expedition HUD.
- Pause menu.
- Inventory/cargo inspection.
- Map with fog, markers, route history option, landmarks, and filters.
- Equipment/loadout screen with before/after stat previews.
- Shop/refinery/research/skills/contracts/codex screens.
- Expedition result summary.
- Failure/rescue screen that previews consequences.
- Settings and complete control remapping.

### 12.2 Expedition HUD

Show at minimum:

- Fuel, hull, heat, cargo used/capacity.
- Current depth and biome.
- Contextual mining target, hardness, progress, and expected category.
- Active contract/objective summary.
- Critical status warnings.
- Small scanner/minimap information after unlocked.

Use icon plus text/color where possible; never communicate critical state by color alone.

### 12.3 Accessibility requirements

- Fully remappable keyboard and controller actions.
- Adjustable controller dead zones, sensitivity, vibration, and aim behavior.
- UI scale and readable font sizes.
- Color-vision-friendly palettes or filters for material/hazard categories.
- Text labels/patterns in addition to ore color.
- Screen shake slider including zero.
- Flash reduction and particle density options.
- Hold/toggle options for mining and thrust where applicable.
- Subtitles/captions for meaningful audio cues.
- Pause during tutorials and menu navigation.
- Adjustable difficulty assists for fuel pressure, damage, heat, and mining duration that do not shame the player.

## 13. Visual and Audio Direction

Do not block gameplay on final assets.

- Start with original geometric tiles, strong silhouettes, material patterns, particles, simple lighting, and generated placeholder icons.
- Keep visual data independent from simulation data.
- Support layered tile rendering, decals, background parallax, actors, particles, lights, and UI.
- Use texture atlases where appropriate and avoid one draw call per tile.
- Add camera look-ahead, damping, bounds behavior near the surface, and optional shake.
- Make valuable veins readable before they are fully exposed through subtle edge clues unlocked or enhanced by scanning.
- Audio needs distinct feedback for drill contact, break, pickup, heat danger, hull damage, cargo full, low fuel, discovery, return, purchase, and upgrade.
- Vary pitch/selection deterministically or cosmetically without affecting simulation.
- If external assets are used, verify their license, store attribution, and never assume a search result is reusable. Prefer self-created placeholders until licensed assets are deliberately selected.

## 14. Primary Technical Stack

Use:

- **C++23** with RAII, strong types where useful, explicit ownership, and no owning raw pointers.
- **CMake 3.28+** with target-based configuration and CMakePresets.json.
- **SDL3** for windowing, input, controller support, timing, filesystem helpers, and platform integration.
- SDL3 renderer/GPU facilities for the initial 2D renderer, hidden behind a narrow presentation interface where it provides real value.
- **SDL3_image** and **SDL3_mixer** if needed for production assets/audio.
- **glm** or small project-owned math types, but do not maintain two competing math systems.
- **EnTT** for dynamic actors/effects only if it simplifies implementation. Do not represent every terrain tile as an ECS entity.
- **nlohmann/json** for definitions and save DTOs, with explicit validation.
- **spdlog** and fmt-style formatting for structured logs.
- **Catch2** for unit/integration tests.
- **Dear ImGui** only for developer tools, inspectors, and balancing panels, never as the shipping player UI.
- **Tracy** as an optional compile-time profiling integration after the vertical slice.

Pin dependency versions through a vcpkg manifest, CPM.cmake lock strategy, or another reproducible method. Do not fetch unpinned moving branches. Record the dependency mechanism in README.md.

If an existing repository already uses a healthy equivalent stack, preserve it when doing so reduces risk and still satisfies the architecture. Do not churn dependencies for aesthetics.

## 15. Architecture

### 15.1 Dependency direction

Use these conceptual layers:

| Layer | Responsibilities | May depend on |
| --- | --- | --- |
| Core | IDs, math helpers, deterministic RNG, time units, result/error types | Standard library |
| Domain/Simulation | Player state, mining, inventory, economy, progression rules, commands/events | Core |
| World | Chunks, generation, tile queries, persistence deltas, landmarks | Core, selected domain DTOs |
| Application | Game state orchestration, update order, use cases, save coordination | Core, Domain, World |
| Presentation | Rendering, UI, animation, audio feedback, camera | Read-only views/events from lower layers, platform abstraction |
| Platform | SDL window/input/audio/filesystem, jobs, OS paths | Core and third-party platform libraries |
| Content | Definition loading, schema validation, stable ID resolution | Core, data DTOs |

Simulation code must not call SDL rendering, UI, wall-clock time, random_device, or filesystem APIs. Presentation may observe simulation but must not own authoritative state.

### 15.2 Practical architecture rules

- Prefer simple value types and composition.
- Use interfaces at genuine seams: renderer, audio, input source, save storage, clock, and job dispatch. Do not create an interface for every class.
- No global mutable state, hidden singletons, or service-locator grab bags.
- The application composition root owns long-lived services and wires dependencies explicitly.
- Keep definition data immutable after successful load.
- Separate definition objects from runtime instances. A MaterialDefinition is content; an InventoryStack is save state.
- Use stable string IDs in data/save files and resolved typed handles at runtime.
- Route player intent through commands such as MoveIntent, MineCommand, UseToolCommand, and InteractCommand.
- Publish typed domain events such as TileMined, CargoAdded, VehicleDamaged, ResearchUnlocked, and ExpeditionEnded.
- Avoid an unbounded universal event bus. Use explicit queues scoped to the simulation tick and well-defined consumers.
- Never allow presentation-only event order to alter gameplay.
- Keep systems small and define their update order in one obvious location.
- Do not overuse inheritance. Use variants, components, strategies, and data definitions where behavior actually varies.
- Errors at content/load boundaries need context-rich messages including file, definition ID, field, and suggested correction.

### 15.3 Fixed-step update order

Use a fixed 60 Hz simulation accumulator with a maximum catch-up step count. Pause or degrade gracefully rather than spiral after a long stall.

Recommended authoritative order:

1. Sample input and convert it to a tick command buffer.
2. Apply menu/game-state transitions scheduled for the tick.
3. Update player intent, equipment modifiers, and action state.
4. Resolve movement and terrain collision.
5. Resolve mining/tool progress and tile mutations.
6. Update hazards, actors, deployables, and environmental fields.
7. Resolve damage, heat, fuel, pickups, and inventory.
8. Apply contracts, progression, discoveries, and economy events.
9. Queue save-safe state changes and audio/visual events.
10. Produce an immutable render snapshot/interpolation state.

### 15.4 Game state flow

Implement explicit states rather than scattered Boolean flags:

- Boot.
- MainMenu.
- LoadingOrCreatingWorld.
- SurfaceHub.
- Expedition.
- Paused overlay.
- ExpeditionResults.
- FailureRecovery.
- ErrorRecovery.

Transitions validate prerequisites and define save boundaries. Pausing freezes authoritative simulation but not necessarily UI animation/audio fades.

### 15.5 Terrain is not ECS

A chunk should use compact, cache-friendly arrays/structures for:

- Tile definition IDs/handles.
- Variant and flags.
- Mining damage/progress only when nonzero, preferably sparse.
- Revealed/scanned/fog state.
- Optional hazard/structure references.
- Dirty/persistence flags.

Actors, pickups, deployables, particles, doors, or dynamic hazards may use ECS. Keep rendering metadata out of authoritative tile storage when it can be derived.

## 16. Suggested Repository Structure

Adapt names to an existing repository without violating layer boundaries.

~~~text
/
  CMakeLists.txt
  CMakePresets.json
  vcpkg.json
  README.md
  LICENSE
  goal.md
  cmake/
  docs/
    GAME_DESIGN.md
    ARCHITECTURE.md
    CONTENT_AUTHORING.md
    SAVE_FORMAT.md
    BALANCE_NOTES.md
    TESTING.md
    THIRD_PARTY_NOTICES.md
    decisions/
  assets/
    audio/
    fonts/
    shaders/
    textures/
    ui/
  data/
    schemas/
    materials.json
    biomes.json
    equipment.json
    modules.json
    skills.json
    recipes.json
    facilities.json
    contracts.json
    landmarks.json
    localization/
  src/
    app/
    core/
    content/
    domain/
    platform/
    presentation/
    world/
    main.cpp
  tests/
    unit/
    integration/
    generation/
    simulation/
  tools/
    balance/
    content_validator/
  .github/workflows/
~~~

Create separate CMake targets such as deepforge_core, deepforge_game, deepforge_content_validator, and deepforge_tests. Domain/world unit tests must not need a window or GPU.

## 17. Data-Driven Content

### 17.1 General rules

- Keep player-facing content out of C++ switch statements wherever practical.
- Validate all data before entering gameplay.
- Reject duplicate IDs, unresolved references, invalid ranges, negative costs, impossible unlocks, and circular recipes.
- Report all validation failures in one run when possible.
- Include a content schema version.
- Support debug hot reload only at safe boundaries; shipping builds may load once.
- Localizable text uses stable localization keys.
- Balance constants belong in definitions/config, not scattered magic numbers.

### 17.2 Example material shape

This is illustrative; improve the schema while preserving the information:

~~~json
{
  "id": "material.prismite",
  "name_key": "material.prismite.name",
  "category": "strategic",
  "hardness": 2.4,
  "density": 1.2,
  "base_value": 38,
  "cargo_units": 1,
  "yield": { "min": 1, "max": 3 },
  "generation": {
    "biomes": ["biome.prism_faults"],
    "min_depth": 280,
    "max_depth": 900,
    "vein_size": [4, 14],
    "rarity_weight": 0.18
  },
  "tags": ["crystal", "conductive"],
  "visual_id": "tile.prismite",
  "break_audio_id": "audio.crystal_break"
}
~~~

### 17.3 Example module shape

~~~json
{
  "id": "module.thermal_reclaimer",
  "name_key": "module.thermal_reclaimer.name",
  "description_key": "module.thermal_reclaimer.description",
  "slot": "utility",
  "power_cost": 2,
  "unlock": "research.thermal_recovery",
  "effects": [
    {
      "type": "convert_heat_to_fuel",
      "threshold": 0.70,
      "rate": 0.04,
      "max_per_second": 0.6
    }
  ],
  "tradeoffs": [
    { "type": "cooling_multiplier", "value": 0.88 }
  ],
  "tags": ["thermal", "efficiency"]
}
~~~

Use a typed effect registry/factory with schema validation. Do not build a scripting language prematurely. If content eventually needs complex behavior, add a narrow, sandboxed mechanism after concrete use cases exist.

## 18. Determinism and Randomness

- Implement a documented project-owned deterministic PRNG wrapper with serializable state or reproducible derived streams.
- Never use implementation-dependent standard-library distributions for cross-platform world layout if their results may differ between libraries.
- Derive streams by hashing world seed plus stable domain labels and coordinates.
- Separate generation, loot, contract, cosmetic, and audio-variation streams.
- Tests must compare stable hashes of representative generated chunks for known seeds.
- Save the world seed, generation/content version, active stream state where necessary, and world deltas.
- Cosmetic randomness need not be saved, but it must never feed back into simulation.

## 19. Save, Load, and Migration

Save data is a product feature, not an afterthought.

### 19.1 Save requirements

- Use the platform-appropriate user data directory obtained through SDL/platform helpers.
- Support multiple named/numbered slots plus an autosave.
- Save player progression, rig/loadout, inventory, facilities, research, skills, contracts, world seed, generated landmarks, persistent chunk deltas, settings where appropriate, and play statistics.
- Never serialize raw pointers, transient ECS entity numbers without a stable mapping, renderer resources, or platform handles.
- Include save schema version, content version, build version, timestamp, playtime, checksum/hash, and a human-readable summary.
- Write to a temporary file, flush/sync as reasonably supported, validate, then atomically replace the main file.
- Retain at least one rotating known-good backup.
- On load failure, leave the damaged file untouched, show a useful error, and offer the backup.
- Implement explicit migrations from every released schema version. Never silently discard unknown required data.
- Test round trips and migration fixtures.

### 19.2 Autosave boundaries

Autosave:

- On successful return to the hub.
- After purchases, research, skill changes, and facility upgrades.
- Before and after failure resolution.
- Periodically during an expedition only from a coherent snapshot and without visible frame hitching.
- On clean quit.

Never save halfway through applying a transaction or tile mutation batch.

## 20. Performance and Resource Budgets

Target modest gaming PCs, not only the development machine.

- Maintain 60 FPS at 1080p on a reasonable integrated/discrete GPU baseline and support higher refresh rates where possible.
- A normal simulation tick should remain comfortably below 4 ms on the target CPU in representative late-game scenes.
- Avoid per-frame heap allocation in hot gameplay/render loops after warm-up.
- Keep visible tile rendering batched/cached.
- Chunk generation should be bounded and occur off the main thread when it can be made safe.
- Chunk publication/load near the player should normally avoid a frame over 16.7 ms.
- Menus must not continuously rebuild expensive layouts or reload assets.
- Asset ownership uses RAII and central caches with clear lifetime.
- Add debug counters for frame time, tick time, draw calls/batches, active chunks, generation queue, memory estimate, actors, and save size.
- Profile before optimizing complex code. Record measured hotspots and changes in BALANCE_NOTES.md or a performance note.

## 21. Quality, Testing, and Tooling

### 21.1 Compiler and static quality

- Enable strong warnings on project code for MSVC, Clang, and GCC.
- Treat warnings as errors in CI after third-party warnings are isolated.
- Provide clang-format configuration and format project sources.
- Add clang-tidy for a high-value, non-noisy ruleset.
- Provide AddressSanitizer and UndefinedBehaviorSanitizer presets where supported.
- Optionally provide ThreadSanitizer for headless/world job tests.
- No undefined behavior, data races, unchecked narrowing in critical math, or signed overflow reliance.

### 21.2 Required automated tests

At minimum test:

- Deterministic PRNG vectors.
- Same-seed chunk generation stability.
- Generation sweep over at least 1,000 seeds.
- Tile mining timing and yield boundaries.
- Collision around corners, shafts, slopes if supported, and chunk borders.
- Fuel, heat, damage, and cargo calculations.
- Inventory stacking, capacity, protected items, and transaction rollback.
- Buy/sell/refine/upgrade transactions.
- Invalid content and unresolved IDs.
- Upgrade prerequisites and caps.
- Skill respec behavior.
- Contract generation eligibility and completion.
- Research unlock idempotence.
- Save/load round trip.
- Interrupted/invalid save recovery.
- Migration fixtures.
- Persistent mined tiles after chunk eviction/reload.
- Failure/rescue consequences.
- Fixed-step determinism under varied render-frame timings.
- A headless scripted expedition from new game to first purchase.

Tests must be deterministic, fast enough for normal development, and runnable through CTest.

### 21.3 Developer tools

Provide a debug/developer overlay, compiled out or disabled in shipping builds, with:

- Teleport to depth/biome.
- Toggle invulnerability/infinite fuel.
- Grant material/credits/insight.
- Spawn hazard/landmark/resource vein.
- Inspect tile/chunk coordinates and generation seed.
- Visualize chunk borders, collision, heat, ore density, and scanner fields.
- Force save/load/autosave corruption test.
- Complete/reset contracts and research.
- Change timescale only for debug testing.
- Live-edit or reload safe balance definitions.
- Export balance and performance metrics.

Cheats must not accidentally persist in ordinary saves without a clear debug flag.

## 22. Build, CI, and Packaging

Provide:

- README quick start for Windows and Linux.
- CMake presets such as dev-debug, dev-release, asan, ubsan, and ci.
- One documented dependency/bootstrap flow.
- Commands to configure, build, run, test, and package.
- CI on Windows and Linux that configures, builds, runs tests, validates content, and uploads logs on failure.
- Release packaging with executable, required libraries, data/assets, licenses, and default configuration.
- A build/version stamp visible in logs and settings.
- Graceful error when assets/data are missing rather than a crash.

Do not require a developer IDE. IDE use may be documented as optional.

## 23. Documentation Required

Keep documentation updated with implementation:

- **README.md:** project summary, current status, build/run/test instructions, controls, known limitations.
- **docs/GAME_DESIGN.md:** implemented loop, mechanics, progression, content cadence, balance philosophy.
- **docs/ARCHITECTURE.md:** modules, dependency direction, game loop, threading, ownership, event flow.
- **docs/CONTENT_AUTHORING.md:** schemas, stable IDs, validation, adding materials/biomes/modules/contracts.
- **docs/SAVE_FORMAT.md:** schema, atomic write process, backup and migration policy.
- **docs/BALANCE_NOTES.md:** current targets, simulations, known balance risks, tuning decisions.
- **docs/TESTING.md:** test commands, suites, debug tools, seed sweeps.
- **docs/THIRD_PARTY_NOTICES.md:** dependency and asset licenses.
- **docs/decisions/**: short architecture decision records only for consequential choices.

Documentation must describe reality. Remove or mark superseded plans.

## 24. Implementation Milestones and Exit Gates

Proceed in order. A milestone is complete only when its exit gate passes.

### Milestone 0: Repository assessment and executable plan

Tasks:

- Inspect the entire existing repository, build scripts, assets, documentation, current failures, and uncommitted changes.
- Preserve compatible working code and user changes.
- Identify toolchain availability.
- Record short decisions for renderer, dependency management, terrain storage, deterministic RNG, and save format.
- Create a prioritized implementation checklist in docs or the agent's task system.
- Establish commands for build, tests, formatter, and content validation.

Exit gate:

- A minimal configure/build command is known.
- Risks and assumptions are documented.
- The next milestone can start without an architecture rewrite.

Do not spend excessive time writing plans before producing the first executable.

### Milestone 1: Foundation and walking skeleton

Tasks:

- Set up CMake targets, dependencies, logging, tests, presets, and CI skeleton.
- Open a window, handle resize/high-DPI, process input, and render a basic scene.
- Implement fixed-step loop, state flow, asset/content loading, and clean shutdown.
- Add headless core tests.
- Add placeholder main menu and a new-world seed flow.

Exit gate:

- Windows and Linux builds are structurally supported.
- Game launches, changes state, renders, accepts keyboard/controller input, and exits cleanly.
- Tests run through CTest.
- Sanitizer build runs the basic test suite without errors.

### Milestone 2: Core mining-loop greybox

Tasks:

- Implement player rig movement, camera, tile collision, drilling, tile break feedback, pickups/cargo, fuel, hull, heat, and depth.
- Implement one deterministic biome, six materials, basic hazards, 32 x 32 chunk streaming, fog/map basics, and persistent tile mutations.
- Implement return to surface, selling, repair/refuel, and at least six equipment tracks.
- Implement failure/rescue and one save slot with atomic save/backup.
- Add settings and basic remapping.

Exit gate:

- From a new game, a player can mine a full cargo, return, sell it, buy an upgrade, save, quit, load, and observe the mined route still present.
- A 20-minute manual smoke test can be completed without a crash or soft lock.
- Required core-loop automated tests pass.
- The first upgrade is reachable in the target 5–10 minutes.

### Milestone 3: Progression vertical slice

Tasks:

- Expand to three biomes and twelve materials.
- Implement scanner, twelve modules, eighteen skill nodes, four facilities, refining, research discoveries, contracts, landmarks, transit shortcuts, and six hazards.
- Add loadout comparison UI and clear unlock presentation.
- Add a first set-piece discovery.
- Implement full versioned save coverage for these systems.
- Create the balance simulation tool.

Exit gate:

- At least three distinct viable builds feel different in play.
- A fresh save supports 60–90 minutes of directed progression without a repetitive mandatory loop.
- All progression screens work with controller only.
- Save/load round trips preserve every implemented system.
- Content validator and balance sanity checks pass.

### Milestone 4: World breadth and version 1 systems

Tasks:

- Reach the version 1 content targets where items meet the quality bar.
- Complete all six biome roles and transition rules.
- Add faction reputation, codex/mastery, deeper contracts, artifact chains, advanced facilities, and build keystones.
- Add major shortcuts, biome-specific route mechanics, and remaining set pieces.
- Add endgame/finale and continued play.
- Add optional New Game+ only after the normal game is coherent.

Exit gate:

- Every mandatory resource and landmark is guaranteed across the generation seed sweep.
- At least six build archetypes are viable.
- The main arc is completable from a clean save.
- There are no known progression soft locks.
- Content has no unvalidated reference or filler item added only to meet a count.

### Milestone 5: UX, accessibility, balance, and polish

Tasks:

- Complete player UI, map, tutorials, feedback, settings, accessibility options, and audio.
- Tune economy using simulator output and real playtests.
- Optimize measured hotspots.
- Test common resolutions, aspect ratios, DPI scales, controllers, alt-tab, focus changes, suspend/resume where applicable, and save directory failures.
- Add crash-safe error reporting/log location guidance.
- Package distributable builds.

Exit gate:

- All Definition of Done checks pass.
- A clean machine can follow README and build/run or use the packaged build.
- No critical/high severity defects remain.
- Performance budgets are met in representative early and late saves.
- At least one full clean-save completion has been performed and documented.

## 25. Agent Execution Protocol

Follow this operating discipline:

1. Read goal.md and the repository before acting.
2. If the repository contains AGENTS.md or equivalent instructions, follow them unless they conflict with this authoritative product goal or safety.
3. Check git status and preserve unrelated user work. Never discard or rewrite user changes.
4. State current assumptions in a short working note. Make reversible reasonable decisions without repeatedly asking questions.
5. Maintain a small prioritized task list with exactly one active implementation objective.
6. Implement the smallest complete vertical step, including code, content, tests, and documentation.
7. Format, build, run relevant tests, and perform a smoke test after each substantial step.
8. Fix regressions before starting the next feature.
9. Keep the game runnable. Use feature flags or isolated branches/modules for incomplete experiments.
10. Record consequential decisions and measured balance/performance facts, not diary-style narration.
11. Replace temporary hacks before calling a milestone complete.
12. At each handoff, report what is playable, exact commands run, test results, known limitations, and the next highest-value task.

### Autonomy rules

- Do not ask the user to choose ordinary implementation details already answered by this file.
- Prefer the approved defaults.
- If ambiguity is reversible and low-risk, make a sensible decision and document it.
- Ask only when a choice is irreversible, changes the product's identity/scope substantially, requires credentials/payment/external publication, or risks user data.
- Do not claim to have run a command, test, or playthrough that was not actually run.
- Do not hide failing tests, warnings, stutters, save compatibility issues, or missing content behind optimistic summaries.
- If blocked, continue with independent useful work and report the exact blocker.

### Change quality

Every feature change should include, where relevant:

- Implementation.
- Data/content definition.
- Automated tests.
- Error handling.
- Player feedback/UI.
- Save compatibility.
- Documentation.
- Performance consideration.

Avoid giant rewrites. Prefer coherent reviewable increments. Do not introduce abstractions that have only one speculative user and no clear testing or ownership benefit.

## 26. Definition of Done

The assignment is done only when all applicable items are true:

### Gameplay

- Core expedition loop is fun, responsive, and understandable without developer explanation.
- The player can progress from the surface through every biome to an ending.
- At least six viable build archetypes exist.
- Gear, modules, skills, research, base facilities, contracts/factions, and discovery/mastery all work and interact.
- Failure is consequential but cannot create an unrecoverable debt/progression spiral.
- No mandatory progress depends on unbounded random chance.

### Engineering

- Clean configure/build/test succeeds using documented commands on supported toolchains.
- No known crashes, hangs, data races, progression soft locks, or save corruption defects.
- Domain/world tests run headlessly.
- Generation and simulation are deterministic where promised.
- Content is validated before play.
- Save operations are versioned, atomic, backed up, migration-tested, and recoverable.
- Ownership and threading are explicit.
- No global mutable grab bag, terrain-as-entity explosion, or presentation-driven simulation.

### UX and accessibility

- Keyboard and controller can complete all player flows.
- Remapping, UI scale, color-independent communication, shake/flash controls, and gameplay assists work.
- Critical status and transaction consequences are clear.
- Tutorials unlock gradually and are reviewable/dismissible.

### Performance and release

- Performance targets are measured and met in representative saves.
- Release package contains only needed binaries/assets/data/licenses.
- Logs and user saves go to appropriate writable locations.
- README and documentation match the shipped behavior.
- Third-party code/assets have compatible licenses and attribution.

## 27. Acceptance Playthrough

Before declaring completion, perform and document at least this clean-save path:

1. Start a new world with a recorded seed.
2. Complete the initial tutorial through the first sale.
3. Buy the first meaningful equipment upgrade.
4. Unlock a module and demonstrate its behavior.
5. Complete a contract and refine a strategic material.
6. Spend a skill point and unlock a base facility.
7. Reach each biome and use its intended counterplay.
8. Activate a permanent shortcut.
9. Fail one expedition and verify fair recovery.
10. Save, quit, reload, and verify world mutations/progression.
11. Complete an artifact/research chain.
12. Finish the final expedition/ending.
13. Continue the save after the ending.
14. If New Game+ exists, verify that its preview and confirmation accurately describe retained/reset state.

Record build, seed, duration, defects found, fixes made, and final save version.

## 28. Stretch Goals

Only begin these after version 1 gates pass:

- Daily-seed-style offline challenge generated from a manually entered seed, with no server requirement.
- Replay/ghost input recording for deterministic debugging.
- Mod-friendly content packs with explicit compatibility/version checks.
- Additional vehicle chassis that change movement and slot layout.
- Photo mode.
- Steam Deck-focused UX testing.
- Local split-screen is not approved by default and requires a new scope decision.
- Richer destructible structures, liquids, or gas simulation only after profiling and gameplay validation.

## 29. Rust Fallback Policy

C++23 is mandatory by default. Do not switch languages because Rust is personally preferred or because C++ requires normal setup work.

Rust may be selected only if:

- The environment cannot install or use a viable C++23 compiler/CMake/SDL3 toolchain after reasonable attempts, or
- An existing healthy repository is already substantially implemented in Rust and rewriting it would waste working code, or
- The user explicitly approves the switch.

Before switching, document the concrete blocker and the cost of remaining on C++.

If the fallback is triggered, use:

- Current stable Rust edition supported by the toolchain.
- Bevy stable or an SDL3-based Rust stack, chosen based on the existing repository and build reliability.
- serde for content/save DTOs, tracing for logs, thiserror/anyhow at appropriate boundaries, and cargo-nextest or standard cargo test.
- The same domain separation, determinism, chunk storage, data-driven content, save guarantees, tests, gameplay requirements, milestones, and Definition of Done.

Do not begin in C++, abandon it mid-project, and rewrite in Rust without a proven blocking reason.

## 30. First Actions for Claude Opus 5

Execute these now when this file is supplied:

1. Read the repository and any local instructions completely.
2. Report the current repository state in no more than roughly 20 concise bullets.
3. Identify the exact build/test/toolchain commands available.
4. Choose or confirm the primary stack, defaulting to C++23 + CMake + SDL3.
5. Create the minimal docs/decision notes and milestone checklist needed to work safely.
6. Start Milestone 1 immediately.
7. Produce a running window and headless test target as the first verified vertical step.
8. Continue toward the Milestone 2 playable mining loop without waiting for further permission unless a genuine blocker under the autonomy rules occurs.

The desired behavior is sustained implementation: inspect, plan briefly, code, build, test, play, measure, fix, document, and repeat until the Definition of Done is satisfied.
