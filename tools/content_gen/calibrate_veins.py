#!/usr/bin/env python3
"""Calibrates ore vein parameters against the density targets in
vein_tuning.json by measuring the real generator output.

    python3 tools/content_gen/calibrate_veins.py <density-tool> [iterations]

The density tool prints lines of the form `DENSITY <material.id> <percent>`
measured inside each material's own depth band, averaged over several seeds.
"""
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
TUNING = os.path.join(HERE, "vein_tuning.json")
SEEDS = [4242, 777, 20260905]


def measure(tool):
    acc = {}
    for seed in SEEDS:
        out = subprocess.run([tool, str(seed)], cwd=ROOT, capture_output=True, text=True).stdout
        for line in out.splitlines():
            if line.startswith("DENSITY "):
                _, mid, pct = line.split()
                acc.setdefault(mid, []).append(float(pct))
    return {k: sum(v) / len(v) for k, v in acc.items()}


def main():
    tool = sys.argv[1]
    iterations = int(sys.argv[2]) if len(sys.argv) > 2 else 4
    for it in range(iterations):
        subprocess.run([sys.executable, os.path.join(HERE, "gen_content.py")], cwd=ROOT,
                       capture_output=True, check=True)
        subprocess.run(["cmake", "--build", "build/headless", "-j2"], cwd=ROOT,
                       capture_output=True, check=True)
        actual = measure(tool)
        data = json.load(open(TUNING, encoding="utf-8"))
        worst = 0.0
        for mid, cfg in data["materials"].items():
            target = cfg["target_density_pct"]
            got = actual.get(mid, 0.0)
            if got <= 0.0:
                cfg["rarity_weight_bp"] = min(9000, int(cfg["rarity_weight_bp"] * 2))
                worst = max(worst, 10.0)
                continue
            ratio = target / got
            worst = max(worst, abs(ratio - 1.0))
            new_rarity = cfg["rarity_weight_bp"] * ratio
            if new_rarity > 9000:
                # Saturated: shrink the vein cell instead (density scales 1/period^2).
                shrink = (new_rarity / 6000.0) ** 0.5
                cfg["field_period"] = max(5, int(round(cfg["field_period"] / shrink)))
                cfg["rarity_weight_bp"] = 6000
            elif new_rarity < 40:
                cfg["field_period"] = min(64, int(round(cfg["field_period"] * 1.4)))
                cfg["rarity_weight_bp"] = max(20, int(round(new_rarity * 1.96)))
            else:
                cfg["rarity_weight_bp"] = int(round(new_rarity))
        json.dump(data, open(TUNING, "w", encoding="utf-8"), indent=2)
        print(f"iteration {it}: worst relative error {worst:.2%}")
        if worst < 0.12:
            break
    subprocess.run([sys.executable, os.path.join(HERE, "gen_content.py")], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
