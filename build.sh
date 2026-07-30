#!/usr/bin/env bash
set -euo pipefail

export VITASDK="${VITASDK:-/usr/local/vitasdk}"

build_dir="${BUILD_DIR:-build}"
build_type="${CMAKE_BUILD_TYPE:-Release}"
package_apk="${VITAARC_PACKAGE_APK:-ON}"

cmake \
  -S . \
  -B "${build_dir}" \
  -DCMAKE_BUILD_TYPE="${build_type}" \
  -DVITAARC_PACKAGE_APK="${package_apk}" \
  "$@"
cmake --build "${build_dir}" --parallel
