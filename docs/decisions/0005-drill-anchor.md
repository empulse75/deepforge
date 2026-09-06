# 5. The drill anchors and advances

Status: accepted

## Context

The brief asks that ascending against gravity be the main return-planning
pressure. The first implementation took that literally: gravity always applied,
thrust always cost fuel, and mining upward meant hovering against the ceiling
while the drill worked.

Measured with the balance simulator, that made the return trip cost roughly 1.3
fuel per tile against a 100-unit starting tank. Five of six simulated
expeditions to 90 m ended in a rescue — not because the player made a mistake,
but because the arithmetic did not close.

## Decision

While the mine input is held against a mineable tile, the rig **anchors**: no
gravity, no drift, no thrust cost. Only the drill draws fuel. When the tile
breaks, the rig **advances** into the space it just cut, if that space is free.

## Consequences

- Two genuinely different ways home, with a real trade between them:
  cutting a shaft costs about 0.15 fuel/tile and is slow; flying an open shaft
  costs about 0.28 fuel/tile and is roughly six times faster. Fuel pressure
  survives, because flying is still the fast option and open shafts have to be
  cut in the first place — usually on the way down.
- Rescues in the simulator fell from 5/6 to 0/16 without touching a single
  balance coefficient.
- Downward mining reads the way the genre expects: the rig holds position while
  the tile breaks, then drops into it.
- The mechanic must be explained, so it is one of the first tutorial cards and
  is called out in the README controls section.
- It required a companion fix: the rig's 0.86-tile collision box could straddle
  two columns, so "mine down" occasionally targeted a column the rig was not
  over. A grounded rig with little horizontal speed now settles toward its
  column.
