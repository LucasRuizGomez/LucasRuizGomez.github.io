#!/usr/bin/env bash
# Configure, build and test one preset.   usage: scripts/build.sh [web-debug|web-release|native-debug|native-release]
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/env.sh
preset="${1:-web-debug}"
cmake --preset "$preset"
cmake --build --preset "$preset"
case "$preset" in
  native-release) ;;   # no test preset: the tests run in the other three
  *) ctest --preset "$preset" ;;
esac
case "$preset" in
  web-*) echo "Built build/$preset/dist  (play it with: scripts/serve.sh $preset)" ;;
esac
