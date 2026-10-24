#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="${1:-$ROOT/build-verify}"
if [[ "$(uname -s)" == Darwin ]]; then
    export DEVELOPER_DIR="${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}"
fi
cmake -S "$ROOT" -B "$BUILD" -DBUILD_TESTING=ON -DSONY2FUJI_ENABLE_OPENMP=OFF -DCMAKE_BUILD_TYPE=Debug -DSONY2FUJI_TEST_RAW="${RAWLAB_TEST_RAW:-}"
cmake --build "$BUILD" -j 6
ctest --test-dir "$BUILD" --output-on-failure
