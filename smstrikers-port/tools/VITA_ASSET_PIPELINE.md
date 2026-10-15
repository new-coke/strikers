# Asset preparation and native Strikers PSARC

The current Vita runtime requires one converted native PSARC. See
[NATIVE_ASSETS.md](NATIVE_ASSETS.md) for the complete graphics/audio/video
conversion command and the matching runtime contract. The extraction and
container layer described below also remains useful for preparing an original
source PSARC on the host; an unconverted source pack cannot be used by the
current native-only Vita graphics loaders.

Implemented 2026-10-05. Container format and license:
[asset_pipeline/FORMAT.md](asset_pipeline/FORMAT.md).

The pipeline extracts the disc's live filesystem, removes duplicate payloads,
packages a single PSARC and checks every archived file against its source using
SHA-256. It keeps original paths, game identity, byte order, movies, sound and
texture/model payloads. No disc padding is copied. Stored blocks avoid adding
decompression work to the Vita CPU. The generated archive contains local game
data and must stay outside Git and VPK packages.
The host tool requires Python 3.10 or newer and no third-party Python packages.
The original source PSARC or ISO remains on the host for rebuilding and validating the native archive.

## Prepare Strikers

Run from the repository root, using the user's own plain ISO/GCM:

```sh
python3 smstrikers-port/tools/vita_prepare_assets.py \
  --profile strikers --iso sms.iso \
  --output ab-artifacts/asset-pipeline-20261005/sms.psarc
```

Supported Strikers identities: G4QE01, G4QP01, G4QJ01. Compressed disc inputs
must first be converted to a plain ISO/GCM. Existing outputs are refused unless
`--replace` is supplied. The source ISO is never overwritten. Work happens in a
private temporary directory; a failed conversion does not replace a previous
archive. A JSON report beside the archive records source identity, archive
identity, inventory, deduplication and per-file size/SHA-256. Only the temporary
directory created by that invocation is removed.

The archive needs a build that includes `src/platform/asset_archive.c` and the
DVD adapter. Older Strikers builds cannot mount it. For the current Vita build, first complete the native conversion in NATIVE_ASSETS.md. Copy the converted `sms.psarc` to
`ux0:data/strikersVita/sms.psarc`; all native consumers select that same file:

```ini
asset_archive = ux0:data/strikersVita/sms.psarc
```

Restart the game. This explicit key takes precedence over the Vita's default
`sms.iso`. Startup reports `(PSARC 1.4)` and the original region identity. With
an ISO/folder fallback installed, removing the key selects that backend. The
source ISO and saves need no changes. An explicitly selected invalid archive fails with an error;
it does not silently switch data sources. Desktop `data=/path/to/sms.psarc`
also recognizes the PSAR header.

### Original source-pack storage

For the verified G4QP01 disc, the original source `sms.psarc` occupies 623,698,090 bytes instead of
the ISO's 1,459,978,240 bytes: 836,280,150 bytes less, a 57.28% reduction. This
comes from removing disc padding and sharing identical payloads; original game
files remain byte-identical in that source pack. Native conversion replaces
graphics/movie payloads and adds prepared audio, so the resulting native pack
has a different size and identity. Original source packs use stored blocks. Native outputs may compress cold
PCM/index records with zlib; loading those records includes decompression.

Use the archive selector above for normal gameplay. It does not require the
automatic test match or frameskip; those remain independent INI controls:

```ini
asset_archive = ux0:data/strikersVita/sms.psarc
vita_test_match = 0
vita_frameskip = 0
```

Keeping both `sms.iso` and `sms.psarc` on the memory card consumes space for both.
After archive verification and hardware validation, an ISO-only fallback copy
can be kept on the host instead. The conversion tool never deletes either the
source ISO or an installed copy. The measured archive size reduction is not an
FPS claim; the hardware comparison is recorded in
[ASSET_PIPELINE_STATUS_2026-10-05.md](../../ASSET_PIPELINE_STATUS_2026-10-05.md).

The PSARC contains `files/...` and `sys/...`; DVD indexing exposes only paths
under `files/`. Region and save-card identity come from `sys/boot.bin`. DVD
reads clamp to each logical file, and retain callbacks, error states, alignment
handling and native renderer memory invalidation. A stored file uses one positional read
for each DVD request, independent of the archive's 64 KiB block boundaries.
The Vita uses `sceIoPread`; host streams are locked for seek/read operations.

## Reuse in other ports

For another plain GameCube disc, preserving its entire extracted layout:

```sh
python3 smstrikers-port/tools/vita_prepare_assets.py \
  --profile gamecube --iso /path/to/own-disc.iso \
  --output /path/to/own-disc.psarc
```

For existing asset directories from any engine:

```sh
python3 smstrikers-port/tools/vita_prepare_assets.py \
  --profile generic --root /path/to/files --root /path/to/sys \
  --output /path/to/game-data.psarc
```

Each root's basename becomes its archive prefix. Root names must be unique
without case. Output must be outside all input trees. Symlinks, unsafe paths and
names that collide without case are rejected. Extraction also checks for FST
name collisions before writing, preventing silent file replacement on macOS or
Windows. Use `--no-dedup` for a reference archive with separate payloads.

`asset_pipeline/psarc.py` is the game-independent container layer. It can be
copied with its CC0 license and format documentation. `asset_pipeline/adapters.py` owns
game discovery and validation; the current GameCube adapter invokes the existing
`extract-disc.py`, which must accompany it. A new engine's adapter should emit
validated roots and game-specific metadata, leaving the container code alone.

The C reader and `include/port/asset_archive.h` can be reused without Strikers,
GX or FIOS. Another port must connect its own file APIs to this reader; producing
a PSARC alone does not make an existing port load it. The reader accepts PSARC
1.4, zlib-tagged 64 KiB blocks, relative manifests and flag 0 or 1. Stored
archives need no zlib library. Compressed blocks need `STRIKERS_ZLIB` and zlib.
Limits: 65,536 TOC entries including the manifest, 32 MiB TOC and 8 MiB path
manifest. Case-insensitive runtime lookup folds ASCII. Other PSARC codecs,
encryption, absolute-path manifests and flags are rejected. Entry path digests
are validated by the host verifier; runtime indexing uses the validated path
manifest and does not use MD5 lookup.

## Implemented asset preparation

The pipeline provides ISO extraction, padding removal, identical-content
deduplication, PSARC packing, full round-trip validation, retained disc
identity and selectable archive reads. Native asset conversion must preserve
Strikers geometry, animation and material contracts and provide a matching
renderer consumer. The 2026-10-05 packaging snapshot did not implement that
renderer path.

The 2026-10-06 follow-up adds a bounded GLT/static-GLG adapter and matching
Aurora AVNR compiler/consumer. See [NATIVE_ASSETS.md](NATIVE_ASSETS.md) for
`--native-compiler`, `--native-only` and the independent native cache archive.
Only cold native sidecars use compression; original game blocks stay stored.
Animated/deformed and unsupported assets retain their original runtime path.

Storage savings and fewer host filesystem opens are useful, but no gameplay FPS
gain follows from packaging alone. Native texture/geometry compilation is a
separate source of possible runtime gains. Do not use archive size as an FPS
claim.

## Validation

`tool_vita_assets` exercises stored and independently built compressed archives,
deduplication/hardlinks, deterministic output, full source comparison, source
mutation, case collisions, symlinks, malformed/truncated tables, corrupt streams,
the Strikers ISO adapter, and the actual C reader/DVD callbacks. The host
contract suite includes this test. `asset_archive_probe` can additionally read
an archive against a complete extracted tree, comparing every payload and
checking partial reads at the file end. The same checks were run with ASan/UBSan.

Real-disc conversion results and bounded hardware validation are recorded in
`ASSET_PIPELINE_STATUS_2026-10-05.md` at the repository root. Generated artifacts
and retail payloads remain in ignored `ab-artifacts/asset-pipeline-20261005/`.
