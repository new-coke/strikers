#!/bin/sh
# Configure an Android build: tools/configure-android.sh <build-dir> [Debug|Release]
#
# The port itself is architecture-neutral, so what this script is really doing is naming the three
# things the Android build cannot infer:
#
#   1. The NDK. Aurora's prebuilt Dawn for android-aarch64 is built with NDK 29.0.14206865 against
#      API 28, and a static libwebgpu_dawn.a has to meet the same libc++ it was compiled with.
#   2. c++_shared. SDL3, Dawn and the game are three separate shared objects in one process; each
#      linking its own copy of libc++ statically gives three copies of every type_info and of the
#      allocator's state.
#   3. vendor SDL3. Aurora needs SDL's Java sources on disk to hand to Gradle, and only the vendored
#      (from-source) provider has them.
#
# Static SDL, which is the default here and is not negotiable on Android: Aurora's window.cpp calls
# Android_LockActivityMutex, an SDL-internal function that libSDL3.so does not export, so a shared
# SDL does not link at all. Everything of SDL's that Java has to reach -- the JNI entry points
# SDLActivity's native methods bind to -- is JNIEXPORT, so it lands in libmain.so's dynamic symbol
# table and survives --gc-sections there.

set -e

BUILD="${1:?usage: tools/configure-android.sh <build-dir> [Debug|Release]}"
TYPE="${2:-Release}"
# Anything past the build directory and the build type is passed on to CMake untouched.
shift
if [ $# -gt 0 ]; then shift; fi

ANDROID_SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$LOCALAPPDATA/Android/Sdk}}"
NDK_VERSION="${ANDROID_NDK_VERSION:-29.0.14206865}"
NDK="${ANDROID_NDK_HOME:-$ANDROID_SDK/ndk/$NDK_VERSION}"
API="${ANDROID_API:-28}"

if [ ! -f "$NDK/build/cmake/android.toolchain.cmake" ]; then
    echo "No NDK at $NDK." >&2
    echo "Install it with: sdkmanager \"ndk;$NDK_VERSION\"" >&2
    exit 1
fi

GENERATOR="${GENERATOR:-Ninja}"
CMAKE="${CMAKE:-cmake}"

PORT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$PORT"

# Use Aurora’s bundled dependencies. Disable pkg-config so it cannot select host libraries.
exec "$CMAKE" -S . -B "$BUILD" -G "$GENERATOR" \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM="android-$API" \
    -DANDROID_STL=c++_shared \
    -DCMAKE_BUILD_TYPE="$TYPE" \
    -DSTRIKERS_VERSION="${STRIKERS_VERSION:-2.0.0}" \
    -DSTRIKERS_AURORA=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DAURORA_SDL3_PROVIDER=vendor \
    -DAURORA_SDL3_LINKAGE=static \
    -DAURORA_DAWN_PROVIDER=package \
    -DCMAKE_DISABLE_FIND_PACKAGE_PNG=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_Freetype=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_zstd=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_SQLite3=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_absl=ON \
    -DAURORA_CACHE_USE_ZSTD=OFF \
    "$@"
