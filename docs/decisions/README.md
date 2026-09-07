# Architecture decision records

One record per decision that was consequential, contested, or expensive to
reverse. Not every choice needs one — most are just code. These are the ones
where a future reader would otherwise ask "why on earth is it like this?"

Each record states the context that forced a choice, the decision, and the
consequences including the ones that hurt. A decision with no downside listed is
usually a decision that has not been thought about hard enough.

| # | Decision | Status |
| --- | --- | --- |
| [1](0001-language-and-stack.md) | C++23 with CMake and SDL3 | accepted |
| [2](0002-dependencies.md) | Pinned dependencies, and three deliberate omissions | accepted |
| [3](0003-integer-worldgen.md) | World generation is integer-only | accepted |
| [4](0004-terrain-storage.md) | Terrain storage and persistence deltas | accepted |
| [5](0005-drill-anchor.md) | The drill anchors and advances | accepted |
| [6](0006-presentation-layer.md) | The interface is drawn from primitives, in four layers | accepted |
| [7](0007-authored-string-table.md) | All player-facing text is authored content, and a missing key is a build failure | accepted |
| [8](0008-generated-bold-font.md) | Bold is a generated table, not a runtime double-strike | accepted |
| [9](0009-derived-power-budget.md) | Progression stats are derived, not stored | accepted |

## Writing a new one

Copy the shape of an existing record; there is no template file, because a
template invites filling in boxes rather than thinking.

- Number it sequentially and never renumber. A record is a historical document.
- Status is `accepted`, `superseded by NNNN`, or `rejected`. Records are not
  edited to reflect a later change of mind — a new record supersedes an old one,
  and the old one stays as written.
- Say what was actually measured, with the numbers, where a measurement drove
  the decision. Record 5 is the model: it exists because simulated expeditions
  were failing 5 times in 6, and that figure is in the record.
