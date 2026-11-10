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
ABI="${ANDROID_ABI:-arm64-v8a}"
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

if [ ! -f "$BUILD/CMakeCache.txt" ]; then
    echo "==> configure"
    sh tools/configure-android.sh "$BUILD" "$TYPE"
fi

echo "==> compile"
# Expected to fail at the link on a clean tree: the stubs do not exist yet, and cannot, until every
# object is there to be read.
set +e
"$CMAKE" --build "$BUILD" --parallel >"$BUILD/compile.log" 2>&1
set -e
DIAG='\.(c|cc|cpp|h|hpp)[:(][0-9]+[:,][0-9]+\)?: (fatal )?error:'
if grep -qE "$DIAG" "$BUILD/compile.log"; then
    echo "==> COMPILE FAILED" >&2
    grep -E "$DIAG" "$BUILD/compile.log" | head -20 >&2
    exit 1
fi
OTHER=$(sed -n 's/^FAILED: \(\[code=[0-9]*\] \)\{0,1\}//p' "$BUILD/compile.log" \
        | tr -d '\r' \
        | sed 's/[[:space:]]*$//' \
        | grep -vx 'libmain\.so' || true)
if [ -n "$OTHER" ]; then
    echo "==> COMPILE FAILED: a build step failed with no compiler diagnostic" >&2
    echo "$OTHER" | sed 's/^/      /' | head -10 >&2
    grep -A4 '^FAILED: ' "$BUILD/compile.log" | head -40 >&2
    exit 1
fi

echo "==> generate stubs"
# In place, in src/platform/, exactly as tools/rebuild.sh does on every other platform: the file is
# generated output that happens to be checked in, and it is per-object-format -- the copy in the
# tree carries Mach-O's leading underscore on every symbol and satisfies nothing on ELF.
STRIKERS_BUILD_DIR="$BUILD" "$PYTHON" tools/genstubs.py \
    --noop '^GX|^snd|^AI|^AR'

echo "==> link"
"$CMAKE" --build "$BUILD" --target strikers --parallel

LIB="$BUILD/libmain.so"
if [ ! -f "$LIB" ]; then
    echo "==> FAILED: no libmain.so produced" >&2
    exit 1
fi

echo "==> stage"
STAGE="$BUILD/jniLibs/$ABI"
mkdir -p "$STAGE"
cp "$LIB" "$STAGE/"
# libc++_shared.so has to travel with it. A plain CMake build does not copy it the way the Android
# Gradle plugin's own CMake integration would, and without it the library does not load at all.
CXXLIB="$NDK/toolchains/llvm/prebuilt/$NDK_HOST/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"
if [ ! -f "$CXXLIB" ]; then
    echo "No libc++_shared.so at $CXXLIB" >&2
    exit 1
fi
cp "$CXXLIB" "$STAGE/"
# The Vulkan stand-in, which is what Dawn's dlopen("libvulkan.so") is meant to find; see
# src/platform/vulkan_shim.c.
if [ -f "$BUILD/libvulkan.so" ]; then
    cp "$BUILD/libvulkan.so" "$STAGE/"
fi
# The adrenotools hooks. Nothing links against these: adrenotools loads them by name out of the
# app's library directory, which is also why the APK has to extract its libraries rather than map
# them from inside itself.
for _hook in libmain_hook.so libfile_redirect_hook.so libgsl_alloc_hook.so libhook_impl.so; do
    _found=$(find "$BUILD/_deps" -name "$_hook" -print -quit 2>/dev/null || true)
    if [ -n "$_found" ]; then
        cp "$_found" "$STAGE/"
    else
        echo "warning: $_hook nao foi produzido; o driver escolhido pelo jogador nao vai carregar" >&2
    fi
done
# SDL is linked into libmain.so, so there is no libSDL3.so to stage; see tools/configure-android.sh.

echo "==> ok: $STAGE"
ls -l "$STAGE"
echo
echo "Next: cd android && ./gradlew assembleDebug"
