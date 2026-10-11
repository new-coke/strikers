#!/bin/zsh
set -euo pipefail

ROOT="${0:A:h}"
BUILD_DIR="${ROOT}/build-vita-gxm-latest"
VITADEBUGGER_ROOT="${VITADEBUGGER_ROOT:-${ROOT}/../.tmp/VitaDebugger}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"
STATIC_GEOMETRY_MB="${STATIC_GEOMETRY_MB:-0}"

cd "${ROOT}"

cmake -S . -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/usr/local/vitasdk/share/vita.toolchain.cmake \
  -DSTRIKERS_VITA=ON \
  -DSTRIKERS_AURORA=ON \
  -DSTRIKERS_VITA_NO_LOGS=ON \
  -DSTRIKERS_VITA_AUDIO_THREAD=ON \
  -DSTRIKERS_VITA_GX_THREAD=OFF \
  -DSTRIKERS_VITA_GXM_DIRECT_STREAM_WRITE=ON \
  -DSTRIKERS_VITA_GXM_DIRECT_DRAW_SUBMIT=ON \
  -DSTRIKERS_VITA_STATIC_GEOMETRY_MB="${STATIC_GEOMETRY_MB}" \
  -DSTRIKERS_VITA_SHADER_PROFILE=SEALED \
  -DSTRIKERS_VITA_NATIVE_CMPR=OFF \
  -DSTRIKERS_VITA_NATIVE_GX_TEXTURES=ON \
  -DSTRIKERS_VITA_FORCE_60HZ=ON \
  -DSTRIKERS_VITA_PROFILER=ON \
  -DSTRIKERS_VITA_PROFILE_AUTOSTART=ON \
  -DSTRIKERS_VITA_FAST_PROFILER_BUILD=OFF \
  -DSTRIKERS_VITADEBUGGER_ROOT="${VITADEBUGGER_ROOT}"

rm -f \
  "${BUILD_DIR}/strikers" \
  "${BUILD_DIR}/strikers.velf" \
  "${BUILD_DIR}/strikers_vita.self" \
  "${BUILD_DIR}/strikers_vita.vpk"

cmake --build "${BUILD_DIR}" --target strikers -j"${JOBS}"
cmake --build "${BUILD_DIR}" --target strikers-velf -j"${JOBS}"
cmake --build "${BUILD_DIR}" --target strikers_vita.self-self -j"${JOBS}"
cmake --build "${BUILD_DIR}" --target strikers_vita.vpk-vpk -j"${JOBS}"

printf '\nProfiling build complete. Trace path on Vita:\n  ux0:data/strikersVita/profile.vptrace\n\n'
printf 'Static geometry cache: %s MiB\n\n' "${STATIC_GEOMETRY_MB}"
shasum -a 256 \
  "${BUILD_DIR}/strikers_vita.self" \
  "${BUILD_DIR}/strikers_vita.vpk"
ls -lh \
  "${BUILD_DIR}/strikers_vita.self" \
  "${BUILD_DIR}/strikers_vita.vpk"
