# Preparing the PSARC and running Strikers Vita

This guide covers the **current vitaGL runtime**, which uses native graphics,
PCM audio, and **H.264/AAC MP4 movies played through SceAvPlayer**. The
recommended script is
[`prepare_strikers_vita.py`](smstrikers-port/tools/prepare_strikers_vita.py).
It prepares and verifies the complete archive. Simply renaming an ISO or
creating a PSARC containing only the original GameCube files is not enough.

## PC requirements

- Your own **uncompressed GameCube ISO/GCM image** of Super Mario Strikers /
  Mario Smash Football. Recognized game IDs: `G4QE01` (USA), `G4QP01`
  (Europe), and `G4QJ01` (Japan). A recognized ID does not guarantee that
  every disc revision or language has been tested in gameplay on the Vita.
- Python **3.10 or later**, CMake **3.24 or later**, a **C++20** compiler,
  FFmpeg with the **libx264** and **AAC** encoders, and `ffprobe`.
  FFmpeg 6 or later is recommended.
- macOS or Linux; on Windows, use **WSL2** with Linux tools.
- An Internet connection for the first build to download the xxHash headers,
  pinned to version 0.8.3 and verified with SHA-256.
- Enough free space for the ISO, extraction, temporary conversions, and the
  final archive. Allow at least **8 GiB of free space in addition to the ISO**
  on your PC. On the Vita, allow approximately **2 GiB** for the PSARC, plus
  space for the application and save files. Actual sizes depend on the disc
  and conversion options.

You do not need VitaSDK, SDL, Aurora, additional Python packages, or a
desktop build of the game to run the preparation script.

On macOS, install the Xcode Command Line Tools (`xcode-select --install`).
If you use Homebrew, install the dependencies with:

```sh
brew install python cmake ffmpeg
```

On Ubuntu/WSL2, using a distribution that provides the required versions:

```sh
sudo apt update
sudo apt install build-essential cmake python3 ffmpeg
```

Check the installed versions with `python3 --version`, `cmake --version`,
and `ffmpeg -version`. RVZ, NKit, and other compressed formats must first
be converted to a full GameCube disc image with an appropriate tool.

## 1. Create the archive

Download or clone this repository, open a terminal in its root directory,
and run the following command, adjusting the two paths:

```sh
python3 smstrikers-port/tools/prepare_strikers_vita.py \
  --iso "/path/to/Mario Smash Football.iso" \
  --output "local-data/sms.psarc"
```

Under WSL2, a Windows drive is accessible as, for example,
`/mnt/c/Games/Mario Smash Football.iso`. Keeping the repository, build
files, and output on the Linux filesystem may reduce the overhead of the
many file operations involved.

The script builds the two host-side converters in
`smstrikers-port/build-asset-tools`, extracts the disc into a temporary
directory, prepares models and textures for the renderer, converts all audio
required by the pipeline to PCM, and converts movies to H.264 Baseline MP4
with 48 kHz stereo AAC-LC audio. It then creates and verifies the PSARC.
Temporary directories are removed on completion; the source ISO is unchanged.

The generated files are:

```text
local-data/
  sms.psarc
  sms.psarc.json
```

The JSON report contains the source identity, archive SHA-256, and conversion
results. Keep it on your PC; it does not need to be copied to the Vita.
Preparation can take several minutes, and the final hash is printed only
after verification. Existing output is rejected by default; add `--replace`
if you intentionally want to regenerate it.

Optional parameters:

```sh
python3 smstrikers-port/tools/prepare_strikers_vita.py \
  --iso "/path/to/game.iso" --output "local-data/sms.psarc" \
  --jobs 4 --video-crf 18 --ffmpeg "/path/to/ffmpeg"
```

`--jobs` controls compilation and video conversion concurrency;
`--video-crf` defaults to 18. Keep these defaults for your first attempt.
Generated ISOs, PSARCs, and converted assets are excluded from Git.

Video conversion preserves the original THP resolution, frame count, and
frame rate (29.97 Hz for the 17 movies on the verified European disc).
Audio is first decoded using the port's DSP arithmetic, with sample counts
and record bounds checked, then resampled and encoded to AAC. This audio
conversion is lossy. Each MP4 is inspected with `ffprobe` and fully decoded
on the PC before the PSARC is finalized.

Movies are stored under `native/v2/video/`, with a binary `STMP4001`
descriptor next to each `.mp4`. The game still requests the original
`.thp` names; the player resolves them inside the PSARC through positional
read callbacks. Movies do not need to be extracted on the Vita. The
complete package removes replaced THP files only after successful conversion.

The default is `--video-format mp4`. To produce the legacy format for an
older version of the port, use `--video-format wavc`; this retains all-intra
H.264 inside THP and uses the older decoder.
`--ffprobe /path/to/ffprobe` selects a specific executable.
PC-side verification does not establish smooth playback, correct colors,
or audio/video synchronization on the console.

## 2. Verify an existing archive

```sh
python3 smstrikers-port/tools/prepare_strikers_vita.py \
  --verify "local-data/sms.psarc"
```

This command requires the JSON report alongside the PSARC and checks its
SHA-256, the native graphics marker, the complete audio index, and the
movie files. For MP4, it also checks that each descriptor matches the
conversion report and that no replaced THP files remain in the package.
It reads and verifies the entire archive, then prints the video format,
movie count, and required player setting. It does not need the converters
or FFmpeg. File verification is not a substitute for testing game startup
and gameplay on the Vita.

The lower-level pipeline remains available in
[`vita_prepare_assets.py`](smstrikers-port/tools/vita_prepare_assets.py),
but its options can produce generic archives or legacy formats that are
incompatible with the current runtime. For a new setup, use the command
in this guide: it enables `--native-graphics-compiler`, audio `all`, and
native video together. The old `--native-compiler` option alone is
not sufficient.

## 3. Install on PS Vita

Your console must be able to run homebrew software and install VPK files
through VitaShell. The renderer also requires the `libshacccg.suprx`
shader compiler, extracted and decrypted on the console and normally placed at
**`ur0:data/libshacccg.suprx`**. See the official
[vitaGL prerequisites](https://github.com/Rinnegatamante/vitaGL#prerequisites)
and the [ShaRKF00D](https://github.com/OsirizX/ShaRKF00D) project for extraction.

For the best possible performance, **CapUnlocker is strongly recommended**.
It unlocks access to the system-reserved fourth CPU core for game applications.
Install it before playing by following the
[CapUnlocker instructions in the README](README.md#capunlocker-for-best-performance).
It is not included in the VPK and is not required just to launch the game.

Use a **VPK compatible with your asset preparation pipeline**: older builds
that only play WAVC/THP cannot use a new MP4 archive with the replaced THPs
removed. Download the full VPK from
[GitHub Releases](https://github.com/robin994/strikersVita/releases), or,
if you build the project yourself, find it at
`smstrikers-port/build-vita-gxm-latest/strikers_vita.vpk` after following
the [README build instructions](README.md#building-for-ps-vita).

1. Transfer the **complete Strikers Vita VPK** to your console using
   VitaShell USB or FTP, open it in VitaShell, and confirm the installation.
   The application title is `Super Mario Strikers Vita`, Title ID `SMSVITA01`.
2. Create the `ux0:data/strikersVita/` directory using VitaShell or FTP/USB.
3. Copy **`sms.psarc`** into that directory without extracting it.
4. Copy [`strikers-vitagl.ini.example`](smstrikers-port/strikers-vitagl.ini.example)
   into the same directory and rename it to **`strikers.ini`**.
   Alternatively, use the matching INI attached to the release, if provided.
5. Make sure the INI contains only one `asset_archive` entry, pointing to
   the path shown below. Back up any existing personal INI before replacing it.
6. Launch the game from its LiveArea bubble and follow the in-game menus.

The final directory layout is:

```text
ux0:data/strikersVita/
  sms.psarc
  strikers.ini
```

Essential settings for normal gameplay:

```ini
asset_archive = ux0:data/strikersVita/sms.psarc
vita_native_audio = 1
vita_native_video = 1
vita_video_player = avplayer
vita_video_archive =
vita_playstation_glyphs = 1
vita_test_match = 0
vita_frameskip = 0
diagnostics = 0
vitagl_timing = 0
vitagl_affine_uv = 0
vitagl_vertex_fog = 0
```

The test-match mode is enabled through the INI only when measurements are
needed; it is not required for normal startup. Experimental shaders remain
disabled in the distributed configuration.

### PlayStation or GameCube button glyphs

`vita_playstation_glyphs = 1` is the default. In supported fonts and prompts,
GameCube A/B/X/Y button glyphs are displayed as **× / ○ / □ / △**
respectively, matching the Vita controls. L and R keep their original
letters. Supported controller badges use the PlayStation layout.
The icons are included in the executable; no extra texture pack is required,
and switching this option does not require regenerating the PSARC.

To use the original GameCube button artwork:

```ini
vita_playstation_glyphs = 0
```

Close and restart the game after changing the setting. Add only one copy
of the option in your personal settings, outside any
`STRIKERS VALIDATED DEFAULTS` block: personal settings take priority.
This option does not remap controls, change literal button names in game
text, or replace button symbols embedded in movies.

### Updating an existing installation

Close the game and back up your personal INI and the `cardA/` and `cardB/`
save directories from `ux0:data/strikersVita/` to your PC. Install the
new complete VPK and replace `sms.psarc` with the verified archive for
the updated pipeline. Preserve your saves and reapply personal settings
to the updated example INI, avoiding duplicate keys. You do not need to
uninstall the application before updating.

## 4. LiveArea: icon, background, and launch button

The source assets are located in
[`smstrikers-port/assets/vita/sce_sys/`](smstrikers-port/assets/vita/sce_sys/):

| File in the VPK | Purpose |
| --- | --- |
| `sce_sys/icon0.png` | Application bubble icon, 128 x 128 |
| `sce_sys/pic0.png` | Application image, 960 x 544 |
| `sce_sys/livearea/contents/template.xml` | `a1` layout and image references |
| `sce_sys/livearea/contents/bg.png` | LiveArea background, 840 x 500 |
| `sce_sys/livearea/contents/startup.png` | Launch button image, 280 x 158 |

The template explicitly assigns `bg.png` to the background and
`startup.png` to the launch button. The images are indexed PNGs, and the
template is UTF-8 with CRLF line endings and `content-rev="3"`.
These constraints follow the
[VitaSDK guidance](https://github.com/vitasdk/samples#notes-on-images).
`template.xml` does not control the application bubble icon; that is
provided by `icon0.png`.

The build checks the source assets and VPK contents, including whether
`eboot.bin` matches the freshly generated SELF. For a manual check:

```sh
python3 smstrikers-port/tools/validate_vita_package.py \
  --vpk smstrikers-port/build-vita-gxm-latest/strikers_vita.vpk \
  --self smstrikers-port/build-vita-gxm-latest/strikers_vita.self
```

To update LiveArea artwork, install the complete VPK. Copying only
`eboot.bin`, as commonly done during development, does not update the
application bubble assets. Close the application and its LiveArea page
before reinstalling. Let VitaShell finish the installation, then reopen
the page. If the shell still shows the previous artwork, reboot the
console after reinstalling and check the files under
`ux0:app/SMSVITA01/sce_sys/`.

The shell also caches assets in **`ur0:appmeta/SMSVITA01/`**.
In the verified on-device case, this directory initially contained only
`icon0.png`: the template under `ux0:app/` alone was not enough to make
the correct LiveArea appear. After reinstalling the full VPK and reloading
the page, the cache also contained `pic0.png` and
`livearea/contents/{template.xml,bg.png,startup.png}`. A console
screenshot confirmed that the Strikers background and launch button
were displayed correctly.
The shell regenerates this cache. The path is useful for confirming
an update, but the assets to distribute remain those packaged in the VPK.
Avoid uninstalling the application as a first troubleshooting step,
since that may remove data you want to preserve.

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| ISO rejected immediately | Check the uncompressed image format and game ID; do not pass RVZ/NKit or a disc from a different game. |
| Compiler, CMake, or FFmpeg not found | Install dependencies in the same environment as your terminal; use WSL2 on Windows. |
| FFmpeg lacks `libx264` | Use an FFmpeg build that includes this encoder. |
| FFmpeg lacks `aac` or `ffprobe` is missing | Install a complete FFmpeg distribution before preparing MP4 files. |
| Output already exists | Choose a new output path or explicitly use `--replace`. |
| SHA-256 differs from the JSON report | Do not use that transfer or output; regenerate or recopy a verified pair. |
| Archive lacks the native marker | Regenerate with `prepare_strikers_vita.py`; a generic or legacy PSARC is insufficient. |
| Game fails to start or load | Check the INI path, complete PSARC, shader module, and `ux0:data/strikersVita/vitagl.log`. |
| Movies missing with a new PSARC | Use an MP4-capable VPK, set `vita_video_player = avplayer` and `vita_native_video = 1`, and leave `vita_video_archive` empty for the complete archive. |
| Original button glyphs still displayed | Check `vita_playstation_glyphs = 1`, remove any overrides setting it to `0`, and restart. Symbols embedded in movies remain unchanged. |
| Old application bubble or LiveArea artwork still displayed | Check and reinstall the complete VPK; the template is under `sce_sys/livearea/contents/`. |
