#!/usr/bin/env bash
# Source this to get the Emscripten SDK, CMake and Ninja on PATH:  source scripts/env.sh
: "${EMSDK:=$HOME/emsdk}"
if [ -f "$EMSDK/emsdk_env.sh" ]; then
  source "$EMSDK/emsdk_env.sh" > /dev/null 2>&1
else
  echo "Emscripten SDK not found at $EMSDK (set EMSDK to its folder)" >&2
fi
# CMake and Ninja installed with "pip install cmake ninja" live in Python's Scripts folder
if ! command -v cmake > /dev/null 2>&1; then
  PY_SCRIPTS="$(python -c 'import sysconfig; print(sysconfig.get_path("scripts"))' 2> /dev/null)"
  [ -n "$PY_SCRIPTS" ] && export PATH="$(cygpath -u "$PY_SCRIPTS" 2> /dev/null || echo "$PY_SCRIPTS"):$PATH"
fi
