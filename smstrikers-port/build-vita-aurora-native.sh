#!/usr/bin/env bash
set -euo pipefail
PORT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PORT_DIR/build-vita-gxm-latest"
VITASDK="${VITASDK:-/usr/local/vitasdk}"
NATIVE_CANDIDATE="${NATIVE_CANDIDATE:-ON}"
JOBS="${JOBS:-8}"
export VITASDK
cmake -S "$PORT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
  -DSTRIKERS_VERSION=1.3.0-aurora-native-20261002 \
  -DSTRIKERS_AURORA=ON -DSTRIKERS_FFMPEG=OFF \
  -DSTRIKERS_VITA_NO_LOGS=OFF -DSTRIKERS_VITA_AUDIO_THREAD=ON \
  -DSTRIKERS_VITA_PROFILER=OFF -DSTRIKERS_VITA_PROFILE_AUTOSTART=OFF \
  -DSTRIKERS_VITA_FAST_PROFILER_BUILD=OFF -DSTRIKERS_VITA_STATIC_GEOMETRY_MB=8 \
  -DSTRIKERS_VITA_GX_THREAD=OFF \
  -DSTRIKERS_VITA_GXM_DIRECT_STREAM_WRITE=ON \
  -DSTRIKERS_VITA_GXM_DIRECT_DRAW_SUBMIT=ON \
  -DAURORA_VITA_DISTINCT_CPU_CORES="$NATIVE_CANDIDATE" \
  -DAURORA_VITA_GXM_IMMEDIATE_DRAW_VIEW="$NATIVE_CANDIDATE" \
  -DSTRIKERS_VITA_SHADER_PROFILE=SEALED \
  -DSTRIKERS_VITA_NATIVE_CMPR=OFF -DSTRIKERS_VITA_NATIVE_GX_TEXTURES=ON \
  -DSTRIKERS_VITA_FORCE_60HZ=ON -DSTRIKERS_VITA_GC_NATIVE_RES=OFF
cmake --build "$BUILD_DIR" --target strikers_vita.vpk-vpk --parallel "$JOBS"
python3 "$PORT_DIR/extern/aurora-vita/tools/check_vita_gxm_binary.py" \
  "$BUILD_DIR/strikers" --map "$BUILD_DIR/strikers.map" \
  --nm "$VITASDK/bin/arm-vita-eabi-nm"
python3 "$PORT_DIR/extern/aurora-vita/tools/vita_build_manifest.py" \
  --repo "$PORT_DIR/.." --build "$BUILD_DIR" \
  --elf "$BUILD_DIR/strikers" --self "$BUILD_DIR/strikers_vita.self" \
  --vpk "$BUILD_DIR/strikers_vita.vpk" --output "$BUILD_DIR/vita-native-manifest.json"
