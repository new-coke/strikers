#!/bin/sh
# Zip the .nro and licences under switch/strikers/. Usage: tools/switch/package.sh [build] [name]

set -e

BUILD="${1:-build-switch}"
NAME="${2:-strikers-switch}"

case "$NAME" in
    .|..|-*|*[!A-Za-z0-9._-]*)
        echo "package.sh: archive-name must contain only letters, digits, '.', '_' and '-'" >&2
        exit 1 ;;
esac

PORT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$PORT"

if [ ! -f "$BUILD/strikers.nro" ]; then
    echo "package.sh: no $BUILD/strikers.nro; run tools/switch/rebuild.sh first" >&2
    exit 1
fi
NVK=$(sed -n 's/^STRIKERS_SWITCH_NVK_DIR:PATH=//p' "$BUILD/CMakeCache.txt")

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT INT TERM
OUT="$STAGE/switch/strikers"
mkdir -p "$OUT"

cp "$BUILD/strikers.nro" "$OUT/"
cp strikers-switch.ini.example "$OUT/strikers.ini.example"
# Aurora and the Vulkan driver are linked into the .nro, so their notices ship too.
mkdir -p "$OUT/licenses"
cp LICENSE-BSD.TXT LICENSE-CC0.txt LICENSE-GPL-2.0.txt "$OUT/licenses/"
cp extern/musyx/LICENSE "$OUT/licenses/LICENSE-MUSYX.txt"
cp extern/aurora/LICENSE "$OUT/licenses/LICENSE-AURORA.txt"
for _l in LICENSE-APACHE-2.0.txt LICENSE-DAWN.txt LICENSE-FMT.txt LICENSE-IMGUI.txt LICENSE-LIBNX.txt \
          LICENSE-MONOCYPHER.txt \
          LICENSE-XXHASH.txt LICENSE-ZSTD.txt; do
    cp "licenses/$_l" "$OUT/licenses/"
done
if [ ! -f "$NVK/NOTICE.md" ]; then
    echo "package.sh: no NOTICE.md beside the Vulkan driver in '$NVK'" >&2
    exit 1
fi
cp "$NVK/NOTICE.md" "$OUT/licenses/NOTICE-NVK.md"

mkdir -p dist
rm -f "dist/$NAME.zip"
(cd "$STAGE" && zip -qrX "$PORT/dist/$NAME.zip" switch)
echo "==> ok: dist/$NAME.zip"
