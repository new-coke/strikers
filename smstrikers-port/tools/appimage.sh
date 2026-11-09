#!/bin/sh
# Turn a Linux archive from tools/package.sh into an AppImage: tools/appimage.sh <archive.tar.gz>
# <out.AppImage>
#
# The archive goes into the AppImage unchanged. The game itself needs nothing more: it links only
# libc, libm and libz and loads SDL's video and audio backends from the host. The settings app
# links the Qt 6.4 it was built against, which the archive leaves to the distro; here that Qt
# comes from Debian 12 (bookworm), whose libraries need no newer glibc than the 2.35 ceiling, and
# travels inside the AppImage with the plugins it loads.
#
# Run it on the architecture it is for: the bundled patchelf and appimagetool run on the build
# machine. Needs curl, apt-get, dpkg-deb, readelf, objdump and Debian's archive keyring
# (debian-archive-keyring), but neither root nor FUSE.

set -e

ARCHIVE="${1:?usage: tools/appimage.sh <archive.tar.gz> <out.AppImage>}"
OUT="${2:?usage: tools/appimage.sh <archive.tar.gz> <out.AppImage>}"
case "$ARCHIVE" in /*) ;; *) ARCHIVE="$PWD/$ARCHIVE" ;; esac
case "$OUT" in /*) ;; *) OUT="$PWD/$OUT" ;; esac

PORT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$PORT"

case "$(uname -m)" in
    x86_64)        ARCH=x86_64;  DEB_ARCH=amd64; TRIPLET=x86_64-linux-gnu ;;
    aarch64|arm64) ARCH=aarch64; DEB_ARCH=arm64; TRIPLET=aarch64-linux-gnu ;;
    *) echo "appimage.sh: no AppImage for $(uname -m)" >&2; exit 1 ;;
esac

KEYRING=/usr/share/keyrings/debian-archive-keyring.gpg
if [ ! -f "$KEYRING" ]; then
    echo "appimage.sh: $KEYRING is missing; install debian-archive-keyring" >&2
    exit 1
fi

# Pinned releases and their checksums, not the AppImage project's moving continuous builds, so a
# release build packs what the last one did.
TOOL_VERSION=1.9.1
RUNTIME_VERSION=20251108
TOOL_URL="https://github.com/AppImage/appimagetool/releases/download/$TOOL_VERSION/appimagetool-$ARCH.AppImage"
RUNTIME_URL="https://github.com/AppImage/type2-runtime/releases/download/$RUNTIME_VERSION/runtime-$ARCH"
case "$ARCH" in
    x86_64)
        TOOL_SHA256=ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
        RUNTIME_SHA256=2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d ;;
    aarch64)
        TOOL_SHA256=f0837e7448a0c1e4e650a93bb3e85802546e60654ef287576f46c71c126a9158
        RUNTIME_SHA256=00cbdfcf917cc6c0ff6d3347d59e0ca1f7f45a6df1a428a0d6d8a78664d87444 ;;
esac

WORK="$PORT/build-appimage-$ARCH"
APTROOT="$WORK/apt"
SYSROOT="$WORK/sysroot"
APPDIR="$WORK/AppDir"
QTPLUG="$SYSROOT/usr/lib/$TRIPLET/qt6/plugins"

# What the host provides: the AppImage project's excludelist, the GPU and audio stack, which has to
# match the host's drivers, and udev and D-Bus, which have to match the host's daemons.
EXCLUDE=" ld-linux-x86-64.so.2 ld-linux-aarch64.so.1 libanl.so.1 libBrokenLocale.so.1 libc.so.6
  libdl.so.2 libm.so.6 libmvec.so.1 libnss_compat.so.2 libnss_dns.so.2 libnss_files.so.2
  libpthread.so.0 libresolv.so.2 librt.so.1 libthread_db.so.1 libutil.so.1 libstdc++.so.6
  libgcc_s.so.1 libz.so.1 libGL.so.1 libEGL.so.1 libGLdispatch.so.0 libGLX.so.0 libOpenGL.so.0
  libdrm.so.2 libglapi.so.0 libgbm.so.1 libvulkan.so.1 libxcb.so.1 libX11.so.6 libX11-xcb.so.1
  libwayland-client.so.0 libwayland-cursor.so.0 libwayland-egl.so.1 libasound.so.2
  libfontconfig.so.1 libfreetype.so.6 libharfbuzz.so.0 libcom_err.so.2 libexpat.so.1
  libgpg-error.so.0 libICE.so.6 libSM.so.6 libusb-1.0.so.0 libuuid.so.1 libjack.so.0
  libpipewire-0.3.so.0 libxcb-dri3.so.0 libxcb-dri2.so.0 libfribidi.so.0 libgmp.so.10
  libudev.so.1 libdbus-1.so.3 "
EXCLUDE=" $(echo $EXCLUDE) "

# The plugins a widgets app loads on X11 and Wayland.
QT_PLUGINS="platforms platformthemes platforminputcontexts imageformats iconengines
  xcbglintegrations wayland-decoration-client wayland-graphics-integration-client
  wayland-shell-integration"

# APT_CONFIG, so the build machine's own apt.conf.d (a docker-clean hook, a proxy) stays out of it.
apt_() {
    APT_CONFIG="$APTROOT/apt.conf" apt-get "$@"
}

# An unprivileged apt with a root of its own, so nothing is installed on the build machine; the
# packages are only unpacked into the sysroot.
if [ ! -d "$QTPLUG/platforms" ]; then
    echo "==> appimage: Qt 6 from Debian 12 ($DEB_ARCH)"
    rm -rf "$APTROOT" "$SYSROOT"
    for _d in etc/apt/apt.conf.d etc/apt/preferences.d etc/apt/sources.list.d etc/apt/trusted.gpg.d \
              var/lib/apt/lists/partial var/cache/apt/archives/partial var/lib/dpkg; do
        mkdir -p "$APTROOT/$_d"
    done
    : > "$APTROOT/var/lib/dpkg/status"
    cat > "$APTROOT/apt.conf" <<EOF
Dir "$APTROOT/";
Dir::State::status "$APTROOT/var/lib/dpkg/status";
Debug::NoLocking "1";
Acquire::Languages "none";
Acquire::Retries "5";
APT::Architecture "$DEB_ARCH";
APT::Architectures { "$DEB_ARCH"; };
EOF
    echo "deb [signed-by=$KEYRING] http://deb.debian.org/debian bookworm main" \
        > "$APTROOT/etc/apt/sources.list"
    apt_ -q update
    apt_ -q -y --download-only --no-install-recommends install \
        libqt6widgets6 qt6-qpa-plugins qt6-wayland qt6-gtk-platformtheme libqt6svg6 \
        qt6-image-formats-plugins patchelf >/dev/null
    mkdir -p "$SYSROOT"
    for _deb in "$APTROOT"/var/cache/apt/archives/*.deb; do
        dpkg-deb -x "$_deb" "$SYSROOT"
    done
fi
PATCHELF="$SYSROOT/usr/bin/patchelf"

# fetch <url> <sha256> <file>: GitHub's release downloads time out now and then, hence the retries.
fetch() {
    if [ -f "$3" ] && echo "$2  $3" | sha256sum -c - >/dev/null 2>&1; then return 0; fi
    curl -fsSL --retry 8 --retry-all-errors --retry-delay 5 -o "$3" "$1"
    if ! echo "$2  $3" | sha256sum -c - >/dev/null 2>&1; then
        echo "appimage.sh: $1 does not match its pinned SHA-256" >&2
        rm -f "$3"
        exit 1
    fi
}
mkdir -p "$WORK/tools"
fetch "$TOOL_URL" "$TOOL_SHA256" "$WORK/tools/appimagetool"
fetch "$RUNTIME_URL" "$RUNTIME_SHA256" "$WORK/tools/runtime"
chmod +x "$WORK/tools/appimagetool"

echo "==> appimage: assembling $APPDIR"
rm -rf "$APPDIR"
GAME="$APPDIR/opt/strikers"
mkdir -p "$GAME" "$APPDIR/usr/lib" "$APPDIR/usr/plugins" "$APPDIR/usr/share/applications" \
    "$APPDIR/usr/share/icons/hicolor/256x256/apps"
tar -xzf "$ARCHIVE" -C "$GAME"
for _need in strikers strikers-settings strikers.ini.example; do
    if [ ! -e "$GAME/$_need" ]; then
        echo "appimage.sh: $_need is missing from $ARCHIVE" >&2
        exit 1
    fi
done

for _p in $QT_PLUGINS; do
    if [ -d "$QTPLUG/$_p" ]; then cp -R "$QTPLUG/$_p" "$APPDIR/usr/plugins/"; fi
done

# The settings app's and the plugins' library closure, breadth first, out of the sysroot.
needed() { readelf -d "$1" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'; }
find_lib() {
    for _dir in "$SYSROOT/usr/lib/$TRIPLET" "$SYSROOT/lib/$TRIPLET"; do
        if [ -e "$_dir/$1" ]; then echo "$_dir/$1"; return 0; fi
    done
    return 1
}
QUEUE="$WORK/queue"
: > "$QUEUE"
echo "$GAME/strikers-settings" >> "$QUEUE"
find "$APPDIR/usr/plugins" -name '*.so' >> "$QUEUE"
_line=1
_missing=""
while _obj=$(sed -n "${_line}p" "$QUEUE"); [ -n "$_obj" ]; do
    _line=$((_line + 1))
    for _lib in $(needed "$_obj"); do
        case "$EXCLUDE" in *" $_lib "*) continue ;; esac
        [ -e "$APPDIR/usr/lib/$_lib" ] && continue
        if ! _src=$(find_lib "$_lib"); then
            case " $_missing " in *" $_lib "*) ;; *) _missing="$_missing $_lib" ;; esac
            continue
        fi
        cp -L "$_src" "$APPDIR/usr/lib/$_lib"
        echo "$APPDIR/usr/lib/$_lib" >> "$QUEUE"
    done
done
if [ -n "$_missing" ]; then
    echo "appimage.sh: not in the Debian sysroot:$_missing" >&2
    exit 1
fi
echo "==> appimage: bundled $(ls "$APPDIR/usr/lib" | wc -l | tr -d ' ') libraries"

# RUNPATHs rather than LD_LIBRARY_PATH, so the game, which the settings app starts, does not
# inherit the bundled libraries.
for _lib in "$APPDIR"/usr/lib/*.so*; do "$PATCHELF" --set-rpath '$ORIGIN' "$_lib"; done
find "$APPDIR/usr/plugins" -name '*.so' -exec "$PATCHELF" --set-rpath '$ORIGIN/../../lib' {} \;
"$PATCHELF" --set-rpath '$ORIGIN/../../usr/lib' "$GAME/strikers-settings"
printf '[Paths]\nPrefix = ../../usr\nPlugins = plugins\n' > "$GAME/qt.conf"

# The glibc ceiling the archive keeps holds for what the AppImage adds.
_glibc=$(find "$APPDIR/usr" -name '*.so*' -type f -exec objdump -T {} + 2>/dev/null |
    sed -n 's/.*GLIBC_\(2\.[0-9]*\).*/\1/p' | sort -t. -k2 -n | tail -n 1)
_ceiling="${STRIKERS_MAX_GLIBC:-2.35}"
if [ -n "$_glibc" ] && [ "${_glibc#2.}" -gt "${_ceiling#2.}" ]; then
    echo "appimage.sh: the bundled Qt needs glibc $_glibc, above the $_ceiling ceiling" >&2
    exit 1
fi

cp assets/linux/appimage/AppRun "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun"
cp assets/linux/appimage/strikers.desktop "$APPDIR/strikers.desktop"
cp assets/linux/appimage/strikers.desktop "$APPDIR/usr/share/applications/strikers.desktop"
cp assets/linux/appimage/strikers.png "$APPDIR/strikers.png"
cp assets/linux/appimage/strikers.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/strikers.png"
ln -s strikers.png "$APPDIR/.DirIcon"

echo "==> appimage: packing $OUT"
mkdir -p "$(dirname "$OUT")"
rm -f "$OUT"
ARCH="$ARCH" VERSION="${STRIKERS_VERSION:-dev}" "$WORK/tools/appimagetool" \
    --appimage-extract-and-run --no-appstream --runtime-file "$WORK/tools/runtime" \
    "$APPDIR" "$OUT" >/dev/null
test -s "$OUT"
echo "==> $OUT ($(du -h "$OUT" | cut -f1))"
