# Strikers native assets and one PSARC

The Vita build consumes one converted `sms.psarc`. The original disc or source
PSARC remains on the host. The Vita loaders accept the native model/texture ABI
and do not fall back to retail GLG/GLT payloads.

## Build the host tools

Reuse the established host test cache:

```sh
cmake -S smstrikers-port -B smstrikers-port/.tmp/native-gxm-host -DSTRIKERS_AURORA=OFF
cmake --build smstrikers-port/.tmp/native-gxm-host --target strikers_compile_native_graphics strikers_compile_native_audio -j6
```

The graphics compiler links the port's texture decoder and the same native file
validator used on Vita. It has no dependency on a graphics translation backend.

## Generate the complete archive

From an original verified source PSARC:

```sh
python3 smstrikers-port/tools/vita_prepare_assets.py \
  --profile strikers \
  --source-archive /path/to/original-source.psarc \
  --output /path/to/native-output/sms.psarc \
  --native-graphics-compiler smstrikers-port/.tmp/native-gxm-host/tests/strikers_compile_native_graphics \
  --native-audio all \
  --audio-compiler smstrikers-port/.tmp/native-gxm-host/tests/strikers_compile_native_audio \
  --native-video --ffmpeg ffmpeg
```

An own plain ISO/GCM may replace `--source-archive` with `--iso`. Source and
output paths must differ. Existing outputs are refused unless `--replace` is
explicit. Conversion uses a private temporary directory and publishes only after
container validation and complete SHA-256 comparison against the prepared tree.
The JSON report records the source, converted files, native coverage and final
archive identity. Runtime data, native outputs and retail payloads stay out of Git.

Already verified audio/video records can be reused with
`--native-media-archive /path/to/prepared.psarc` instead of the audio/video
compilation options. Only the audio catalogue, PCM records and native movie
records are imported. Model, texture and shader sidecars from older renderers
are excluded. Graphics are regenerated from the original source.

The existing logical `.glg` and `.glt` paths remain stable for scene scripts;
their contents are replaced:

- `NSMD0001`: little-endian model state, stream descriptors, vertex/index arrays,
  reference matrices, material mappings and animation data. Skin, morph, bone
  maps and stitching use `NSSK0001`. No retail model chunk is retained as fallback.
- `NSTB0001`: native texture dictionary; every entry contains `NSTX0001`.
- `NSTX0001`: dimensions, mip extents, material metadata, exact decoded RGBA
  pixels and checksum. Palette lookup, tiled-source decoding and swizzle
  conversion happen on the host. Every mip is compared against the production
  decoder before publication.
- Embedded texture entries in font/menu resource bundles are also replaced;
  directory hashes and non-texture resource bytes remain intact. Resource
  dictionaries are discovered by structure, including `.Dmn` and future filename
  extensions. Directories
  are rebuilt for the actual native payload sizes.

Native textures are uploaded to the sole VitaGL owner during loading. The game
retains metadata and owned GPU handles, without another persistent CPU copy of
file texture pixels. Texture and resource marker retirement both release their
GPU resources. Runtime-generated ramps, surfaces and effects remain dynamic.
Native geometry still participates in the game's live pose, material and draw
preparation; this format is not a claim that every per-draw CPU operation has
been removed.

The complete pack also contains the original data still consumed by game logic,
scene scripts and animation loaders. Original compressed audio remains an input
to the exact PCM validation and stream control; the native PCM records replace
matching DSP decode work. This does not claim every original non-graphics file
has been converted to a different format. Native H.264 movies replace the
original movie payloads at their logical paths, with original audio and timing
retained. H.264 conversion is lossy at the selected CRF; model/texture conversion
is not. GameCube executables, debug symbol outputs and unused system boot code
are omitted from this Vita pack. `sys/boot.bin` retains the disc/save identity.

## Install and configure

Install the matching SELF/VPK and put the complete archive at
`ux0:data/strikersVita/sms.psarc`. Do not mix a native-only build with an older
source archive. `sys/strikers-native.bin` identifies the required graphics ABI.
The new default INI is generated as profile `2026-10-15-vitagl-unified`:

```ini
asset_archive = ux0:data/strikersVita/sms.psarc
vita_native_audio = 1
vita_native_video = 1
vita_test_match = 0
vita_frameskip = 0
fixed_dt = 0
```

There is no second native-archive selector. All consumers use `asset_archive`.
The target remains 60 FPS. Simulation uses elapsed time with the original fixed
simulation tick, independently of presentation cadence. Native preparation and
a successful build do not prove any particular gameplay frame rate.

For delivery verification only, `asset_verify_manifest` can select a small
host-generated manifest containing the expected SHA-256 of each 1 MiB block.
Create it without modifying the archive:

```sh
python3 smstrikers-port/tools/vita_native_manifest.py /path/to/native-output/sms.psarc /path/to/native-verify.bin
```

Transfer the manifest and compare its readback bytes before selecting
`asset_verify_manifest=ux0:data/strikersVita/native-verify.bin` for one verification
launch. Remove this temporary INI key before normal gameplay.
The Vita's DMAC5 SHA-256 service verifies every block and writes
`native_asset_verification.json`, then exits without starting gameplay. This
check is disabled when the key is absent and adds no archive scan to normal
startup. Its result reports chunk verification; the expected whole-file hash is
metadata from the independently verified manifest, not a separately computed
whole-file digest on Vita.

## Validation

`native_graphics` tests the actual C++ parser, malformed bounds and checksums.
It can additionally scan a complete archive:

```sh
smstrikers-port/.tmp/native-gxm-host/tests/native_graphics_test /path/to/native-output/sms.psarc
```

The Python tests cover all supported source packet/texture header variants,
lossless skin normal conversion, native array byte order and embedded resource
repacking. The compiler validates all converted models and compares every
converted texture mip with the production decoder. Hardware menus, manual
matches, animated models, particles, goals, shadows, audio and transitions still
require checks on the Vita with the exact paired build/archive.
