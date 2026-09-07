# 7. All player-facing text is authored content, and a missing key is a build failure

Status: accepted

## Context

Every definition file already carried `name_key` and `description_key` fields,
but there was no table behind them. The client turned a key into a label by
stripping the prefix, replacing underscores with spaces and title-casing the
result — so `material.myco_chalk.name` displayed as "Myco Chalk".

That worked well enough for names to hide the problem, because the ids had been
authored carefully. It did not work at all for descriptions: `pretty_key` on
`skill.keen_eye.description` yields the word "Description", so every inspector
panel in the game was empty. Screens looked thin because there was nothing to
put in them, and the reason was invisible.

The failure mode also mattered. A key with no text degraded silently into a
plausible-looking identifier. Nothing crashed, nothing logged, and the only way
to notice was for a person to read every screen.

## Decision

One authored table, `data/strings.en.json`, holding all 566 player-facing names
and descriptions. `ContentDatabase::text(key)` resolves it.

A referenced key with no entry is a **content validation error**, named, that
fails the load and therefore the test suite. Three tests guard the table:

- every key referenced by any definition has text;
- no entry is unreachable from any definition (orphans are deleted or wired up);
- text is printable ASCII, has no leading or trailing space, and `.name` values
  are single-line and under 40 characters.

The domain layer still refuses transactions by content id, because it has no
business knowing display names. The client translates those refusals at the one
layer that does, so "requires research research.field_metallurgy" reaches the
player as "Requires research Field Metallurgy".

## Consequences

- Inspectors have something to say. The screens did not need more layout; they
  needed content, and this is where it lives.
- Adding content now has an extra required step, and the build enforces it. That
  is the point: the alternative was a silent, plausible-looking wrong label.
- The ASCII rule exists because both bitmap faces cover ASCII 32-126 only.
  Anything outside that draws as a blank space, which is exactly the class of
  defect that ships unnoticed — so a test rejects it at the door.
- Nothing checks prose against numbers. A description that quotes "eight per
  cent more yield" will not be caught if the effect is retuned to ten. Writing
  descriptions in terms of the values in the data is a discipline, not a
  guarantee, and `docs/CONTENT_AUTHORING.md` says so.
- The file is named `strings.en.json` and the loader looks for exactly that
  name. This is not a localisation system: there is one language, no locale
  negotiation and no plural or gender handling. It is a strings table that a
  localisation system could later be built around, and calling it anything
  grander would oversell it.
