# Android

Super Mario Strikers v2.0.0, natively, on an arm64 Android device. This preview combines the
upstream v2.0.0 game and rollback multiplayer with Luis Santos's Android platform code.

Requires game data from your own copy of Super Mario Strikers. No game assets are included.

The APK installs as **Strikers 2.0 Settings Preview** alongside the earlier builds. It has
its own preferences and app-specific data directory.

## What it needs

- **A 64-bit ARM device on Android 9 (API 28) or later, with Vulkan.** Dawn has exactly one backend
  on Android and it is Vulkan; there is no GL fallback worth having. 32-bit and x86 Android are not
  built: the prebuilt Dawn package exists for `android-aarch64` alone, and a 32-bit address space is
  not a comfortable place for this game's heap.
- **Your own disc.** A disc image (`.iso`, `.gcm`, `.ciso` or `.gcz`) or the extracted disc's
  `files` folder, US, European or Japanese.

## Building

The **Android APK** workflow builds the ARM64 test APK from source and uploads it as
`strikers.apk`. The CI APK uses debug signing. Android requires the same signing key
to update an existing installation with the same package ID.


Two steps, deliberately separate. The native build is CMake and the NDK, the same build every other
platform gets; Gradle only packages what it produced, so Gradle's own idea of which CMake and which
NDK to use never gets a vote.

```sh
# 1. the game
tools/build-android.sh                  # -> build-android/jniLibs/arm64-v8a/

# 2. the app
cd android && ./gradlew assembleRelease # -> app/build/outputs/apk/release/
```

The Android build defaults to native version `2.0.0` so its multiplayer handshake matches the
upstream release even when the port is built from a local commit.

The first one wants `ANDROID_SDK_ROOT` (or `ANDROID_HOME`) pointing at an SDK with NDK
**29.0.14206865** installed, which is the NDK Aurora's prebuilt Dawn was compiled with. A different
NDK is a different libc++, and `libwebgpu_dawn.a` is a static archive that has to meet the one it
was built against.

```sh
sdkmanager "ndk;29.0.14206865" "platforms;android-35" "build-tools;35.0.1"
```

`tools/build-android.sh` runs the same three steps as `tools/rebuild.sh` -- compile, generate stubs,
link -- because the stub set cannot be known until every translation unit has compiled, and then
stages `libmain.so` and `libc++_shared.so` where the Gradle project expects them. It rewrites
`src/platform/stubs_generated.c`, which is generated output that happens to be checked in and is
per-object-format: the copy in the tree carries Mach-O's leading underscore on every symbol and
satisfies nothing on ELF.

## Game files

Choose **Disc Image…** and select a local disc image in the Android file picker. The launcher
remembers the selection. Press **Play** to read the image in place. If a storage provider can
only stream the image, download it locally before choosing it.

For an extracted disc, place its `files` directory under
`Android/data/io.github.newcoke.strikers.settingspreview/files/` with USB or ADB. The launcher
also finds supported images in that app-specific directory when no image is selected.

## Settings and controls

The Android launcher provides video, control and game settings. It writes the corresponding keys to
`strikers.ini` while preserving existing comments and unrelated keys. See `strikers.ini.example`
for the native configuration keys. The frame limit follows the display by default and can be capped
from Settings.

The on-screen pad and USB or Bluetooth controllers use the same SDL gamepad path. The on-screen pad
hides when a physical controller connects.

## Texture packs

Open **Settings → Video → Texture packs** and import a Dolphin-format ZIP with PNG or DDS
replacements. Choose an individual pack, **All packs**, or **Off**. Imported packs are
stored under `mods/textures` in the app's game folder. You can also copy an extracted
pack folder there over USB or ADB. Pack changes apply the next time you start the game.
Android defaults to a 512 MiB texture cache. You can override it with `texture_cache_mb`
in `strikers.ini`.

## Online play

On the in-game main menu, press the on-screen **RB** button (GameCube **Z**) or the controller's
displayed Online button. Use the stick and face buttons to navigate the online panel. The same pad
edits addresses in the character wheel. Use **Nearby rooms** on the same network or **Join by
address** for a direct connection.

Peers need the same upstream version and a compatible disc region. US and European copies can play
together. Japanese copies match other Japanese copies. Android-to-Android and Android-to-PC matches
still need device testing.

## What is missing

- **Discord Rich Presence and Discord invites.** The upstream integration uses desktop IPC.
- **The desktop settings app.** Android has its own launcher settings.

## Reading a crash

The port installs no signal handler on Android: `debuggerd` already writes a tombstone with
symbolised frames, the registers and the other threads, which is strictly more than the desktop
handler prints.

```sh
adb logcat -s strikers SDL DEBUG AndroidRuntime
adb pull /sdcard/Android/data/io.github.newcoke.strikers.settingspreview/files/strikers-log.txt
```

The second is the port's own log: `STRIKERS_LOG` is pointed at it before the game starts, because
stderr on Android goes nowhere anyone can read.
