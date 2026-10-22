# Strikers Vita

An unofficial port of **Super Mario Strikers / Mario Smash Football** for
PlayStation Vita. The current runtime uses VitaGL, PC-prepared native graphics,
PCM audio, and **H.264/AAC MP4 movies streamed from PSARC through SceAvPlayer**.
This project is still under development. The next performance milestone is
**stable 30 FPS in rendered cutscenes**, followed by 60 FPS. Neither target
has been achieved or certified yet.

- [PSARC asset preparation and PS Vita installation](ASSET_SETUP.md)
- [Performance measurements, MP4 playback and the 30 FPS milestone](smstrikers-port/performance/MIDTERM_30FPS_MP4_2026-10-22.md)
- [Local release candidate and tester instructions](TESTING_RC.md)
- [Example runtime configuration](smstrikers-port/strikers-vitagl.ini.example)
- [LiveArea artwork and layout sources](smstrikers-port/assets/vita/sce_sys)

You must supply your own compatible GameCube game image. Disc images, PSARC
archives and extracted game assets are **not** included in this repository
or in the VPK.

## Quick start

On your computer, install Python 3.10+, CMake 3.24+, a C++20 compiler, FFmpeg
with the `libx264` and `aac` encoders, and `ffprobe`. On Windows, use WSL2.
From the repository root:

```sh
python3 smstrikers-port/tools/prepare_strikers_vita.py \
  --iso "/path/to/Mario Smash Football.iso" \
  --output "local-data/sms.psarc"

python3 smstrikers-port/tools/prepare_strikers_vita.py \
  --verify "local-data/sms.psarc"
```

The tool builds the host asset converters, prepares native graphics and audio,
converts movies to MP4 and verifies the archive. It produces `sms.psarc`
and `sms.psarc.json`; keep the JSON report on your computer for
verification. The default video format is `mp4`. A generic PSARC
containing only the original disc files is not sufficient for this runtime.

On the PS Vita:

1. Set up VitaShell and `ur0:data/libshacccg.suprx` as described in the
   [installation guide](ASSET_SETUP.md#3-installare-sulla-ps-vita).
2. Install the **complete VPK from the matching build**, with MP4/SceAvPlayer support.
3. Copy `sms.psarc` to **`ux0:data/strikersVita/sms.psarc`** without extracting it.
4. Copy the [example configuration](smstrikers-port/strikers-vitagl.ini.example)
   to **`ux0:data/strikersVita/strikers.ini`**.
5. Launch Strikers Vita from LiveArea and follow the game's original menus.

The [full setup guide](ASSET_SETUP.md) covers dependencies, file transfers,
INI settings, updating without losing saves, and troubleshooting.

## PlayStation button glyphs

PlayStation button glyphs are enabled by default. To select them in
`ux0:data/strikersVita/strikers.ini`:

```ini
vita_playstation_glyphs = 1
```

Set the value to `0` to keep the original GameCube glyphs, then restart
the game. This changes supported button-font glyphs and prompt artwork,
**not** the controller mapping.

| Original control | PS Vita button |
| --- | --- |
| A | Cross |
| B | Circle |
| X | Square |
| Y | Triangle |
| L / R | L / R |
| Z | SELECT |
| START | START |

Button symbols embedded in movies and any literal button names in game text
remain part of the original game assets.

## Building for PS Vita

In the project's configured macOS/VitaSDK environment:

```sh
smstrikers-port/build-vita-vitagl.sh
```

The script reuses `smstrikers-port/build-vita-gxm-latest` and explicitly
disables the profiler. The resulting package is `strikers_vita.vpk` in
that directory. To produce a tester release candidate with diagnostics,
profiling and logging disabled:

```sh
smstrikers-port/build-vita-vitagl.sh --release-candidate
```

This requires a clean, committed source tree. It generates a dedicated
`strikers.ini` and `release-candidate.json` with the source commit and
file SHA-256 hashes alongside the VPK. **Keep the release VPK local:** provide
it privately to testers; do not commit it, attach it to a GitHub release,
or upload it to GitHub. See [tester instructions](TESTING_RC.md).

This build script expects VitaSDK at `/usr/local/vitasdk`; VitaSDK is not
needed just to prepare the game assets.

Dependency licenses and notices are preserved in their respective repository
files. The current renderer uses [vitaGL](https://github.com/Rinnegatamante/vitaGL).
