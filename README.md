# Project Deepforge

A 2D mining, exploration and progression game. You pilot a subterranean
prospecting rig above an unstable world: descend, cut through rock, read the
geology, haul ore back to the surface, and rebuild the rig into something that
can reach the next layer down.

Original code, content, art, audio and world. No third-party game assets.

---

## Current status

Playable and buildable end to end. What exists today:

| Area | State |
| --- | --- |
| Core loop | Complete: descend, mine, manage fuel/cargo/hull/heat, return, sell, upgrade, descend deeper |
| World | Seeded, chunked (32x32), deterministic integer generation with persistent modifications |
| Biomes | 6 defined with depth bands, hazards, landmarks and gating |
| Materials | 30 (20+ mineable), density-calibrated against measured targets |
| Progression | 9 equipment tracks, 31 modules, 36 skill nodes, 18 research nodes, 6 facilities, 6 recipes, 37 contracts, 3 factions |
| Save | Versioned (schema 4), atomic, checksummed, backed up, with tested migrations |
| Input | Keyboard, mouse and SDL gamepads, fully remappable |
| Accessibility | Shake/flash/particle sliders, colour-vision palette, hold-vs-toggle, text scale, captions, difficulty assists |
| Interface | Instrument-panel UI drawn entirely from rectangles: two original bitmap faces, procedural icons, segmented gauges, live before/after stat previews |
| Text | Every player-facing name and description is authored content in `data/strings.en.json`, checked at load and by test |
| Tests | 76 deterministic headless CTest cases, including a 1000-seed generation sweep |
| Tools | Content validator, balance simulator, headless autopilot, screenshot capture |

Known limitations are listed at the bottom of this file. Read them before
judging the build; they are stated plainly rather than glossed over.

---

## Build and run

### Requirements

- CMake 3.28 or newer
- A C++23 compiler: GCC 13+, Clang 17+, or MSVC 19.38+ (Visual Studio 2022 17.8+)
- Ninja (recommended)
- Git (dependencies are fetched at configure time)
- Linux only: SDL3's build dependencies. On Debian/Ubuntu:

  ```sh
  sudo apt install build-essential cmake ninja-build git \
      libx11-dev libxext-dev libxrandr-dev libxi-dev libxcursor-dev \
      libxfixes-dev libxss-dev libxtst-dev libxinerama-dev libxxf86vm-dev \
      libwayland-dev libxkbcommon-dev libegl1-mesa-dev libgl1-mesa-dev \
      libasound2-dev libpulse-dev libudev-dev pkg-config
  ```

No IDE is required. Any IDE that understands CMake presets will work.

### Dependencies

All third-party code is pinned and fetched by CMake `FetchContent` at configure
time; there is no vendored source in the repository.

| Dependency | Version | Purpose | Licence |
| --- | --- | --- | --- |
| SDL3 | `release-3.4.16` | Window, input, gamepads, audio, timing, paths | zlib |
| nlohmann/json | 3.11.3 (release archive, SHA-256 pinned) | Content and save DTOs | MIT |
| Catch2 | v3.7.1 | Tests | BSL-1.0 |

To use an SDL3 you already have installed, configure with
`-DDEEPFORGE_USE_SYSTEM_SDL=ON`.

### Quick start

```sh
git clone https://github.com/empulse75/deepforge.git
cd deepforge

cmake --preset dev-release
cmake --build --preset dev-release
ctest --preset dev-release

./build/dev-release/src/deepforge          # Linux / macOS
build\dev-release\src\deepforge.exe        # Windows
```

The first configure builds SDL3 from source and takes a few minutes. Subsequent
builds reuse it.

### Presets

| Preset | What it is for |
| --- | --- |
| `dev-debug` | Everyday development: debug symbols, assertions, all targets |
| `dev-release` | Optimised with debug info; what you want to actually play |
| `release` | Packaging build |
| `headless` | Simulation, world, content and tests only; no SDL, no window |
| `asan` / `ubsan` / `tsan` | Sanitizer builds of the headless targets |
| `ci` | Release build with warnings promoted to errors |

### Commands

```sh
# Configure, build, test
cmake --preset dev-release
cmake --build --preset dev-release
ctest --preset dev-release

# Headless only: no display needed, fast
cmake --preset headless && cmake --build --preset headless && ctest --preset headless

# Validate game content without launching
./build/dev-release/tools/deepforge-validate --data data

# Measure the economy
./build/dev-release/tools/deepforge-balance --data data --seeds 16 --depth 240
./build/dev-release/tools/deepforge-balance --data data --seeds 16 --pacing

# Package
cmake --build --preset release --target package

# The whole verification gate: 5 presets, 3 sanitizers, headless smoke test.
# Configures whatever it needs, so it works from a fresh clone.
tools/verify.sh
tools/verify.sh --quick   # headless build and tests only
```

If the game cannot find its data it says so and exits with a usable message
rather than crashing. Override the location with `--data <dir>`, and the save
and log location with `--user-dir <dir>`.

---

## Controls

Everything below is remappable in **Settings -> Controls**, for both keyboard
and controller. Every screen and flow is completable with a controller alone.

| Action | Keyboard | Controller |
| --- | --- | --- |
| Move | `A` / `D` or arrows | Left stick, D-pad |
| Thrust / ascend | `W`, `Up` or `Space` | `A` |
| Brake | `Left Shift` | `B` |
| Drill down / left / right / up | `S` / `J` / `L` / `I` | D-pad, right trigger |
| Interact | `E` | `X` |
| Scan | `Q` | `Y` |
| Emergency reposition | `U` | — (bind one) |
| Map | `M` | `Back` |
| Pause | `Esc` | `Start` |
| Menu select / back | `Enter` / `Backspace` | `A` / `B` |
| Developer overlay | `F3` | — |

Menus also take the mouse: hovering a row moves the highlight, clicking selects,
the wheel scrolls a long list, and settings sliders drag. Mouse, keyboard and
controller all drive the same focus, so nothing is reachable by only one of
them, and the pointer stays invisible until it is actually moved.

Two things are worth knowing before your first descent:

- **The drill anchors.** While the drill is engaged against rock, the rig braces
  and pays no thrust. Cutting a shaft upward is slow but cheap; flying up an
  open shaft is fast but thirsty. That choice is the return-planning game.
- **Ascending is the expensive half.** Watch fuel against your depth, not fuel
  on its own.

---

## Where your files live

| What | Linux | Windows |
| --- | --- | --- |
| Saves | `$XDG_DATA_HOME/deepforge/saves` or `~/.local/share/deepforge/saves` | `%APPDATA%\Deepforge\saves` |
| Settings | `.../deepforge/settings.json` | `%APPDATA%\Deepforge\settings.json` |
| Log | `.../deepforge/deepforge.log` | `%APPDATA%\Deepforge\deepforge.log` |

Saves are written to a temporary file, validated, and only then swapped in, with
the previous good save kept as a rotating `.bak`. If a save is damaged, the load
menu says so and offers the backup; the damaged file is never deleted.

---

## Documentation

| Document | Contents |
| --- | --- |
| [docs/INSTALL.md](docs/INSTALL.md) | Prerequisites per platform, building, running, packaging, troubleshooting |
| [CONTRIBUTING.md](CONTRIBUTING.md) | How to work on this: the verification gate, house style, what a good change looks like |
| [docs/GAME_DESIGN.md](docs/GAME_DESIGN.md) | The implemented loop, mechanics, progression and balance philosophy |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Layers, dependency direction, update order, ownership, threading |
| [docs/CONTENT_AUTHORING.md](docs/CONTENT_AUTHORING.md) | Data schemas, stable ids, validation, adding content |
| [docs/SAVE_FORMAT.md](docs/SAVE_FORMAT.md) | Save schema, atomic write, backup and migration policy |
| [docs/BALANCE_NOTES.md](docs/BALANCE_NOTES.md) | Measured figures, tuning decisions and known balance risks |
| [docs/TESTING.md](docs/TESTING.md) | Test commands, suites, developer tools, seed sweeps |
| [docs/THIRD_PARTY_NOTICES.md](docs/THIRD_PARTY_NOTICES.md) | Dependency licences, and why the fonts and audio are original |
| [docs/decisions/](docs/decisions/README.md) | Nine architecture decision records, with the reasoning and the costs |

---

## Known limitations

Stated honestly, because a build that oversells itself wastes your time:

- **Verified on Linux only.** The Windows toolchain path is set up (presets,
  MSVC warning flags, `%APPDATA%` paths, CI workflow) but no Windows build of
  this revision has been run. Treat Windows as untested rather than supported.
- **No full-game playthrough has been performed.** The acceptance playthrough in
  `docs/TESTING.md` has been executed headlessly up to the first purchases and
  through failure recovery, not to an ending. There is no ending set piece yet.
- **Endgame content is not implemented.** The Fracture generates and the
  research web reaches its capstone, but the finale expedition, New Game+ and
  the artifact chain payoff are content, not systems, and are not written.
- **Transit lifts are new and lightly exercised.** Activated nodes persist and
  can launch and extract an expedition (covered by tests), but no long
  playthrough has leaned on them.
- **One language, and no localisation system.** All 566 player-facing strings
  are authored in `data/strings.en.json` and the loader reads exactly that file.
  There is no locale negotiation, no plural or gender handling, and no second
  language. It is a strings table a localisation system could be built around,
  not a localisation system.
- **macOS gets the Linux user-data path.** `default_user_data_dir()` has no
  macOS branch, so saves land in `~/.local/share/deepforge` rather than
  `~/Library/Application Support`. A gap, not a decision.
- **Audio is synthesised, not composed.** Every cue is generated from
  oscillators at runtime. It is functional feedback, not a soundtrack.
- **Performance has not been profiled on a range of hardware.** The chunk
  texture cache keeps the frame cost to a dozen quads, and tick time is well
  inside budget in the debug overlay on the development machine, but no numbers
  from a representative GPU baseline exist yet.

## Licence

MIT. See [LICENSE](LICENSE).
