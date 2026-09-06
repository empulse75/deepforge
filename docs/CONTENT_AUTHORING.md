# Content authoring

All player-facing content lives in `data/*.json`. Nothing in `src/` contains a
balance number that an author would want to change, and there are no content
switch statements in C++ — behaviour varies through the typed effect registry.

## Rules

- Every definition has a **stable string id** (`material.prismite`,
  `module.thermal_reclaimer`). Ids appear in data files and save files; runtime
  code uses resolved typed handles.
- Every file carries `schema_version`. A file newer than the build understands is
  rejected with a clear message.
- All content is validated before gameplay starts, and **every** problem in a run
  is reported together rather than one per attempt.
- Definition data is immutable after load.
- Display text uses `name_key` / `description_key` localisation keys. Until a
  translation table exists the UI derives a readable name from the key.

## Validating

```sh
./build/dev-release/tools/deepforge-validate --data data
```

A failure looks like this and names the file, definition, field and a fix:

```
content validation FAILED (2 problem(s)):
  - materials.json [material.prismite] .generation: unresolved biome reference
    'biome.prism_fault' (check data/biomes.json for the correct id)
  - recipes.json [recipe.freebie] .outputs: recipe converts a material into more
    of itself for free (this would allow unbounded value creation)
```

The validator also runs as a CTest case, so a bad edit fails the test suite.

## Files and load order

Load order matters only for reference resolution; the validator checks
cross-table references afterwards regardless.

```
materials.json -> hazards.json -> landmarks.json -> biomes.json -> equipment.json
  -> research.json -> modules.json -> skills.json -> facilities.json
  -> recipes.json -> factions.json -> contracts.json -> economy.json
```

## Materials

```jsonc
{
  "id": "material.prismite",
  "name_key": "material.prismite.name",
  "category": "strategic",     // rock|commodity|strategic|volatile|artifact|
                               // structural|boundary|fluid|empty
  "hardness": 2.4,             // drives mine time; scaled by biome hardness_scale
  "density": 1.2,
  "base_value": 40,            // credits per unit at the surface
  "cargo_units": 1,
  "yield": { "min": 1, "max": 3 },
  "light_emission": 0.35,
  "tags": ["crystal", "conductive"],
  "color": [126, 196, 226],    // placeholder presentation only
  "break_audio_id": "audio.crystal_break",
  "generation": {
    "biomes": ["biome.prism_faults", "biome.mycelial_deeps"],
    "min_depth": 270,
    "max_depth": 1050,
    "vein_size": [4, 14],      // random-walk length of one vein
    "rarity_weight_bp": 6599,  // chance in basis points that a vein cell spawns
    "field_period": 13,        // tiles per vein cell; density scales as 1/period^2
    "field_threshold_bp": 3600 // field noise gate; higher clumps veins harder
  }
}
```

`protected_sample: true` (implied for `artifact`) routes the material into
protected sample storage instead of normal cargo, so a failed run cannot cost a
discovery. `volatile_damage` applies hull damage on extraction unless mitigated
by stability. `mineable: false` marks refinery outputs that never generate.

### Validation

Duplicate ids, negative hardness/value/cargo, inverted yield or vein ranges,
inverted depth ranges, unresolved biome references, and any material whose depth
band does not intersect any of its listed biomes (it could never generate).

## Biomes

```jsonc
{
  "id": "biome.prism_faults",
  "min_depth": 250, "max_depth": 650,
  "transition_band": 40,          // tiles over which the previous biome blends out
  "base_material": "material.shale",
  "soft_material": "material.loam",
  "structural_material": "material.granite_spur",
  "cave_threshold_bp": 3000,      // higher = more open space
  "cave_period": 18,
  "ambient_heat": 1.6,            // heat per second at this depth
  "hardness_scale": 1.25,
  "sky_color": [30, 34, 54], "fog_color": [12, 14, 24],
  "hazards":   [{"id": "hazard.shard_burst", "weight_bp": 4500}],
  "landmarks": ["landmark.prism_geode", "landmark.transit_node"],
  "gate_research": "research.deep_survey",
  "gate_depth_proof": 200
}
```

Biome depth coverage must start at 0 and be contiguous; a gap is an error.

## The effect registry

Equipment tiers, modules, skills, research and facility tiers all use the same
effect list, which means every source of a bonus passes through the same caps.

```jsonc
"effects": [
  {"type": "drill_power_mul", "value": 1.3},
  {"type": "grant_flag", "flag": "see_vein_direction"},
  {"type": "convert_heat_to_fuel", "threshold": 0.70, "rate": 0.55}
],
"tradeoffs": [
  {"type": "cooling_mul", "value": 0.88}
]
```

Effect types are declared in `src/content/Effects.hpp` and mapped in
`Effects.cpp`. An unknown type is a validation error, not a silent no-op. Adding
one means adding the enum value, the name mapping, and a case in `apply_effect`
and `StatBlock::clamp_to_caps`.

Capability flags (`grant_flag`) unlock behaviour rather than numbers:
`brake_without_fuel`, `see_vein_direction`, `break_brittle_clusters`,
`survive_heat_spike`, `auto_mark_valuables`, `scan_through_rock`,
`spore_immunity`, `lava_shielding`, `magnetic_stabiliser`, `free_unstuck`,
`transit_access`, `grapple_anchor`, `hover_assist`, `portable_refinery`,
`deployable_beacon`.

We deliberately do **not** have a scripting language. If content eventually needs
behaviour the registry cannot express, add a narrow sandboxed mechanism once
there are concrete use cases.

## Modules

```jsonc
{
  "id": "module.thermal_reclaimer",
  "slot": "utility",              // drill|mobility|survival|survey|utility
  "power_cost": 2,                // against the shared utility power budget
  "cost_credits": 940,
  "cost_components": [{"material": "material.refined_plate", "amount": 2}],
  "unlock": "research.thermal_recovery",
  "effects":   [{"type": "convert_heat_to_fuel", "threshold": 0.70, "rate": 0.55}],
  "tradeoffs": [{"type": "cooling_mul", "value": 0.88}],
  "tags": ["thermal", "efficiency"]
}
```

A module with no effects is a validation error: it cannot create a decision.
Anything strong enough to matter should carry at least one understandable
tradeoff.

## Skills, research, recipes, facilities, contracts

- **Skills** need a branch, row, cost, prerequisites and effects. `keystone: true`
  plus a shared `exclusive_group` makes a set mutually exclusive. Prerequisite
  cycles are detected and rejected.
- **Research** may require a sample material and amount, has prerequisites, and
  unlocks modules, recipes, biomes or passive effects. Cycles are rejected.
- **Recipes** must have inputs and outputs. A recipe that converts a material
  into more of itself for free, or multiplies raw value more than 6x with no
  cost, is a validation error.
- **Facilities** need consecutive tiers from 0 and may unlock recipes.
- **Contracts** need a reward and, for delivery kinds, a non-empty
  `material_pool`. At runtime the eligibility check additionally verifies that
  the target is generated at a reachable depth or producible by an unlocked
  recipe, so the board never offers something impossible.

## Adding content

1. Edit the intent in `tools/content_gen/gen_content.py` (cost curves, tiers and
   tables are authored there so they stay internally consistent), or edit
   `data/*.json` directly for one-off content.
2. Run `python3 tools/content_gen/gen_content.py` if you used the generator, and
   review the diff.
3. Run `deepforge-validate`.
4. For a new ore, set a `target_density_pct` in
   `tools/content_gen/vein_tuning.json` and run the calibrator (see
   `docs/BALANCE_NOTES.md`) rather than guessing `rarity_weight_bp`.
5. Run `ctest`.

## Hot reload

Debug builds may reload content at safe boundaries only (from the main menu or
the hub, never mid-expedition), because handles resolved into runtime state must
stay valid. Shipping builds load once.
