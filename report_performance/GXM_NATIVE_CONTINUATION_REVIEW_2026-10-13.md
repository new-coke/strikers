# Strikers Vita - native GXM continuation review (2026-10-13)

**Later device state supersedes this static-preview checkpoint.** The installed
SELF is now `ccfe94c1521d8e5667a74d8471d0304bcbb4b992e5ff8c7338d20149f2505517`,
with dynamic CPU/PN geometry, resident UVs and validated within-draw vertex
compaction enabled. Three same-build quiet pairs measure 6.286 → 10.515 FPS
in the heavy live-play window; one presented-frame comparison is pixel-exact.
Textures, full materials, HUD/EFB passes and full Aurora removal remain
incomplete. See [the current performance review](GXM_NATIVE_PERFORMANCE_REVIEW_2026-10-13.md)
for the exact installed/config identity, hardware evidence and remaining work.
The original review below preserves the earlier implementation milestones.


## Visual regression diagnosis after user hardware feedback

The user confirmed that the standalone native candidate produced audio with a
black scene and depth-like white/gray shapes. Draw submission/progression was
never sufficient evidence of visual correctness. The visual gate is **FAIL**.

A real transform bug was corrected: the model/view matrix uses NL's column
layout, whereas GLX forwards the projection in Dolphin's row layout and GX
consumes only six coefficients. The previous native path multiplied those raw
layouts directly. `native_match_transform.hpp` now reconstructs precisely the
same perspective/orthographic coefficients and composes a column-layout MVP.
An independent oracle uses the actual `glxCopyMatrix`, `nlMultMatrices` and
Dolphin projection constructors; **1,153 comparisons** and the near/far
regression pass. The native shader's depth conversion remains unchanged.

The renderer now supports an opt-in synchronized presented-frame capture.
Readback runs on the GPU owner between frames, fences the GPU and display
queue, verifies the current scanout pointer/format/extent against its own color
surface, then copies pixels. It cannot substitute the depth surface or a stale
Aurora render target. Host tests cover ownership, frame boundaries, delayed
callbacks, foreign scanout rejection and driver readback failure. Normal play
performs no capture fence or copy unless selected through the INI.

The projection-only SELF `37b48995bfac299abfbf09ae2ff1f061532a55ec842f680100ca25d09f9e3dff`
was installed and verified by full readback. A live-play-frame-120 capture
shows **511,684 / 522,240 RGB-black pixels (97.9787%)**, only 13 distinct RGB
colors, a white ball and edge geometry. This proves a real missing-scene
problem in the presented **color** buffer; it does not prove full game
correctness or FPS. Projection repair alone did **not** fix the visual defect.
The temporary snapshot INI was restored and reread with the original hash
`7db19fd763528598cc11757a2f1b7918b324f8cf21e0f00d2c2e2440d8d1164f`.

Known omissions remain material: the native fragment shader samples zero
textures; indexed-PN/skinned characters are explicitly rejected; shadow/EFB,
alpha-tested materials, HUD and several geometry/pass variants are absent.
The static mesh sidecar also selects only 1,800 entries in hash order. The
source archive contains 3,755 compatible records before deduplication,
requiring 14,013,308 bytes for the full static preview. Thus the current
8-MiB/1,800-record cap is not a complete live-game asset set.

`gxm_native_visual_debug=1` selects bounded per-view admission/drop counters,
first rejected raster/program/user-data bits and a missing source identity.
It is OFF by default; this diagnostic establishes attribution, never FPS.
The normal source path retains its strict material/geometry admission rules.

The shared `.previous` set now contains the matching 1,800-mesh coverage build.
Older non-finite-MVP validation and installation records remain as small
historical evidence; there was no hardware coredump for that validation exit.
Current binary/package/installation hashes and source identities are in
`report_performance/evidence/NATIVE_GXM_RENDERER_2026-10-13/validation.json`.

### Corrected static mesh coverage

The per-view diagnostic run at frame 1,680 established concrete admission
failures. In Shadowed (view 3), 181,872 candidate packets had no mesh in the
sidecar; 81,848 were rejected by the user-data contract and 29,021 by raster
admission. Characters (view 11) separately rejected 40,773 skinned packets;
its non-skinned source misses and other exclusions are not skinning support.
The sampled `GLUD_EnvDiffuse | GLUD_NoFog` user-data mask was `0x00008040`.
Skybox (view 2) also had an exact source identity absent from the truncated pack.
None of the queued sampled packets used cull-all or disabled RGB writes.

A second asset bug was found in `extract_gpu_color`: the old stress-scene
probe admitted only meshes with at least 20 vertices and 30 indices. Those
lower bounds were incorrectly inherited by the real match-sidecar builder.
The Vita's missing 27-index and 12-index sample meshes exist in the archive
but failed that probe filter. The decoder now admits valid triangles/quads;
the stress-scene selector retains its 20-vertex / 30-index minimum and
upper budget separately, while the match builder uses the
same 64,000-vertex / 192,000-index bounds as the runtime parser. Invalid
indices, checksum, truncation and non-finite positions are still rejected.

The corrected full static preview contains **4,779 distinct meshes** from
4,822 geometry records (**43 exact duplicates**, zero incompatible or
budget-rejected records), **589,095 vertices**, **1,425,687 U16 indices** and
**14,881,806 bytes**. Runtime/producer limits are explicitly bounded at
5,000 meshes / 16 MiB. The previous 1,800-mesh pack remains the canonical
rollback asset; no ELF/SELF/VPK set was copied into the evidence directory.
Exact missing identities sampled in views 2, 3 and 11 all resolve in the
new catalog. Shader/material/alpha/skinning omissions remain unchanged.

Six targeted CTest cases pass, including three tool regression cases for
small triangles/quads, distinct probe/match upper budgets, and malformed
GPU payloads. Both contract and production-renderer/mock-driver tests also
pass with ASan/UBSan against every one of the 4,779 real meshes. This is host
and artifact evidence; the corrected device capture follows separately.

### Installed full-catalog visual result

The corrected SELF `032ab9474cfd8dbc04955309854f75b19ba6ca9d21770444efa17df586ed7134`
and 4,779-mesh sidecar `9018aaef5ba70b2c462eb4085b1fa03e0d8126eed9df216cc7107aa882c19888`
were installed together and reread in full. VPK:
`004ffa8040bababe2bfa0988e51c5bbd1a5511ae9378f7fbae9e7ff82336257d`.
The new runtime reports the full catalog, frame **1,680**, **299,566**
accepted draws, **zero source misses**, no native driver/draw rejections and
zero upload/queue/present errors. These are coverage/error results, not FPS.

The synchronized live-play-frame-120 capture now has **642 / 522,240** black
pixels (**0.1229%**) and 37 distinct RGB colors. It shows a pale field surface,
edge geometry and a white ball. The missing-catalog blackout is corrected,
but **visual correctness still FAILS**: textures/grass/material colors,
animated characters, HUD and effects are absent. The change from mostly
black to a pale vertex-color preview is not a full native-material fix.
The current image is `debug_frame_play_120.png`; the earlier black image is
retained as `debug_frame_play_120.projection-only.png`.

The next visual implementation must add resident textures/UVs and native
material/shader contracts (including texture alpha tests), dynamic color
invalidation/EnvDiffuse and indexed-PN skinning. Aurora remains linked for
bootstrap, FIFO owner scheduling and source-write revisions. Removing it
now still does not constitute a complete native Strikers renderer.

The normal INI has been restored and verified. The targeted Companion kill
returned an error and a launch URI did not restart the existing capture
process. After the user closed Strikers from LiveArea, the same installed SELF
was relaunched and a fresh report verified `visual_debug=0`, the full 4,779
catalog and no native driver errors. `normal-launch-identity.json` and
`native-status.normal.csv` bind this verification. No global app kill or reboot
was substituted. The earlier capture is not a quiet-FPS run; all three shared
CMake profiler options remain explicitly OFF.

The following sections describe earlier implementation stages and artifacts.
Their device/acceptance statements must not be read as current visual proof.

## Corrective installation after first hardware run

The first installed native-owner candidate initialized successfully and
accepted **44,188 native draws**, then stopped at frame 352 with
`native_nonfinite_mvp`. This was the renderer's own validation rejecting
producer data, not a reported driver allocation failure. The original
status and artifact identities are preserved as small historical evidence.
Its earlier `.previous` symbol set was superseded by the later verified
projection-only build; this was a validation exit, without a hardware coredump.

The producer now validates the copied MVP before queueing the draw. Invalid
transforms are rejected and counted as `invalid_mvp`; the first frame/view
and all 16 component bit patterns are recorded for later source diagnosis.
The strict owner-side validation remains enabled. This prevents an invalid
packet from poisoning the renderer's sticky error state and stopping valid
draws. The upstream cause of the non-finite transform remains to diagnose.

NaN and positive/negative infinity checks for every matrix component,
mock-driver regression, 1,800-mesh ASan/UBSan checks, the **4/4 native CTest**
targets, full Vita rebuild and native binary isolation audit passed.
Corrected SELF installed and reread in full at 17:49 Europe/Rome:
`d8778c3441e6856dee4f3f2b390a5f2c5969277f1037eb25ed26e5bdfbe95927`.
Corrected VPK:
`2e79558c9aee20d36b8f3fc5a033bac430d92b0c4843bdf6d23ee71364001a43`.
The expanded catalog and INI are unchanged. Runtime verification of this
corrected candidate is recorded in the current `install-identity.json`.
The first failed run is retained separately in
`native-status.mvp-error.csv`, `validation.mvp-error.json` and
`install-identity.mvp-error.json` under the native-renderer evidence directory.

Corrected runtime readback confirms `native_match_v3` and
`renderer=strikers_native_gxm`: frame **3,960**, **202,839 queued/accepted
native draws**, zero renderer draw rejections and zero upload/queue/present
errors. Native error code is 0 and the reported error is `none`. **684**
invalid MVP packets were skipped before queueing; the first was frame 349,
view 11 (Characters), with all 16 components carrying quiet-NaN bits
`7fc00000`. This establishes native initialization/submission and
progression beyond the earlier stop, not complete visual correctness or FPS.
The current installed status is preserved in `native-status.installed.csv`.

## Device installation (2026-10-13, 17:31 Europe/Rome)

The candidate SELF and matching expanded 1,800-mesh catalog have now been
installed into the existing `SMSVITA01` application. Both files were staged
and read back before promotion, then read back in full from their final paths.
Verified `eboot.bin` SHA-256:
`dab57d27225a24986745952c2899a927cb3d3d1b7e9a15a5a5eab82091c5d46d`.
Verified `gxm_native_match_meshes.bin` SHA-256:
`f23352c248dd4ed8a291af619fbb9eca89ff805c7931804482186ac2aff62326`.

The INI was preserved byte-for-byte (SHA-256
`7db19fd763528598cc11757a2f1b7918b324f8cf21e0f00d2c2e2440d8d1164f`).
Its effective native-match flag is 1, standalone probe is 0 and automatic
test match is 1. Rollback reuses the existing `.previous.self` and original
640-mesh catalog; no complete build copies were added. Remote transaction
stage/backup files were removed after verification. Application launch was
requested through Companion; visual correctness and FPS remain unverified.
Detailed installation and launch evidence:
`evidence/NATIVE_GXM_RENDERER_2026-10-13/install-identity.json`.

## Latest update: Strikers-owned native GXM renderer

This update supersedes the earlier statement that no standalone GPU renderer
had been added. The historical review below describes the preceding tranche.
Aurora remains linked for bootstrap, FIFO scheduling and source-write revision
tracking; it no longer performs GPU upload, draw or presentation in the new
opt-in mode. This is not yet a complete equivalent native Strikers renderer.

Implemented:

- `include/port/native_gxm_renderer.hpp` and
  `src/platform/native_gxm_renderer.cpp`: a Strikers-owned renderer with no
  Aurora/GX types or symbol references. It owns GXM initialization/termination,
  the context and ring buffers, mapped GPU allocations, three framebuffer
  surfaces, DF32 depth, shader patcher/program storage and display queue.
- At native-match preload, the drained owner thread shuts down the GX draw
  sink and old renderer before initializing the new GXM owner. There is no
  overlap between two active GXM instances. If native initialization fails,
  startup stops with an error; it never reactivates GX. Suppression stays
  enabled until the compatibility worker shuts down.
- Two fixed vertex-color preview shaders compile during initialization;
  one vertex program and all 32 fragment blend/color-mask variants are
  patched before gameplay. Draws use resident mesh IDs, packed local U16
  indices, a copied per-instance MVP and typed raster state. No shader
  compilation, pipeline creation or geometry conversion occurs during draw.
- BeginScene restores all used context state every frame, including signed
  Y viewport scale. A native clear precedes the admitted main-view draws.
  Display-buffer rotation commits only after successful queue insertion.
- The real game loop uses native begin/end/present after handoff, skipping
  Aurora frame submission, cache transitions, renderer snapshots and readback.
  The Hitz barrier preserves FIFO ordering; native frames do not copy an EFB
  or submit a second Aurora present. GPU/display fences precede destruction.
- The existing source-registration, generation and dirty-range validation
  continue to protect immutable geometry. Dynamic PN/skin sources, custom
  scissor/viewport, EFB/shadow passes and unported materials remain excluded.

Build opt-in (default OFF):

```sh
cd smstrikers-port
STRIKERS_NATIVE_MATCH_DIRECT=ON STRIKERS_NATIVE_GXM_RENDERER=ON \
STRIKERS_GX_THREAD=ON ./build-vita-native-gxm.sh
```

Runtime remains controlled by `gxm_native_match_direct=1` in `strikers.ini`.
The real automatic match remains controlled by `vita_test_match=1`, with
`gxm_direct_probe=0`. The matching expanded 1,800-mesh pack must be supplied
as `ux0:data/strikersVita/gxm_native_match_meshes.bin` for the coverage test.
No SELF, sidecar or INI was deployed during this development tranche.

Validation:

- Port native CTest targets: **4/4 PASS**, including the production renderer
  with a mocked GPU/display driver compiled against real VitaSDK declarations.
- Injected failures throughout initialization, incomplete GPU upload, invalid
  local indices, failed display-queue insertion, owner-thread rejection,
  delayed callbacks, complete context reset and retirement before unmap:
  PASS. Driver simulation is CPU/lifetime evidence, not hardware evidence.
- ASan/UBSan: PASS for the contract and renderer/driver tests against all
  **1,800 real meshes**, including release of CPU upload bytes before draws.
- Existing Aurora host regression suite: **38/38 PASS**.
- ARM VitaSDK renderer syntax with warnings as errors: PASS.
- Legacy and Aurora-backed native-bridge main.cpp syntax: PASS.
- Full shared-directory Vita ELF/VELF/SELF/VPK build: PASS; VPK integrity and
  embedded SELF equality: PASS. Profiling options explicitly OFF.
- ELF/map isolation audit: **12,938 executable symbols**, native GXM draw and
  present, no GL/vitaGL/vita2d entrypoints or libraries.
- Native renderer object: **zero Aurora symbol references**. The bridge
  object retains only scheduling/revision/shutdown references; no calls to
  Aurora create_pipeline, draw or display_copy.

Final local SELF SHA-256:
`dab57d27225a24986745952c2899a927cb3d3d1b7e9a15a5a5eab82091c5d46d`.
Final local VPK SHA-256:
`076c900e6d2ea694dbfcee5de90769bb465da5ce0c4b210be28cbf23c311d90c`.
The existing `.previous` rollback SELF remains
`06fb6568eefd7455139c6c1fa35c015cb11769f2577f84f39cf3bba4b8a326aa`;
no new build directory or complete artifact backup was created.
Small verification evidence:
`evidence/NATIVE_GXM_RENDERER_2026-10-13/validation.json`.

Remaining gates: real-device native initialization/accepted draws and visual
inspection; resident texture/material/lighting/TEV support; PN skinning;
EFB shadows, HUD and other passes; independent scheduling/revision tracking
and bootstrap before removing Aurora from the link. No hardware visual,
stability or FPS gain is claimed for this candidate. Source remains local
and uncommitted.

## Checkout and hardware state verified

- Root: `/Users/robin994/Documents/Code/strikersVita`, branch `main`,
  HEAD `a175e0a1` (2026-10-11 17:44 local time).
- Aurora submodule HEAD `be08d453` (2026-10-11).
- The direct GXM bridge, native preview, manifest builder and associated
  changes are **uncommitted local work**, not a new main-branch commit.
- No standalone StrikersNativeGXM backend has been added. The game still
  depends on `aurora::vita::renderer()` and Aurora's owner/FIFO worker.
- Actual Vita `eboot.bin` read back by RETR has SHA-256
  `06fb6568eefd7455139c6c1fa35c015cb11769f2577f84f39cf3bba4b8a326aa`.
  The FTP SIZE command gave a misleading size; the full hash matches the
  existing 2026-10-13 09:38 SELF. Do not infer that a new build was installed.
- Hardware report is still `native_match_v1` (frame 2880, 1,046,314 callbacks,
  **0 native draws queued/accepted**, 1,003,335 rejected by the old material
  gate). Thus its black screen is *not* evidence of GXM throughput cost.

## Review: blockers ranked by severity

1. P0 - No complete native game render path: characters with indexed PN skinning,
   texture/TEV/material variants, EFB shadows, HUD and other pass effects are
   not implemented. The experimental vertex-color preview only approximates
   six 3D GLX program families. Unsupported packets are dropped, not passed
   to GX. It is not a visually correct Strikers match.
2. P0 - Current test artifact is stale: new 1,800-mesh expanded manifest and
   preview-material code are newer than the installed SELF. The Vita previously
   held the older 640-mesh manifest. A test must use a newly compiled SELF
   **and matching** `gxm_native_match_meshes.bin` sidecar; deployment was
   deliberately not performed in this continuation.
3. P1 - Renderer dependency: strict native mode suppresses old GX decoder
   batches and callback jobs, but GX APIs still execute on the game producer
   and the target/worker/renderer live in Aurora. An Aurora-free renderer
   does not exist; removing the library today would break GPU initialization,
   pipeline creation, uploads, render target handling and presentation.
4. P1 - Hot path still calculates hashes over each GLG display-list index
   stream and the position/color source arrays on every candidate draw, then
   searches by source identity. This is not GX->GXM vertex translation, but
   it is avoidable CPU work once immutable source lifetimes are tracked.
   Introduce an asset-registration-time binding and generation-safe cache,
   **not** unsafe pointer reuse; past bypasses have broken GPU resource lifetime.
5. P1 - Shader variants are created lazily for each raster state, while
   sealed precompiled-shader coverage is not proven for a full match.
   Native-preview draw completion and correctness are still unverified.
6. P2 - The old preload retained per-mesh std::vector capacities even after
   `clear()`; for the ~6 MiB expanded scene this is wasteful on Vita.

## Implemented in this continuation (no Vita build)

- Added `include/port/native_match_gpu_assets.hpp`: a backend-neutral,
  portable `PreparedGpuAssets` containing immutable source identities,
  sorted hash lookup, compact slice descriptors and two contiguous CPU
  upload arrays (vertex / U16 index); no Aurora/sceGxm handle types.
- The preparation stage releases each decoded mesh vector as soon as it is
  copied into the shared upload arrays. After synchronous GPU upload, the
  bridge releases the *entire* packed CPU upload arrays while preserving
  identity/offset metadata for future draws.
- Updated `src/platform/native_match_direct.cpp` to consume that shared
  backend-neutral store instead of retaining `sManifest.meshes`,
  `sRawHashIndex` and `sGpuMeshes` separately. GPU resources are still
  managed by the existing Aurora GXM owner until that layer is replaced.
- Added `tests/native_match_gpu_assets_test.cpp` plus CMake registration.
  The test verifies packed offsets, local U16 indices, hash collision safety,
  absent identities, and lookup after releasing CPU geometry.

## Validation

- Portable C++ host test: PASS using synthetic records.
- Full real expanded AVNR catalog: PASS, **1,800 meshes, 258,528 vertices,
  207,333 triangles**; validates every source identity, slice boundary and
  index offset in the packed upload buffers.
- VitaSDK ARM C++20 `-fsyntax-only` of updated
  `src/platform/native_match_direct.cpp`: PASS. This is **not** a Vita
  link, SELF/VPK conversion or device test.
- Existing Aurora host CTest suite: **38/38 PASS**.
- `git diff --check` in root and nested Aurora tree: PASS.
- No automatic Vita build, installation, INI change, push or commit.

## Next engineering sequence

1. Bind source-registered native GPU asset IDs when static GLG resources
   are loaded, with explicit lifetime/generation invalidation. Avoid per-draw
   source-content hashing while protecting mutable/animated models.
2. Convert material and texture descriptors offline; load resident Vita
   texture handles; compile required GXM shader variants before the first
   game frame. No GX material interpretation and no fallback.
3. Add native indexed-PN/skinning with per-character animation palette,
   then separate shadow, EFB, pass ordering, HUD and frame presentation.
4. Implement `StrikersNativeGXM` as a full renderer owner separate from
   Aurora, including scene lifetime, buffers, shader patcher, depth/EFB,
   display queue and fences. Remove Aurora only once the real match path
   no longer needs its frontend or resource scheduler.
5. Complete a *fresh* hardware coverage run with correct SELF/manifest:
   native accepted vs rejected draws, view and program/material coverage,
   GPU errors, visual results and genuine match FPS. No 60 FPS claim from
   the one-mesh probe or from a zero-draw black screen.

The current change is a safe preparatory slice, **not** a playable fix.
