# Strikers asset workflow integration, 2026-10-05

## Implemented scope

The Strikers source now includes a reusable PSARC 1.4 packer, game adapters,
verified ISO extraction, a portable C archive reader, and an opt-in DVD backend.
The archive can replace the data source selected by DVD without changing the
game's paths, callbacks, partial-read rules, memory publication or region/save
identity. Usage and cross-port integration are documented in
[VITA_ASSET_PIPELINE.md](smstrikers-port/tools/VITA_ASSET_PIPELINE.md).

The container layer is distributed under CC0-1.0, with the complete license
preserved beside the source. Container details are documented in
[FORMAT.md](smstrikers-port/tools/asset_pipeline/FORMAT.md).
The C reader uses the selected platform file APIs. Original assets remain
byte-identical. Offline geometry/texture preparation requires a matching
Strikers compiler and renderer consumer; packaging alone does not reduce
steady-state rendering work.

## Real-disc result

Source: local `sms.iso`, disc identity G4QP01. All original retail/developer
payloads in the disc FST were included, with five system files.

| Quantity | Bytes |
|---|---:|
| Original ISO | 1,459,978,240 |
| Logical payloads, including system files | 648,504,578 |
| Duplicate payloads eliminated | 24,935,033 |
| Final stored PSARC | 623,698,090 |

Archive reduction against ISO: **57.28%**. This includes disc padding removal;
deduplication contributes about 24.94 MB. The payload is not compressed at
runtime. There are 1,420 archived files, including 1,415 game files; 1,350
distinct payloads. The first local extraction/packing/full-validation run took
6.44 seconds. This is a host conversion time, not a Vita loading measurement.

- ISO SHA-256: `54cf50f242764801822a4e3b7b816a8c4367797d196e3210f2f6b4da1f8cd69e`
- PSARC SHA-256: `79c9db029380164e9c9bc393e3b2511579362960eb63f95501d15d968ebba65c`

Two independent preparations produced the same PSARC SHA-256. Every packed
file's size and SHA-256 were compared to its prepared source. The actual C
reader and DVD adapter also compared every file against the pre-existing local
extracted tree. That check passed both with zlib linked and with no zlib
dependency. Local reports/payloads are in ignored
`ab-artifacts/asset-pipeline-20261005/` and are not included in Git or the VPK.

## Source and build identity

- Root base: `fcbc550ca7a4c3931c31078e53ddff301750360e`, plus local changes.
- Embedded Aurora base: `39fb53e916f00036a20ddd734de960d6b40554fd`, retaining
  the local vertex-program recipe refresh fix from the previous GPU-crash work.
- Build: `1.3.0-assets-20261005`, Release/LTO, native GXM, existing async-GX,
  direct-submit, native-texture and static-geometry configuration retained.
- SELF SHA-256: `b7324a95ff5106d201ab9a3671dbbe09171b3bc245c6da1939909784092efca8`
- ELF SHA-256: `b86b60f8e3a88c73decd4431f2cba390a2b50c4ffec2f55c4c3d477827dd7a8d`
- VPK SHA-256: `5bcd623529b3c771b497e2d7ed0a94b7b78b4b896e86d980862069ce4f665c73`

The matching ELF/map/VELF/SELF/VPK are preserved in the ignored `build/`
subdirectory. The VPK's eboot is byte-identical to the preserved SELF. Relevant
source-file hashes are recorded in `source-identity.json`.

## Local verification

- All 19 host contract tests passed.
- The asset test contains 10 synthetic cases, including corrupt/truncated
  archives, independent compressed fixtures, deduplication, source equivalence,
  case collisions, symlinks, source mutation and real DVD callback/EOF behavior.
- The same asset tests passed with the C reader/DVD probe built under ASan and
  UBSan.
- The no-zlib C reader passed full real-archive comparison.
- Vita build/package succeeded. GXM binary audit passed with 12,696 executable
  symbols inspected, native draw/present found, and no GL/vgl/vita2d imports.
- `git diff --check` passed.

These results validate source/container/backend behavior. Storage reduction does
not establish a gameplay FPS gain. Hardware results follow with installed
binary, archive and configuration identities.

## Hardware verification

The new SELF was installed and read back with SHA-256 `b7324a95…`. The complete
623,698,090-byte archive was compared byte for byte with the hashed host archive
before promotion to `ux0:data/strikersVita/sms.psarc`. A transient FTP timeout
required retrying the readback; the unverified file was not activated. Metadata
queries use binary mode: the FTP server's ASCII `SIZE` scans large file contents.
The original installed binary and INI were backed up, and `sms.iso` was retained.

The diagnostic launch explicitly reports:

```text
[port] DVD: 1415 files in ux0:data/strikersVita/sms.psarc (PSARC 1.4)
[port] DVD: disc G4QP01 (Europe)
```

The popup/save-load FEN fingerprints also report `source=PSARC`, with the expected
distinct FNV values `8447d462` and `dffa4a45`. Actual clocks were CPU 444, GPU 222,
bus 222 and XBAR 166 MHz. The seed was `0x53545249`, fixed timestep
`16.666666667` ms, WARM shaders, `gxm_disable=0x8`, frameskip 0 and overlay 0.

Three sessions completed, **1,380 sampled live-play frames** in total:

| Session | Samples | Live-play interval | Elapsed FPS | Median ms | P95 ms |
|---|---:|---|---:|---:|---:|
| PSARC, diagnostics + screenshot | 180 | 0–179 | 14.830 | 41.503 | 68.468 |
| ISO, quiet | 600 | 0–599 | 25.079 | 39.771 | 43.407 |
| PSARC, quiet | 600 | 0–599 | 24.979 | 39.806 | 43.818 |

The screenshot run is excluded from the performance comparison. Frame 60 took
4,366.660 ms, including 4,326.767 ms in presentation/capture; screenshot
serialization perturbs the timing. Its PPM is byte-identical to the previous
ISO capture at live-play frame 60, SHA-256
`4e8b49308aed48819f9c96e7307c8c84debe849ce91cc06e743c2839d1a7257f`.
Models, stadium, textures and HUD were visible in hardware captures.

The two quiet sessions used the **same SELF**, settings and seeded live-play
interval, with diagnostics and screenshots disabled. CSV headers confirm the
same actual clocks. PSARC elapsed FPS differs by **−0.398%** from ISO. A single
600-frame pair does not establish a meaningful speed difference; there is no
demonstrated gameplay FPS improvement from this packaging change. Neither quiet
sample has a frame meeting the 16.67 ms 60-FPS deadline. Loading time was not
benchmarked separately.

- Diagnostic CSV SHA-256: `caa8c90ef7517f4812740e64171c22608141c89ab164437e7336546e55c423b4`
- ISO quiet CSV SHA-256: `9b5ef386615811b0513d232676faf944aa567f5df0d93adc04d0e1c854db46dd`
- PSARC quiet CSV SHA-256: `5c00a2b1c9ccc0a68e407b57026ad43740a7bb0d12660c2468692cb4b62cc12f`

The normal INI was restored byte for byte, SHA-256
`14473a76e92bfc2f8af49be028b2c091d2cd889a6dae0146ba011c9d5051d93a`.
The new build and archive remain installed; the normal configuration selects
the original ISO until `asset_archive` is added. Automatic test match and
frameskip are both 0. Stability evidence is limited to the captured sessions,
not a complete match or long suspend/resume test.

No new `.psp2dmp` appeared between the pre-test and final directory snapshots.
Two pre-existing dumps were retained. Final installed SELF and restored INI
were read back and checked; identities are in `final-device/identity.json`.

## Normal PSARC selection, 2026-10-06

PSARC is now the selected package for normal gameplay on the user's Vita. A
fresh readback confirmed the same installed SELF (`b7324a95…`) and the expected
623,698,090-byte archive. The original `sms.iso` was already absent from the
device before this configuration change; this operation did not delete it.
The host's original ISO remains available for rebuilding the archive.

The following selector was added before the managed defaults in the device INI:

```ini
asset_archive = ux0:data/strikersVita/sms.psarc
```

Automatic test match and frameskip remain 0. The INI was staged, read back,
promoted with a backup of the previous configuration, then read back again.
Installed INI SHA-256:
`5429ed04a2986c31fab2ceb92bb3a66eb8d342e8c05a05052db8254124cf5c4d`.
The runtime preserves this selector outside its regenerated default block.
The change takes effect on the next launch. No binary changes or additional
performance measurements were needed for the selector; the hardware evidence
above remains the evidence for this build and archive.

Current asset footprint on the memory card is 623.70 MB instead of an ISO-only
1,459.98 MB: 836.28 MB less (57.28%). The expected archive size was checked again
today; its complete byte comparison was performed in the hardware validation
above. Backup INI, readback and device identities for this change are in ignored
`ab-artifacts/psarc-default-20261006/`. The project INI example and preparation
guide now recommend PSARC for normal Vita gameplay while retaining ISO support.

## Remaining native-asset work

The original disc inventory includes 164 GLT bundles and 149 GLG model files.
The packaging step preserves them; it does not compile them into GPU resources.
A useful next implementation must do the following at their real format boundary:

1. Parse immutable GLT/GLG views with checked extents and retain exact source
   identities, texture handles, mip/palette metadata and model packet semantics.
2. Compile eligible texture mip chains into the GXM layouts consumed by the
   existing native upload path. Start with lossless I8/IA8/RGB565 conversions.
   CMPR-to-BC1 must preserve GX interpolation/alpha or use a separately evaluated
   quality policy; changing block endianness alone is not sufficient.
3. Add a matching Aurora consumer whose native sidecar key includes source
   identity, format version and complete layout. Ownership/retirement must remain
   explicit; do not enable `cacheable` on every frontend texture as a shortcut.
4. Compile only immutable model geometry with a proved layout. Animated,
   deformed, dynamic and unsupported packets keep the GX fallback. Material/TEV
   behavior cannot be dropped simply because vertex bytes were prepared offline.
5. Compare enabled/fallback paths on the same Vita build, seed, clocks, shader
   cache state, frameskip policy and live-play interval before claiming an FPS
   improvement. The PSARC can carry native sidecars alongside original data once
   those compiler/consumer contracts exist.
