# Testing

Every test is deterministic and headless: no window, no GPU, no wall clock, no
network. The whole suite runs in under three seconds on a two-core machine, so
there is no excuse for not running it before a commit.

## Commands

```sh
# Fast loop: simulation, world, content, economy and saves, no SDL
cmake --preset headless
cmake --build --preset headless
ctest --preset headless

# Full build including the client
cmake --preset dev-debug && cmake --build --preset dev-debug && ctest --preset dev-debug

# Sanitizers (headless targets)
cmake --preset asan  && cmake --build --preset asan  && ctest --preset asan
cmake --preset ubsan && cmake --build --preset ubsan && ctest --preset ubsan
cmake --preset tsan  && cmake --build --preset tsan  && ctest --preset tsan

# One suite, or one case
./build/headless/tests/deepforge_tests "[simulation]"
./build/headless/tests/deepforge_tests "generation sweep over 1000 seeds"
./build/headless/tests/deepforge_tests --list-tests
```

## What is covered

76 CTest cases across five suites.

### `tests/unit` — core, inventory, economy, saves

| Area | Cases |
| --- | --- |
| PRNG | Known splitmix64 vectors, reproducibility, stream independence (draining a cosmetic stream must not move ore), unbiased `below()`, `unit_float` range, state round trip |
| Noise | Integer determinism, bounds, spatial variation |
| Coordinates | `floor_div`/`floor_mod` on negatives, chunk/tile round trip over a 140x140 block |
| Inventory | Capacity, stacking, removal, never negative, protected sample storage |
| Transactions | A failed multi-step change rolls back completely |
| Economy | Selling, equipment prerequisites and tier caps, module power budget, skill prerequisites and exclusive keystones, respec, research idempotence and sample consumption, refining, rescue cap, first-rescue-free, contract eligibility and single payout |
| Balance invariants | Stat caps hold with every upgrade in the game bought; no recipe multiplies value without bound; every material consumed by an upgrade or recipe is re-obtainable; every paid equipment tier has at least one effect; the power core track strictly raises the module power budget at every tier |
| Text | Every content key has authored text; the string table has no unreachable entries; names are short, single-line and printable ASCII |
| Saves | Full field round trip, atomic write with backup rotation, corruption detection and recovery, garbage input, migration from every released schema, refusal of a newer schema, dropping content that no longer exists |

### `tests/generation` — world

- Same seed produces identical chunks.
- Chunk hashes are identical regardless of the order chunks are generated in
  (no cross-chunk state leaks).
- A vein reads the same from either side of a chunk border, before and after
  eviction.
- World boundaries are indestructible and enclosing.
- The entry shaft is open on 60 seeds.
- No hazard is placed inside the tutorial-safe layer, on 40 seeds.
- **A 1000-seed sweep** checking: no invalid tile references, enough starter ore
  in the band a first expedition can reach, sane air/solid density, and an open
  shaft. Runs in about 4 seconds.
- Every material with generation rules is reachable, sampled across all six
  biome bands on four seeds.

### `tests/simulation` — the rig

- Mine time follows the documented formula and its clamps; harder rock is
  slower, a stronger drill is faster.
- Unmineable tiles are never breakable.
- Mining yields cargo, clears the tile and records the codex entry.
- Interrupted mining keeps its progress, then decays after the configured delay.
- Collision: landing, resting without sinking, corners along a chunk border, and
  never being trapped inside terrain.
- Thrust burns fuel; the drill anchor makes cutting upward cost no hover fuel.
- Heat rises with depth and cooling holds it down.
- Falling far enough damages the hull.
- Cargo mass slows acceleration but never immobilises the rig.
- **Fixed-step determinism**: the same command stream produces bit-identical
  state whether it is delivered in one batch of 600 steps or in nine ragged
  batches, which is what proves render frame timing cannot affect the
  simulation.
- Mined tiles survive chunk eviction and reload.
- Volatile material damages a rig that extracts it carelessly.

### `tests/integration` — the loop

- A new game reaches its first equipment purchase headlessly, within the pacing
  budget, without stalling.
- Save, quit, reload preserves progression **and** the tunnel the player carved.
- A failure preserves all permanent progression and returns a usable rig.
- Eight consecutive failures with zero credits never strand the player.
- State transitions reject invalid moves.
- An autosave exists and is loadable after an expedition.
- Twenty simulated minutes of play across many expeditions with no crash, no
  stall, no negative credits and no error state.
- Transit nodes: activation records a launch point, launching starts the run at
  that depth (never inside rock), climbing back to the entry depth extracts, and
  the nodes survive a save round trip.
- Landmark-driven contracts (cache recovery, infrastructure activation, rescue,
  anomaly scan) do **not** complete on depth alone — the test drives an empty
  expedition outcome and asserts they stay open, then supplies the real landmark
  interactions and asserts they close.

### Content validation

`deepforge-validate` runs as a CTest case, so a bad content edit fails the
suite rather than the game.

## Developer tools

### Content validator

```sh
./build/dev-release/tools/deepforge-validate --data data
```

Reports every problem in one run with file, definition, field and a suggested
fix. Exit code 1 on failure.

### Balance simulator

```sh
# Expedition model: duration, credits, depth, credits per minute, rescues
./build/dev-release/tools/deepforge-balance --data data --seeds 16 --depth 240

# Pacing: time to the first upgrade, and whether any seed cannot reach one
./build/dev-release/tools/deepforge-balance --data data --seeds 16 --pacing

# Trace one run tick by tick
./build/dev-release/tools/deepforge-balance --data data --seeds 1 --verbose
```

The pacing mode exits non-zero if any seed fails to reach a first upgrade in
twelve runs, so it can gate a release.

### Screenshot capture

The client can play a scripted expedition headlessly and write a screenshot,
which is how the rendering is verified without a display:

```sh
SDL_VIDEODRIVER=offscreen ./build/dev-release/src/deepforge \
    --data data --user-dir /tmp/dfuser \
    --screenshot /tmp/shot.bmp --screenshot-seed 20260905 \
    --screenshot-screen expedition --screenshot-ticks 2400
```

`--screenshot-screen` accepts `expedition`, `map`, `hub`, `garage`, `modules`,
`skills` and `menu`.

### Developer overlay

`F3` in game. Shows frame time, tick time, resident and delta chunk counts,
chunks generated and evicted, cached chunk textures, draw calls, rig position,
velocity, ground and anchor state, game state, seed, biome, aggregated stats and
playtime.

### Headless frame runner

```sh
./build/dev-release/src/deepforge --headless-frames 300
```

Runs the real client loop for a fixed number of frames. Useful under a
sanitizer or with `SDL_VIDEODRIVER=dummy` in CI.

## Acceptance playthrough

The full acceptance path in the design brief runs from a clean save to an
ending. **It has not been completed**, because the ending set piece is not
implemented. What has been executed and verified, headlessly and repeatedly:

| Step | Status |
| --- | --- |
| 1. Start a new world with a recorded seed | Done — seeds 20260905, 4242, 31415 and 1000 sweep seeds |
| 2. Tutorial through the first sale | Done — integration test asserts it |
| 3. Buy the first meaningful upgrade | Done — integration test asserts it within the pacing budget |
| 4. Unlock a module and demonstrate its behaviour | Partial — unit tests cover purchase, install, power budget and stat effect; no scripted in-game demonstration |
| 5. Complete a contract and refine a material | Partial — both covered by unit tests, not by a scripted playthrough |
| 6. Spend a skill point and unlock a facility | Done — unit tests |
| 7. Reach each biome and use its counterplay | Not done |
| 8. Activate a permanent shortcut | Not done |
| 9. Fail an expedition and verify fair recovery | Done — two integration tests |
| 10. Save, quit, reload, verify world mutations | Done — integration test |
| 11. Complete an artifact/research chain | Not done |
| 12. Finish the final expedition | Not implemented |
| 13. Continue after the ending | Not implemented |
| 14. New Game+ | Not implemented |

Recorded for the runs that were performed: build 0.4.0 (RelWithDebInfo, GCC
13.3, Linux x86-64), save schema 3, world gen version 1, content schema 1.

## Shutdown ordering

The client owns SDL resources, so it must be destroyed before `SDL_Quit`. That
ordering bug segfaulted at exit and was invisible to the unit suite because the
suite never constructs a client. The guard is the CI step:

```sh
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  ./build/ci/src/deepforge --data data --user-dir "$TMPDIR/dfuser" --headless-frames 300
```

It runs the real client loop and exits, so a resource released in the wrong
order fails the build.

## Adding a test

Drop a `*.test.cpp` under the matching `tests/` directory; the glob picks it up
on the next configure. Use `dft::shipped_content()` for the real content
database and `dft::temp_dir(name)` for a clean scratch directory. Tests must not
depend on wall-clock time, the machine's random state, or files outside their
temp directory.
