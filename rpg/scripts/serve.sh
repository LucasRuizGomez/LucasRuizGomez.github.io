#!/usr/bin/env bash
# Serve a web build on http://127.0.0.1:8377   usage: scripts/serve.sh [web-debug|web-release] [port]
# (Browsers refuse to load WebAssembly from file://, so the game needs a local server.)
set -euo pipefail
cd "$(dirname "$0")/.."
preset="${1:-web-debug}"
port="${2:-8377}"
dist="build/$preset/dist"
[ -f "$dist/index.html" ] || { echo "No build in $dist. Run scripts/build.sh $preset first." >&2; exit 1; }
echo "Hollow Crypt ($preset): http://127.0.0.1:$port/"
exec python -m http.server "$port" --bind 127.0.0.1 --directory "$dist"
