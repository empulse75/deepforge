#!/usr/bin/env bash
#
# Coverage run (TEST-05). Advisory: nothing gates on these numbers yet; the
# gate is tools/verify.sh. It instruments the headless test binary with
# --coverage, runs the suite, and prints a per-layer gcovr summary.
#
#   tools/coverage.sh
#
# What the numbers mean: only code a test executes can be counted. For the
# two layers that sit behind the SDL gate (presentation, platform), the
# headless run measures the extracted pure seams — FocusModel, InputCore, and
# the Theme machinery — which is exactly what this card added tests for; the
# rest of those layers is exercised only by the visual baselines and the
# client smoke test, and those need a client-enabled build.
#
# Needs gcovr (apt install gcovr) and a gcov that matches the compiler.
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR="build/coverage"
LAYERS="core content store world domain app platform presentation"

echo "== configure and build: coverage"
cmake --preset coverage >/dev/null
cmake --build --preset coverage -j"$(nproc 2>/dev/null || echo 4)"

echo "== test: coverage"
ctest --preset coverage

if ! command -v gcovr >/dev/null; then
    echo "gcovr is not installed (apt install gcovr); the run above completed, the summary is skipped." >&2
    exit 0
fi

json_dir="$(mktemp -d)"
trap 'rm -rf "$json_dir"' EXIT
# The gcov working-directory noise is on third-party headers (Catch2, libstdc++);
# the project sources under src/ — what this summary reports — are processed
# cleanly.
gcovr "$BUILD_DIR" --root . --filter 'src/' --gcov-ignore-errors=no_working_dir_found \
    --json -o "$json_dir/coverage.json" >/dev/null

python3 - "$json_dir/coverage.json" "$LAYERS" <<'PY'
import json, sys

data = json.load(open(sys.argv[1]))
layers = sys.argv[2].split()
agg = {}
for f in data["files"]:
    parts = f["file"].replace("\\", "/").split("/")
    if "src" not in parts:
        continue
    layer = parts[parts.index("src") + 1]
    total = covered = 0
    for line in f.get("lines", []):
        total += 1
        if line.get("count", 0) > 0:
            covered += 1
    d = agg.setdefault(layer, [0, 0])
    d[0] += covered
    d[1] += total

print()
print("== coverage by layer (gcovr, headless instrumented run)")
print(f"{'layer':<14}{'covered':>9}{'lines':>9}{'%':>9}")
for layer in layers:
    if layer not in agg:
        print(f"{layer:<14}{'-':>9}{'-':>9}{'not instrumented':>9}")
        continue
    covered, total = agg[layer]
    pct = 100.0 * covered / total if total else 0.0
    print(f"{layer:<14}{covered:>9}{total:>9}{pct:>8.1f}%")
PY
