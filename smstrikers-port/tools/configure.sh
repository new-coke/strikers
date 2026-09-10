#!/bin/sh
# Configure a build whose dependencies are fit to package, which local development does not need:
# tools/configure.sh <build-dir> [Debug|Release]

set -e

BUILD="${1:?usage: tools/configure.sh <build-dir> [Debug|Release]}"
TYPE="${2:-Release}"

# Overridable, so the script can be exercised on a machine without Ninja.
GENERATOR="${GENERATOR:-Ninja}"

PORT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$PORT"

# clang everywhere; clang-cl on Windows, which means the MSVC C++ ABI there, unless
# CMAKE_TOOLCHAIN_FILE is set, in which case it decides and the compiler arguments are not emitted
# at all.
if [ -z "${CMAKE_TOOLCHAIN_FILE:-}" ]; then
    case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) CC=clang-cl; CXX=clang-cl ;;
    *)                    CC=clang;    CXX=clang++  ;;
    esac
fi

# Hermetic dependencies, and this is the whole of why: Aurora asks find_package for libpng,
# Freetype, zstd, SQLite3 and abseil before it builds its own, and a CI runner has Homebrew, so it
# found them and linked them by absolute path.
set -- -S . -B "$BUILD" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$TYPE" \
    -DSTRIKERS_AURORA=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_DISABLE_FIND_PACKAGE_PNG=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_Freetype=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_zstd=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_SQLite3=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_absl=ON \
    -DAURORA_CACHE_USE_ZSTD=OFF \
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded

if [ -n "${CMAKE_TOOLCHAIN_FILE:-}" ]; then
    set -- "$@" -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE"
else
    set -- "$@" -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX"
fi

exec cmake "$@"
