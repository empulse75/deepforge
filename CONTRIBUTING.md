# Contributing

## The gate

One command has to pass before anything is committed:

```bash
tools/verify.sh
```

It configures and builds `dev-release`, `ci`, `headless`, `asan` and `ubsan`,
runs the suite under the last three, and finishes with a headless client run
that exercises SDL start-up and teardown. `--quick` runs the headless build and
tests only, which is the right loop while iterating.

CI runs the same presets, so a green `verify.sh` is a good predictor of a green
build — the difference is that CI also builds on Windows and runs the balance
regression gate.

## What a good change looks like

- **It has a test, or it explains why it cannot have one.** Prefer a test that
  catches the *class* of bug over one that catches the instance. When the power
  core track turned out to grant nothing, the useful test was not "power core
  tier 3 gives 3 power" — it was "every equipment tier the player pays for does
  something", which would have caught it the day the track was authored.
- **It leaves the failure mode better than it found it.** Silent degradation is
  the enemy: a missing string that title-cases an id, a flag typo that starts
  the game with a default, a stat that reads a stale cached field. Prefer a
  loud, named failure at load or build time.
- **Every file boundary declares its ceiling.** Reading a whole file and parsing
  it costs roughly four times the file in resident memory, so `core/JsonFile`
  refuses before reading: 8 MB for machine-written ingest (data dir, store, save
  slots) and 1 MB for the settings file. The shipped corpus (largest file 52 KB)
  and a normal settings file (~4 KB) sit orders of magnitude under; the ceiling is
  for a planted file, not for anything the game produces.
- **The accessors after a parse are checked, not blind.** The parse is
  non-throwing; it was `get<bool>` on a hand-edited string that killed the process.
  A wrong type in content is a named load failure; in a settings file it is a named
  failure carrying the remedy the app already offers ("delete it to restore
  defaults"); a value of the right type but outside its documented domain falls
  back to the documented default with a warning; in a save, where the file's
  convention is to drop data it cannot use, the element is dropped with a warning.
- **The comments say why, not what.** `// Clamp to the view` is noise. `// The
  builder sets this only when there is genuinely nothing to list, so an empty
  screen explains itself instead of reading as a blank panel` is worth its line.
- **The commit message explains the reasoning.** Look at `git log` for the
  register: what was wrong, what was decided, what it cost.

## Style

Formatting is not a matter of opinion here — `.clang-format` decides:

```bash
find src tests tools -name '*.cpp' -o -name '*.hpp' | grep -v FontData | xargs clang-format -i
```

`src/presentation/FontData.hpp` is generated; never format or hand-edit it. Run
`python3 tools/content_gen/gen_font.py` instead.

The command above is a gate, not a suggestion: CI runs it as
`--dry-run --Werror` and fails. `.pre-commit-config.yaml` runs the same check
over the files you have staged; install it once with `pre-commit install`.

`.clang-tidy` carries a deliberately small, low-noise ruleset. If a check fires,
fix the code rather than widening the suppression list.

Beyond that:

- C++23. `std::expected` is the error type; `std::format` does the formatting.
  `<print>` is deliberately unused, because GCC 13 does not have it.
- Layers depend downward only: core → content → world → domain → app →
  platform/presentation → game. `docs/ARCHITECTURE.md` has the table.
- The simulation is integer-only and deterministic. A float in world generation
  is a bug, not a style preference — see ADR 3.
- Anything an effect can modify is derived at the point of use, not cached in
  `PlayerProgress` — see ADR 9.

## Adding content

Content lives in `data/*.json` and is validated at load:

```bash
./build/dev-release/tools/deepforge-validate --data data
```

Every `name_key`, `description_key` and `summary_key` must have an entry in
`data/strings.en.json`, or validation fails and names the missing key. Orphaned
entries fail too. `docs/CONTENT_AUTHORING.md` has the rules and the reasoning.

For a new ore, set a `target_density_pct` in `tools/content_gen/vein_tuning.json`
and run the calibrator rather than guessing `rarity_weight_bp`; see
`docs/BALANCE_NOTES.md`.

## Changing the interface

Read ADR 6 first. The short version: screens compose tokens from `Theme.hpp` and
never invent a colour or a spacing; builders describe a row and never compute a
rectangle; widgets own all geometry.

Verify rendering by cropping and magnifying actual pixels, not by looking at a
scaled-down screenshot. A bold `m` that renders as a solid block is invisible at
1:1 and obvious at 8x. ADR 8 exists because that mistake was made twice.

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  ./build/dev-release/src/deepforge --data data \
  --screenshot /tmp/hub.bmp --screenshot-screen hub
```

## Architecture decisions

If a change is consequential, contested, or expensive to reverse, add a record
to `docs/decisions/`. The index there explains the format and the numbering
rule. Records are never edited to reflect a later change of mind; a new record
supersedes an old one, and the old one stays as written.

## Honesty

The README has a "Known limitations" section and the docs say when something is
untested. Keep that true. If a change makes a limitation obsolete, delete the
bullet; if it adds one, add it. A build that oversells itself wastes the reader's
time, and this project would rather be trusted than admired.
