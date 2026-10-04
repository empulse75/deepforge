# Save format

Save data is treated as a product feature. The guarantees below are enforced by
code and covered by tests in `tests/unit/SaveFile.test.cpp`.

## Guarantees

1. A failed or interrupted write never damages the existing save.
2. Every released schema version loads, through explicit migrations.
3. A corrupted save is reported clearly, left on disk untouched, and a rotating
   backup is offered.
4. Content that no longer exists is dropped with a logged warning rather than
   crashing the load.
5. A save is never written part-way through a transaction or a tile-mutation
   batch.

## Location

| Platform | Path |
| --- | --- |
| Linux | `$XDG_DATA_HOME/deepforge/saves`, else `~/.local/share/deepforge/saves` |
| Windows | `%APPDATA%\Deepforge\saves` |
| Fallback | `<cwd>/deepforge-data/saves` |

Files are `slot<N>.dfsave` with `slot<N>.dfsave.bak` alongside. Slot 0 is the
autosave. `--user-dir <dir>` overrides the whole location, which is what the
tests use.

## File shape

UTF-8 JSON, one space of indentation.

```jsonc
{
  "save_schema_version": 5,      // bumped whenever the payload layout changes
  "content_schema_version": 1,   // shape of data/*.json when written
  "world_gen_version": 1,        // bumped when generation output changes
  "build_version": "0.4.0",
  "timestamp": 1789000000,       // unix seconds
  "play_seconds": 4820.5,
  "summary": "Depth 812 m, 4321 credits",   // shown in the load menu
  "checksum": 12297829382473034410,          // FNV-1a 64 of payload.dump()
  "payload": { ... }
}
```

The checksum is computed over the compact serialisation of `payload` only, so
the header can be read and displayed without trusting the body.

### Payload

| Key | Contents |
| --- | --- |
| `world_seed`, `world_name`, `play_seconds` | World identity |
| `progress.credits`, `.insight`, `.skill_points_earned/_spent` | Currencies and points |
| `progress.equipment` | Object keyed by equipment id -> tier |
| `progress.facilities` | Object keyed by facility id -> tier |
| `progress.reputation` | Object keyed by faction id -> value |
| `progress.owned_modules`, `.installed_modules` | Arrays of module ids (owned is sorted for stable diffs) |
| `progress.skills`, `.research`, `.unlocked_biomes` | Arrays of stable ids |
| `progress.loadout_presets` | Array of arrays of module ids |
| `progress.codex` | `[{id, mined, first_depth}]` |
| `progress.discovered_landmarks`, `.transit_nodes` | Arrays of stable 64-bit ids |
| `progress.transit_node_positions` | `[{id, x, y}]` for activated nodes, so the hub can offer them as launch points |
| `progress.contracts`, `.completed_contracts` | Contract instances with template id, target, progress, claim state |
| `progress.vault`, `.samples` | `[{id, amount}]` |
| `progress.stats` | Playtime statistics |
| `rig` | Position, velocity, fuel, hull, heat, mode, deepest depth this run |
| `cargo`, `samples` | Current expedition inventories |
| `world_deltas` | Per-chunk differences from generated terrain |

Everything references content by **stable string id**. Nothing serialises a
pointer, an array index into a content table, a transient entity number, a
renderer resource or a platform handle.

### World deltas

Terrain is reproducible from the seed, so only divergence is stored:

```jsonc
{
  "cx": -2, "cy": 7,
  "tiles":     [[45, 0, 4]],          // [local index, material index, flags]
  "mining":    [[46, 0.5]],           // [local index, progress 0..1]
  "landmarks": [{"id": 5150, "a": true, "l": false}],
  "revealed":  [0,0,0,255, ...]       // 128-byte fog bitset, omitted when unused
}
```

Fog is a bitset rather than per-tile records specifically so that walking
through a chunk does not create a thousand delta entries.

On load the chunk is regenerated from the seed and the delta replayed over it,
which is why an evicted chunk comes back exactly as the player left it. The
persistence test in `tests/simulation` proves this by mining a tunnel, evicting
every chunk, and re-reading the tiles.

## Atomic write

`SaveSystem::write` does the following, in order:

1. `create_directories` on the save directory; a failure here is reported, not
   swallowed.
2. Serialise to a string. If serialisation fails, nothing on disk is touched.
3. Write `slot<N>.dfsave.tmp`, flush, and check the stream state.
4. **Read the temporary file back and fully deserialise it.** If it does not
   parse and validate, the temporary file is deleted and the write is reported
   as failed — the previous save is still intact.
5. Rotate: delete the old `.bak`, rename the current save to `.bak` (falling back
   to a copy if rename fails across filesystems).
6. Rename the temporary file over the real path.

Step 4 is the important one. A save is never allowed to replace a good file
until it has been proven loadable.

## Autosave boundaries

- On successful return to the hub.
- After a purchase, research completion, skill unlock, facility upgrade,
  refining batch, or contract change.
- Before resolving a failure, and again after.
- On a clean quit from the hub or an expedition.

Autosaves always target slot 0.

## Migration

`kSaveSchemaVersion` is the current version (5). Loading applies migration steps
in sequence until the payload reaches the current version:

| From | To | What changes |
| --- | --- | --- |
| 1 | 2 | Adds the protected sample containers (`payload.samples`, `progress.samples`) |
| 2 | 3 | Adds loadout presets, faction reputation, codex entries and the utility power budget, each with a working default |
| 3 | 4 | Adds transit node positions. Older saves keep their node ids; a node becomes selectable again the next time the player visits it, so nothing is lost |
| 4 | 5 | Adds `payload.content_pack_id`, defaulting to `base`. Mandated from slice 1 so a future content pack cannot reinterpret a save carved from different content |

A save from a **newer** schema is refused with an explicit message telling the
player to update, rather than being loaded partially.

When adding a schema version:

1. Bump `kSaveSchemaVersion` in `src/core/Version.hpp.in`.
2. Add a `migrate_N_to_N+1(json&)` function in `SaveFile.cpp` and a case in the
   migration loop.
3. Add the new version to the loop in the migration test. The test replays a
   schema-1 fixture through every released version and asserts the fields that
   must survive and the defaults that must be filled.

Migrations must never silently discard required data. If a field cannot be
migrated meaningfully, the migration should fail loudly instead.

## Recovery

`SaveSystem::inspect` returns slot metadata for the load menu without trusting
the file: a damaged save is reported with `damaged = true`, the parse error as
its summary, and whether a backup exists. The load menu shows damaged slots in
red and offers "Restore backup of selected slot", which calls
`load_game(slot, useBackup = true)`.

The damaged file is never overwritten or deleted by the recovery path, so a
player can send it in or inspect it themselves.
