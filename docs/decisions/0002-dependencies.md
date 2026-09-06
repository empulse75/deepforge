# 2. Pinned dependencies, and three deliberate omissions

Status: accepted

## Context

The brief lists a preferred stack: SDL3, SDL3_image, SDL3_mixer, glm or
project-owned math, EnTT, nlohmann/json, spdlog, Catch2, Dear ImGui and Tracy.
It also requires pinned versions, a reproducible fetch mechanism, and warns
against churning dependencies for aesthetics.

## Decision

Taken as specified: **SDL3** (`release-3.4.16`), **nlohmann/json** (3.11.3, via
the release archive with a pinned SHA-256) and **Catch2** (v3.7.1). All three
are fetched by CMake `FetchContent` with pinned tags; no moving branches, no
vendored copies.

Deliberately omitted, each with a reason:

**spdlog** — replaced by a ~150-line project-owned logger over `std::format`.
Logging is a genuine seam (`LogSink`), the simulation layer needs a
dependency-free one, and the file sink, level filtering and the ring buffer the
debug overlay reads are all the project actually needs. This avoids a
compile-heavy dependency in every translation unit for functionality measured in
a page of code.

**glm** — the brief explicitly permits "small project-owned math types, but do
not maintain two competing math systems". `src/core/Math.hpp` is the single math
system: `Vec2`, `Vec2i`, `Rect` and a handful of free functions. The game is 2D
and uses no matrices.

**EnTT** — the brief says to use it "only if it simplifies implementation" and
explicitly forbids terrain-as-entities. Terrain is arrays; hazards and landmarks
are small vectors owned by their chunk; there is exactly one player rig. An ECS
would add a dependency and a layer of indirection to manage a few dozen objects.

**Dear ImGui** — the shipping UI must be our own, and ImGui is developer-only in
the brief. The developer overlay renders through the same `Draw` helper as the
game UI, which keeps it available in every build configuration and adds no
dependency. If a heavier inspector is ever wanted, ImGui can be added behind the
existing debug flag without touching gameplay code.

**SDL3_image / SDL3_mixer** — there are no image or audio assets. Tiles are
drawn as geometry, the font is generated, and audio is synthesised. These become
necessary the day production assets arrive, not before.

**Tracy** — the brief places it after the vertical slice. The debug overlay
already reports frame time, tick time, chunk counts and draw calls.

## Consequences

- Three dependencies instead of ten; configure and build stay fast, and the
  licence surface stays small (zlib, MIT, BSL-1.0).
- The logger, math and debug overlay are our maintenance burden. All three are
  small, tested through use, and behind interfaces.
- Adding any omitted dependency later is additive, not a rewrite.
