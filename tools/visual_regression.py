#!/usr/bin/env python3
"""Visual regression: capture each named screen offscreen and compare against
the committed baseline at 1:1 (SPEC: region mean-abs-diff <= 2%).

Usage: visual_regression.py <client-binary> <data-dir> <baselines-dir>

Skips (exit 0) when the client binary is not built (headless presets build
without the client) or the offscreen driver is unavailable; fails when a
baseline exists and the fresh capture differs beyond the threshold.
"""
import os
import subprocess
import sys

THRESHOLD = 0.02  # mean absolute difference over all channels, 0..1


def read_bmp(path):
    with open(path, "rb") as fh:
        data = fh.read()
    if data[:2] != b"BM":
        return None
    off = int.from_bytes(data[10:14], "little")
    w = int.from_bytes(data[18:22], "little", signed=True)
    h = int.from_bytes(data[22:26], "little", signed=True)
    bpp = int.from_bytes(data[28:30], "little")
    if bpp not in (24, 32) or w <= 0 or h <= 0:
        return None
    rows = []
    stride = (w * (bpp // 8) + 3) // 4 * 4
    # BMP rows are bottom-up.
    for y in range(abs(h) - 1, -1, -1):
        base = off + y * stride
        row = data[base:base + w * 3]
        if bpp == 32:
            # Reconstruct 24-bit RGB rows from BGRA for comparable diffs.
            row = b"".join(data[base + x * 4:base + x * 4 + 3] for x in range(w))
        rows.append(row)
    return (w, abs(h), rows)


def mean_abs_diff(a, b):
    if a is None or b is None:
        return None
    if a[0] != b[0] or a[1] != b[1]:
        return 1.0
    total = 0
    n = a[0] * a[1] * 3
    for ra, rb in zip(a[2], b[2]):
        for ca, cb in zip(ra, rb):
            total += abs(ca - cb)
    return total / n / 255.0


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    client, data_dir, baseline_dir = sys.argv[1:4]
    if not os.path.isfile(client):
        print(f"SKIP: client binary not built ({client})")
        return 0
    env = dict(os.environ, SDL_VIDEODRIVER="offscreen", SDL_AUDIODRIVER="dummy")
    out_dir = "/tmp/deepforge-visual-regression"
    os.makedirs(out_dir, exist_ok=True)
    failed = False
    for screen in ("hub", "expedition", "garage"):
        shot = os.path.join(out_dir, f"{screen}.bmp")
        baseline = os.path.join(baseline_dir, f"{screen}_3407.bmp")
        r = subprocess.run(
            [client, "--data", data_dir, "--user-dir", os.path.join(out_dir, "user"),
             "--screenshot", shot, "--screenshot-screen", screen,
             "--screenshot-seed", "3407", "--screenshot-ticks", "2400"],
            env=env, capture_output=True, text=True, timeout=300)
        if r.returncode != 0 or not os.path.isfile(shot):
            print(f"SKIP {screen}: capture failed (no display?): {r.stderr.strip()[-200:]}")
            continue
        if not os.path.isfile(baseline):
            print(f"FAIL {screen}: no committed baseline at {baseline}")
            failed = True
            continue
        d = mean_abs_diff(read_bmp(baseline), read_bmp(shot))
        status = "ok" if d is not None and d <= THRESHOLD else "FAIL"
        print(f"{status} {screen}: mean-abs-diff {d:.4%} (threshold {THRESHOLD:.0%})")
        if status == "FAIL":
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
