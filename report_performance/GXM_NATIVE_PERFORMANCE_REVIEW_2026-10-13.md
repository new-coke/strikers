# Strikers native GXM performance review — 2026-10-13

The current renderer is a **partial vertex-color preview**, with native
static/dynamic geometry but no texture sampling, complete material lighting,
HUD/EFB passes, or complete Aurora removal. It has no GX fallback for dropped
packets. The user-reported poor frame rate is confirmed by a fresh quiet
hardware capture, not inferred from counters or the build.

## Current build result before the optimization

Installed SELF `a866c5b4351e8b5bf95b4351f5508a4d542d108ddd7f92a23f734f6479bcc232`,
full 4779-mesh catalog, matching UV sidecar, original native-direct INI.
Measurement overrides disable diagnostics, overlay, visual counters, debug
menu, task profiling and snapshots; test match/seed/fixed timestep are selected
by the INI. Actual clocks are 444/222/222/166 MHz CPU/GPU/bus/xbar, despite
requested CPU 500 MHz. The capture covers 300 consecutive live-play frames
600–899 after 600 live-play warm-up frames.

| Metric | Before optimization |
|---|---:|
| Elapsed FPS | 10.228 |
| Mean frame | 97.772 ms |
| Median frame | 41.032 ms |
| p95 / p99 | 166.988 / 178.277 ms |
| Frames within 16.667 ms | 0% |
| Frames within 33.333 ms | 2.333% |

Thirty-frame segments alternate between roughly 6 FPS and 26–28 FPS. A
previous GX CSV still present on the device is stale and is excluded. Historical
20–25 FPS numbers were measured on other rendering paths/artifacts and do not
establish the exact regression magnitude for this current incomplete preview.
The visual diagnostic capture measured 5.746 FPS but includes logs/counters
and a presented-frame readback; it is excluded from quiet comparisons.

## Measured attribution and implementation

A separate diagnostic run on the slot-fence build
`5df279f3ede2dab10022542218db3948a71252cb32fb2145ee374cfd9b543dff`
keeps global diagnostics disabled but enables the explicit native phase clocks.
It is attribution only. In source frames 1560–1800, per completed-frame deltas
show producer preparation at 129.246–131.218 ms/frame, the conservative GPU
Finish fence at 3.598–3.677 ms/frame, GPU-owner upload at 5.390–5.486 ms/frame,
and GXM draw binding/submission at 0.941–1.004 ms/frame. Producer waits overlap
owner work; these values must not be summed as total elapsed frame time.
The dominant measured native cost is rebuilding expanded indexed vertices on
the game thread.

The implemented `gxm_native_compact_vertices=1` experiment deduplicates only
within one immutable model snapshot, using original source index plus PN
selector. It transforms/decodes a repeated vertex once and remaps triangle
indices without changing triangle order, primitive conversion, pose, color,
UV, normal, draw order, filters or resource lifetime. The same source index
with two bone slots remains two vertices. There is no cross-frame geometry
reuse, unchecked source cache, or changed admission for unsupported materials.
The direct GXM upload now admits one/two unique vertices in a valid degenerate
indexed triangle, preserving the old triangle's empty pixel coverage.

An independent opt-in `gxm_native_slot_fence=1` implementation uses fragment
notifications to retire the actual dynamic slot before CPU writes. It retains
the global Finish control as default 0 and is **not part of the compact-vertex
comparison or a claimed FPS improvement**. Native timing is OFF for all quiet
runs. This experiment was deprioritized after the measured producer cost.

## Validation and current comparison

- Nine native host tests: PASS. Topology/attribute equivalence includes all four
  supported primitive types, repeated indices, distinct PN selectors, hash
  collisions, source index 65535, invalid/NaN data and degenerate triangles.
- Production renderer/driver and model builder under ASan/UBSan: PASS, including
  the complete 4779-mesh catalog. These establish CPU contracts, not pixels/FPS.
- Shared Release Vita build: PASS; profiler, autostart and fast profiling OFF.
- VPK embedded SELF equals the built SELF. The renderer object has no Aurora
  symbol references; the application still links Aurora for bootstrap, FIFO
  ownership and source revisions, so Aurora removal is incomplete.
- One matching `.previous` ELF/VELF/map/SELF/VPK set is retained in the shared
  build directory. No complete build products are kept in evidence.

The compact comparison uses installed SELF
`ccfe94c1521d8e5667a74d8471d0304bcbb4b992e5ff8c7338d20149f2505517`
for both modes, three alternating pairs (0/1, 1/0, 0/1), 120 consecutive
live-play frames 120–239 after 120 live-play warm-up frames. Content/seed,
fixed timestep, clocks, CPU topology and other renderer options are identical.
The only performance flag changed is compact vertices. All six quiet runs
completed with matching SELF/config identities and actual 444/222 MHz clocks.

| Metric (360 samples per mode) | Expanded control | Compact snapshot |
|---|---:|---:|
| Elapsed FPS | 6.286 | 10.515 |
| FPS range across three runs | 6.260–6.299 | 10.477–10.539 |
| Mean frame | 159.094 ms | 95.105 ms |
| Median frame | 163.179 ms | 97.012 ms |
| p95 / p99 | 165.649 / 177.846 ms | 100.483 / 111.237 ms |
| Within 33.333 / 16.667 ms | 0% / 0% | 0% / 0% |

Paired mean-frame reductions were 40.600%, 40.166% and 39.895%. Aggregating
samples gives **40.221% less frame time / 67.283% more elapsed FPS** in this
heavy window. This effect clearly exceeds the observed run variability. It
is a native snapshot optimization of the current incomplete preview, not
complete visual correctness, general whole-match FPS or a 30/60-FPS result.
The initial 10.228-FPS measurement used a later 600–899 window mixing fast and
slow phases; it must not be used as the control for this 120–239 comparison.
Temporary INIs are restored and verified after every run, and the Strikers
process is positively stopped before a new capture.

Evidence: `evidence/NATIVE_GXM_RENDERER_2026-10-13/performance/` contains source/
artifact identities, INIs, complete CSVs, analyses and installation journals.
The first diagnostic run's VELF/SELF/VPK were retained when its ELF/map were
accidentally overwritten during a failed preparation; this is recorded in
`rollback-preparation.json`. The next rollback rotation retained a complete
matching set and supersedes it. There was no hardware crash in these runs.

## Remaining development priorities

1. Complete the full rendering acceptance gate. The three quiet performance
   pairs and one exact presented-image comparison are complete.
2. Replace repeated typed model decode/CPU PN expansion with resident typed
   source geometry and GPU pose palettes, with explicit source/color lifetime.
   The old engine's weighted software skinning remains a separate cost.
3. Connect native texture records and exact material/sampler state. The UV
   stream and texture decoder/GXM programs exist but real jobs still report
   `texture_samples=0` and `texture_binding=not_connected`.
4. Complete lighting/alpha, HUD clipping/viewports, shadows and ordered EFB
   passes. White silhouettes and draw counters are not visual correctness.
5. Replace Aurora bootstrap/FIFO/revision services only after the producer and
   lifetime/pass contracts are complete. A backend-only change is insufficient
   to remove all game-side GX calls or reach 60 FPS.


## Presented pixels and normal delivery

A separate control/compact visual pair uses the same SELF, seed, fixed timestep
and live-play snapshot frame 120, with native phase timing enabled in both.
The captured presented 960×544 RGB buffers are **byte-identical**:
`12fcd79bc090fc2da242b371f8ab11e55dd8baf138635c282a132cd45f32a2c9`,
zero changed pixels and zero mean absolute RGB error. Both files verify the
native-owned front color surface after GPU/display completion. This confirms
one exact image-equivalence check of the snapshot optimization. It does not
repair the current white materials or missing HUD/textures. In the matching
source-frame 1560–1680 diagnostic interval, producer preparation fell from
128.787 to 69.403 ms/frame, and upload from 5.366 to 3.808 ms/frame. Thus about
70 ms/frame of this native producer cost remains even after compaction; the
next target is resident typed geometry/GPU pose rather than more broad state
switches. These phase clocks remain attribution only. The visual pair's
single-frame timing includes readback/PPM I/O and is excluded from FPS results.

The compact flag is enabled for normal device use by a single preserved
custom INI prefix; all other original settings remain byte-for-byte below it.
Native phase timing, visual counters, snapshots and the slot-fence experiment
remain OFF. The original normal INI (SHA-256 `7db19fd763528598cc11757a2f1b7918b324f8cf21e0f00d2c2e2440d8d1164f`)
remains available as the small restore file. Final artifact/config hashes and
fresh-process proof are recorded in
`evidence/NATIVE_GXM_RENDERER_2026-10-13/performance/normal-optimized-identity.json`.
No commit or push was requested or performed.


Final normal launch is verified on a fresh process at source frame 120:
SELF `ccfe94c1521d8e5667a74d8471d0304bcbb4b992e5ff8c7338d20149f2505517`,
normal INI `099f0e2bb971f521d66f086b778f5b53d9dcfb66edfe497ec6af2ac627e45e75`.
Runtime reports `compact_vertices=1`, `dynamic_slot_fence=0 timing=0`,
`visual_debug=0`, full mesh catalog 4779 and `native_error=0x00000000`.
Snapshots and global diagnostics are OFF. Other original settings, including
normal timestep/demo/debug-menu preferences, remain in the original INI body.
This normal-launch proof is progression/error evidence; the FPS results above
come from the separate sealed quiet configurations, with debug menu OFF.
