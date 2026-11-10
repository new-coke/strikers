# Android

This fork runs Strikers natively on ARM64 Android. The Android code builds on
[Luis Santos's port](https://github.com/luisxl15/Strikers-Android-Port). The game
and rollback code come from [Strikers upstream](https://github.com/new-coke/strikers).

## Install

- Use an ARM64 device running Android 9 or later with Vulkan support.
- Use data from your own US, European, or Japanese copy of Super Mario Strikers.
  The APK contains no game assets.
- Download `strikers.apk` from a successful [Android APK workflow run](https://github.com/keeganwhitacre/strikers-droid/actions?query=branch%3Aandroid-port)
  or build it below. CI artifacts require a signed-in GitHub account.

The app appears as **Strikers**. It retains the settings preview's package ID and
storage location. Android requires the same signing key for an in-place update.
CI builds use a debug key and are intended for testing. Back up the app's game
directory before removing an existing installation.

## Build from source

Install JDK 21, CMake, Ninja, Python 3, and the Android SDK command-line tools.
Set `ANDROID_HOME` to the SDK directory. Use NDK 29.0.14206865 to match Aurora's
prebuilt Dawn library.

From `smstrikers-port/`:

```sh
sdkmanager "ndk;29.0.14206865" "platforms;android-35" "build-tools;35.0.1"
./tools/build-android.sh
./android/gradlew -p android assembleRelease
```

The native script builds the game, generates platform stubs, and stages ARM64
libraries in `build-android/jniLibs/arm64-v8a/`. Gradle packages them at
`android/app/build/outputs/apk/release/app-release.apk`. The native build
regenerates the tracked `src/platform/stubs_generated.c` locally. Keep that
generated change out of Android maintenance commits.

The game reports version `2.0.0` by default to match the upstream multiplayer
handshake. When updating the fork, check the target upstream version and network
compatibility before changing the native version or Android package metadata.

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
together. Japanese copies match other Japanese copies.

## What is missing

- **Discord Rich Presence and Discord invites.** The upstream integration uses desktop IPC.
- **The desktop settings app.** Android has its own launcher settings.

## Device testing

Run this checklist on an ARM64 Android device after syncing with upstream and
before distributing a new APK. Record the commit, device model, Android version,
GPU and driver, disc region, and results in the release notes or test issue.

| Area | Checks |
| --- | --- |
| Install and launch | Install the APK. Pick a local disc image, launch a match, and relaunch the app to check that the selection persists. Try upgrading from a build signed with the same key and verify that settings and saves survive. |
| Native settings | Change resolution, frame limit, and touch settings. Restart the game to check that settings applied. Open Settings from the touch overlay and return to the match. |
| Input and lifecycle | Play with touch, connect and disconnect a physical controller, then background and resume the game. Verify that the overlay returns when the controller disconnects and that inputs release on focus loss. |
| Graphics and textures | Play with the system Vulkan driver. If supported, test a custom driver and return to the system driver. Import a known texture pack, confirm a visible replacement, disable the pack, and confirm the original texture returns. |
| Online | Play Android-to-Android and Android-to-desktop using builds of the same upstream version. Test discovery and direct connection, then complete a match and rematch. |

For crashes or input problems, capture Android's log and the native game log:

```sh
adb logcat -s strikers SDL DEBUG AndroidRuntime
adb pull /sdcard/Android/data/io.github.newcoke.strikers.settingspreview/files/strikers-log.txt
```

Use the [fork's issue tracker](https://github.com/keeganwhitacre/strikers-droid/issues)
for Android reports. Include the APK build, device details, and steps to reproduce.
