#!/bin/zsh
set -euo pipefail

ROOT="${0:A:h}"
# Reuse the established shared Vita build directory for VitaGL.
# Do not create per-experiment build trees or full artifact copies.
BUILD_DIR="${ROOT}/build-vita-gxm-latest"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"

cd "${ROOT}"

# This compatibility entry point builds the VitaGL application in the shared cache.
cmake -S . -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/usr/local/vitasdk/share/vita.toolchain.cmake \
  -DSTRIKERS_AURORA=OFF \
  -DSTRIKERS_VITA_VITAGL=ON \
  -DSTRIKERS_VITA_NO_LOGS=OFF \
  -DSTRIKERS_VITA_PROFILER=OFF \
  -DSTRIKERS_VITA_PROFILE_AUTOSTART=OFF \
  -DSTRIKERS_VITA_AUDIO_THREAD=ON \
  -DSTRIKERS_VITA_LTO_JOBS=auto

# Force regeneration of the final Vita artifacts. Object files and static
# libraries stay cached, so only stale dependencies are rebuilt.
rm -f \
  "${BUILD_DIR}/strikers" \
  "${BUILD_DIR}/strikers.velf" \
  "${BUILD_DIR}/strikers_vita.self" \
  "${BUILD_DIR}/strikers_vita.vpk"

cmake --build "${BUILD_DIR}" --target strikers -j"${JOBS}"
cmake --build "${BUILD_DIR}" --target strikers-velf -j"${JOBS}"
cmake --build "${BUILD_DIR}" --target strikers_vita.self-self -j"${JOBS}"
cmake --build "${BUILD_DIR}" --target strikers_vita.vpk-vpk -j"${JOBS}"

echo
echo "Build complete:"
shasum -a 256 \
  "${BUILD_DIR}/strikers_vita.self" \
  "${BUILD_DIR}/strikers_vita.vpk"
ls -lh \
  "${BUILD_DIR}/strikers_vita.self" \
  "${BUILD_DIR}/strikers_vita.vpk"
