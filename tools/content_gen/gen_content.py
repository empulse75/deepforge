#!/usr/bin/env python3
"""Authoring helper for Project Deepforge content.

Balance curves in the design brief are formulas (Section 10.3). Rather than
hand-typing hundreds of costs and hoping they stay consistent, we author the
*intent* here and emit data/*.json. The emitted files are the source of truth
consumed by the game; re-run this script after editing intent, then review the
diff. Fields that were added to the data by hand after this script last ran are
listed here too, so a re-run reproduces the committed files byte for byte - the
gate keeps that honest via tools/content_gen/check_regeneration_noop.py.

    python3 tools/content_gen/gen_content.py
"""
import json
import os

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "data")
SCHEMA = 1


def round_to_5(v):
    return int(5 * round(v / 5.0))


def cost_curve(base, growth, tiers, milestones=None):
    """upgrade_cost(tier) = round_to_5(base * growth^tier), tier 0 is free."""
    milestones = milestones or {}
    out = [0]
    for t in range(1, tiers):
        c = round_to_5(base * (growth ** (t - 1)))
        if t in milestones:
            c = round_to_5(c * 1.35)  # qualitative tiers cost more
        out.append(c)
    return out


def write(name, obj):
    obj = {"schema_version": SCHEMA, **obj}
    path = os.path.abspath(os.path.join(OUT, name))
    text = json.dumps(obj, indent=2, ensure_ascii=False)
    # The committed data is canonical and a test pins regeneration as byte-for-byte, so a
    # re-run that changes no content changes no bytes: it keeps the final-newline
    # convention of the file it is overwriting. A file it creates still gets one.
    trailing = "\n"
    if os.path.exists(path):
        with open(path, "rb") as f:
            trailing = "\n" if f.read().endswith(b"\n") else ""
    if os.path.exists(path) and open(path, encoding="utf-8").read() == text + trailing:
        print(f"unchanged {path}")
        return
    with open(path, "w", encoding="utf-8") as f:
        f.write(text + trailing)
    print(f"wrote {path}")


# ---------------------------------------------------------------------------
# Biomes
# ---------------------------------------------------------------------------
BIOMES = [
    # id, name, min, max, base, soft, structural, cave_bp, cave_period, heat, hardness
    ("weathered_shelf", "Weathered Shelf", 0, 250, "loam", "loam", "granite_spur", 2600, 22, 0.0, 1.0,
     [40, 36, 46], [16, 15, 20]),
    ("prism_faults", "Prism Faults", 250, 650, "shale", "loam", "granite_spur", 3000, 18, 1.6, 1.25,
     [30, 34, 54], [12, 14, 24]),
    ("mycelial_deeps", "Mycelial Deeps", 650, 1050, "myco_chalk", "loam", "granite_spur", 3600, 16, 2.4, 1.5,
     [28, 44, 36], [10, 20, 15]),
    ("ember_mantle", "Ember Mantle", 1050, 1650, "basalt", "shale", "granite_spur", 2800, 17, 7.5, 1.85,
     [56, 30, 24], [26, 12, 10]),
    ("machine_strata", "Machine Strata", 1650, 2400, "slagcrete", "basalt", "granite_spur", 3200, 19, 5.0, 2.3,
     [38, 40, 48], [14, 16, 22]),
    ("the_fracture", "The Fracture", 2400, 4200, "voidshale", "slagcrete", "granite_spur", 3400, 15, 9.0, 2.9,
     [44, 26, 56], [18, 10, 24]),
]

BIOME_HAZARDS = {
    "weathered_shelf": [("unstable_ceiling", 7000), ("gas_vent", 3000)],
    "prism_faults": [("shard_burst", 4500), ("unstable_ceiling", 3000), ("electric_arc", 2500)],
    "mycelial_deeps": [("spore_cloud", 4500), ("corrosive_seep", 3500), ("unstable_ceiling", 2000)],
    "ember_mantle": [("lava_pocket", 5000), ("gas_vent", 3000), ("shard_burst", 2000)],
    "machine_strata": [("sentry_turret", 3500), ("crushing_piston", 3000), ("magnetic_field", 2000),
                       ("electric_arc", 1500)],
    "the_fracture": [("lava_pocket", 2500), ("electric_arc", 2500), ("magnetic_field", 2500),
                     ("shard_burst", 2500)],
}

BIOME_LANDMARKS = {
    "weathered_shelf": ["supply_cache", "collapsed_survey_post", "open_cavern", "crumbly_seam",
                        "rescue_beacon"],
    "prism_faults": ["supply_cache", "prism_geode", "transit_node", "repair_station", "open_cavern",
                     "challenge_shaft", "artifact_chamber"],
    "mycelial_deeps": ["mycelial_hollow", "supply_cache", "transit_node", "repair_station",
                       "collapsed_survey_post", "artifact_chamber", "rescue_beacon"],
    "ember_mantle": ["thermal_vent", "cooling_pocket", "repair_station", "transit_node", "supply_cache",
                     "challenge_shaft", "artifact_chamber"],
    "machine_strata": ["foundry_ruin", "transit_node", "repair_station", "artifact_chamber",
                       "challenge_shaft", "supply_cache"],
    "the_fracture": ["fracture_gate", "artifact_chamber", "transit_node", "thermal_vent",
                     "challenge_shaft", "repair_station"],
}

BIOME_GATES = {
    "prism_faults": ("", 200),
    "mycelial_deeps": ("research.deep_survey", 600),
    "ember_mantle": ("research.thermal_recovery", 1000),
    "machine_strata": ("research.alloy_forging", 1600),
    "the_fracture": ("research.rift_analysis", 2350),
}

biomes = []
for (bid, name, lo, hi, base, soft, struc, cave, period, heat, hard, sky, fog) in BIOMES:
    gate = BIOME_GATES.get(bid, ("", 0))
    biomes.append({
        "id": f"biome.{bid}",
        "name_key": f"biome.{bid}.name",
        "description_key": f"biome.{bid}.description",
        "min_depth": lo,
        "max_depth": hi,
        "transition_band": 40,
        "base_material": f"material.{base}",
        "soft_material": f"material.{soft}",
        "structural_material": f"material.{struc}",
        "cave_threshold_bp": cave,
        "cave_period": period,
        "ambient_heat": heat,
        "hardness_scale": hard,
        "sky_color": sky,
        "fog_color": fog,
        "music_id": f"audio.music.{bid}",
        "gate_research": gate[0],
        "gate_depth_proof": gate[1],
        "hazards": [{"id": f"hazard.{h}", "weight_bp": w} for h, w in BIOME_HAZARDS[bid]],
        "landmarks": [f"landmark.{l}" for l in BIOME_LANDMARKS[bid]],
    })
write("biomes.json", {"biomes": biomes})

# ---------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------
ALL = ["weathered_shelf", "prism_faults", "mycelial_deeps", "ember_mantle", "machine_strata",
       "the_fracture"]


def mat(mid, name, cat, hardness, value, **kw):
    _current_material[0] = f"material.{mid}"
    d = {
        "id": f"material.{mid}",
        "name_key": f"material.{mid}.name",
        # ADR 7: every referenced key needs authored text, so every definition emits both
        # keys; the materials and recipes were hand-edited to add this before the emitter
        # knew about it, which is the drift the no-op test now pins.
        "description_key": f"material.{mid}.description",
        "category": cat,
        "hardness": hardness,
        "base_value": value,
    }
    d.update(kw)
    return d


TUNING_PATH = os.path.join(os.path.dirname(__file__), "vein_tuning.json")
TUNING = {}
if os.path.exists(TUNING_PATH):
    with open(TUNING_PATH, encoding="utf-8") as _f:
        TUNING = json.load(_f)

_current_material = [None]


def gen(biomes_, lo, hi, vein, weight, period=24, threshold=0):
    """Generation rule. rarity_weight_bp / field_period are calibrated by
    tools/content_gen/calibrate_veins.py against the density targets in
    vein_tuning.json, so edit that file rather than these literals."""
    mid = _current_material[0]
    t = TUNING.get("materials", {}).get(mid, {})
    return {"generation": {"biomes": [f"biome.{b}" for b in biomes_], "min_depth": lo, "max_depth": hi,
                           "vein_size": t.get("vein_size", list(vein)),
                           "rarity_weight_bp": t.get("rarity_weight_bp", weight),
                           "field_period": t.get("field_period", period),
                           "field_threshold_bp": t.get("field_threshold_bp", threshold)}}


materials = [
    mat("air", "Air", "empty", 0.0, 0, solid=False, mineable=False, cargo_units=0,
        color=[8, 8, 12], density=0.0),
    mat("bedrock", "Fracture Bedrock", "boundary", 999.0, 0, mineable=False, cargo_units=0,
        color=[26, 24, 30], density=0.0),

    # --- rock ---
    mat("loam", "Packed Loam", "rock", 0.55, 0, color=[96, 78, 58], cargo_units=0, pattern_id=1,
        break_audio_id="audio.soil_break"),
    mat("shale", "Fault Shale", "rock", 1.05, 0, color=[78, 82, 96], cargo_units=0, pattern_id=2),
    mat("myco_chalk", "Myco Chalk", "rock", 1.45, 0, color=[104, 118, 92], cargo_units=0, pattern_id=3),
    mat("basalt", "Ember Basalt", "rock", 2.5, 0, color=[62, 46, 44], cargo_units=0, pattern_id=4,
        heat_on_mine=1.5),
    mat("slagcrete", "Slagcrete", "rock", 3.3, 0, color=[86, 88, 84], cargo_units=0, pattern_id=5),
    mat("voidshale", "Void Shale", "rock", 4.2, 0, color=[54, 44, 70], cargo_units=0, pattern_id=6),
    mat("granite_spur", "Granite Spur", "structural", 7.5, 0, color=[120, 116, 112], cargo_units=0,
        pattern_id=7),

    # --- commodities ---
    dict(mat("ferrite", "Ferrite", "commodity", 1.15, 5, color=[168, 122, 92], density=1.0,
             yield_={"min": 1, "max": 2}, tags=["metal", "common"], break_audio_id="audio.ore_break"),
         **gen(ALL[:3], 8, 700, (4, 12), 3400, 22, 1800)),
    dict(mat("cuprite", "Cuprite", "commodity", 1.5, 13, color=[196, 124, 62], density=1.05,
             tags=["metal", "conductive"], break_audio_id="audio.ore_break"),
         **gen(ALL[:4], 110, 1200, (3, 10), 2400, 20, 3000)),
    dict(mat("argentine", "Argentine", "commodity", 2.0, 34, color=[196, 200, 208], density=1.2,
             tags=["metal", "precious"], break_audio_id="audio.ore_break"),
         **gen(ALL[1:5], 380, 1800, (3, 9), 1700, 19, 4200)),
    dict(mat("aurelite", "Aurelite", "commodity", 2.6, 72, color=[214, 176, 66], density=1.4,
             tags=["metal", "precious"], break_audio_id="audio.ore_break"),
         **gen(ALL[2:6], 760, 2600, (2, 8), 1100, 18, 5200)),
    dict(mat("platinex", "Platinex", "commodity", 3.4, 148, color=[178, 208, 214], density=1.6,
             tags=["metal", "precious", "dense"], break_audio_id="audio.ore_break"),
         **gen(ALL[3:6], 1350, 3400, (2, 7), 800, 17, 6000)),
    dict(mat("starmetal", "Starmetal", "commodity", 4.4, 300, color=[228, 234, 255], density=1.9,
             tags=["metal", "precious", "rare"], break_audio_id="audio.ore_break"),
         **gen(ALL[4:6], 2100, 4200, (2, 6), 520, 16, 6800)),

    # --- strategic ---
    dict(mat("prismite", "Prismite", "strategic", 2.4, 40, color=[126, 196, 226], density=1.2,
             yield_={"min": 1, "max": 3}, tags=["crystal", "conductive"],
             break_audio_id="audio.crystal_break", light_emission=0.35),
         **gen(["prism_faults", "mycelial_deeps"], 270, 1050, (4, 14), 1900, 20, 3600)),
    dict(mat("mycelith", "Mycelith", "strategic", 2.1, 50, color=[142, 214, 150], density=0.9,
             yield_={"min": 1, "max": 3}, tags=["organic", "bio"], light_emission=0.25),
         **gen(["mycelial_deeps", "ember_mantle"], 680, 1500, (4, 12), 1700, 20, 3800)),
    dict(mat("emberglass", "Emberglass", "strategic", 3.0, 84, color=[236, 138, 74], density=1.3,
             tags=["thermal", "crystal"], heat_on_mine=3.0, light_emission=0.5,
             break_audio_id="audio.crystal_break"),
         **gen(["ember_mantle", "machine_strata"], 1080, 2400, (3, 10), 1300, 19, 4600)),
    dict(mat("cogsteel", "Cogsteel", "strategic", 3.8, 130, color=[150, 158, 176], density=1.7,
             tags=["metal", "machined"]),
         **gen(["machine_strata", "the_fracture"], 1680, 3400, (3, 9), 1000, 18, 5000)),
    dict(mat("riftalloy", "Riftalloy", "strategic", 4.8, 215, color=[186, 140, 236], density=1.8,
             tags=["exotic", "metal"], light_emission=0.4),
         **gen(["the_fracture"], 2420, 4200, (3, 8), 900, 17, 5200)),

    # --- volatile ---
    dict(mat("fulmenite", "Fulmenite", "volatile", 2.2, 165, color=[248, 208, 96], density=1.1,
             volatile_damage=16.0, heat_on_mine=6.0, tags=["volatile", "energetic"],
             light_emission=0.7, break_audio_id="audio.volatile_break"),
         **gen(["ember_mantle", "machine_strata"], 1100, 2400, (2, 5), 700, 26, 6200)),
    dict(mat("voidcell", "Void Cell", "volatile", 3.2, 340, color=[126, 96, 236], density=1.2,
             volatile_damage=26.0, heat_on_mine=4.0, tags=["volatile", "exotic"],
             light_emission=0.8, break_audio_id="audio.volatile_break"),
         **gen(["the_fracture"], 2450, 4200, (2, 4), 520, 26, 6900)),

    # --- artifacts (protected samples) ---
    dict(mat("echo_shard", "Echo Shard", "artifact", 2.8, 0, color=[150, 236, 236], density=0.5,
             protected_sample=True, tags=["sample", "crystal"], light_emission=0.6,
             break_audio_id="audio.artifact_break"),
         **gen(["prism_faults"], 300, 650, (1, 1), 90, 40, 8600)),
    dict(mat("spore_codex", "Spore Codex", "artifact", 2.6, 0, color=[196, 236, 150], density=0.5,
             protected_sample=True, tags=["sample", "organic"], light_emission=0.5,
             break_audio_id="audio.artifact_break"),
         **gen(["mycelial_deeps"], 700, 1050, (1, 1), 90, 40, 8600)),
    dict(mat("relic_core", "Relic Core", "artifact", 3.6, 0, color=[236, 196, 120], density=0.6,
             protected_sample=True, tags=["sample", "machined"], light_emission=0.6,
             break_audio_id="audio.artifact_break"),
         **gen(["machine_strata"], 1700, 2400, (1, 1), 80, 40, 8700)),
    dict(mat("fracture_seed", "Fracture Seed", "artifact", 4.6, 0, color=[236, 150, 236], density=0.6,
             protected_sample=True, tags=["sample", "exotic"], light_emission=0.9,
             break_audio_id="audio.artifact_break"),
         **gen(["the_fracture"], 2500, 4200, (1, 1), 70, 40, 8800)),

    # --- refined outputs (never generated) ---
    mat("refined_plate", "Refined Plate", "strategic", 0.0, 90, color=[186, 190, 198], density=1.0,
        mineable=False, solid=False, tags=["component", "refined"]),
    mat("circuit_lattice", "Circuit Lattice", "strategic", 0.0, 170, color=[132, 224, 200], density=0.7,
        mineable=False, solid=False, tags=["component", "refined"]),
    mat("thermal_core", "Thermal Core", "strategic", 0.0, 260, color=[240, 150, 90], density=0.9,
        mineable=False, solid=False, tags=["component", "refined"]),
    mat("drive_cell", "Drive Cell", "strategic", 0.0, 420, color=[168, 150, 248], density=0.8,
        mineable=False, solid=False, tags=["component", "refined"]),
]
# `yield_` -> `yield`
for m in materials:
    if "yield_" in m:
        m["yield"] = m.pop("yield_")
write("materials.json", {"materials": materials})

# ---------------------------------------------------------------------------
# Hazards
# ---------------------------------------------------------------------------
hazards = [
    dict(id="hazard.lava_pocket", kind="lava_pocket", contact_damage=16.0, area_damage=3.0, radius=2.2,
         heat_per_second=26.0, telegraph_seconds=0.8, min_depth=1000, countered_by="lava_shielding",
         color=[250, 130, 50]),
    dict(id="hazard.gas_vent", kind="gas_vent", contact_damage=6.0, area_damage=2.0, radius=2.6,
         heat_per_second=8.0, telegraph_seconds=1.1, min_depth=60, color=[190, 220, 140]),
    dict(id="hazard.unstable_ceiling", kind="unstable_ceiling", contact_damage=14.0, area_damage=0.0,
         radius=1.4, telegraph_seconds=1.4, min_depth=20, color=[150, 140, 130]),
    dict(id="hazard.spore_cloud", kind="spore_cloud", contact_damage=3.0, area_damage=2.5, radius=3.2,
         telegraph_seconds=1.2, min_depth=640, countered_by="spore_immunity", color=[170, 230, 160]),
    dict(id="hazard.corrosive_seep", kind="corrosion", contact_damage=9.0, area_damage=1.5, radius=1.8,
         telegraph_seconds=1.0, min_depth=680, color=[190, 240, 110]),
    dict(id="hazard.electric_arc", kind="electric_arc", contact_damage=13.0, area_damage=0.0, radius=2.0,
         telegraph_seconds=0.9, min_depth=260, countered_by="magnetic_stabiliser", color=[150, 210, 255]),
    dict(id="hazard.crushing_piston", kind="crushing_piston", contact_damage=26.0, area_damage=0.0,
         radius=1.2, telegraph_seconds=1.5, min_depth=1650, color=[190, 170, 120]),
    dict(id="hazard.magnetic_field", kind="magnetic_field", contact_damage=2.0, area_damage=1.0,
         radius=3.6, telegraph_seconds=1.0, min_depth=1650, countered_by="magnetic_stabiliser",
         color=[140, 150, 240]),
    dict(id="hazard.sentry_turret", kind="sentry_turret", contact_damage=11.0, area_damage=4.0, radius=5.0,
         telegraph_seconds=1.3, min_depth=1700, color=[240, 90, 90]),
    dict(id="hazard.shard_burst", kind="shard_burst", contact_damage=10.0, area_damage=3.0, radius=2.4,
         telegraph_seconds=0.7, min_depth=250, color=[200, 230, 255]),
]
for h in hazards:
    h["name_key"] = h["id"] + ".name"
    h["description_key"] = h["id"] + ".description"
write("hazards.json", {"hazards": hazards})

# ---------------------------------------------------------------------------
# Landmarks
# ---------------------------------------------------------------------------
def lm(lid, kind, lo, hi, w, h, spacing, rmin, rmax, region, biomes_, **kw):
    d = dict(id=f"landmark.{lid}", name_key=f"landmark.{lid}.name",
             description_key=f"landmark.{lid}.description", kind=kind, min_depth=lo, max_depth=hi,
             width=w, height=h, min_spacing=spacing, per_region_min=rmin, per_region_max=rmax,
             region_height=region, biomes=[f"biome.{b}" for b in biomes_])
    d.update(kw)
    return d


landmarks = [
    lm("supply_cache", "cache", 40, 4200, 5, 3, 130, 1, 2, 200,
       ["weathered_shelf", "prism_faults", "mycelial_deeps", "ember_mantle", "machine_strata",
        "the_fracture"],
       loot_materials=["material.ferrite", "material.cuprite", "material.argentine"],
       loot_amount_min=4, loot_amount_max=12),
    lm("repair_station", "repair_station", 260, 4200, 7, 5, 320, 1, 1, 350,
       ["prism_faults", "mycelial_deeps", "ember_mantle", "machine_strata", "the_fracture"]),
    lm("transit_node", "transit_node", 280, 4200, 9, 6, 380, 1, 1, 400,
       ["prism_faults", "mycelial_deeps", "ember_mantle", "machine_strata", "the_fracture"]),
    lm("thermal_vent", "thermal_vent", 1050, 4200, 5, 8, 180, 1, 3, 250,
       ["ember_mantle", "the_fracture"]),
    lm("cooling_pocket", "cooling_pocket", 1050, 1650, 6, 4, 200, 1, 2, 250, ["ember_mantle"]),
    lm("collapsed_survey_post", "collapsed_facility", 60, 1050, 11, 6, 300, 1, 1, 300,
       ["weathered_shelf", "mycelial_deeps"],
       loot_materials=["material.ferrite", "material.cuprite"], loot_amount_min=3, loot_amount_max=9),
    lm("artifact_chamber", "artifact_chamber", 300, 4200, 9, 7, 420, 1, 1, 420,
       ["prism_faults", "mycelial_deeps", "ember_mantle", "machine_strata", "the_fracture"],
       loot_materials=["material.echo_shard", "material.spore_codex", "material.relic_core",
                       "material.fracture_seed"],
       loot_amount_min=1, loot_amount_max=1),
    lm("challenge_shaft", "challenge_room", 300, 4200, 9, 14, 500, 0, 1, 500,
       ["prism_faults", "ember_mantle", "machine_strata", "the_fracture"],
       loot_materials=["material.aurelite", "material.platinex"], loot_amount_min=6, loot_amount_max=16),
    lm("rescue_beacon", "rescue_beacon", 80, 1050, 3, 3, 260, 0, 1, 300,
       ["weathered_shelf", "mycelial_deeps"]),
    lm("crumbly_seam", "crumbly_layer", 60, 250, 14, 2, 150, 1, 2, 200, ["weathered_shelf"]),
    lm("open_cavern", "cavern", 30, 650, 16, 9, 180, 1, 2, 220, ["weathered_shelf", "prism_faults"]),
    lm("prism_geode", "cavern", 270, 650, 11, 8, 240, 1, 2, 250, ["prism_faults"],
       loot_materials=["material.prismite"], loot_amount_min=6, loot_amount_max=14),
    lm("mycelial_hollow", "cavern", 680, 1050, 13, 9, 240, 1, 2, 250, ["mycelial_deeps"],
       loot_materials=["material.mycelith"], loot_amount_min=6, loot_amount_max=14),
    lm("foundry_ruin", "collapsed_facility", 1680, 2400, 15, 8, 340, 1, 1, 350, ["machine_strata"],
       loot_materials=["material.cogsteel", "material.platinex"], loot_amount_min=5, loot_amount_max=12),
    lm("fracture_gate", "set_piece", 2600, 4200, 17, 12, 4000, 1, 1, 1600, ["the_fracture"], unique=True),
]
write("landmarks.json", {"landmarks": landmarks})

# ---------------------------------------------------------------------------
# Equipment - 9 tracks. Milestone tiers grant qualitative capability.
# ---------------------------------------------------------------------------
def tiers(track_id, base, growth, count, per_tier_effects, milestones):
    costs = cost_curve(base, growth, count, milestones)
    out = []
    for t in range(count):
        eff = list(per_tier_effects(t))
        if t in milestones:
            eff.extend(milestones[t]["effects"])
        out.append({
            "tier": t,
            "cost_credits": costs[t],
            "name_key": f"equipment.{track_id}.tier{t}",
            "summary_key": milestones[t]["summary_key"] if t in milestones else
                           f"equipment.{track_id}.tier{t}.summary",
            "effects": eff,
        })
    # strategic component costs on the last third of the track
    comp_by_track = {
        "drill_power": "material.refined_plate",
        "drill_efficiency": "material.circuit_lattice",
        "engine": "material.drive_cell",
        "fuel_tank": "material.refined_plate",
        "cargo_bay": "material.refined_plate",
        "hull": "material.refined_plate",
        "cooling": "material.thermal_core",
        "scanner": "material.circuit_lattice",
        "power_core": "material.drive_cell",
    }
    comp = comp_by_track[track_id]
    for t in range(count):
        if t >= count - 3 and t > 0:
            amount = 1 + (t - (count - 3))
            out[t]["cost_components"] = [{"material": comp, "amount": amount}]
    return out


# ADR 9: the power core is the only thing that raises the module power budget, so every
# tier the player pays for moves it. The ladder is +1 per tier until the top tier, which
# is the one a full module loadout is bought for and so jumps to 7. data/strings.en.json
# quotes these numbers in the tier summaries, so both sides move together.
POWER_CORE_POWER = {1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 6: 7}


def power_core_effects(tier):
    if not tier:
        return []
    effects = [{"type": "utility_power_add", "value": POWER_CORE_POWER[tier]}]
    if tier % 3 == 0:  # qualitative tiers also carry a protected sample slot
        effects.append({"type": "protected_sample_slots", "value": 1})
    return effects


def eq(track_id, name, base, growth, count, per_tier, milestones):
    return {
        "id": f"equipment.{track_id}",
        "name_key": f"equipment.{track_id}.name",
        "description_key": f"equipment.{track_id}.description",
        "track": track_id,
        "tiers": tiers(track_id, base, growth, count, per_tier, milestones),
    }


equipment = [
    eq("drill_power", "Drill Power", 195, 1.62, 9,
       lambda t: [{"type": "drill_power_add", "value": round(0.55 * t, 3)}] if t else [],
       {3: {"summary_key": "equipment.drill_power.tier3.summary",
            "effects": [{"type": "grant_flag", "flag": "break_brittle_clusters"}]},
        6: {"summary_key": "equipment.drill_power.tier6.summary",
            "effects": [{"type": "mine_radius_add", "value": 1}]}}),
    eq("drill_efficiency", "Drill Efficiency", 165, 1.55, 8,
       lambda t: [{"type": "drill_efficiency_mul", "value": round(1.0 - 0.07 * t, 3)}] if t else [],
       {4: {"summary_key": "equipment.drill_efficiency.tier4.summary",
            "effects": [{"type": "grant_flag", "flag": "see_vein_direction"}]}}),
    eq("engine", "Engine Assembly", 215, 1.58, 9,
       lambda t: ([{"type": "thrust_mul", "value": round(1.0 + 0.11 * t, 3)},
                   {"type": "brake_mul", "value": round(1.0 + 0.09 * t, 3)}] if t else []),
       {2: {"summary_key": "equipment.engine.tier2.summary",
            "effects": [{"type": "grant_flag", "flag": "brake_without_fuel"}]},
        7: {"summary_key": "equipment.engine.tier7.summary",
            "effects": [{"type": "grant_flag", "flag": "hover_assist"}]}}),
    eq("fuel_tank", "Fuel Cell Rack", 150, 1.5, 9,
       lambda t: [{"type": "fuel_capacity_add", "value": 42 * t}] if t else [],
       {5: {"summary_key": "equipment.fuel_tank.tier5.summary",
            "effects": [{"type": "hover_fuel_mul", "value": 0.85}]}}),
    eq("cargo_bay", "Cargo Bay", 180, 1.52, 9,
       lambda t: [{"type": "cargo_capacity_add", "value": 22 * t}] if t else [],
       {4: {"summary_key": "equipment.cargo_bay.tier4.summary",
            "effects": [{"type": "grant_flag", "flag": "auto_mark_valuables"}]},
        7: {"summary_key": "equipment.cargo_bay.tier7.summary",
            "effects": [{"type": "cargo_mass_mul", "value": 0.8}]}}),
    eq("hull", "Hull Plating", 200, 1.56, 9,
       lambda t: ([{"type": "hull_max_add", "value": 26 * t},
                   {"type": "impact_damage_mul", "value": round(1.0 - 0.05 * t, 3)}] if t else []),
       {5: {"summary_key": "equipment.hull.tier5.summary",
            "effects": [{"type": "grant_flag", "flag": "free_unstuck"}]}}),
    eq("cooling", "Cooling Loop", 225, 1.6, 8,
       lambda t: ([{"type": "cooling_mul", "value": round(1.0 + 0.16 * t, 3)},
                   {"type": "heat_capacity_add", "value": 12 * t}] if t else []),
       {3: {"summary_key": "equipment.cooling.tier3.summary",
            "effects": [{"type": "grant_flag", "flag": "survive_heat_spike"}]}}),
    eq("scanner", "Survey Scanner", 240, 1.57, 8,
       lambda t: ([{"type": "scanner_range_add", "value": 4 * t},
                   {"type": "scanner_detail_add", "value": round(0.28 * t, 3)}] if t else []),
       {2: {"summary_key": "equipment.scanner.tier2.summary",
            "effects": [{"type": "vein_reveal_add", "value": 0.35}]},
        6: {"summary_key": "equipment.scanner.tier6.summary",
            "effects": [{"type": "grant_flag", "flag": "scan_through_rock"}]}}),
    eq("power_core", "Utility Power Core", 260, 1.63, 7,
       power_core_effects,
       {1: {"summary_key": "equipment.power_core.tier1.summary", "effects": []},
        4: {"summary_key": "equipment.power_core.tier4.summary", "effects": []}}),
]
write("equipment.json", {"equipment": equipment})

# ---------------------------------------------------------------------------
# Research web
# ---------------------------------------------------------------------------
def rs(rid, cost, prereqs=(), sample=None, sample_amount=1, modules=(), recipes=(), biomes_=(),
       effects=(), capstone=False):
    d = dict(id=f"research.{rid}", name_key=f"research.{rid}.name",
             description_key=f"research.{rid}.description", insight_cost=cost,
             prerequisites=[f"research.{p}" for p in prereqs])
    if sample:
        d["requires_sample"] = f"material.{sample}"
        d["requires_sample_amount"] = sample_amount
    if modules:
        d["unlocks_modules"] = [f"module.{m}" for m in modules]
    if recipes:
        d["unlocks_recipes"] = [f"recipe.{r}" for r in recipes]
    if biomes_:
        d["unlocks_biomes"] = [f"biome.{b}" for b in biomes_]
    if effects:
        d["effects"] = list(effects)
    if capstone:
        d["capstone"] = True
    return d


research = [
    rs("field_metallurgy", 1, recipes=["refined_plate"], modules=["focused_cutter"]),
    rs("crystal_optics", 2, ("field_metallurgy",), sample="echo_shard",
       modules=["directional_scanner", "material_filter"], recipes=["circuit_lattice"]),
    rs("deep_survey", 3, ("crystal_optics",), modules=["hazard_imager"],
       biomes_=["mycelial_deeps"],
       effects=[{"type": "scanner_range_add", "value": 3}]),
    rs("symbiotic_shielding", 3, ("deep_survey",), sample="spore_codex",
       modules=["spore_filter", "bio_regenerator"],
       effects=[{"type": "grant_flag", "flag": "spore_immunity"}]),
    rs("thermal_recovery", 4, ("deep_survey",), modules=["thermal_reclaimer", "heat_sink"],
       recipes=["thermal_core"], biomes_=["ember_mantle"]),
    rs("volatile_handling", 4, ("thermal_recovery",),
       modules=["precision_extractor", "kinetic_buffer"],
       effects=[{"type": "volatile_stability_add", "value": 0.35}]),
    rs("alloy_forging", 5, ("thermal_recovery",), modules=["insulated_plating"],
       recipes=["drive_cell", "emberglass_ingot"], biomes_=["machine_strata"]),
    rs("ancient_mechanisms", 6, ("alloy_forging",), sample="relic_core",
       modules=["landmark_triangulator", "grappling_anchor"],
       effects=[{"type": "grant_flag", "flag": "transit_access"}]),
    rs("magnetic_damping", 5, ("ancient_mechanisms",), modules=["field_stabiliser"],
       effects=[{"type": "grant_flag", "flag": "magnetic_stabiliser"}]),
    rs("lattice_engineering", 6, ("ancient_mechanisms",), modules=["cargo_compactor", "auto_collector"],
       recipes=["lattice_press"]),
    rs("rift_analysis", 8, ("magnetic_damping", "lattice_engineering"), sample="fracture_seed",
       modules=["rift_lens"], biomes_=["the_fracture"]),
    rs("pulse_drilling", 5, ("volatile_handling",), modules=["pulse_drill", "wide_fracture_head"]),
    rs("expedition_logistics", 4, ("field_metallurgy",),
       modules=["deployable_beacon", "portable_refinery", "low_fuel_hover"],
       effects=[{"type": "grant_flag", "flag": "deployable_beacon"}]),
    rs("salvage_protocols", 3, ("expedition_logistics",), modules=["emergency_seal", "afterburner"],
       effects=[{"type": "wreck_recovery_add", "value": 0.25},
                {"type": "rescue_fee_mul", "value": 0.8}]),
    rs("deep_cooling", 6, ("thermal_recovery", "salvage_protocols"), modules=["cryo_manifold"],
       effects=[{"type": "cooling_mul", "value": 1.15}]),
    rs("resonance_mapping", 7, ("crystal_optics", "ancient_mechanisms"), modules=["resonance_array"],
       effects=[{"type": "grant_flag", "flag": "scan_through_rock"}]),
    rs("fracture_containment", 10, ("rift_analysis",), modules=["containment_field"],
       effects=[{"type": "hazard_damage_mul", "value": 0.85}]),
    rs("deepforge_protocol", 14, ("fracture_containment", "resonance_mapping", "deep_cooling"),
       capstone=True,
       effects=[{"type": "drill_power_mul", "value": 1.2},
                {"type": "hull_max_mul", "value": 1.15},
                {"type": "grant_flag", "flag": "lava_shielding"}]),
]
write("research.json", {"research": research})

# ---------------------------------------------------------------------------
# Modules - 26 distinct, six archetypes
# ---------------------------------------------------------------------------
def md(mid, slot, power, cost, effects, tradeoffs=(), unlock=None, tags=(), components=()):
    d = dict(id=f"module.{mid}", name_key=f"module.{mid}.name",
             description_key=f"module.{mid}.description", slot=slot, power_cost=power,
             cost_credits=cost, effects=list(effects), tags=list(tags))
    if tradeoffs:
        d["tradeoffs"] = list(tradeoffs)
    if unlock:
        d["unlock"] = f"research.{unlock}"
    if components:
        d["cost_components"] = [{"material": f"material.{m}", "amount": a} for m, a in components]
    return d


modules = [
    # --- drill ---
    md("focused_cutter", "drill", 1, 220, [{"type": "drill_power_mul", "value": 1.3}],
       [{"type": "mine_radius_add", "value": 0}], "field_metallurgy", ["drill", "precision"]),
    md("wide_fracture_head", "drill", 3, 900,
       [{"type": "mine_radius_add", "value": 1}, {"type": "yield_mul", "value": 1.1}],
       [{"type": "drill_efficiency_mul", "value": 1.35}, {"type": "drill_power_mul", "value": 0.85}],
       "pulse_drilling", ["drill", "area"], [("refined_plate", 2)]),
    md("pulse_drill", "drill", 3, 1050,
       [{"type": "drill_power_mul", "value": 1.75}, {"type": "heat_generation_mul", "value": 1.4}],
       [{"type": "cooling_mul", "value": 0.9}], "pulse_drilling", ["drill", "thermal"],
       [("thermal_core", 1)]),
    md("precision_extractor", "drill", 2, 780,
       [{"type": "yield_mul", "value": 1.35}, {"type": "volatile_stability_add", "value": 0.4}],
       [{"type": "drill_power_mul", "value": 0.9}], "volatile_handling", ["drill", "volatile"]),
    md("harmonic_bit", "drill", 2, 640,
       [{"type": "drill_power_mul", "value": 1.22}, {"type": "drill_efficiency_mul", "value": 0.85}],
       [{"type": "impact_damage_mul", "value": 1.15}], "crystal_optics", ["drill", "crystal"]),

    # --- mobility ---
    md("afterburner", "mobility", 2, 480,
       [{"type": "thrust_mul", "value": 1.4}],
       [{"type": "hover_fuel_mul", "value": 1.3}, {"type": "heat_generation_mul", "value": 1.15}],
       "salvage_protocols", ["mobility", "speed"]),
    md("stabiliser_gyros", "mobility", 1, 320,
       [{"type": "brake_mul", "value": 1.45}, {"type": "impact_damage_mul", "value": 0.8}],
       [{"type": "thrust_mul", "value": 0.95}], None, ["mobility", "safety"]),
    md("grappling_anchor", "mobility", 2, 860,
       [{"type": "grant_flag", "flag": "grapple_anchor"}, {"type": "hover_fuel_mul", "value": 0.9}],
       [{"type": "cargo_capacity_add", "value": -6}], "ancient_mechanisms", ["mobility", "traversal"]),
    md("low_fuel_hover", "mobility", 2, 540,
       [{"type": "grant_flag", "flag": "hover_assist"}, {"type": "hover_fuel_mul", "value": 0.72}],
       [{"type": "thrust_mul", "value": 0.9}], "expedition_logistics", ["mobility", "efficiency"]),
    md("ballast_trim", "mobility", 1, 400,
       [{"type": "cargo_mass_mul", "value": 0.75}],
       [{"type": "brake_mul", "value": 0.9}], None, ["mobility", "cargo"]),

    # --- survival ---
    md("heat_sink", "survival", 2, 560, [{"type": "cooling_mul", "value": 1.4}],
       [{"type": "cargo_capacity_add", "value": -8}], "thermal_recovery", ["thermal", "survival"]),
    md("kinetic_buffer", "survival", 2, 620, [{"type": "impact_damage_mul", "value": 0.55}],
       [{"type": "thrust_mul", "value": 0.92}], "volatile_handling", ["survival", "armor"]),
    md("emergency_seal", "survival", 1, 380,
       [{"type": "hull_max_add", "value": 30}, {"type": "rescue_fee_mul", "value": 0.7}],
       [{"type": "cargo_mass_mul", "value": 1.08}], "salvage_protocols", ["survival", "recovery"]),
    md("insulated_plating", "survival", 3, 1180,
       [{"type": "hazard_damage_mul", "value": 0.6}, {"type": "depth_heat_mul", "value": 0.75}],
       [{"type": "thrust_mul", "value": 0.88}, {"type": "cargo_mass_mul", "value": 1.12}],
       "alloy_forging", ["survival", "thermal"], [("refined_plate", 2), ("thermal_core", 1)]),
    md("spore_filter", "survival", 1, 470, [{"type": "grant_flag", "flag": "spore_immunity"}],
       [{"type": "cooling_mul", "value": 0.92}], "symbiotic_shielding", ["survival", "bio"]),
    md("bio_regenerator", "survival", 2, 900,
       [{"type": "hull_max_mul", "value": 1.12}, {"type": "repair_cost_mul", "value": 0.75}],
       [{"type": "cargo_capacity_add", "value": -6}], "symbiotic_shielding", ["survival", "bio"]),
    md("cryo_manifold", "survival", 3, 1450,
       [{"type": "cooling_mul", "value": 1.7}, {"type": "heat_capacity_add", "value": 40}],
       [{"type": "drill_power_mul", "value": 0.92}], "deep_cooling", ["thermal", "survival"],
       [("thermal_core", 2)]),
    md("field_stabiliser", "survival", 2, 1150,
       [{"type": "grant_flag", "flag": "magnetic_stabiliser"},
        {"type": "hazard_damage_mul", "value": 0.85}],
       [{"type": "scanner_range_add", "value": -3}], "magnetic_damping", ["survival", "machine"]),
    md("containment_field", "survival", 4, 2200,
       [{"type": "volatile_stability_add", "value": 0.6}, {"type": "hazard_damage_mul", "value": 0.7}],
       [{"type": "thrust_mul", "value": 0.85}], "fracture_containment", ["survival", "volatile"],
       [("drive_cell", 1), ("thermal_core", 1)]),

    # --- survey ---
    md("directional_scanner", "survey", 2, 520,
       [{"type": "scanner_range_add", "value": 6}, {"type": "grant_flag", "flag": "see_vein_direction"}],
       [{"type": "drill_efficiency_mul", "value": 1.06}], "crystal_optics", ["survey", "scanner"]),
    md("material_filter", "survey", 1, 340,
       [{"type": "scanner_detail_add", "value": 0.6}, {"type": "grant_flag", "flag": "auto_mark_valuables"}],
       (), "crystal_optics", ["survey", "scanner"]),
    md("hazard_imager", "survey", 2, 700,
       [{"type": "scanner_detail_add", "value": 0.5}, {"type": "hazard_damage_mul", "value": 0.82}],
       [{"type": "scanner_range_add", "value": -2}], "deep_survey", ["survey", "safety"]),
    md("landmark_triangulator", "survey", 2, 980,
       [{"type": "scanner_range_add", "value": 10}, {"type": "contract_reward_mul", "value": 1.12}],
       [{"type": "cooling_mul", "value": 0.94}], "ancient_mechanisms", ["survey", "contracts"]),
    md("resonance_array", "survey", 3, 1600,
       [{"type": "grant_flag", "flag": "scan_through_rock"}, {"type": "scanner_range_add", "value": 12},
        {"type": "insight_gain_mul", "value": 1.2}],
       [{"type": "hull_max_add", "value": -20}], "resonance_mapping", ["survey", "research"],
       [("circuit_lattice", 2)]),
    md("rift_lens", "survey", 3, 2100,
       [{"type": "scanner_detail_add", "value": 1.0}, {"type": "insight_gain_mul", "value": 1.35}],
       [{"type": "heat_generation_mul", "value": 1.2}], "rift_analysis", ["survey", "exotic"],
       [("circuit_lattice", 2), ("drive_cell", 1)]),

    # --- utility ---
    md("cargo_compactor", "utility", 2, 880,
       [{"type": "cargo_capacity_add", "value": 28}],
       [{"type": "cargo_mass_mul", "value": 1.15}], "lattice_engineering", ["utility", "cargo"]),
    md("auto_collector", "utility", 1, 620, [{"type": "auto_collect_radius", "value": 4.5}],
       [{"type": "drill_efficiency_mul", "value": 1.05}], "lattice_engineering", ["utility", "quality"]),
    md("deployable_beacon", "utility", 1, 420,
       [{"type": "grant_flag", "flag": "deployable_beacon"}, {"type": "wreck_recovery_add", "value": 0.2}],
       (), "expedition_logistics", ["utility", "recovery"]),
    md("portable_refinery", "utility", 3, 1350,
       [{"type": "grant_flag", "flag": "portable_refinery"}, {"type": "refine_yield_mul", "value": 1.1}],
       [{"type": "cargo_capacity_add", "value": -12}], "expedition_logistics", ["utility", "economy"],
       [("refined_plate", 2)]),
    md("thermal_reclaimer", "utility", 2, 940,
       [{"type": "convert_heat_to_fuel", "threshold": 0.7, "rate": 0.55}],
       [{"type": "cooling_mul", "value": 0.88}], "thermal_recovery", ["thermal", "efficiency"]),
    md("assay_uplink", "utility", 1, 760,
       [{"type": "sale_value_mul", "value": 1.12}, {"type": "contract_reward_mul", "value": 1.1}],
       [{"type": "cargo_capacity_add", "value": -5}], None, ["utility", "economy"]),
]
write("modules.json", {"modules": modules})

# ---------------------------------------------------------------------------
# Skill tree - 36 nodes across three branches, with exclusive keystones
# ---------------------------------------------------------------------------
def sk(sid, branch, row, cost, effects, prereqs=(), keystone=False, group=None):
    d = dict(id=f"skill.{sid}", name_key=f"skill.{sid}.name",
             description_key=f"skill.{sid}.description", branch=branch, row=row, cost=cost,
             effects=list(effects), prerequisites=[f"skill.{p}" for p in prereqs])
    if keystone:
        d["keystone"] = True
    if group:
        d["exclusive_group"] = group
    return d


skills = [
    # --- Prospector ---
    sk("keen_eye", "prospector", 0, 1, [{"type": "scanner_range_add", "value": 2}]),
    sk("rich_seams", "prospector", 1, 1, [{"type": "yield_mul", "value": 1.08}], ("keen_eye",)),
    sk("assayer", "prospector", 1, 1, [{"type": "sale_value_mul", "value": 1.06}], ("keen_eye",)),
    sk("vein_reader", "prospector", 2, 2,
       [{"type": "vein_reveal_add", "value": 0.25}, {"type": "grant_flag", "flag": "see_vein_direction"}],
       ("rich_seams",)),
    sk("deep_pockets", "prospector", 2, 1, [{"type": "cargo_capacity_add", "value": 12}], ("rich_seams",)),
    sk("negotiator", "prospector", 2, 2, [{"type": "contract_reward_mul", "value": 1.15}], ("assayer",)),
    sk("clean_extraction", "prospector", 3, 2,
       [{"type": "yield_mul", "value": 1.12}, {"type": "volatile_stability_add", "value": 0.15}],
       ("vein_reader",)),
    sk("curator", "prospector", 3, 2,
       [{"type": "insight_gain_mul", "value": 1.2}, {"type": "protected_sample_slots", "value": 1}],
       ("negotiator",)),
    sk("market_sense", "prospector", 3, 2, [{"type": "sale_value_mul", "value": 1.1}], ("assayer",)),
    sk("prospector_mastery", "prospector", 4, 3,
       [{"type": "yield_mul", "value": 1.2}, {"type": "grant_flag", "flag": "auto_mark_valuables"}],
       ("clean_extraction", "deep_pockets")),
    # keystones (mutually exclusive)
    sk("keystone_lodefinder", "prospector", 5, 4,
       [{"type": "scanner_range_add", "value": 14}, {"type": "grant_flag", "flag": "scan_through_rock"},
        {"type": "yield_mul", "value": 0.94}],
       ("prospector_mastery",), keystone=True, group="prospector_keystone"),
    sk("keystone_highgrader", "prospector", 5, 4,
       [{"type": "sale_value_mul", "value": 1.25}, {"type": "yield_mul", "value": 1.15},
        {"type": "scanner_range_add", "value": -4}],
       ("prospector_mastery",), keystone=True, group="prospector_keystone"),

    # --- Engineer ---
    sk("tinkerer", "engineer", 0, 1, [{"type": "drill_efficiency_mul", "value": 0.95}]),
    sk("coolant_routing", "engineer", 1, 1, [{"type": "cooling_mul", "value": 1.12}], ("tinkerer",)),
    sk("field_repairs", "engineer", 1, 1, [{"type": "repair_cost_mul", "value": 0.85}], ("tinkerer",)),
    sk("torque_tuning", "engineer", 2, 2, [{"type": "drill_power_mul", "value": 1.12}],
       ("coolant_routing",)),
    sk("fuel_chemistry", "engineer", 2, 2,
       [{"type": "refuel_cost_mul", "value": 0.82}, {"type": "drill_efficiency_mul", "value": 0.94}],
       ("field_repairs",)),
    sk("heat_exchange", "engineer", 2, 2, [{"type": "heat_generation_mul", "value": 0.88}],
       ("coolant_routing",)),
    sk("refining_theory", "engineer", 3, 2, [{"type": "refine_yield_mul", "value": 1.18}],
       ("fuel_chemistry",)),
    sk("hardened_frame", "engineer", 3, 2,
       [{"type": "hull_max_add", "value": 24}, {"type": "impact_damage_mul", "value": 0.88}],
       ("field_repairs",)),
    sk("power_budgeting", "engineer", 3, 3, [{"type": "drill_power_mul", "value": 1.08},
                                             {"type": "cooling_mul", "value": 1.08}],
       ("torque_tuning", "heat_exchange")),
    sk("engineer_mastery", "engineer", 4, 3,
       [{"type": "drill_power_mul", "value": 1.15}, {"type": "drill_efficiency_mul", "value": 0.9}],
       ("power_budgeting",)),
    sk("keystone_overclock", "engineer", 5, 4,
       [{"type": "drill_power_mul", "value": 1.35}, {"type": "heat_generation_mul", "value": 1.3},
        {"type": "cooling_mul", "value": 0.92}],
       ("engineer_mastery",), keystone=True, group="engineer_keystone"),
    sk("keystone_thermal_symbiosis", "engineer", 5, 4,
       [{"type": "convert_heat_to_fuel", "threshold": 0.55, "rate": 0.7},
        {"type": "cooling_mul", "value": 1.2}, {"type": "drill_power_mul", "value": 0.95}],
       ("engineer_mastery",), keystone=True, group="engineer_keystone"),

    # --- Pathfinder ---
    sk("sure_footing", "pathfinder", 0, 1, [{"type": "impact_damage_mul", "value": 0.9}]),
    sk("thrust_control", "pathfinder", 1, 1, [{"type": "thrust_mul", "value": 1.08}], ("sure_footing",)),
    sk("light_touch", "pathfinder", 1, 1, [{"type": "hover_fuel_mul", "value": 0.92}], ("sure_footing",)),
    sk("hazard_sense", "pathfinder", 2, 2, [{"type": "hazard_damage_mul", "value": 0.85}],
       ("thrust_control",)),
    sk("long_haul", "pathfinder", 2, 2, [{"type": "fuel_capacity_add", "value": 45}], ("light_touch",)),
    sk("emergency_protocols", "pathfinder", 2, 2,
       [{"type": "rescue_fee_mul", "value": 0.75}, {"type": "wreck_recovery_add", "value": 0.2}],
       ("sure_footing",)),
    sk("shaft_runner", "pathfinder", 3, 2,
       [{"type": "thrust_mul", "value": 1.12}, {"type": "brake_mul", "value": 1.12}],
       ("hazard_sense",)),
    sk("ballast_master", "pathfinder", 3, 2, [{"type": "cargo_mass_mul", "value": 0.82}], ("long_haul",)),
    sk("deep_acclimation", "pathfinder", 3, 2, [{"type": "depth_heat_mul", "value": 0.85}],
       ("hazard_sense",)),
    sk("pathfinder_mastery", "pathfinder", 4, 3,
       [{"type": "thrust_mul", "value": 1.15}, {"type": "grant_flag", "flag": "free_unstuck"}],
       ("shaft_runner", "ballast_master")),
    sk("keystone_deep_diver", "pathfinder", 5, 4,
       [{"type": "depth_heat_mul", "value": 0.6}, {"type": "hazard_damage_mul", "value": 0.75},
        {"type": "thrust_mul", "value": 0.92}],
       ("pathfinder_mastery",), keystone=True, group="pathfinder_keystone"),
    sk("keystone_sprinter", "pathfinder", 5, 4,
       [{"type": "thrust_mul", "value": 1.4}, {"type": "hover_fuel_mul", "value": 0.75},
        {"type": "hull_max_add", "value": -25}],
       ("pathfinder_mastery",), keystone=True, group="pathfinder_keystone"),
]
write("skills.json", {"skills": skills})

# ---------------------------------------------------------------------------
# Facilities + recipes
# ---------------------------------------------------------------------------
def ftier(t, credits, components, effects=(), recipes=(), summary=None):
    d = dict(tier=t, cost_credits=credits, effects=list(effects))
    if components:
        d["cost_components"] = [{"material": f"material.{m}", "amount": a} for m, a in components]
    if recipes:
        d["unlocks_recipes"] = [f"recipe.{r}" for r in recipes]
    if summary:
        d["summary_key"] = summary
    return d


facilities = [
    dict(id="facility.garage", name_key="facility.garage.name",
         description_key="facility.garage.description",
         tiers=[ftier(0, 0, []), ftier(1, 420, [], [{"type": "repair_cost_mul", "value": 0.92}]),
                ftier(2, 1150, [("refined_plate", 3)],
                      [{"type": "repair_cost_mul", "value": 0.85}]),
                ftier(3, 2900, [("refined_plate", 6), ("circuit_lattice", 2)],
                      [{"type": "repair_cost_mul", "value": 0.78}])]),
    dict(id="facility.refinery", name_key="facility.refinery.name",
         description_key="facility.refinery.description",
         tiers=[ftier(0, 0, [], recipes=["refined_plate"]),
                ftier(1, 520, [], [{"type": "refine_yield_mul", "value": 1.05}],
                      ["circuit_lattice"]),
                ftier(2, 1400, [("refined_plate", 4)],
                      [{"type": "refine_yield_mul", "value": 1.12}],
                      ["thermal_core", "emberglass_ingot"]),
                ftier(3, 3300, [("refined_plate", 8), ("thermal_core", 2)],
                      [{"type": "refine_yield_mul", "value": 1.2}],
                      ["drive_cell", "lattice_press"])]),
    dict(id="facility.research_lab", name_key="facility.research_lab.name",
         description_key="facility.research_lab.description",
         tiers=[ftier(0, 0, []), ftier(1, 600, [], [{"type": "insight_gain_mul", "value": 1.1}]),
                ftier(2, 1600, [("circuit_lattice", 3)],
                      [{"type": "insight_gain_mul", "value": 1.2},
                       {"type": "protected_sample_slots", "value": 1}]),
                ftier(3, 3600, [("circuit_lattice", 6), ("drive_cell", 2)],
                      [{"type": "insight_gain_mul", "value": 1.3},
                       {"type": "protected_sample_slots", "value": 1}])]),
    dict(id="facility.contract_terminal", name_key="facility.contract_terminal.name",
         description_key="facility.contract_terminal.description",
         tiers=[ftier(0, 0, []), ftier(1, 380, [], [{"type": "contract_reward_mul", "value": 1.08}]),
                ftier(2, 1250, [("refined_plate", 3)],
                      [{"type": "contract_reward_mul", "value": 1.16}]),
                ftier(3, 3000, [("circuit_lattice", 4)],
                      [{"type": "contract_reward_mul", "value": 1.25}])]),
    dict(id="facility.recovery_bay", name_key="facility.recovery_bay.name",
         description_key="facility.recovery_bay.description",
         tiers=[ftier(0, 0, []),
                ftier(1, 450, [], [{"type": "rescue_fee_mul", "value": 0.85},
                                   {"type": "wreck_recovery_add", "value": 0.15}]),
                ftier(2, 1300, [("refined_plate", 3)],
                      [{"type": "rescue_fee_mul", "value": 0.7},
                       {"type": "wreck_recovery_add", "value": 0.25}]),
                ftier(3, 2800, [("thermal_core", 2), ("refined_plate", 5)],
                      [{"type": "rescue_fee_mul", "value": 0.55},
                       {"type": "wreck_recovery_add", "value": 0.35}])]),
    dict(id="facility.logistics_lift", name_key="facility.logistics_lift.name",
         description_key="facility.logistics_lift.description",
         tiers=[ftier(0, 0, []), ftier(1, 900, [], [{"type": "refuel_cost_mul", "value": 0.92}]),
                ftier(2, 2200, [("drive_cell", 1)], [{"type": "refuel_cost_mul", "value": 0.85}]),
                ftier(3, 4200, [("drive_cell", 3), ("circuit_lattice", 4)],
                      [{"type": "refuel_cost_mul", "value": 0.78},
                       {"type": "grant_flag", "flag": "transit_access"}])]),
]
write("facilities.json", {"facilities": facilities})

def rec(rid, facility, tier, inputs, outputs, credits=0, byproducts=(), unlock=None):
    d = dict(id=f"recipe.{rid}", name_key=f"recipe.{rid}.name",
             description_key=f"recipe.{rid}.description", facility=f"facility.{facility}",
             facility_tier=tier,
             inputs=[{"material": f"material.{m}", "amount": a} for m, a in inputs],
             outputs=[{"material": f"material.{m}", "amount": a} for m, a in outputs],
             credit_cost=credits)
    if byproducts:
        d["byproducts"] = [{"material": f"material.{m}", "amount": a} for m, a in byproducts]
    if unlock:
        d["unlock"] = f"research.{unlock}"
    return d


recipes = [
    rec("refined_plate", "refinery", 0, [("ferrite", 8), ("cuprite", 3)], [("refined_plate", 1)], 20,
        unlock="field_metallurgy"),
    rec("circuit_lattice", "refinery", 1, [("prismite", 5), ("cuprite", 6)], [("circuit_lattice", 1)], 45,
        byproducts=[("ferrite", 1)], unlock="crystal_optics"),
    rec("thermal_core", "refinery", 2, [("emberglass", 4), ("mycelith", 3), ("refined_plate", 1)],
        [("thermal_core", 1)], 90, unlock="thermal_recovery"),
    rec("drive_cell", "refinery", 3, [("cogsteel", 4), ("circuit_lattice", 2), ("aurelite", 3)],
        [("drive_cell", 1)], 180, unlock="alloy_forging"),
    rec("emberglass_ingot", "refinery", 2, [("emberglass", 6)], [("refined_plate", 2)], 30,
        byproducts=[("thermal_core", 0)], unlock="alloy_forging"),
    rec("lattice_press", "refinery", 3, [("riftalloy", 2), ("circuit_lattice", 2)],
        [("drive_cell", 1), ("circuit_lattice", 1)], 150, unlock="lattice_engineering"),
]
# drop zero-amount byproducts
for r in recipes:
    if "byproducts" in r:
        r["byproducts"] = [b for b in r["byproducts"] if b["amount"] > 0]
        if not r["byproducts"]:
            del r["byproducts"]
write("recipes.json", {"recipes": recipes})

# ---------------------------------------------------------------------------
# Factions + contracts
# ---------------------------------------------------------------------------
factions = [
    dict(id="faction.shelf_cooperative", name_key="faction.shelf_cooperative.name",
         description_key="faction.shelf_cooperative.description",
         tier_thresholds=[0, 40, 110, 240],
         tier_unlocks_modules=["module.stabiliser_gyros", "module.ballast_trim", "module.assay_uplink"]),
    dict(id="faction.deepline_survey", name_key="faction.deepline_survey.name",
         description_key="faction.deepline_survey.description",
         tier_thresholds=[0, 60, 150, 320],
         tier_unlocks_modules=["module.material_filter", "module.hazard_imager",
                               "module.landmark_triangulator"]),
    dict(id="faction.emberworks_guild", name_key="faction.emberworks_guild.name",
         description_key="faction.emberworks_guild.description",
         tier_thresholds=[0, 80, 190, 400],
         tier_unlocks_modules=["module.heat_sink", "module.cryo_manifold", "module.containment_field"]),
]
write("factions.json", {"factions": factions})


def ct(cid, kind, faction, rewards, **kw):
    d = dict(id=f"contract.{cid}", name_key=f"contract.{cid}.name",
             description_key=f"contract.{cid}.description", kind=kind)
    if faction:
        d["faction"] = f"faction.{faction}"
    d.update(rewards)
    d.update(kw)
    return d


DELIVER = [
    ("shelf_ferrite_run", "shelf_cooperative", ["ferrite"], 12, 26, 0,
     dict(reward_credits=90, reward_credits_per_unit=5, reward_reputation=6)),
    ("shelf_copper_order", "shelf_cooperative", ["cuprite"], 8, 18, 120,
     dict(reward_credits=140, reward_credits_per_unit=9, reward_reputation=8)),
    ("survey_prismite_batch", "deepline_survey", ["prismite"], 6, 14, 280,
     dict(reward_credits=240, reward_credits_per_unit=22, reward_insight=1, reward_reputation=10)),
    ("survey_silver_consignment", "deepline_survey", ["argentine"], 8, 16, 400,
     dict(reward_credits=320, reward_credits_per_unit=20, reward_reputation=10)),
    ("guild_mycelith_cultures", "deepline_survey", ["mycelith"], 6, 12, 680,
     dict(reward_credits=380, reward_credits_per_unit=28, reward_insight=1, reward_reputation=12)),
    ("guild_emberglass_order", "emberworks_guild", ["emberglass"], 5, 12, 1080,
     dict(reward_credits=560, reward_credits_per_unit=44, reward_reputation=14)),
    ("guild_gold_tithe", "emberworks_guild", ["aurelite"], 6, 14, 800,
     dict(reward_credits=520, reward_credits_per_unit=38, reward_reputation=12)),
    ("strata_cogsteel_demand", "emberworks_guild", ["cogsteel"], 5, 11, 1700,
     dict(reward_credits=880, reward_credits_per_unit=70, reward_insight=2, reward_reputation=16)),
    ("fracture_riftalloy_pact", "deepline_survey", ["riftalloy"], 4, 9, 2450,
     dict(reward_credits=1400, reward_credits_per_unit=120, reward_insight=3, reward_reputation=20)),
    ("mixed_metals_manifest", "shelf_cooperative", ["ferrite", "cuprite", "argentine"], 14, 30, 300,
     dict(reward_credits=300, reward_credits_per_unit=11, reward_reputation=10)),
    ("component_supply_plates", "shelf_cooperative", ["refined_plate"], 2, 5, 250,
     dict(reward_credits=340, reward_credits_per_unit=110, reward_reputation=12)),
    ("component_supply_cores", "emberworks_guild", ["thermal_core"], 1, 3, 1100,
     dict(reward_credits=700, reward_credits_per_unit=290, reward_reputation=16)),
]
contracts = []
for cid, fac, pool, amin, amax, depth, rewards in DELIVER:
    contracts.append(ct(cid, "deliver_materials", fac, rewards,
                        material_pool=[f"material.{m}" for m in pool],
                        amount_min=amin, amount_max=amax, min_depth_unlocked=depth))

DEPTH = [
    ("first_descent", None, 150, 200, dict(reward_credits=120, reward_insight=1), 0),
    ("faults_probe", "deepline_survey", 420, 520, dict(reward_credits=260, reward_reputation=8), 250),
    ("deeps_probe", "deepline_survey", 820, 950, dict(reward_credits=420, reward_insight=1,
                                                      reward_reputation=10), 650),
    ("mantle_probe", "emberworks_guild", 1300, 1500, dict(reward_credits=700, reward_insight=2,
                                                          reward_reputation=12), 1050),
    ("strata_probe", "emberworks_guild", 1900, 2200, dict(reward_credits=1100, reward_insight=2,
                                                          reward_reputation=14), 1650),
    ("fracture_probe", "deepline_survey", 2700, 3100, dict(reward_credits=1800, reward_insight=4,
                                                           reward_reputation=20), 2400),
]
for cid, fac, dmin, dmax, rewards, unlock in DEPTH:
    contracts.append(ct(cid, "reach_depth", fac, rewards, depth_min=dmin, depth_max=dmax,
                        min_depth_unlocked=unlock))

OTHER = [
    ("scan_fault_anomaly", "scan_anomaly", "deepline_survey", 300,
     dict(reward_credits=200, reward_insight=1, reward_reputation=8), dict(depth_min=300, depth_max=650)),
    ("scan_hollow_anomaly", "scan_anomaly", "deepline_survey", 700,
     dict(reward_credits=340, reward_insight=1, reward_reputation=10),
     dict(depth_min=700, depth_max=1050)),
    ("scan_forge_anomaly", "scan_anomaly", "deepline_survey", 1700,
     dict(reward_credits=760, reward_insight=2, reward_reputation=14),
     dict(depth_min=1700, depth_max=2400)),
    ("recover_shelf_cache", "recover_cache", "shelf_cooperative", 100,
     dict(reward_credits=180, reward_reputation=8), dict(depth_min=100, depth_max=250)),
    ("recover_faults_cache", "recover_cache", "shelf_cooperative", 300,
     dict(reward_credits=300, reward_reputation=10), dict(depth_min=300, depth_max=650)),
    ("recover_strata_cache", "recover_cache", "emberworks_guild", 1700,
     dict(reward_credits=900, reward_reputation=14), dict(depth_min=1700, depth_max=2400)),
    ("stable_fulmenite", "extract_volatile", "emberworks_guild", 1100,
     dict(reward_credits=820, reward_insight=1, reward_reputation=14),
     dict(material_pool=["material.fulmenite"], amount_min=3, amount_max=7)),
    ("stable_voidcell", "extract_volatile", "emberworks_guild", 2450,
     dict(reward_credits=1600, reward_insight=3, reward_reputation=18),
     dict(material_pool=["material.voidcell"], amount_min=2, amount_max=5)),
    ("restore_transit_faults", "activate_infrastructure", "deepline_survey", 300,
     dict(reward_credits=420, reward_insight=1, reward_reputation=12),
     dict(depth_min=280, depth_max=1050)),
    ("restore_transit_deep", "activate_infrastructure", "emberworks_guild", 1650,
     dict(reward_credits=1050, reward_insight=2, reward_reputation=16),
     dict(depth_min=1650, depth_max=2400)),
    ("locate_lost_shelf_crew", "rescue_prospector", "shelf_cooperative", 80,
     dict(reward_credits=220, reward_reputation=12), dict(depth_min=80, depth_max=250)),
    ("locate_lost_deeps_crew", "rescue_prospector", "shelf_cooperative", 680,
     dict(reward_credits=520, reward_reputation=14), dict(depth_min=680, depth_max=1050)),
    ("austere_run_shelf", "no_module_class", "shelf_cooperative", 200,
     dict(reward_credits=260, reward_insight=1, reward_reputation=12),
     dict(depth_min=250, depth_max=650)),
    ("austere_run_deep", "no_module_class", "emberworks_guild", 1050,
     dict(reward_credits=880, reward_insight=2, reward_reputation=16),
     dict(depth_min=1050, depth_max=1650)),
]
for cid, kind, fac, unlock, rewards, extra in OTHER:
    contracts.append(ct(cid, kind, fac, rewards, min_depth_unlocked=unlock, **extra))

AUTHORED = [
    ("story_first_haul", "deliver_materials", None, 0,
     dict(reward_credits=150, reward_insight=1),
     dict(material_pool=["material.ferrite"], amount_min=10, amount_max=10, authored=True,
          repeatable=False)),
    ("story_echo_recovery", "deliver_materials", "deepline_survey", 280,
     dict(reward_credits=400, reward_insight=2, reward_reputation=15),
     dict(material_pool=["material.echo_shard"], amount_min=1, amount_max=1, authored=True,
          repeatable=False)),
    ("story_relic_recovery", "deliver_materials", "deepline_survey", 1700,
     dict(reward_credits=1200, reward_insight=4, reward_reputation=25),
     dict(material_pool=["material.relic_core"], amount_min=1, amount_max=1, authored=True,
          repeatable=False, requires_research="research.ancient_mechanisms")),
    ("story_fracture_seed", "deliver_materials", "deepline_survey", 2450,
     dict(reward_credits=2400, reward_insight=6, reward_reputation=30),
     dict(material_pool=["material.fracture_seed"], amount_min=1, amount_max=1, authored=True,
          repeatable=False, requires_research="research.rift_analysis")),
    ("story_final_descent", "reach_depth", None, 2600,
     dict(reward_credits=4000, reward_insight=8),
     dict(depth_min=3600, depth_max=3600, authored=True, repeatable=False,
          requires_research="research.deepforge_protocol")),
]
for cid, kind, fac, unlock, rewards, extra in AUTHORED:
    contracts.append(ct(cid, kind, fac, rewards, min_depth_unlocked=unlock, **extra))

write("contracts.json", {"contracts": contracts})

# ---------------------------------------------------------------------------
# Economy constants
# ---------------------------------------------------------------------------
write("economy.json", {"economy": {
    "base_fuel_capacity": 100.0,
    "base_cargo_capacity": 40.0,
    "base_hull": 100.0,
    "base_heat_capacity": 100.0,
    "free_service_fuel_fraction": 0.35,
    "free_service_hull_fraction": 0.25,
    "base_mine_seconds": 0.62,
    "min_mine_seconds": 0.09,
    "max_mine_seconds": 8.0,
    "mine_hardness_exponent": 0.85,
    "base_drill_power": 1.0,
    "mine_fuel_base": 0.22,
    "mine_fuel_hardness_exponent": 0.60,
    "thrust_fuel_per_second": 2.5,
    "lateral_fuel_per_second": 0.5,
    "idle_fuel_per_second": 0.02,
    "gravity": 24.0,
    "max_fall_speed": 22.0,
    "impact_damage_threshold": 13.0,
    "impact_damage_per_unit": 2.4,
    "heat_decay_per_second": 7.0,
    "heat_damage_per_second": 6.0,
    "repair_credits_per_hull": 0.8,
    "refuel_credits_per_unit": 0.35,
    "rescue_fee_base": 55.0,
    "rescue_fee_depth_factor": 0.15,
    "rescue_fee_cap_fraction": 0.35,
    "rescue_cargo_loss_fraction": 0.55,
    "wreck_recovery_fraction": 0.4,
    "mine_progress_decay_delay": 2.5,
    "mine_progress_decay_rate": 0.35,
    "starting_credits": 60,
    "first_upgrade_target_cost": 240,
    "insight_per_discovery": 1.0,
    "sell_price_depth_bonus_per_100m": 0.015,
    "cargo_mass_accel_penalty": 0.22
}})
