#!/usr/bin/env bash
# Full verification pass: build every preset that guards something different,
# run the suite under each sanitizer, and smoke-test the client headless.
#
# This is what "it works" means for this project; run it before every commit.
set -euo pipefail
cd "$(dirname "$0")/.."

for preset in dev ci headless asan ubsan; do
    echo "== build $preset"
    cmake --build "build/$preset" -j8
done

for preset in headless asan ubsan; do
    echo "== test $preset"
    ctest --test-dir "build/$preset" --output-on-failure
done

echo "== headless client smoke test"
rm -rf /tmp/deepforge-smoke
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
    ./build/dev/src/deepforge --data data --user-dir /tmp/deepforge-smoke --headless-frames 300

echo "== all green"
