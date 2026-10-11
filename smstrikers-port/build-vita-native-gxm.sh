#!/bin/zsh
set -euo pipefail

ROOT="${0:A:h}"
# Reuse one checkout-local build directory for all Vita GXM iterations.
# Do not create per-experiment build trees or full artifact copies.
BUILD_DIR="${ROOT}/build-vita-gxm-latest"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"

cd "${ROOT}"

# Keep the hardware-tested R3-A topology/submit flags explicit: cached CMake
# values from other Aurora candidate presets must never leak into a control.
# The Lean GLX change is opt-in until its hardware freeze is understood.
cmake -S . -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/usr/local/vitasdk/share/vita.toolchain.cmake \
  -DSTRIKERS_VITA=ON \
  -DSTRIKERS_AURORA=ON \
  -DSTRIKERS_VITA_NO_LOGS=OFF \
  -DSTRIKERS_VITA_PROFILER=OFF \
  -DSTRIKERS_VITA_PROFILE_AUTOSTART=OFF \
  -DSTRIKERS_VITA_FAST_PROFILER_BUILD=OFF \
  -DSTRIKERS_VITA_STATIC_GEOMETRY_MB=8 \
  -DSTRIKERS_VITA_GC_NATIVE_RES="${STRIKERS_GC_NATIVE_RES:-OFF}" \
  -DSTRIKERS_VITA_AUDIO_THREAD=ON \
  -DSTRIKERS_VITA_GX_THREAD="${STRIKERS_GX_THREAD:-ON}" \
  -DSTRIKERS_VITA_GXM_LEAN_RENDER="${STRIKERS_GXM_LEAN_RENDER:-OFF}" \
  -DSTRIKERS_VITA_GXM_DIRECT_STREAM_WRITE="${STRIKERS_GXM_DIRECT_STREAM_WRITE:-OFF}" \
  -DSTRIKERS_VITA_GXM_DIRECT_DRAW_SUBMIT="${STRIKERS_GXM_DIRECT_DRAW_SUBMIT:-ON}" \
  -DAURORA_VITA_DISTINCT_CPU_CORES=OFF \
  -DAURORA_VITA_GXM_IMMEDIATE_DRAW_VIEW=OFF \
  -DSTRIKERS_VITA_LTO_JOBS=auto \
  -DSTRIKERS_VITA_SHADER_PROFILE=SEALED \
  -DSTRIKERS_VITA_NATIVE_CMPR=OFF \
  -DSTRIKERS_VITA_NATIVE_GX_TEXTURES=ON \
  -DSTRIKERS_VITA_FORCE_60HZ=ON

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
