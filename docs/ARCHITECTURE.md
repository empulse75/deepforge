# Architecture

## Layers and dependency direction

Each layer is a separate CMake target. A layer may only link the layers above it
in this table, and CMake enforces that by what each target links.

| Layer | Target | Responsibilities | Links |
| --- | --- | --- | --- |
| Core | `deepforge_core` | Ids, math, deterministic RNG and noise, result/error types, logging, version stamp | standard library only |
| Content | `deepforge_content` | Definition structs, JSON loading, schema validation, stable-id resolution, the effect registry | core |
| World | `deepforge_world` | Chunk storage, generation passes, tile queries, persistence deltas, landmark and hazard placement | core, content |
| Domain | `deepforge_domain` | Rig simulation, mining, inventory, economy, progression, contracts, commands and events | core, content, world |
| App | `deepforge_app` | Game state machine, use cases, save coordination, the scripted autopilot | domain |
| Platform | `deepforge_platform` | SDL window, renderer handle, input, gamepads, audio, settings and OS paths | core, SDL3 |
| Presentation | `deepforge_presentation` | Camera, tile rendering, HUD, screens, particles, debug overlay | app, platform, SDL3 |

`deepforge_game` is the executable and links presentation plus app.
`deepforge_tests`, `deepforge_content_validator` and `deepforge_balance_sim`
link only headless layers, so **the entire simulation, world and economy can be
built and tested with no window, no GPU and no SDL** (`--preset headless`).

Rules that hold throughout:

- Simulation code never calls SDL rendering, UI, the wall clock,
  `std::random_device` or the filesystem. The one exception is the initial world
  seed, drawn once in `GameSession::new_game`; everything downstream derives
  from it.
- Presentation observes simulation state through an immutable `RenderSnapshot`
  and a per-tick `EventQueue`. It never owns authoritative state and never feeds
  back into it.
- There is no global mutable state, no service locator and no singleton beyond
  the logger's sink list. `Client` is the composition root: it constructs the
  content database, session, input, audio and renderer, and wires them
  explicitly.
- Definition data is immutable after a successful load. Definitions
  (`MaterialDefinition`) and runtime instances (`InventoryStack`) are separate
  types.
- Data files and saves use stable string ids; runtime code uses typed handles
  (`MaterialHandle`, `ModuleHandle`, ...) resolved once at load.

## Game loop

`Client::run` is a variable-rate render loop wrapped around a fixed-rate
simulation:

```
frame:
  poll SDL events
  sample input -> action states -> TickCommands
  accumulator += frame time
  while accumulator >= 1/60 and steps < 5:
      GameSession::tick(commands)
      drain domain events into presentation
      accumulator -= 1/60
      steps++
  if accumulator still overflowing: drop it (degrade, never spiral)
  render(alpha = accumulator / (1/60))
```

The catch-up cap is five steps. After a long stall the game drops the backlog
rather than trying to simulate through it. Rendering interpolates between the
previous and current rig position using `alpha`.

### Authoritative update order

`ExpeditionSim::tick` runs in exactly this order every step:

1. Advance timers (elapsed, damage cooldown, coyote time).
2. Apply the deterministic unstuck action, if requested.
3. Compute the cargo load fraction.
4. Resolve movement: drill anchor check, then acceleration, gravity, thrust,
   fuel draw, integration, axis-separated collision, lane alignment.
5. Resolve mining: target selection, quote, fuel draw, progress, tile break,
   yield, drill advance.
6. Update heat: ambient by biome and depth, generation, cooling, heat-to-fuel
   conversion, damage above capacity.
7. Update hazards: proximity, telegraph, contact and area damage.
8. Update discovery: fog reveal, scanner pulse, landmark discovery and
   activation.
9. Decay stale mining progress near the rig.
10. Auto-collect, depth records, biome transitions, rescue and return checks.

`GameSession::tick` then decides state transitions and produces the snapshot.

## Game states

```
Boot -> MainMenu -> LoadingWorld -> SurfaceHub <-> Expedition
                                       ^              |
                                       |              v
                              ExpeditionResults   FailureRecovery
                                       ^              |
                                       +--------------+
Paused overlays Expedition.  ErrorRecovery is entered when a load fails.
```

Transitions validate prerequisites (`GameSession::transition`) and define the
save boundaries. Autosaves happen on return to the hub, after any purchase,
research, skill change or facility upgrade, before and after failure resolution,
and on a clean quit. A save is never written part-way through a transaction: the
economy applies changes atomically first, then the session saves.

## Terrain is not entities

A chunk is three parallel fixed arrays of 1024 entries (material index, cosmetic
variant, flags) plus small vectors for landmarks, hazards and sparse mining
progress. Nothing about a tile is an entity, and rendering metadata is derived
rather than stored.

Persistence stores only what diverged from generation:

- changed tiles (local index -> material)
- partial mining progress
- landmark activated/looted state
- a 128-byte revealed bitset per chunk, so walking through a chunk does not
  create a thousand delta records

On load, a chunk is regenerated from the seed and the delta is replayed over it.

## Determinism

- Every operation in world generation is integer-only, so a seed produces
  bit-identical terrain on any platform and standard library. Noise is value
  noise on an integer lattice with Q16.16 fixed-point interpolation.
- The PRNG is a project-owned xoshiro256\*\* with an explicit serialisable state.
  Standard-library distributions are never used for anything affecting layout.
- Streams are derived by hashing `(world seed, named stream, coordinates)`. Ore,
  caves, hazards, landmarks, loot, contracts and cosmetics are all separate
  streams, so adding a cosmetic consumer cannot move ore for an existing seed.
- Vein and landmark placement is keyed to world-space cells rather than chunks,
  and every chunk considers all cells overlapping its bounds plus a margin, so a
  vein looks the same regardless of which chunk generated it first. The
  generation-order test locks this in.
- Simulation determinism is per-platform: physics uses IEEE-754 floats with a
  fixed step and no fast-math. The determinism test proves the same command
  stream produces identical state regardless of how frames were batched.

## Threading and ownership

The current build is single-threaded. Chunk generation is bounded per call and
measured well inside the frame budget, so no worker pool exists yet. The seams
that would need one are already in place: `WorldMap::ensure_chunk` is the single
entry point for generation, and generation itself takes only immutable inputs
(content database, seed, coordinate) and produces a self-contained `Chunk`. When
a job system is added, workers can publish finished chunks at a tick boundary
without ever touching live gameplay state.

Ownership is explicit throughout: `std::unique_ptr` for the session's long-lived
services, RAII wrappers for the SDL window, renderer, gamepad, audio stream and
chunk textures. There are no owning raw pointers.

## Events and commands

Player intent flows in as `TickCommands` (a plain struct built from action
states) and simulation results flow out as typed `DomainEvent`s in a bounded
per-tick `EventQueue`. There is no global event bus; the queue has exactly one
consumer (`Client::drain_domain_events`), it is cleared at the top of each tick,
and presentation-only ordering can never alter gameplay.

## Errors

`Result<T>` is `std::expected<T, Error>`. `Error` carries file, definition id,
field and a suggested correction, so a content mistake reports as:

```
materials.json [material.prismite] .generation: unresolved biome reference
'biome.prism_fault' (check data/biomes.json for the correct id)
```

Content loading accumulates every problem into an `ErrorList` and reports them
all in one run rather than one per attempt.
