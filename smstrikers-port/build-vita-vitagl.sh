#!/bin/zsh
set -euo pipefail

ROOT="${0:A:h}"
# Reuse the established shared Vita build directory for VitaGL.
# Do not create per-experiment build trees or full artifact copies.
BUILD_DIR="${ROOT}/build-vita-gxm-latest"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"
MODE="${1:-normal}"
PROFILE_ARGS=(-USTRIKERS_VERSION -DSTRIKERS_VITA_NO_LOGS=OFF)
if [[ "${MODE}" == "--release-candidate" ]]; then
  # Rebind the shared cache to this checkout and remove stale experiment flags.
  PROFILE_ARGS=(--fresh -DSTRIKERS_VITA_NO_LOGS=ON
    -DSTRIKERS_VERSION="${STRIKERS_RELEASE_VERSION:-1.3.0-rc.20261022.1}")
elif [[ "${MODE}" != "normal" || "$#" -gt 1 ]]; then
  echo "Usage: $0 [--release-candidate]" >&2
  exit 2
fi

cd "${ROOT}"

# This compatibility entry point builds the VitaGL application in the shared cache.
cmake -S . -B "${BUILD_DIR}" \
  "${PROFILE_ARGS[@]}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/usr/local/vitasdk/share/vita.toolchain.cmake \
  -DSTRIKERS_AURORA=OFF \
  -DSTRIKERS_VITA_VITAGL=ON \
  -DSTRIKERS_VITA_AVC_CONTINUITY_PROBE=OFF \
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

if [[ "${MODE}" == "--release-candidate" ]]; then
  cp "${ROOT}/strikers-release.ini" "${BUILD_DIR}/strikers.ini"
  python3 "${ROOT}/tools/write_vita_release_manifest.py" --build "${BUILD_DIR}"
fi

echo
echo "Build complete:"
shasum -a 256 \
  "${BUILD_DIR}/strikers_vita.self" \
  "${BUILD_DIR}/strikers_vita.vpk"
ls -lh \
  "${BUILD_DIR}/strikers_vita.self" \
  "${BUILD_DIR}/strikers_vita.vpk"
