#!/bin/sh
# Build the Android native side, end to end: tools/build-android.sh [build-dir] [Debug|Release]
#
# The same three steps as tools/rebuild.sh, for the same reason -- the stub set cannot be known
# until every translation unit has compiled -- plus a fourth: staging the shared objects where the
# Gradle project in android/ expects to find them.

set -e

BUILD="${1:-build-android}"
TYPE="${2:-Release}"

PORT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$PORT"

ANDROID_SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$LOCALAPPDATA/Android/Sdk}}"
NDK_VERSION="${ANDROID_NDK_VERSION:-29.0.14206865}"
NDK="${ANDROID_NDK_HOME:-$ANDROID_SDK/ndk/$NDK_VERSION}"
CMAKE="${CMAKE:-cmake}"
PYTHON="${PYTHON:-python3}"

# The NDK's own LLVM, because genstubs.py needs an nm that reads aarch64 ELF and the host's may not.
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) NDK_HOST=windows-x86_64 ;;
Darwin)               NDK_HOST=darwin-x86_64 ;;
*)                    NDK_HOST=linux-x86_64 ;;
esac
NDK_BIN="$NDK/toolchains/llvm/prebuilt/$NDK_HOST/bin"
if [ ! -d "$NDK_BIN" ]; then
    echo "No NDK toolchain at $NDK_BIN" >&2
    exit 1
fi
PATH="$NDK_BIN:$PATH"
export PATH

# Clear the staged payload before rebuilding so failures cannot package stale libraries.
rm -f "$BUILD"/jniLibs/arm64-v8a/*.so

echo "==> configure"
sh tools/configure-android.sh "$BUILD" "$TYPE"

echo "==> compile"
"$CMAKE" --build "$BUILD" --target strikers_android_scan

echo "==> generate stubs"
STRIKERS_SYMBOL_PREFIX= STRIKERS_BUILD_DIR="$BUILD" "$PYTHON" tools/genstubs.py \
    --noop '^GX|^snd|^AI|^AR' --out "$BUILD/stubs_generated.c"

echo "==> link and stage"
"$CMAKE" --build "$BUILD" --target strikers_android_stage

# Java resolves these entry points by name at runtime.
LIB="$BUILD/jniLibs/arm64-v8a/libmain.so"
SYMBOLS=$(llvm-nm --dynamic --defined-only --format=posix "$LIB" | cut -d ' ' -f 1)
for SYMBOL in SDL_main \
    Java_io_github_newcoke_strikers_TouchGamepad_sendNative \
    Java_io_github_newcoke_strikers_TouchGamepad_nativeIsActive \
    Java_io_github_newcoke_strikers_FrameMetrics_readNative; do
    if ! printf '%s\n' "$SYMBOLS" | grep -Fx "$SYMBOL" >/dev/null; then
        echo "Missing Android entry point: $SYMBOL" >&2
        exit 1
    fi
done
