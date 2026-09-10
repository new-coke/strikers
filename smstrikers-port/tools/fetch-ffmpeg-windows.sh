#!/bin/sh
# Put a Windows ffmpeg in extern/ffmpeg-windows-x86_64, which CMake searches when pkg-config cannot
# answer. Usage: fetch-ffmpeg-windows.sh [destination]. BtbN's lgpl shared build;
# STRIKERS_FFMPEG_URL overrides.

# Pinned to one month-end BtbN build by URL and sha256 (the last build of each month stays up,
# dailies go away), so a changed download is refused rather than linked. STRIKERS_FFMPEG_URL and
# STRIKERS_FFMPEG_SHA256 override the pair; the unpack is stamped and reused only for that build.
set -e

PORT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST="${1:-$PORT/extern/ffmpeg-windows-x86_64}"
PINNED_URL="https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-08-31-13-27/ffmpeg-n9.0.1-11-ge47273f4d9-win64-lgpl-shared-9.0.zip"
PINNED_SHA="83a824f0729a69d143c9865125bb86988a11dd388325f0033711045522068aa0"
URL="${STRIKERS_FFMPEG_URL:-$PINNED_URL}"
if [ -n "${STRIKERS_FFMPEG_URL:-}" ] && [ "$STRIKERS_FFMPEG_URL" != "$PINNED_URL" ]; then
    # Another build than the pinned one: only a digest given for it applies.
    WANT_SHA="${STRIKERS_FFMPEG_SHA256:-}"
else
    WANT_SHA="${STRIKERS_FFMPEG_SHA256:-$PINNED_SHA}"
fi
STAMP="$DEST/.strikers-ffmpeg"

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    fi
}

if [ -f "$DEST/lib/avcodec.lib" ] && [ -f "$DEST/include/libavcodec/avcodec.h" ]; then
    # An if, not `x=$([ -f ... ] && ...)`: under set -e a missing stamp exited the script.
    HAVE_SHA=
    HAVE_URL=
    if [ -f "$STAMP" ]; then
        HAVE_SHA=$(awk 'NR == 1 {print $1}' "$STAMP")
        HAVE_URL=$(sed -n '1s/^[0-9a-f]* *//p' "$STAMP")
    fi
    if [ "$HAVE_URL" = "$URL" ] && { [ -z "$WANT_SHA" ] || [ "$HAVE_SHA" = "$WANT_SHA" ]; }; then
        echo "==> ffmpeg already unpacked in $DEST ($HAVE_SHA)"
        exit 0
    fi
    echo "==> ffmpeg in $DEST is not the build asked for (stamp: ${HAVE_SHA:-none} ${HAVE_URL:-none}); refetching"
fi

WORK=$(mktemp -d)
# The temporary directory goes even when curl or the unpacker fails, so a half-downloaded zip cannot
# make the next run fail.
trap 'rm -rf "$WORK"' EXIT INT TERM

ZIP="$WORK/ffmpeg.zip"
echo "==> fetching $URL"
curl -fsSL --retry 3 --retry-delay 5 -o "$ZIP" "$URL"

# Say which build this actually was, and refuse one that is not the build asked for.
GOT_SHA=$(sha256_of "$ZIP")
if [ -z "$GOT_SHA" ]; then
    if [ -n "$WANT_SHA" ]; then
        echo "fetch-ffmpeg-windows.sh: no sha256sum or shasum on PATH, so the download cannot be verified" >&2
        exit 1
    fi
    echo "==> (no sha256sum or shasum on PATH; the download was not fingerprinted)"
    GOT_SHA=unverified
fi
echo "==> $GOT_SHA  $(basename "$URL")"
if [ -n "$WANT_SHA" ] && [ "$GOT_SHA" != "$WANT_SHA" ]; then
    echo "fetch-ffmpeg-windows.sh: sha256 mismatch for $URL" >&2
    echo "    expected $WANT_SHA" >&2
    echo "    got      $GOT_SHA" >&2
    echo "    A pinned build does not change; either the download was tampered with or" >&2
    echo "    the pin above was edited without its digest. Re-pin both together." >&2
    exit 1
fi
if [ -z "$WANT_SHA" ]; then
    echo "==> (no digest given for $URL; this build is unpinned)"
fi

# Three unpackers, because the hosts disagree about which exists: a GitHub Windows runner has bsdtar
# and 7z, Git Bash may have neither unzip nor a zip-capable tar, a Linux host has unzip.
echo "==> unpacking"
if command -v unzip >/dev/null 2>&1; then
    unzip -q "$ZIP" -d "$WORK/x"
elif tar -xf "$ZIP" -C "$WORK" 2>/dev/null; then
    mkdir -p "$WORK/x"
    mv "$WORK"/ffmpeg-* "$WORK/x/"
elif command -v 7z >/dev/null 2>&1; then
    7z x -bso0 -o"$WORK/x" "$ZIP"
else
    echo "fetch-ffmpeg-windows.sh: no unzip, no zip-capable tar and no 7z on PATH" >&2
    exit 1
fi

# The zip holds one top-level directory named for the build; take what is inside it, so the
# destination is include/ lib/ bin/ whatever it was called.
TOP=$(find "$WORK/x" -mindepth 1 -maxdepth 1 -type d | head -n 1)
if [ -z "$TOP" ] || [ ! -d "$TOP/include" ] || [ ! -d "$TOP/lib" ]; then
    echo "fetch-ffmpeg-windows.sh: unexpected archive layout under $WORK/x" >&2
    find "$WORK/x" -maxdepth 2 >&2
    exit 1
fi

rm -rf "$DEST"
mkdir -p "$(dirname "$DEST")"
mv "$TOP" "$DEST"
printf '%s  %s\n' "$GOT_SHA" "$URL" > "$STAMP"

# Keep avcodec, avutil and swresample; avformat, avfilter, avdevice and swscale are 58 MB the game
# never loads, and the CI volume has been down to 77 MB. Import libraries go with their DLLs.
for _drop in avformat avfilter avdevice swscale postproc; do
    rm -f "$DEST"/bin/"$_drop"-*.dll
    rm -f "$DEST"/lib/"$_drop".lib "$DEST"/lib/"$_drop"-*.def \
          "$DEST"/lib/lib"$_drop".dll.a "$DEST"/lib/lib"$_drop".a
    rm -rf "$DEST/include/lib$_drop"
done
rm -f "$DEST"/bin/ffmpeg.exe "$DEST"/bin/ffplay.exe "$DEST"/bin/ffprobe.exe
rm -rf "$DEST/doc" "$DEST/presets"

echo "==> ffmpeg in $DEST"
ls "$DEST/bin" | sed 's/^/    /'
