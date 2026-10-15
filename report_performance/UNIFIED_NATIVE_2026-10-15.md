# Unified native assets — 2026-10-15

## Accepted delivery — 2026-10-16

The owner confirmed regular audio and a correct rolling ball in a manual match
on SELF `66c2caa0a7a1005ad656d6e97a93697f5a7461e294bf3d676c697414111736ed`.
The subsequent crowd-only correction is installed as SELF
`a5d09afdcda739167bfaac5768eb046fd59b9aa055bfe9f35ddc939d967efba6`;
the owner confirmed that spectators are correct and animated. Audio and ball
corrections are unchanged in this build. The startup preparation screen was
also confirmed visible during the preceding manual validation.

The final VPK SHA-256 is
`078508848c2b32ae7669d171ba13c4aa2b65202aed56e4893a9ce4b5fe18e891`.
Its packaged `eboot.bin` equals the SELF read back from the Vita. The normal INI
SHA-256 is `aac80bdf22392a11167a11dd9f713ffcff4088d3e9c898f9e6e50f5314620759`.
That INI was restored and verified byte-for-byte, and a normal restart was
requested successfully. There was no new dump in the subsequent observation.
Diagnostics are disabled in that restart; it is not an independent visual menu
or FPS measurement. Source, package and test identities are recorded in
`final-build-identity.json`; earlier identity files describe earlier candidates.

## Runtime change

The Vita loaders now require converted native graphics. The sole configured
archive is `ux0:data/strikersVita/sms.psarc`; graphics, native audio and native
video all use `asset_archive`. The previous secondary archive selector and its
graphics-sidecar reader have been removed.

Logical `.glg` / `.glt` names remain stable for scene scripts, but their retail
contents are replaced. `NSMD0001` carries little-endian model arrays, indices,
streams, material/animation state and reference matrices. `NSSK0001` carries
native skin, bone, morph and stitching data; software-vertex float normals are
preserved without quantization. `NSTB0001` is the texture dictionary and
`NSTX0001` contains decoded RGBA mip levels and engine material metadata.
Embedded menu/font images are converted and their resource directories rebuilt.

The Vita graphics loaders reject retail payloads. The archive also carries a
version marker checked before mounting the disc filesystem. The linked Vita ELF
contains no retired model conversion functions. Desktop retains its existing retail loader path. The two shared reader units
compile with C++20 without changing the rest of the desktop game dialect; the
full desktop game build is outside this validation.

File textures upload through the existing single VitaGL owner during loading.
Only metadata and GPU ownership remain resident on the CPU. Both texture and
resource marker retirement evict their owned GPU textures. Dynamic surfaces,
light ramps and generated effects retain their existing update paths. Expanded
asynchronous texture bundles use correctly sized owned buffers instead of
writing into the source game's fixed-size staging buffers.

## Prepared data and trade-offs

| Converted data | Coverage |
| --- | ---: |
| Model files | 149 |
| Texture bundles | 164 |
| Texture occurrences inside those bundles | 1,738 |
| Resource bundles containing converted images | 38 |
| Embedded image occurrences | 770 |
| Unique native texture requests | 1,809 |
| Native PCM records | 22,977 |
| Native movies | 17 |

The resulting archive is **1,791,284,489 bytes**. SHA-256:
`699670609b952510615d869b344c18bda6b7b1f0cce8868e0248b9204cec220d`.
Its larger size reflects predecoded texture/audio data. Graphics, movies and
control files use stored blocks. Prepared PCM/index records use optional zlib
blocks and include decompression when loaded. Model and texture conversion
preserves the source values; H.264 video conversion uses the selected lossy CRF setting.

No source GLG/GLT payload or obsolete graphics sidecar is retained as a fallback.
Unused GameCube executables, debug symbol files and boot code are omitted.
Game scripts, animation/control metadata and save identity remain where the
game still consumes them. Original compressed audio remains an input to exact
PCM validation and stream control; this is not a claim that every non-graphics
retail file has been removed or converted. Live geometry, pose, material and
draw preparation also remain runtime work.

## Host and package verification

- All 44 host tests pass, including the actual native parsers, conversion
  variants, malformed bounds, texture checksums, default INI and installation
  verifier behavior.
- The actual C++ consumer scans every model and texture bundle in the complete
  generated PSARC. All 149 models, 1,738 bundled texture occurrences, 38 resource
  bundles and 770 embedded images pass. The consumer discovers resource dictionaries by
  structure and rejects any original embedded texture, including `.Dmn`.
- Every converted mip is compared against the production decoder before
  publication. The packer checks prepared-tree/archive SHA-256 identity.
- ASan/UBSan checks pass for the native graphics consumer over the complete
  archive, the production VitaGL driver and the production archive verifier
  using an SDK mock.
- Normal Vita build reuses `build-vita-gxm-latest`; profiling and profile
  autostart are explicitly off. VPK `eboot.bin` equals the generated SELF.
- All 30 VitaGL compilation units retain `READBACKS_SPEEDHACK=1`,
  `HAVE_SHADER_CACHE=1`, `NO_DEBUG=1`, `DRAW_SPEEDHACK=1` and
  `STORE_DEPTH_STENCIL=1`. No `glFinish` / `glFlush` call is linked.

The normal INI profile is `2026-10-15-vitagl-unified`, with `vita_test_match=0`,
`vita_frameskip=0`, `fixed_dt=0`, native audio/video enabled and diagnostics off.
There is no second archive selector. The existing 60 FPS target and simulation
timing behavior are unchanged. Host and package checks establish no FPS gain.

## Hardware delivery and corrected startup

The archive has been transferred to a separate staging filename, leaving the
accepted build/data pair recoverable. The delivery verifier checks all 1,709
blocks with native DMAC5 SHA-256 against a host-verified manifest. Its whole-file
hash field is the expected manifest identity; it does not represent a separate
whole-file digest computed on Vita. The small manifest itself is read back
byte-for-byte before use. This optional verification mode exits after checking
and adds no archive scan to normal startup.

The initial archive passed its byte verification but the first normal startup
crashed in animation lookup before `GLInventory::Create()`. The matching dump
and build were retained; lookup now returns no animation until initialization.
A second startup reached the menu but failed while sizing a missing button
texture. Independent inventory identified two unconverted `.Dmn` containers
(MainUI and InGameUI), containing 168 images. Resource discovery now uses the
bounded container header instead of a filename-extension allowlist. A real
compiler regression test covers `.Dmn` and an arbitrary future extension.
The C++ consumer rejects the initial archive and accepts the corrected archive.
Both manual recoveries and fallback SELF/INI identities were verified.

The corrected archive replaces those two resources and zeroes their obsolete
source payloads. Every other logical entry is byte-verified against the previous
pack (24,384 unchanged entries). Inactive device data was updated using tested
positioned FTP writes; every changed range was read back exactly. A complete
new native checksum pass completed: all 1,709 chunks match.

The owner confirmed a complete menu with regular audio, then confirmed that
characters, field, goals and HUD look correct in a manual match. Follow-up fixes
for audio stutter, the rolling ball and crowd textures are accepted as detailed
below. The final diagnostic run loaded native models/HUD/textures without a GPU
error. Its status at frame 1,860 records 342,582 draws and 70 rejected packets;
unsupported line strips (primitive 5) remain a known renderer limitation. The
rejected count is not zero. No quiet gameplay performance measurement was made.

An unrelated dump was initially misattributed, then identified as process
`PHGV00001` before any rollback. Dump monitoring now reads thread/process
identity rather than relying on a new `eboot.bin` dump filename.

## Startup display, audio and ball corrections — 2026-10-16

The embedded-font startup display is restored and was confirmed visible on
hardware with SELF `d8e9f438a6b766dddaf8834dc55618e09a620f40ac90662ce1529fc99275eefc`.
It shows actual startup stages and shader-family progress; it requires no
archive assets. It remains allocated until both current and queued scanout
have replaced its buffer. The initial attempt to capture a manual match was
interrupted by an owner-confirmed manual restart, not a Strikers crash.

That run exposed a 751,697 us wall-time mixer invocation. This includes lock
waits, processing and native record loading; it is not an isolated disk-time
measurement. Native PCM loading now uses a bounded background cache;
the mixer uses a nonblocking cache lookup and retains its exact existing
ADPCM decoder while a cold record is pending. The change does not imply that
all source audio/control inputs have been removed.

The first asynchronous-audio candidate (SELF `b5f541a62b3e01fc6cd6a3c424174e3999a6c3084b8ad4cae18006e6bf12f4b8`)
crashed in its new worker. The matching dump identifies `SMSVITA01` and a
`pthread_cond_wait` / `pthread_mutex_unlock` fault. Vita libstdc++ weak gthread
activation compiled out the new standard mutex initialization/locking.
Explicit pthread synchronization replaces those standard wrappers; startup
screen initialization moves into the existing pre-archive memory bootstrap,
after the SDK thread runtime is ready. The host concurrency test covers a
deliberately stalled archive read and verifies that mixer calls return without
waiting for it. Manual recovery was completed and the fallback SELF/INI were
independently read back before installing the correction. The failed build's
matching symbols and attributed dump are retained.

The corrected worker performs record I/O, zlib decompression and validation
outside the cache lock. The mixer uses a try-lock lookup and does no archive
I/O or cache allocation. The queue is bounded to 64 requests, bindings to 128
and cached PCM to 2 MiB. The worker runs on CPU0 below audio output priority.
On a cold or mismatched record, the existing ADPCM decoder remains available;
each cached frame is checked against raw bytes, coefficients and history.

The owner confirmed regular audio in the corrected manual match. In the later
crowd-validation run, the last retained diagnostic audio line reports 9,000
output buffers, zero reported underruns, output error zero and maximum mixer
wall time 22,272 us. These observations come from different diagnostic windows
than the earlier 751,697 us invocation; they are not a matched quiet A/B or
proof of a gameplay FPS gain.

The rolling-ball code supplies five motion-blur passes with constant alpha
of approximately 10–32%. The renderer previously applied constant RGB but
ignored that alpha for diffuse-only materials. The correction preserves
explicit-alpha priority and the lit material's separate contract. The actual
GPU-driver mock checks the submitted alpha uniform. The owner confirmed that
the ball is correct while rolling.

## Animated crowd correction — 2026-10-16

The crowd material samples the stable `idle/idle` texture handle. Stadium
bundles can use different dictionary entry IDs. The source game's shared
staging-buffer updates previously hid that distinction; owned native loading
left the stable handle pointing to its empty placeholder, producing black
rectangles.

Both state changes and replay loading now remap the native atlas entry to the
stable handle. The asynchronous request captures its remap callback at
submission, so restoring the global callback cannot change a pending request.
The native bundle admission test covers handle remapping, preserved image
alpha, skipped entries, failed admission and malformed bounds. No archive
reconversion was required. The owner confirmed: spectators are correct and
animated on the final installed SELF.

## Remaining validation limits

The diagnostic run also reported a native intro-movie decode with no output
picture. That decoder behavior predates this storage-path change; movie
playback is not validated by the successful menu/manual match and remains an
open issue. Unsupported line strips remain as recorded above. The target is
still 60 FPS with independent simulation timing and no frameskip. This delivery
establishes no new FPS result.

Evidence: [identities and device records](evidence/UNIFIED_NATIVE_2026-10-15/).
Reproduction: [native asset pipeline](../smstrikers-port/tools/NATIVE_ASSETS.md).
