# Installing, building and running

Everything here has been run on the platform it describes unless the section
says otherwise. Where a step is untested, it says so rather than implying more
confidence than the project has earned.

- [Quick start](#quick-start)
- [Prerequisites](#prerequisites)
- [Configure and build](#configure-and-build)
- [Running the game](#running-the-game)
- [Build presets](#build-presets)
- [Running the tests](#running-the-tests)
- [Packaging a build](#packaging-a-build)
- [Where your files live](#where-your-files-live)
- [Troubleshooting](#troubleshooting)

---

## Quick start

```bash
git clone https://github.com/empulse75/deepforge.git
cd deepforge
cmake --preset dev-release
cmake --build --preset dev-release
./build/dev-release/src/deepforge
```

The first configure downloads and builds SDL3 from source, which takes a few
minutes. Every later build reuses it and takes seconds.

---

## Prerequisites

| Requirement | Minimum | Notes |
| --- | --- | --- |
| CMake | 3.28 | Presets version 6 are used |
| C++ compiler | GCC 13+, Clang 17+, MSVC 19.38+ | C++23; needs `<expected>` and `<format>` |
| Ninja | any recent | Named by every preset |
| Git | any recent | CMake fetches SDL3 with it |

Nothing is vendored into this repository. CMake fetches SDL3, nlohmann/json and
Catch2 at configure time, each pinned to an exact version — see
[`docs/decisions/0002-dependencies.md`](decisions/0002-dependencies.md).

### Linux

SDL3 is not packaged on most current distributions, so it is built from source
and needs the windowing development headers present:

```bash
# Debian / Ubuntu
sudo apt install build-essential cmake ninja-build git \
     libx11-dev libxext-dev libxrandr-dev libxi-dev libxcursor-dev \
     libxfixes-dev libxss-dev libwayland-dev libxkbcommon-dev libasound2-dev

# Fedora
sudo dnf install gcc-c++ cmake ninja-build git \
     libX11-devel libXext-devel libXrandr-devel libXi-devel libXcursor-devel \
     wayland-devel libxkbcommon-devel alsa-lib-devel

# Arch
sudo pacman -S base-devel cmake ninja git libx11 libxext libxrandr libxi \
     libxcursor wayland libxkbcommon alsa-lib
```

This is the platform the project is developed and tested on (Ubuntu 24.04,
GCC 13.3, CMake 3.28.3, Ninja 1.11.1).

If you already have SDL3 installed, skip building it:

```bash
cmake --preset dev-release -DDEEPFORGE_USE_SYSTEM_SDL=ON
```

### macOS

**Untested.** The code has no platform-specific paths beyond SDL's own and the
user-data directory, so it is expected to work, but nobody has run it. Treat a
failure here as a bug worth reporting rather than as your mistake.

```bash
xcode-select --install
brew install cmake ninja
```

### Windows

**Untested**, with the same caveat. Install Visual Studio 2022 (17.8 or newer)
with the C++ workload, then from a Developer Command Prompt:

```bat
cmake --preset dev-release
cmake --build --preset dev-release
```

---

## Configure and build

Presets do the work; you should not need to pass cache variables by hand.

```bash
cmake --preset dev-release          # configure into build/dev-release
cmake --build --preset dev-release  # build
```

Useful options, all off by default unless stated:

| Option | Default | Effect |
| --- | --- | --- |
| `DEEPFORGE_BUILD_CLIENT` | `ON` | Build the SDL3 client. `OFF` drops the SDL dependency entirely |
| `DEEPFORGE_BUILD_TESTS` | `ON` | Build the Catch2 suite |
| `DEEPFORGE_BUILD_TOOLS` | `ON` | Build the validator and balance simulator |
| `DEEPFORGE_USE_SYSTEM_SDL` | `OFF` | Use an installed SDL3 instead of fetching one |
| `DEEPFORGE_WARNINGS_AS_ERRORS` | `OFF` | On in the `ci` preset |
| `DEEPFORGE_SANITIZER` | empty | `address`, `undefined` or `thread` |
| `DEEPFORGE_ENABLE_LTO` | `OFF` | Link-time optimisation. On in the `release` preset only |
| `DEEPFORGE_DISABLE_COMPILER_CACHE` | `OFF` | Never use a compiler cache, even if one is installed |

### Compiler cache

If `ccache` or `sccache` is installed, it is detected before any target exists and
used as the compiler launcher; that is why a second build is often near-instant.
Nothing is hard-coded, so a machine without a cache program builds normally. Pass
`-DCMAKE_CXX_COMPILER_LAUNCHER=...` (or the `C` equivalent) to pick your own
launcher — yours wins over detection — or `-DDEEPFORGE_DISABLE_COMPILER_CACHE=ON`
to force the cache off, which is what you want for a clean measurement build.
Configure says which of the three happened:

```
-- Compiler cache: /usr/bin/ccache
-- Compiler cache: none (no ccache or sccache found)
-- Compiler cache: none (DEEPFORGE_DISABLE_COMPILER_CACHE=ON)
```

### Link-time optimisation

`DEEPFORGE_ENABLE_LTO` is opt-in per preset; today only `release` sets it. It is
guarded by `check_ipo_supported`, so a toolchain without a working LTO plugin —
notably the MinGW cross-compiler — falls back to a build without LTO and says so
(`-- LTO: requested but unsupported by this toolchain, disabled (...)`) rather
than failing. `optimization/OPTIMIZATION-REPORT.md` has the measurements.

---

## Running the game

```bash
./build/dev-release/src/deepforge
```

The game finds its content next to the executable first, then `./data`. If it
cannot find it, it says so and exits with a usable message rather than crashing.

```
--data <dir>              where to load game content from
--user-dir <dir>          where saves, settings and the log are written
--version                 print the version and exit
--help                    the full list, including the QA options below
```

For CI and QA there are also headless modes, which need no display:

```bash
# Run 300 frames with no input and exit; guards start-up and teardown order.
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  ./build/dev-release/src/deepforge --headless-frames 300

# Play a scripted world on a fixed seed and write one screenshot.
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  ./build/dev-release/src/deepforge \
  --screenshot /tmp/hub.bmp --screenshot-screen hub --screenshot-seed 20260905
```

`--screenshot-screen` accepts any screen by name (`expedition`, `hub`, `garage`,
`modules`, `skills`, `research`, `refinery`, `contracts`, `facilities`, `codex`,
`map`, `pause`, `results`, `failure`, `settings`, `bindings`, `credits`, `menu`).

An unrecognised option is refused with exit code 2 rather than ignored, so a
typo in a CI script fails loudly instead of quietly running a default.

---

## Build presets

| Preset | Build type | Client | For |
| --- | --- | --- | --- |
| `dev-debug` | Debug | yes | Day-to-day work with a debugger |
| `dev-release` | RelWithDebInfo | yes | Playing it, and the smoke test |
| `release` | Release | yes | Packaging |
| `headless` | RelWithDebInfo | **no** | Tests on a machine with no display or GPU |
| `ci` | RelWithDebInfo | yes | Warnings as errors |
| `asan` | Debug | no | AddressSanitizer + LeakSanitizer |
| `ubsan` | Debug | no | UndefinedBehaviorSanitizer |
| `tsan` | Debug | no | ThreadSanitizer |

`headless`, `asan`, `ubsan` and `tsan` set `DEEPFORGE_BUILD_CLIENT=OFF`, so they
build and test the simulation, world generation, content and save layers without
SDL at all. That is what makes the suite runnable in a container.

---

## Running the tests

```bash
tools/verify.sh           # the whole gate: builds dev-release, ci, headless,
                         # asan, ubsan, tsan; tests headless, asan, ubsan, tsan;
                         # then the smoke test
tools/verify.sh --quick   # headless build and tests only
```

`verify.sh` configures whatever presets it needs, so it works from a fresh clone
with no build directories. Or drive CTest directly:

```bash
cmake --preset headless && cmake --build --preset headless
ctest --preset headless
```

Individual cases, by name or tag:

```bash
./build/headless/tests/deepforge_tests "generation sweep over 1000 seeds"
./build/headless/tests/deepforge_tests "[economy]"
./build/headless/tests/deepforge_tests --list-tests
```

[`docs/TESTING.md`](TESTING.md) covers what each suite actually asserts, and
what is deliberately not covered.

---

## Packaging a build

```bash
cmake --preset release
cmake --build --preset release
cmake --install build/release --prefix dist
```

That produces a `dist/` containing the executable, the `data/` directory, the
SDL3 shared library and SDL's licence text. The installed binary runs standalone
from that directory — it is not dependent on the build tree.

---

## Where your files live

Saves, settings and the log go to the platform user-data directory, or wherever
`--user-dir` points.

| What | Linux and macOS | Windows |
| --- | --- | --- |
| Saves | `~/.local/share/deepforge/saves` | `%APPDATA%\Deepforge\saves` |
| Settings | `~/.local/share/deepforge/settings.json` | `%APPDATA%\Deepforge\settings.json` |
| Log | `~/.local/share/deepforge/deepforge.log` | `%APPDATA%\Deepforge\deepforge.log` |

`$XDG_DATA_HOME` is honoured when set, and the fallback if neither it nor
`$HOME` exists is `./deepforge-data`.

There is no macOS branch in `default_user_data_dir()`, so macOS currently gets
the Linux path rather than `~/Library/Application Support`. That is a known gap,
not a deliberate choice — it is listed in the README's limitations. It was
established by reading `src/app/SaveFile.cpp`, which has a `_WIN32` branch and
an `#else` branch with no macOS case: no test asserts it, no CI job covers it,
and it has not been run on Apple hardware.

Saves are versioned, atomic and checksummed, with a rotating backup — see
[`docs/SAVE_FORMAT.md`](SAVE_FORMAT.md).

---

## Troubleshooting

**SDL3 fails to build with missing `X11/Xlib.h` or similar.**
The windowing development headers are not installed. See the Linux package list
above. This is the single most common first-build failure.

**`No available video device`.**
There is no display — you are on a server, in a container, or over SSH without
X forwarding. Either use the headless presets, or set
`SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy` for a windowless run.

**`Project Deepforge could not start: content directory not found`.**
Run from the repository root, or pass `--data /path/to/data`. The game looks
next to the executable first, then in `./data`.

**Content validation fails with `no text for key '...'`.**
A definition references a string key with no entry in `data/strings.en.json`.
Add it; the loader names the missing key. See
[`docs/CONTENT_AUTHORING.md`](CONTENT_AUTHORING.md).

**The configure step takes minutes and appears to hang.**
It is building SDL3 from source on first configure. Subsequent configures reuse
it. `-DDEEPFORGE_USE_SYSTEM_SDL=ON` skips it if you have SDL3 installed.

**A compiler error about `<expected>` or `<format>`.**
The toolchain is too old for C++23. GCC 13, Clang 17 or MSVC 19.38 are the
floor. Note that `<print>` is deliberately not used, because GCC 13 lacks it.

**Tests pass under `headless` but fail under `asan`.**
That is the sanitizer earning its place — treat it as a real bug, and report the
full ASan report rather than only the failing test name.
