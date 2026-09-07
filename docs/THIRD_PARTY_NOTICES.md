# Third-party notices

Project Deepforge itself is MIT licensed (see `LICENSE`). All game content —
code, world generation, balance data, the bitmap font, every sprite drawn by the
renderer and every sound synthesised at runtime — is original to this project.

No third-party game assets, sprites, level layouts, dialogue, branding or audio
are used, and none were derived from any existing game.

## Dependencies

Fetched at configure time by CMake `FetchContent` at the pinned versions below.
Nothing is vendored into this repository.

### SDL3 — `release-3.4.16`

Windowing, input, gamepads, audio output, timing and platform paths.

- Upstream: https://github.com/libsdl-org/SDL
- Licence: zlib
- Redistribution: the zlib licence permits use and redistribution in source and
  binary form. The packaged build ships the SDL3 shared library alongside the
  executable, and SDL's own `LICENSE.txt` is installed with it.

### nlohmann/json — 3.11.3

Parsing and writing content definitions and save files. Fetched as the official
`include.zip` release archive with a pinned SHA-256.

- Upstream: https://github.com/nlohmann/json
- Licence: MIT
- Redistribution: header-only; compiled into the executable. The MIT licence
  requires the copyright notice and permission notice be included, which this
  file provides.

### Catch2 — v3.7.1

Test framework. **Build-time only** — it is not linked into the game executable
and is not redistributed.

- Upstream: https://github.com/catchorg/Catch2
- Licence: Boost Software License 1.0

## Fonts

Both faces are original bitmap designs, authored pixel by pixel in
`tools/content_gen/gen_font.py` and generated into
`src/presentation/FontData.hpp`:

- **Ui** — proportional 7x9 with true lowercase, ascenders and descenders. The
  generator derives every glyph's advance from its own ink, and emits a second
  bold weight by widening each stem one pixel without closing the one-pixel
  counters inside letters such as `m` and `W`.
- **Micro** — a compact 4x6 uppercase face for dense numeric readouts.

No third-party font file is used, referenced, embedded or redistributed, and
neither face is traced from or derived from an existing typeface.

## Audio

Every sound effect is synthesised at runtime from oscillators and envelopes in
`src/platform/Audio.cpp`. There are no audio files in the repository.

## Licence texts

Full licence texts for the dependencies above are distributed with their source,
which CMake places under `build/<preset>/_deps/`. The release package installs
SDL3's licence alongside the binary. If external assets are ever added to this
project, their licences must be verified before use and recorded in this file
together with attribution requirements; a search result is not a licence.
