# Native material, character and HUD implementation in progress

Current device comparison SELF:
`ccfe94c1521d8e5667a74d8471d0304bcbb4b992e5ff8c7338d20149f2505517`.
Static meshes, UV residency and CPU/PN dynamic model snapshots are installed.
Texture sampling, complete materials, HUD/EFB passes and full Aurora removal
remain incomplete. Three same-SELF quiet pairs establish a 40.221% frame-time
reduction for within-draw vertex compaction: 6.286 → 10.515 FPS in the heavy
live-play window. Full evidence and final runtime configuration are recorded
in [the performance review](GXM_NATIVE_PERFORMANCE_REVIEW_2026-10-13.md).

The following sections preserve the earlier source-only milestones. They are
historical implementation checkpoints; later delivery supersedes their
statements that the candidate or UV sidecar was not installed.

## Earlier source checkpoint


The installed device artifact is still the static vertex-color preview:
SELF `032ab9474cfd8dbc04955309854f75b19ba6ca9d21770444efa17df586ed7134`,
mesh catalog `9018aaef5ba70b2c462eb4085b1fa03e0d8126eed9df216cc7107aa882c19888`.
The normal INI and fresh `visual_debug=0` restart were verified. The user's
latest device feedback confirms missing characters, textures and HUD. No
visual/FPS acceptance is claimed for the source work below.

## Implemented source contracts

- `native_texture_record.hpp/.cpp`: Strikers-owned AVNR request/path/reader,
  exact source-byte verification, XXH64 payload verification, mip/stride/
  format/extent validation and owned upload data. All 763 real texture records
  in `sms-native-gxm-static.psarc` passed host validation (760 RGBA, 3 BC1).
- `native_match_uv.hpp`, `build_native_match_uvs.py`: immutable float3 UV0
  stream, matched by parent mesh payload hash and all three source hashes.
  All 4,779 meshes have UV0. Sidecar 7,222,100 bytes, SHA-256
  `b9578b3b76208a5b8edef97a082abfbfc0b15b66a958c72885dfc2e8a6e5ef94`.
- `native_gxm_renderer`: optional textured vertex/fragment programs,
  32 raster/blend/write-mask variants, immutable texture allocation with
  64-MiB budget, point/linear/mip controls, clamp/repeat admission, RGB tint,
  alpha override, greater alpha test and x2 RGB. Initial C++ VitaSDK syntax
  passed. Mock driver covers texture draw plus every UV/program warm-up and
  texture allocation failure. These are driver/lifetime tests, not shader
  compiler or pixel proof. Ordinary preview initialization is unchanged.
- `native_model_vertices.hpp`: typed array snapshot for immediate HUD vertices,
  CPU-skinned arrays and rigid indexed-PN matrices. Inputs are original
  host-order model indices, bounded typed arrays and explicit bone selectors;
  no GX opcode decoder. Output owns position/RGBA/UV/normal and triangle indices.
- `port/native_assets.hpp` no longer includes an Aurora descriptor header.

## Original producer/GPU backlog (historical checkpoint)

1. Load the UV sidecar during native preload, warm the texture shaders and upload
   immutable UV bytes on the existing GPU owner.
2. Resolve PlatTexture source metadata, request the exact native record, upload
   once, cache with explicit texture/resource lifetime invalidation. Preserve
   immutable source copies and reject invalid/unsupported data before queueing.
3. Connect typed material uniforms/samplers and native texture handles to model
   jobs. Keep job snapshots within FIFO transport capacity or queue an owning
   job pointer with correct release on enqueue failure and owner completion.
4. Capture original model indices at `dlMakeDisplayList` creation **before** GX
   encoding; capture stitch selectors from `ShaderSkinMesh::StitchModel` typed
   `stitchArray`, rather than rereading PN bytes from a GX display list.
5. Allocate bounded dynamic GPU frame storage. Fence reuse before overwriting
   data consumed by the GPU. Patch vertex layouts for the owned model vertex.
6. Capture `GLUD_Skin` bone matrix rows (selector = reg + 99), current CPU-skinned
   arrays, immediate `indexBuffer==0` HUD arrays and view transforms. Render
   dynamic models natively; add exact clip/viewport/scissor handling.
7. Complete material semantics beyond single-texture unlit: lights, additional
   texture layers, EnvDiffuse, EFB/shadows and ordered HUD/effects. Basic texture
   submission alone cannot establish material correctness.
8. Build only in `build-vita-gxm-latest`, preserve a single `.previous` set, stage/
   verify exact SELF plus sidecars, run INI-controlled visual captures, restore
   INI, then verify characters, textures and HUD separately. FPS is a separate
   quiet acceptance gate.

At the original source checkpoint no new candidate had been installed. `validation.json` in the prior
renderer evidence directory identifies the earlier installed preview; source
hash differences are expected while this implementation is in progress.
Items 1, 4, 5 and 6 below have since been partially or fully implemented as
described in the continuation section; this numbered list is retained as the
original work plan, not an updated outstanding-task list.

## Continuation: dynamic GLG model producer and resident GXM snapshots

The direct match bridge now captures the *original* host-order u16 index
sequence at permanent `dlMakeDisplayList` creation, before the pointer is
replaced by a GX display list. The new bounded
`native_model_sources.hpp` registry tracks the DL handle, compiled data
address, marker lifetime, original topology, a new generation on pointer
reuse, and an owned index vector. It is invalidated when a GLG resource marker
is rewound. After `ShaderSkinMesh::StitchModel`, the typed
`stitchArray[packetIndex]` selectors are captured as PN matrix slots
3,6,...,27; no GX command buffer is decoded at draw time.

`native_match_direct.cpp` now routes admitted packets containing a PN stream,
CPU-skinned positions from the transient frame heap, or immediate indexed-by-
vertex HUD positions through `prepare_model_vertices`. The new route owns
position, color, UV, normal and triangulated u16 indices. For indexed PN, it
reads the original `GLUD_Skin` typed 3x4 transforms and applies each packet's
selector before enqueue. The FIFO transports an owning pointer to the
immutable snapshot; the GPU-owner callback releases that snapshot after
copying it into GPU-mapped memory. Inline-callback serial zero is handled
without double-free. Missing or ambiguous geometry is rejected; there is
still **no GX fallback**.

`native_gxm_renderer` now has bounded, three-slot dynamic vertex/UV/index
storage (1 MiB / 512 KiB / 512 KiB per slot). It copies 20-byte position/color
and separate 12-byte UV streams into GPU-owned storage and submits native
`sceGxmDraw`. On reuse of a slot that contains prior dynamic draws, it uses
the conservative `sceGxmFinish` fence. This is a correctness-first
implementation and its synchronization overhead has not been measured.
Temporary CPU model arrays are never referenced asynchronously.

Native preload can now read
`ux0:data/strikersVita/gxm_native_match_uvs.bin`, verify its parent mesh
payload hash and per-record source identities, upload the UV stream on the
GPU owner, and compile/patch the existing textured programs. Absence is
nonfatal, producing the old vertex-color preview. A present but incompatible
UV sidecar fails closed. At that source checkpoint no sidecar delivery was recorded. The subsequent
deployment and present device identity are recorded below.

Host verification at that source checkpoint:

- Host registry test, original vertex snapshot test, UV contract test: PASS.
- Source/vertex tests under AddressSanitizer and UndefinedBehaviorSanitizer:
  PASS (LeakSanitizer disabled on macOS).
- Production native renderer + simulated GPU/display driver: PASS, including
  dynamic-copy independence from CPU buffer reuse, triple-frame rollover fence,
  invalid indices and allocation failure cleanup.
- VitaSDK cross-compiler `-fsyntax-only`: PASS for the native match bridge,
  native renderer, skin stitching, GLX display list, GLX memory and GLX
  producer. The skin stitching source was checked with the newly added
  per-source feature flag.
- Shared Vita CMake configure-only: PASS. The regenerated compilation database
  confirms `STRIKERS_VITA_NATIVE_MATCH_DIRECT=1` on `gluSkinMesh.cpp`
  and both native feature gates on the direct match bridge.
- No full ELF/SELF/VPK build, device install, visual correctness run, FPS
  measurement, commit or push was performed.

Remaining blockers for correct gameplay: resolve the current Strikers
`PlatTexture` and sampler state into exact native texture records, create and
invalidate GPU texture handles, choose `draw_textured` for real material jobs,
and implement the missing lighting/TEV/multiple layers, alpha-test admission,
native viewport/scissor, EFB shadow passes, overlay material semantics and
animation corner cases. The dynamic path currently submits **vertex-color
preview** geometry; characters/HUD and textured materials have not been
visually validated on hardware. A successful host/mock test is not evidence
of playable FPS or complete rendering.


## Installed dynamic preview and performance continuation

The later device installation at 21:39 installed SELF
`a866c5b4351e8b5bf95b4351f5508a4d542d108ddd7f92a23f734f6479bcc232`
with the full 4779-mesh catalog and matching 7,222,100-byte UV sidecar
`b9578b3b76208a5b8edef97a082abfbfc0b15b66a958c72885dfc2e8a6e5ef94`.
This supersedes the source-only checkpoint above. The running producer can
submit CPU/PN dynamic geometry, while the report explicitly still says
`texture_samples=0` and `texture_binding=not_connected`. Those draw counts
establish neither characters/HUD pixels nor correct material rendering.

The first fresh quiet capture for that SELF, diagnostics/overlay/native visual
counters/debug menu disabled, covered live-play frames 600–899 at actual
444 MHz CPU / 222 MHz GPU. It measured 10.228 elapsed FPS, 97.772 ms mean,
41.032 ms median, 166.988 ms p95 and 178.277 ms p99. It contains intervals
around 6 FPS and intervals around 26–28 FPS, so comparisons must preserve the
same workload and frame window. Older GX captures are historical context,
not a matched regression result for this SELF.

Current source adds an opt-in `gxm_native_slot_fence=1` retirement path. A
fragment notification from EndScene certifies completion of the exact old
frame slot before any CPU overwrite; newer GPU scenes can remain in flight.
The original global `sceGxmFinish` path remains default 0. Optional
`gxm_native_timing=1` counters collect producer snapshot preparation, GPU-owner
upload/draw, slot-fence and display-queue costs. They are attribution only;
producer waits overlap owner work and must not be summed as frame time.

Nine native host tests pass. The driver test covers delayed notification
completion, completed-slot fast path, wait failure, missing notification
region, CPU snapshot lifetime, old Finish control and error cleanup. The
shared Release Vita build passes with profiler/autostart/fast profiling OFF.
Quiet same-SELF hardware comparison and final restored device state are
recorded in `GXM_NATIVE_PERFORMANCE_REVIEW_2026-10-13.md`.


## Producer snapshot compaction after diagnostic attribution

The slow live-play interval measured native producer snapshot preparation at
129–131 ms/frame, versus 3.6–3.7 ms/frame for the conservative Finish fence.
`gxm_native_compact_vertices=1` now retains the original indexed topology
inside each owned snapshot and decodes/transforms a `(source index, PN slot)`
only once. It preserves every expanded triangle attribute bit against the
old path in host tests. It does not reuse pose/color/UV across draws or frames,
drop normals, change materials, batch/reorder draws or weaken source bounds.
The full shared Vita build and nine host tests pass; renderer/full-catalog
and builder ASan/UBSan tests pass. The installed compact comparison SELF is
`ccfe94c1521d8e5667a74d8471d0304bcbb4b992e5ff8c7338d20149f2505517`.
Hardware comparison remains recorded separately in the performance review;
no texture, full visual or 60 FPS acceptance follows from this change.
