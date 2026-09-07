# 9. Progression stats are derived, not stored

Status: accepted

## Context

`PlayerProgress::maxUtilityPower` was a stored field, initialised to 6 and
serialised into the save. The modules screen showed "0 / 6 power used", and the
economy refused an install that would exceed it.

Nothing ever changed it. The power core equipment track — nine tiers, up to
2990 credits, named for the thing it was meant to raise — granted a couple of
protected sample slots and otherwise did nothing at all. A player could buy the
whole track and their module budget would still be 6.

The bug survived because the field looked like state. It was saved, loaded and
migrated correctly, and every test that touched it passed.

## Decision

Add a `utility_power_add` effect and compute the budget from the stat block:

```cpp
i32 max_utility_power(const ContentDatabase& db, const PlayerProgress& p) {
    return p.maxUtilityPower + compute_stats(db, p).utilityPower;
}
```

Every consumer — the economy's install check, the hub row, the modules screen
header — calls that function rather than reading the field.

The general rule this settles: **anything an effect can modify is derived from
the effect registry at the point of use, never cached in `PlayerProgress`.**
`PlayerProgress` stores what the player did — tiers bought, modules owned,
research completed. What that adds up to is computed.

## Consequences

- The power core track does what its name and price imply, and each tier is a
  strict improvement over the last.
- Two tests now guard the class of bug rather than the instance: every paid
  equipment tier must have at least one effect, and the power core budget must
  increase strictly at every tier. The first would have caught this on the day
  the track was authored.
- `maxUtilityPower` remains in the save as the base allowance. It stays for save
  compatibility and because a future effect could plausibly want to change the
  base rather than the bonus; it is no longer read directly anywhere else.
- Deriving costs a `compute_stats` call, which walks equipment, modules, skills,
  research and facilities. That is fine at the call sites involved — screen
  builds and transactions — and would not be fine inside the simulation tick.
  The existing rule stands: stats are recomputed when the loadout changes, never
  per tick.
