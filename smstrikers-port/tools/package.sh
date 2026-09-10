#!/bin/sh
# Build an archive from a completed build directory, without the disc data: tools/package.sh
# <build-dir> <archive-name> [zip|tar.gz]

set -e

BUILD="${1:?usage: tools/package.sh <build-dir> <archive-name> [zip|tar.gz]}"
NAME="${2:?usage: tools/package.sh <build-dir> <archive-name> [zip|tar.gz]}"

# OUT is removed recursively below, so the caller-controlled component stays a basename.
case "$NAME" in
    .|..|-*|*[!A-Za-z0-9._-]*)
        echo "package.sh: archive-name must contain only letters, digits, '.', '_' and '-'" >&2
        exit 1 ;;
esac

PORT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$PORT"

# Windows by what the build produced, as tools/rebuild.sh also decides it.
if [ -f "$BUILD/strikers.exe" ]; then
    BIN="$BUILD/strikers.exe"
    WINDOWS=1
    FORMAT="${3:-zip}"
elif [ -f "$BUILD/strikers" ]; then
    BIN="$BUILD/strikers"
    WINDOWS=0
    FORMAT="${3:-tar.gz}"
else
    echo "package.sh: no executable in $BUILD; was it linked?" >&2
    exit 1
fi

case "$FORMAT" in
    zip|tar.gz) ;;
    *)
        echo "package.sh: format must be 'zip' or 'tar.gz' (got '$FORMAT')" >&2
        exit 1 ;;
esac

OUT="dist/$NAME"
rm -rf "$OUT"
mkdir -p "$OUT"
cp "$BIN" "$OUT/"

if [ "$WINDOWS" = "1" ]; then
    # Dawn and SDL3 are prebuilt DLLs on Windows and travel with the executable.
    find "$BUILD" -name '*.dll' -exec cp {} "$OUT/" \; 2>/dev/null || true

    # Those DLLs use the dynamic C++ runtime even though the game is built /MT, and Windows does not
    # ship it.
    if ! command -v llvm-readobj >/dev/null 2>&1; then
        echo "package.sh: llvm-readobj is required to identify the Windows architecture" >&2
        exit 1
    fi

    # but only the ones something in the archive actually imports.
    NEEDED=$(
        for _image in "$BIN" "$OUT"/*.[Dd][Ll][Ll]; do
            [ -f "$_image" ] || continue
            llvm-readobj --coff-imports "$_image" 2>/dev/null |
                sed -n 's/^ *Name: *//p'
        done | tr 'A-Z' 'a-z' |
            grep -cE '^(vcruntime|msvcp|msvcr|concrt|mfc|vcomp)' || true
    )
    if [ "${NEEDED:-0}" -eq 0 ]; then
        echo "==> no app-deployed Visual C++ runtime is imported; none bundled"
        NO_CRT_NEEDED=1
    fi

    MACHINE=$(llvm-readobj --file-headers "$BIN" 2>/dev/null || true)
    case "$MACHINE" in
        *IMAGE_FILE_MACHINE_AMD64*) CRT_ARCH=x64 ;;
        *IMAGE_FILE_MACHINE_ARM64*) CRT_ARCH=arm64 ;;
        *IMAGE_FILE_MACHINE_I386*)  CRT_ARCH=x86 ;;
        *)
            echo "package.sh: could not identify the Windows architecture of $BIN" >&2
            exit 1 ;;
    esac

    if [ -z "${NO_CRT_NEEDED:-}" ]; then
        CRT_HINT="${VCToolsRedistDir:-}"
        if [ -n "$CRT_HINT" ] && command -v cygpath >/dev/null 2>&1; then
            CRT_HINT=$(cygpath -u "$CRT_HINT")
        fi
        CRT_DIR=
        for CRT_ROOT in \
            "$CRT_HINT" \
            "/c/Program Files/Microsoft Visual Studio" \
            "/c/Program Files (x86)/Microsoft Visual Studio"
        do
            [ -n "$CRT_ROOT" ] && [ -d "$CRT_ROOT" ] || continue
            CRT_DIR=$(find "$CRT_ROOT" -type d \
                -path "*/$CRT_ARCH/Microsoft.VC*.CRT" -print 2>/dev/null |
                sort | tail -n 1)
            [ -n "$CRT_DIR" ] && break
        done
        if [ -z "$CRT_DIR" ]; then
            echo "package.sh: Visual C++ redistributable not found for $CRT_ARCH" >&2
            exit 1
        fi
        CRT_COUNT=$(find "$CRT_DIR" -maxdepth 1 -type f -iname '*.dll' -print |
            wc -l | tr -d '[:space:]')
        if [ "$CRT_COUNT" -eq 0 ]; then
            echo "package.sh: no runtime DLLs found in $CRT_DIR" >&2
            exit 1
        fi
        find "$CRT_DIR" -maxdepth 1 -type f -iname '*.dll' \
            -exec cp {} "$OUT/" \;
        echo "==> bundled $CRT_COUNT Visual C++ runtime DLL(s) for $CRT_ARCH"
    fi
fi

# On macOS the movie decoder is Homebrew's libavcodec by absolute path, which the audit below
# refuses; this copies the closure beside the executable and rewrites the load commands.
./tools/bundle-dylibs.sh "$OUT/$(basename "$BIN")" "$OUT"

cp strikers.ini.example "$OUT/"

# The licence texts the program is redistributed under: ODE's BSD, the decompilation's CC0, the
# eCos files' GPL, and MusyX's own, renamed so the archive's notices are one flat set.
cp LICENSE-BSD.TXT LICENSE-CC0.txt LICENSE-GPL-2.0.txt "$OUT/"
cp extern/musyx/LICENSE "$OUT/LICENSE-MUSYX.txt"

# FFmpeg's licence travels with FFmpeg: the DLLs on Windows, the bundled dylibs on macOS.
FFMPEG_SHIPPED=0
for _lib in "$OUT"/av*.[Dd][Ll][Ll] "$OUT"/libav*.dylib "$OUT"/libav*.so*; do
    if [ -f "$_lib" ]; then FFMPEG_SHIPPED=1; fi
done
if [ "$FFMPEG_SHIPPED" = 1 ]; then
    cp LICENSE-LGPL-2.1.txt "$OUT/"
    echo "==> included LICENSE-LGPL-2.1.txt (FFmpeg libraries are in this archive)"
fi

# The pipeline cache seed is optional: a build without one pays more on its first run.
if [ -f "$BUILD/initial_pipeline_cache.db" ]; then
    cp "$BUILD/initial_pipeline_cache.db" "$OUT/"
    echo "==> included initial_pipeline_cache.db"
else
    echo "==> no initial_pipeline_cache.db in $BUILD"
fi

# The settings app is a separate Qt project, bundled when QT_PREFIX or the default prefix has Qt and
# skipped when it does not.
# Required, and SETTINGS=0 to say otherwise: the game runs this app on a first launch and waits for
# it, so an archive without it is not a smaller product but an incomplete one.
SETTINGS="${SETTINGS:-1}"
QT_PREFIX="${QT_PREFIX:-${QT_ROOT_DIR:-}}"
if [ -z "$QT_PREFIX" ] && [ -d /opt/homebrew/opt/qt ]; then
    QT_PREFIX=/opt/homebrew/opt/qt
fi
# Forward slashes first: install-qt-action exports a native Windows path, CMake takes it as a -D
# value, and CMake reads a backslash as the start of an escape sequence -- \a and \s are not valid
# ones, so the configure dies before it looks for Qt. `tr` because this script is /bin/sh.
QT_PREFIX=$(printf '%s' "$QT_PREFIX" | tr '\\' '/')
SETTINGS_ARTEFACT=""
if [ "$SETTINGS" = 1 ] && { [ -z "$QT_PREFIX" ] || [ ! -d "$QT_PREFIX" ]; }; then
    echo "package.sh: no Qt found, so strikers-settings cannot be built." >&2
    echo "  The game runs it on a first launch and waits for it, so an archive" >&2
    echo "  without it is incomplete. Point QT_PREFIX at a Qt 6 prefix, or pass" >&2
    echo "  SETTINGS=0 to package without it deliberately." >&2
    exit 1
fi
if [ "$SETTINGS" = 1 ]; then
    echo "==> settings app: building with Qt at $QT_PREFIX"
    SETTINGS_BUILD="build-settings-package"
    if [ "$WINDOWS" = "1" ]; then
        SETTINGS_BUNDLE=OFF
    else
        SETTINGS_BUNDLE=$([ "$(uname -s)" = Darwin ] && echo ON || echo OFF)
    fi
    # Quiet when it works, and its whole output when it does not: discarding stdout meant a Windows
    # failure printed the "building with Qt at ..." line and then nothing at all.
    SETTINGS_LOG="$SETTINGS_BUILD.log"
    if ! { cmake -S settings -B "$SETTINGS_BUILD" -DCMAKE_BUILD_TYPE=Release \
                 -DCMAKE_PREFIX_PATH="$QT_PREFIX" -DBUILD_TESTING=OFF \
                 -DSTRIKERS_SETTINGS_BUNDLE="$SETTINGS_BUNDLE" \
           && cmake --build "$SETTINGS_BUILD" --config Release -j4; } >"$SETTINGS_LOG" 2>&1; then
        echo "package.sh: the settings app failed to build, with Qt at $QT_PREFIX:" >&2
        cat "$SETTINGS_LOG" >&2
        exit 1
    fi
    if [ "$WINDOWS" = "1" ]; then
        SETTINGS_EXE=$(find "$SETTINGS_BUILD" -name 'strikers-settings.exe' | head -n 1)
        cp "$SETTINGS_EXE" "$OUT/"
        # Qt's own deployment tool: the DLLs and the platform plugin.
        "$QT_PREFIX/bin/windeployqt" --release --no-translations --no-compiler-runtime \
            "$OUT/strikers-settings.exe" >/dev/null
        SETTINGS_ARTEFACT="strikers-settings.exe"
    elif [ "$(uname -s)" = Darwin ]; then
        rm -rf "$OUT/strikers-settings.app"
        cp -R "$SETTINGS_BUILD/strikers-settings.app" "$OUT/"
        # macdeployqt copies the frameworks and the cocoa plugin in, so the app runs on a machine
        # without Qt.
        "$QT_PREFIX/bin/macdeployqt" "$OUT/strikers-settings.app" -always-overwrite >/dev/null 2>&1
        codesign --force --deep -s - "$OUT/strikers-settings.app" 2>/dev/null || true
        # The executable inside the bundle, not the bundle: the check after the archive is
        # made is a whole-line match against the listing, and tar writes a directory as
        # "strikers-settings.app/" with a trailing slash. A regular file is the same string
        # in both checks, and is the thing worth checking anyway.
        SETTINGS_ARTEFACT="strikers-settings.app/Contents/MacOS/strikers-settings"
    else
        cp "$SETTINGS_BUILD/strikers-settings" "$OUT/"
        SETTINGS_ARTEFACT="strikers-settings"
        # Linux leans on the distro's Qt, as check-runtime-deps.sh does.
    fi
    echo "==> settings app: bundled"
else
    echo "==> settings app: SETTINGS=0, archive ships without strikers-settings"
fi

# The copy in dist/, once everything is in it: the game, and the settings app where it is a plain
# executable or a bundle's main executable.
./tools/check-runtime-deps.sh "$OUT/$(basename "$BIN")" "$OUT"
# The name that was actually bundled, rather than the three it might have been.
# Guessing broke on Windows: MSYS's POSIX layer resolves a stat of "foo" to
# "foo.exe" when only the latter exists, so `[ -f "$OUT/strikers-settings" ]`
# was true, and the unsuffixed path it then passed to llvm-readobj -- a native
# Windows program, with no such rule -- did not exist.
if [ -n "$SETTINGS_ARTEFACT" ]; then
    ./tools/check-runtime-deps.sh "$OUT/$SETTINGS_ARTEFACT" "$OUT"
fi

# What the archive has to contain, by name, checked before it is made and again after.
NEEDED_FILES="$(basename "$BIN") strikers.ini.example
              LICENSE-BSD.TXT LICENSE-CC0.txt LICENSE-GPL-2.0.txt LICENSE-MUSYX.txt"
if [ "$FFMPEG_SHIPPED" = 1 ]; then
    NEEDED_FILES="$NEEDED_FILES LICENSE-LGPL-2.1.txt"
fi
if [ -n "$SETTINGS_ARTEFACT" ]; then
    NEEDED_FILES="$NEEDED_FILES $SETTINGS_ARTEFACT"
fi
for _need in $NEEDED_FILES; do
    if [ ! -e "$OUT/$_need" ]; then
        echo "package.sh: $_need is missing from $OUT" >&2
        exit 1
    fi
done

cd dist
rm -f "$NAME.$FORMAT"
if [ "$FORMAT" = "zip" ]; then
    if command -v 7z >/dev/null 2>&1; then
        7z a -tzip "$NAME.zip" "$NAME" >/dev/null
        LISTING=$(7z l -ba -slt "$NAME.zip" | sed -n 's/^Path = //p' | tr '\\' '/')
    else
        zip -r -q "$NAME.zip" "$NAME"
        LISTING=$(unzip -Z1 "$NAME.zip")
    fi
else
    tar -czf "$NAME.tar.gz" "$NAME"
    LISTING=$(tar -tzf "$NAME.tar.gz")
fi
for _need in $NEEDED_FILES; do
    if ! printf '%s\n' "$LISTING" | grep -qx "$NAME/$_need"; then
        echo "package.sh: $NAME/$_need is not in $NAME.$FORMAT" >&2
        exit 1
    fi
done
echo "==> dist/$NAME.$FORMAT ($(printf '%s\n' "$LISTING" | grep -c .) entries)"
