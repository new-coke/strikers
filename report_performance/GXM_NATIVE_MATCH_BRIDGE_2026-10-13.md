# Strikers Vita — live-game bridge to GXM (experimental)
13 October 2026

## Intent

The requested outcome is a real Mario-vs-Luigi match rendered by directly
submitted native GXM draw packets, with preconverted GPU assets and zero GX
translation/fallback. The previous standalone 60 Hz test was NOT gameplay.
The existing regular match with gxm_direct_probe=0 still used GX-to-GXM.

This branch is a strict, partial real-match integration step. It does NOT yet
render the complete game; unported draws are dropped with counters.

## Code and data

CMake opt-in: STRIKERS_VITA_NATIVE_MATCH_DIRECT=ON.
Shell build flag: STRIKERS_NATIVE_MATCH_DIRECT=ON.
Runtime INI: gxm_native_match_direct=1; use vita_test_match=1 and
gxm_direct_probe=0 to run the game's real CPU-vs-CPU simulation.

Compile flag is restricted to src/Game/main.cpp, src/NL/glx/glxSend.cpp,
src/NL/glx/glxSwap.cpp, src/platform/native_match_direct.cpp and
extern/aurora-vita/lib/gx/fifo.cpp. Aurora's backend worker dispatch was also
updated to name its native callback route explicitly. The other GXM shader
and resource sources retain the previous compile configuration. No VitaGL
path was added.

tools/build_native_match_meshes.py produces a versioned SMNVM001 index
from AVNR v2 records. It carries original GLG source identity (compiled
DL vertex byte stream and position/color source arrays, count/primitive)
plus prepacked GPU position-f32x4 / color-RGBA8 vertex buffers and U16
triangle indices. No GX vertex decoding or canonical packing occurs in
the draw loop.

Host-built manifest: ab-artifacts/native-gxm-direct-20261013/
gxm_native_match_meshes.bin. Contains 640 static meshes, 90,452 vertices,
214,731 indices (71,577 triangles), 2,271,806 bytes.
SHA-256: de156eb435eab8f09e85f1843c6104c7d993c6a1652aea4cb779ec980dd41f9c.

The private asset sidecar has been uploaded and checked by FTP at
ux0:data/strikersVita/gxm_native_match_meshes.bin, without changing the
installed SELF or INI.

All 640 selected AVNR source vertex byte streams were independently checked
against the exact per-vertex repeated-index encoding in dlMakeDisplayList;
640/640 match its indexed GX attributes (without a direct PN index).

include/port/native_match_manifest.hpp bounds-checks and hashes the pack.
include/port/native_match_direct.hpp and
src/platform/native_match_direct.cpp implement one-time GXM buffer/pipeline
uploads after the full game Initialize() completes (including glStartup,
GXInit and the GXM owner-worker startup), but before gameplay. Enabling
strict GX suppression earlier would prevent GXInit from configuring the
runtime. The 640 meshes share
ONE packed vertex buffer and ONE packed index buffer with aligned per-mesh
slices instead of 1280 independent GPU allocations. During gameplay,
glx_SendFrame_cb forwards packet/view and direct matrix information to
the bridge. A typed DrawPacket is submitted on the GXM owner via ordered
async callbacks; unique per-instance uniform revisions prevent stale MVP.
Source lookup uses a sorted raw-hash index plus full-identity equality, not
a linear scan of all 640 meshes on every draw. All source-array hashes are
still recomputed per draw for safety; there is no trusted immutable-generation
cache yet.

Both simple and Hitz (the game's default) swap modes now use a renderer-GXM
display copy on the owner thread without GXCopyDisp/GX state setters. Hitz
no longer waits for an absent GX draw-done token. Its VI swap/retrace path
is retained. Legacy game screen/movie captures are NOT implemented here.

For unsupported materials the bridge does NOT execute glx_DrawPacket or the
legacy GX path. The Aurora FIFO worker rejects GX command buffers BEFORE
decoding them. Additionally, ordinary GX runtime callbacks such as GXCopyTex
and GXCopyDisp are suppressed under strict mode; GXM frame lifecycle,
presentation, profiling and draw callbacks are explicitly routed through
run_sync_native/run_async_native. This is not the same as removing all GX
API callsites from source: unported target/copy and other legacy code still
generate commands that are counted and discarded.

Coverage status is recorded at:
ux0:data/strikersVita/gxm_native_match_status.csv.
Counters include native_draws_queued, native_draws_accepted,
native_draws_rejected, unsupported_material, unsupported_raster,
unsupported_view, unsupported_geometry, missing_asset, upload_errors,
queue_errors, present_errors, gx_suppressed_bytes, and
gx_suppressed_callbacks. Accepted means GXM did not report a renderer
failure; it is not a substitute for visual gameplay validation.

## Coverage gaps (critical)

SUPPORTED:
- Existing real game simulation and CPU players, audio.
- Up to 640 offline static GLG-native meshes with stable source identity.
- Per-instance model/view/projection matrices as GXM typed uniforms.
- Unlit vertex-color materials with the default glossiness state (value 1).
- The active-texture mask, texconfig, is authoritative; disabled texture
  slots contain 0xffffffff and are NOT treated as active samplers.
- Native depth, culling, color write and standard alpha-blend variants;
  alpha-test, uncommon blend modes and polygon offset are rejected.
- Ordered worker submission with both GX FIFO and legacy GX callbacks
  explicitly suppressed under strict native mode.

NOT IMPLEMENTED:
- Textured materials, all required TEV combinations, nonstandard blending,
  alpha test, polygon offset, lighting and material glossiness variants.
- Skinning/PN palette and the live player animation mesh families.
- Native shadow views, EFB render-to-texture and pass ordering.
- HUD, 2D overlays, movie/video and final compositing.
- Removal of all legacy GX call sites from the entire engine.
- Hardware image matching, completeness and full-game FPS testing.

The resulting incomplete native graphics CANNOT be represented as a
fully playable equivalent to Strikers; visual elements may be missing.
No fallback is silently substituted. The default runtime flag stays OFF
so a compiled binary remains backward compatible with ordinary GX-to-GXM
unless the explicit INI gate is enabled.

## Validation and remaining work

The native manifest was successfully parsed on host: 640 meshes and a
corrupted checksum is correctly rejected. VitaSDK syntax checking passed
for the bridge, Hitz/simple glxSwap, glxSend and main. The strict
native/legacy callback version passed a complete Vita ELF link and SELF/VPK
conversion on 13 October 2026. The final artifact includes the follow-up
corrections moving preload after Initialize()/GXInit and using texconfig
instead of testing disabled-texture sentinel pointers.

FINAL VERIFIED BUILD (local, not deployed; 2026-10-13 09:38 Europe/Rome):

- `smstrikers-port/build-vita-gxm-latest/strikers_vita.self`
  SHA-256 `06fb6568eefd7455139c6c1fa35c015cb11769f2577f84f39cf3bba4b8a326aa`
  (3,619,922 bytes).
- `smstrikers-port/build-vita-gxm-latest/strikers_vita.vpk`
  SHA-256 `08a8d160c94065439ceea77f62d9a7beeb0a249ee7bcb709f86637b6bba1f73f`.
- Embedded `eboot.bin` is byte-for-byte identical to the SELF. The archive
  passed integrity checks and includes `sce_sys/param.sfo`.
- Symbols verified in ELF: `port::directmatch::{preload,on_packet,present_native,shutdown}`,
  `aurora::gx::fifo::{run_sync_native,run_async_native,set_native_match_strict_gx,
  native_match_suppressed_gx_callbacks}`.
- The compilation timestamps of `src/Game/main.cpp` and
  `src/platform/native_match_direct.cpp` objects are newer than source.

Host validation: all 38 Aurora Vita contract tests passed (2026-10-13,
sequentially limited to 2 jobs); vita_cpu_workers passed in 0.8s when rerun
outside a heavily concurrent cross-build. The pack hash/bounds rejection,
git diff --check, Aurora nested diff --check, and shell syntax checks passed.
The GPU geometry pack also passed a full 640-mesh slice/offset/alignment
walk: 1,809,040 vertex bytes and 429,462 index bytes; no source position
record exceeds the runtime's 1 MiB hashing guard (maximum 227,052 bytes).
Standalone GXM hardware test from an earlier build measured 60 Hz for
ONE mesh; not evidence of playable-match performance.

The current worktree remains uncommitted. No SELF/VPK install or INI change
has occurred for this new build. The private 640-mesh manifest was uploaded
in the prior stage; its device readback matched SHA-256 at that time. Only
a real-hardware run with `gxm_native_match_direct=1` and
`vita_test_match=1` can establish native coverage or reveal runtime issues.

The next engineering tasks are to bind native materials/textures from
asset precompilation, export indexed PN/skinning uniforms, handle EFB
shadows and viewport passes without GX, and only then remove all GX
entrypoints. Maintain a fail-closed policy throughout.

No commits or push. Do not deploy an incomplete renderer over the user's
working Vita SELF without a separate backed-up validation decision.
