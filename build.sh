#!/usr/bin/env bash
set -euo pipefail

export VITASDK="${VITASDK:-/usr/local/vitasdk}"

build_dir="${BUILD_DIR:-build}"
build_type="${CMAKE_BUILD_TYPE:-Release}"

cmake \
  -S . \
  -B "${build_dir}" \
  -DCMAKE_BUILD_TYPE="${build_type}" \
  "$@"
cmake --build "${build_dir}" --parallel
