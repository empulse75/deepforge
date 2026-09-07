# 6. The interface is drawn from primitives, in four layers

Status: accepted

## Context

The first interface worked and read as programmer art: a 5x7 uppercase-only
bitmap face, flat lists, and each screen inventing its own colours, spacings and
row geometry. Nothing was broken, but nothing looked considered either, and
several defects came directly from the lack of shared structure — the settings
sliders drew a value with no track, and the code that dragged them re-derived a
row rectangle by hand that had drifted out of alignment with where the slider
was actually painted.

The obvious alternative was to adopt a UI library. Dear ImGui was in the brief's
preferred stack. But it brings its own look, its own font handling and its own
event model, and the whole project is otherwise asset-free and drawn from
rectangles, which is what keeps it crisp at any integer scale.

## Decision

Build the interface from primitives, in four layers with a strict dependency
direction:

| File | Owns |
| --- | --- |
| `Theme.hpp` | Design tokens: colours, spacing scale, motion rates |
| `Draw.cpp` | Primitives: bevelled panels, inset wells, proportional text, segmented gauges, procedural icons |
| `Ui.cpp` | Immediate-mode widgets with a small persistent store for animation |
| `Client.cpp` | Per-screen builders that fill a `MenuScreen`, and painters that lay one out |

Screens compose tokens and never invent a colour or a spacing. Builders describe
a row — label, value, before/after deltas, the reason it is unavailable — and
never compute a rectangle. Widgets own all geometry.

The art direction is an industrial instrument panel: machined slate housings,
amber for anything the player acts on, cyan for anything the instruments report.

## Consequences

- Mouse support became a second input into one focus model rather than a second
  code path. A row is active if the pointer is over it *or* the focus index
  points at it, and it commits on click or on confirm. That is why no screen is
  reachable by only one device, and why the pointer stays inert until it is
  actually moved, so controller players never see a stray highlight.
- The slider bug could not recur: the track rectangle is computed in one place
  and both the painter and the drag test read it.
- A row that is merely unaffordable stays focusable and explains itself in the
  inspector. Only pure readouts — a results total, a codex entry — are marked
  `inert` and drop out of navigation. Before this distinction existed the cursor
  skipped every unaffordable row and landed on "Back", so the panel that exists
  to say *why* something is unavailable could never be reached.
- Colour is never the only carrier of meaning: every state also has an icon, a
  shape or a label, and the gauge palette has a colour-vision variant that
  separates on blue/yellow/white rather than red/green.
- The cost is that everything must be drawn by hand, including text layout,
  wrapping and eliding. That is a real ongoing cost, accepted because the
  alternative was a dependency whose look would have to be fought.
- No image assets, so the repository stays asset-free and the licence position
  stays simple.
