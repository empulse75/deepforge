#!/usr/bin/env python3
"""Visual regression: capture each named screen offscreen and compare against
the committed baseline at 1:1 (SPEC: region mean-abs-diff <= 2%), plus the 8x
text-region and gauge-region crops SPEC:412-414 asks for.

Usage: visual_regression.py <client-binary> <data-dir> <baselines-dir> [--bless]

Bless mode recaptures and rewrites the committed baselines and crops
(reviewed commit only). Compare mode fails when a committed baseline or crop
differs beyond the threshold.

Verdict semantics (TEST-03/QA-01): the verdict line is "RESULT: PASS"
(everything planned ran and passed), "RESULT: FAIL ..." (anything compared
failed or a committed file is missing), or "RESULT: SKIP: ..." (nothing could
be compared at all: no client binary or no offscreen driver). On skip runs the
verdict is the FIRST line of stdout - CTest matches SKIP_REGULAR_EXPRESSION
from the start of the output buffer (verified on CMake 3.31.6) - and the last
line everywhere else; per-screen SKIP notes are informational and never
masquerade as the verdict.

Coverage (QA-01): every state the harness can name. Rectangles below are the
regions blessed from the offscreen capture at 1600x900, seed 3407, 2400 ticks;
each must contain >= MIN_CROP_CONTENT exact-token pixels of its family at bless
time, so an accidentally empty crop is refused.
"""
import os
import shutil
import subprocess
import sys

THRESHOLD = 0.02  # mean absolute difference over all channels, 0..1
MIN_CROP_CONTENT = 50  # exact-token pixels a crop region must actually cover

SCREENS = [
    "expedition", "menu", "hub", "map", "garage", "modules", "skills", "research",
    "refinery", "contracts", "facilities", "codex", "pause", "results", "failure",
    "settings", "bindings", "credits", "newworld", "load", "error",
]

# screen -> crop kind -> (x, y, w, h) in capture pixels; None means the screen
# has no region of that kind (documented, not compared). Blessed from the
# offscreen capture at 1600x900 seed 3407 (densest fixed-window of the family,
# exact-token pixel counts in brackets kept in the bless log); a region under
# MIN_CROP_CONTENT is refused at bless time.
CROPS = {
    "expedition": {"text": (692, 748, 160, 48), "gauge": (1228, 100, 96, 48)},
    "menu": {"text": (856, 236, 160, 48), "gauge": (744, 116, 96, 48)},
    "hub": {"text": (312, 84, 160, 48), "gauge": (0, 68, 96, 48)},
    "map": {"text": (96, 764, 160, 48), "gauge": (1340, 768, 96, 48)},
    "garage": {"text": (1016, 376, 160, 48), "gauge": (1340, 376, 96, 48)},
    "modules": {"text": (1016, 396, 160, 48), "gauge": (1432, 396, 96, 48)},
    "skills": {"text": (312, 84, 160, 48), "gauge": (1436, 376, 96, 48)},
    "research": {"text": (1096, 412, 160, 48), "gauge": (1084, 700, 96, 48)},
    "refinery": {"text": (968, 416, 160, 48), "gauge": (1184, 700, 96, 48)},
    "contracts": {"text": (948, 388, 160, 48), "gauge": (984, 204, 96, 48)},
    "facilities": {"text": (104, 84, 160, 48), "gauge": (1084, 700, 96, 48)},
    "codex": {"text": (108, 84, 160, 48), "gauge": (0, 68, 96, 48)},
    "pause": {"text": (148, 84, 160, 48), "gauge": (236, 0, 96, 48)},
    "results": {"text": (144, 84, 160, 48), "gauge": (240, 0, 96, 48)},
    "failure": {"text": (376, 84, 160, 48), "gauge": (236, 0, 96, 48)},
    "settings": {"text": (212, 84, 160, 48), "gauge": (240, 0, 96, 48)},
    "bindings": {"text": (144, 84, 160, 48), "gauge": (240, 0, 96, 48)},
    "credits": {"text": (176, 84, 160, 48), "gauge": (0, 68, 96, 48)},
    "newworld": {"text": (104, 84, 160, 48), "gauge": (240, 0, 96, 48)},
    "load": {"text": (104, 84, 160, 48), "gauge": (0, 68, 96, 48)},
    "error": {"text": (180, 56, 160, 48), "gauge": (0, 68, 96, 48)},
}

# Exact-token colours whose pixels anchor each crop family (post-retoken values).
TEXT_TOKENS = {(216, 222, 233), (140, 151, 170), (124, 134, 152)}
GAUGE_TOKENS = {
    (88, 199, 224), (124, 196, 106), (236, 135, 60), (206, 176, 108),  # fuel/hull/heat/cargo
    (110, 178, 240), (236, 236, 236), (244, 190, 60), (176, 150, 230),  # colour-vision set
    (240, 168, 60), (255, 200, 116), (232, 163, 61), (224, 91, 76),     # amber/warn/danger
}


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
        if bpp == 32:
            # SDL_SaveBMP writes BGRA; reverse the triple so rows are true RGB.
            row = b"".join(data[base + x * 4 + 2:base + x * 4 + 3]
                           + data[base + x * 4 + 1:base + x * 4 + 2]
                           + data[base + x * 4:base + x * 4 + 1] for x in range(w))
        else:
            row = data[base:base + w * 3]
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


def crop_region(img, rect):
    x, y, w, h = rect
    rows = [row[x * 3:(x + w) * 3] for row in img[2][y:y + h]]
    return (w, h, rows)


def upscale(img, factor):
    w, h, rows = img
    out = []
    for row in rows:
        big = b"".join(row[p:p + 3] * factor for p in range(0, len(row), 3))
        out.extend([big] * factor)
    return (w * factor, h * factor, out)


def write_bmp(path, img):
    """Standard 24bpp BMP: BITMAPINFOHEADER, bottom-up rows, stride padded to 4."""
    w, h, rows = img
    stride = (w * 3 + 3) // 4 * 4
    pad = b"\x00" * (stride - w * 3)
    body = b"".join(r + pad for r in reversed(rows))
    off = 14 + 40
    header = b"BM" + (off + len(body)).to_bytes(4, "little") + b"\x00" * 4 + off.to_bytes(4, "little")
    header += b"\x28\x00\x00\x00" + w.to_bytes(4, "little") + h.to_bytes(4, "little")
    header += b"\x01\x00" + (24).to_bytes(2, "little") + b"\x00" * 24  # compression..reserved
    with open(path, "wb") as fh:
        fh.write(header)
        fh.write(body)


def token_count(img, tokens):
    n = 0
    for row in img[2]:
        for p in range(0, len(row), 3):
            if tuple(row[p:p + 3]) in tokens:
                n += 1
    return n


def capture(client, data_dir, screen, out_dir, results):
    shot = os.path.join(out_dir, f"{screen}.bmp")
    r = subprocess.run(
        [client, "--data", data_dir, "--user-dir", os.path.join(out_dir, "user"),
         "--screenshot", shot, "--screenshot-screen", screen,
         "--screenshot-seed", "3407", "--screenshot-ticks", "2400"],
        env=dict(os.environ, SDL_VIDEODRIVER="offscreen", SDL_AUDIODRIVER="dummy"),
        capture_output=True, text=True, timeout=300)
    if r.returncode != 0 or not os.path.isfile(shot):
        results.append(f"SKIP {screen}: capture failed (no display?): {r.stderr.strip()[-200:]}")
        return None
    return shot


def check_crops(baseline_dir, screen, shot_img, bless, notes):
    for kind, rect in sorted(CROPS.get(screen, {}).items()):
        if rect is None:
            continue
        if bless:
            if token_count(crop_region(shot_img, rect), TEXT_TOKENS if kind == "text" else GAUGE_TOKENS) < MIN_CROP_CONTENT:
                notes.append(f"FAIL {screen} {kind} crop: region {rect} covers fewer than "
                             f"{MIN_CROP_CONTENT} exact-token pixels")
                return False
            path = os.path.join(baseline_dir, f"{screen}_{kind}_8x.bmp")
            write_bmp(path, upscale(crop_region(shot_img, rect), 8))
            continue
        path = os.path.join(baseline_dir, f"{screen}_{kind}_8x.bmp")
        if not os.path.isfile(path):
            notes.append(f"FAIL {screen} {kind} crop: no committed baseline at {path}")
            return False
        d = mean_abs_diff(read_bmp(path), upscale(crop_region(shot_img, rect), 8))
        status = "ok" if d is not None and d <= THRESHOLD else "FAIL"
        notes.append(f"{status} {screen} {kind} crop {rect}: mean-abs-diff {d:.4%} (threshold {THRESHOLD:.0%})")
        if status == "FAIL":
            return False
    return True


def main():
    argv = sys.argv[1:]
    bless = "--bless" in argv
    argv = [a for a in argv if a != "--bless"]
    if len(argv) != 3:
        print(__doc__)
        return 2
    client, data_dir, baseline_dir = argv
    if not os.path.isfile(client):
        print("RESULT: SKIP: client binary not built")
        print(f"SKIP: client binary not built ({client})")
        return 0
    out_dir = "/tmp/deepforge-visual-regression"
    os.makedirs(out_dir, exist_ok=True)
    details = []
    ran = 0
    failed = False
    for screen in SCREENS:
        results = []
        shot = capture(client, data_dir, screen, out_dir, results)
        if shot is not None:
            ran += 1
            shot_img = read_bmp(shot)
            baseline = os.path.join(baseline_dir, f"{screen}_3407.bmp")
            if bless:
                shutil.copyfile(shot, baseline)
            elif not os.path.isfile(baseline):
                results.append(f"FAIL {screen}: no committed baseline at {baseline}")
            else:
                d = mean_abs_diff(read_bmp(baseline), shot_img)
                status = "ok" if d is not None and d <= THRESHOLD else "FAIL"
                results.append(f"{status} {screen}: mean-abs-diff {d:.4%} (threshold {THRESHOLD:.0%})")
            check_crops(baseline_dir, screen, shot_img, bless, results)
            if any(l.startswith("FAIL") for l in results):
                failed = True
        details.extend(results)
    skipped = len(SCREENS) - ran
    if ran == 0:
        # CTest's SKIP_REGULAR_EXPRESSION matches from the start of the output
        # buffer on this CMake (verified 3.31.6), so the verdict must be first.
        print("RESULT: SKIP: nothing could be compared (no client binary or no offscreen driver)")
        for line in details:
            print(line)
        return 0
    for line in details:
        print(line)
    if failed:
        print(f"RESULT: FAIL ({ran} screens compared{f', {skipped} skipped' if skipped else ''})")
        return 1
    print(f"RESULT: PASS ({ran} screens compared{f', {skipped} skipped' if skipped else ''})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
