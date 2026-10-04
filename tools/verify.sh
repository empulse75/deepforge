#!/usr/bin/env bash
#
# The full verification gate. This is what "it works" means for this project,
# and it is what CI runs; run it before every commit.
#
# It configures each preset it needs, so it works from a fresh clone with no
# prior build directories. The first run fetches and builds SDL3 and so takes
# several minutes; later runs reuse the CMake cache and are fast.
#
#   tools/verify.sh            # everything
#   tools/verify.sh --quick    # headless build + tests only, no sanitizers
#
set -euo pipefail
cd "$(dirname "$0")/.."

QUICK=0
[[ "${1:-}" == "--quick" ]] && QUICK=1

# dev-release builds the client; headless/asan/ubsan are client-free, so they
# need no display, no GPU and no SDL at all.
if [[ $QUICK -eq 1 ]]; then
    BUILD_PRESETS=(headless)
    TEST_PRESETS=(headless)
else
    BUILD_PRESETS=(dev-release ci headless asan ubsan tsan)
    TEST_PRESETS=(headless asan ubsan tsan)
fi

for preset in "${BUILD_PRESETS[@]}"; do
    echo "== configure and build: $preset"
    cmake --preset "$preset" >/dev/null
    cmake --build --preset "$preset" -j"$(nproc 2>/dev/null || echo 4)"
done

for preset in "${TEST_PRESETS[@]}"; do
    echo "== test: $preset"
    ctest --preset "$preset"
done

# The pacing gate is a release blocker (SPEC slice 1): every seed must reach a
# first upgrade within twelve runs. The tool exits non-zero otherwise.
echo "== pacing gate (release blocker)"
./build/headless/tools/deepforge-balance --data data --seeds 16 --pacing

# The smoke test guards SDL start-up and, more importantly, teardown ordering:
# it has caught a segfault at exit that no unit test could reach.
if [[ $QUICK -eq 0 ]]; then
    echo "== headless client smoke test"
    smoke_dir="$(mktemp -d)"
    trap 'rm -rf "$smoke_dir"' EXIT
    SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
        ./build/dev-release/src/deepforge \
        --data data --user-dir "$smoke_dir" --headless-frames 300
fi

echo "== all green"
