# Strikers / Aurora-Vita R3-A: early static producer bypass

Date: 2026-10-10
Status: opt-in source implementation and host tests; Vita hardware validation pending.

## Motivation and prior measurements

The 2026-10-09 quiet A/B showed GX -> GXM around 20.265 FPS versus
native recipe + cache around 15.0 FPS (about 34% more mean frame time).
Although native-recipe admission rose sharply, mode 2 reached the native
registry *after* the original GLX state switching and draw-time setup.
The existing persistent GXM geometry cache and pipeline packet therefore
did not remove enough producer work to establish any FPS benefit.

native-preparation, documented in `NATIVE_PREPARATION_2026-10-10.md`, added permanent
GLG source-generation tracking, owner-level retirement, and reuse of
current native material CPU snapshots. The async `shared_ptr` fix from
`AURORA_VITA_R2_GTHREAD_COREDUMP_2026-10-10.md` is in the same source
checkout but still requires hardware confirmation.

## R3-A implemented scope

This tranche introduces a **new earlier callsite** for the existing,
separately guarded native recipe; it does **not** pretend to perform
unconditional native GXM submission from the main CPU thread.

- `aurora-vita/lib/gx/native_model_options.hpp`:
  `STRIKERS_GXM_NATIVE_MODEL_EARLY=1`, disabled by default.
- `aurora-vita/lib/gx/native_model_registry.hpp`:
  `NativeModelEarlyFacts` plus a conservative admission contract.
  `NativeModelRegistry::dispatch_prepared` does an exact semantic lookup,
  requires a nonzero GLG source generation and an already captured recipe,
  flushes pending GX only on an eligible cache hit, and sends the same
  ordered FIFO native-model segment. A miss leaves the original callback
  untouched. This remains a backend-independent registry interface.
- `smstrikers-port/src/NL/glx/glxSend.cpp`:
  before the state-switching branch of `glx_SendFrame_cb`, attempt the
  early dispatcher only when all conditions below hold. The original
  draw callback runs unmodified for failures. Success preserves the
  post-draw GX current-matrix reset in FIFO order.
- `aurora-vita/lib/gx/native_model_cache.hpp`:
  optional `producer_early_*` event counters. These use the existing
  diagnostic mutex and must stay OFF for unbiased quiet performance A/B.

### Admission is intentionally extremely narrow

1. `gxm_native_model_draw=2`, `gxm_native_model_prepared=1` and
   `gxm_native_model_early=1`; running async GX worker required.
2. Exactly a *draw-only* GLX callback (`flags == 0x800`).
3. Existing permanent GLG display list, valid streams, nonzero current
   asset generation, and a **previously captured recipe** with an exact
   descriptor/material/texture identity (hash alone never matches).
4. Existing static view `GLV_UnsortedPerspective`, unchanged view,
   unlit `prog_3d_unlit`, identity model matrix, current program and
   streams already bound.
5. No userdata, GLX dirty state/texture work, or temporary draw modifiers
   (translucency, fog override, co-planar, viewport, constant colour,
   diffuse overrides, no-rasterized-alpha).
6. The GX consumer still independently validates pinned source, GXM
   residency, shader, pipeline, material/TEV, texture/TLUT/EFB bindings,
   geometry revisions/layout and current uniforms. On failure it replays
   the original pinned display list in that same FIFO slot.

**Non-goals**: dynamic transforms/skin, CHARACTERS, SHADOWED, TEV indirect,
GX resource scheduling, or compiling every GPU pipeline at GLG load time.
GPU resources still get prepared at the first valid GXM owner-thread draw.
R3-A is an incremental producer bypass experiment, not R3 complete.

## Risk controls

- No change to renderer selection (GXM only), native GXM allocations,
  geometry budget, stream vertex policy or GPU/CPU synchronization.
- Existing `NativeModelRegistry` handles source retirement, and queued
  recipe/source references retain their lifetimes independently.
- Exactly same FIFO order and original GX fallback as existing mode 2.
- Defaults are OFF; unknown states can never use the shortcut.
- Runtime counters distinguish early attempts, accepted hits, rejected
  source/material admission and queue rejection.

## Host verification (2026-10-10)

- Focused Aurora host CTest: 13/13 PASS (registry, caches, textures,
  prepared pipeline, submission).
- R3-A registry test: positive warmed recipe; negative dirty state,
  different view/program/streams/matrix/userdata/modifier, missing
  source generation, missing material variant and retired source;
  flush callback executes only on a known cache hit.
- Vita managed INI test: PASS for default OFF and override ON,
  stale defaults refresh and first-user-value-wins behavior.
- `git diff --check` in both worktrees: PASS.
- These checks cannot prove real GPU image equivalence or performance.

## Vita build and validation

A separate candidate build directory avoids touching
`smstrikers-port/build-vita-gxm-latest` (the older crash-symbol ELF).
The requested Vita build uses CMake with GXM, async GX ON and
`STRIKERS_VITA_GXM_DIRECT_STREAM_WRITE=OFF`.
Its artifact identifiers and audit outcome are recorded after completion.

Recommended hardware sequence, only after user-requested installation:

1. Baseline with the same new SELF, `gxm_native_model_draw=0`;
   confirm stable gameplay and no recurrence of the async reference crash.
2. Recipe control with `gxm_native_model_draw=2`,
   `gxm_native_model_cache=1`, `gxm_native_model_uniform_build=1`,
   `gxm_native_model_prepared=1`, `gxm_native_model_early=0`.
3. Change only `gxm_native_model_early=1`, same assets, clocks, seed,
   scene, geometry budget, warmup and sample window.
4. Diagnostic run (separate from FPS): check `producer_early_attempt`,
   `producer_early_admitted`, `producer_early_rejected`, fallback GX,
   texture/pipeline guards and scene ordering. If zero early admissions,
   **do not** treat that as a performance optimization.
5. Compare 960x544 images at several gameplay positions, HUD and EFB
   interactions, plus prolonged gameplay. Disable the early flag to
   roll back just this experiment.

No device INI change, Vita installation, Git commit or push is part of
this tranche. More substantial R3 geometry/material loading and R4 full
producer bypass still remain open.

## Completed Vita test build and ELF audit

An **isolated full Vita Release build** completed successfully on
2026-10-10 (GCC 15.2.0, GXM native, GX worker ON, direct streamed GPU
vertex OFF). Existing `build-vita-gxm-latest` artifacts were not modified.

Artifacts in `smstrikers-port/build-vita-gxm-r3a-20261010/`:

| File | SHA-256 |
| --- | --- |
| `strikers` (ARM ELF with symbols) | `5d701d384b7be6ada9993338b28d10d0c3453c5bca3b3cb55a45a94cfaacfe82` |
| `strikers_vita.self` | `760a4c7d64656d8b4f4edf66c22846aeb9c6812ac1fab6cde76ff21ee6bcdbb1` |
| `strikers_vita.vpk` | `efaecf1120537dcec87172c9ee3e7ecd5e16e5157d0c44ca8428479da3573401` |

The VPK `eboot.bin` hashes **identically** to the standalone SELF.
The ELF is 32-bit ARM; `sceGxmDraw` and `sceGxmEndScene` are present.
An audit of symbols found no VitaGL, vita2d or GL draw entry points.
R3-A option and `producer_early_*` counter strings are linked into the
binary. Worker symbol: `aurora::gx::fifo::vita_worker_main` at
`0x8123057c` (link-time VMA, not device crash relocation).
The linked worker assembly contains ARM `LDREX/STREX` and `DMB`
instruction sequences, unlike the previous unsafe single-thread
reference-count path; this remains subject to hardware stability testing.

Source compatibility:
- Full Aurora host CTest **37/37 PASS**; focused registry/cache/texture/
  submission **13/13 PASS**.
- Vita managed INI default/override test **PASS**.
- Root/submodule `git diff --check` **PASS**.
- Build and CMake logs: `ab-artifacts/native-model-registry-20261010/r3a-test-build/`.

The linker emitted legacy MusyX warnings about enum width and `wchar_t`
EABI attributes. It returned success, but those warnings are a residual
toolchain/ABI risk to investigate independently. This build is **not**
proof of runtime image equivalence, FPS improvement or crash freedom.
The old R1 SELF was re-hashed intact:
`612ddb81ef6106479e6502d9a98d02ef60589f7742515edc762050dab39d28c6`.

## Post-audit R3-A test build with co-planar state correction

The `glx_DrawPacket` reference implementation calls `gxSetCoPlanar(glx_CoPlanar)`
unconditionally before each display-list draw, even when no GLX dirty flag is
set. The R3-A early producer initially skipped that state restoration. In
`src/NL/glx/glxSend.cpp`, the warmed-recipe dispatch flush callback now calls
`gxSetCoPlanar(false)` **before** `GXFlush` and the native FIFO segment. The
early gate already rejects any callback with a co-planar modifier. On a
producer cache miss, neither operation executes and the original callback
remains the fallback. This prevents stale GX co-planar state from a previous
draw from leaking into the native material guard or its GX fallback.

The pre-audit R3-A ELF/map/VELF/SELF/VPK were preserved unchanged in
`ab-artifacts/native-model-registry-20261010/r3a-test-build/before-coplanar-fix/`.
An incremental **Vita** build of CMake target `strikers_vita.vpk-vpk` completed
successfully using the existing isolated `build-vita-gxm-r3a-20261010` tree.

| New artifact | SHA-256 |
| --- | --- |
| `strikers` (debug ELF) | `0a519085f747d77a32331fde7cb1397648add8994af5084d32fb540ec7aaa20e` |
| `strikers.velf` | `125ed99a83f4a01f8c4426a2c1497a60fcf140930f11b941e90f6e9b2a04981e` |
| `strikers_vita.self` | `55897b45ca677e9c62c236bed4f41231c73a0451481043b8b57ce14e18ead1b5` |
| `strikers_vita.vpk` | `b4c8197199f9619b91ea477509f54137c288ae679929acb9f0c239df1e0ffc54` |

The VPK `eboot.bin` is byte-identical to the new SELF. Debug ELF remains
32-bit ARM, includes `sceGxmDraw`/`sceGxmEndScene`, and has no VitaGL/GL
draw entry points. The worker `vita_worker_main` at link-time address
`0x8123057c` contains 14 `ldrex`, 14 `strex`, and 17 `dmb` instructions,
consistent with the atomic `shared_ptr` fix. The R3-A early-mode flag and
counter strings are present in the new binary.

Host Aurora CTest eventually passed **37/37** together, after removing
concurrent Vita build pressure. An earlier parallel run was 36/37 and an
isolated `vita_cpu_workers` retry also failed due its mock semaphore's
3-second timeout while external compilations were running; after the CPU
load decreased the isolated test passed, then the entire suite passed
37/37. Do not silently erase this load-related flakiness. Root and Aurora
`git diff --check` pass. Legacy MusyX linker warnings concerning `wchar_t`
and enum ABI remain a separate risk; the toolchain returned successful
link, SELF and VPK creation.

Logs: `ab-artifacts/native-model-registry-20261010/r3a-test-build/`
(`build-after-coplanar.log`, `host-final-all-20261010.log`). The console's
installed SELF was checked read-only after the build and still hashes to
`612ddb81ef6106479e6502d9a98d02ef60589f7742515edc762050dab39d28c6`.
No Vita deployment, INI change, git commit or push was performed.

This is a candidate **for hardware testing**, not a validated speedup.
Compiling all mesh/material assets at GLG load time, dynamically transformed
models and complete Characters/Shadowed bypass remain future R3/R4 work.

## Hardware Test A: prepared=1, early=0 (first run)

After the R3-A SELF was installed, Test A was configured on hardware with
`gxm_native_model_prepared=1`, `gxm_native_model_early=0`, native model draw
mode 2, native cache and uniform builder enabled, and cache metrics on.
The running INI retained these values after the app rewrote its managed
defaults section (only an additional blank line was observed).

The device wrote `ux0:data/strikersVita/r3a_prepared.log` (68,055 bytes,
SHA-256 `9e066c0285dea15e924e616b37206ef83fb96bbafedfddfae504e485d20f605a`).
Read-only recovery saved a copy at
`ab-artifacts/native-model-registry-20261010/device-r3a-test-a-results/`.
The log confirms native GXM initialization, `[match] begin rematch=0
kickoff=0`, and display/present telemetry reaching `n=512`. No fatal or
allocator error was logged. The last available CPU/frontend-bound display
profile reported `cpu_frame_avg_us=77192.6`; **this is not an eligible
gameplay-frame FPS measurement**. One `scene_budget_exceeded scenes=6 budget=5`
warning was observed. The device had no new `psp2core` entry beyond the
earlier R2 crash, but this alone does not prove full-run stability.

**Acquisition incomplete**: neither `r3a_prepared_perf.csv` nor
`r3a_prepared_frames.csv` exists. In the current source,
`port::capture_completed_performance` returns unless
`g_pGame->IsGameplayOrOvertime()` and only writes its CSV after 60 eligible
warmup frames plus 300 eligible snapshots. `PortBenchFrameEnd` likewise
requires both match-active and play-active, then 60+300 eligible frames
before writing. A match-begin log and 512 total presents are insufficient
to establish 360 qualifying gameplay iterations. The run must not be
counted as an FPS/geometry/material speedup or slowdown.

Next step: repeat **Test A with the current INI unchanged** until the game
reaches live gameplay and 360 eligible play iterations complete. Check both
CSV files appear. If gameplay is visible for substantially longer yet files
remain missing, investigate the gameplay-state capture gate. Do not enable
R3-A `early=1` or draw a performance conclusion before obtaining this baseline.
No code, Vita binary, or device INI changes were made during this inspection.

## Follow-up: completed Test A and Test B

Both capture pairs completed. Early producer attempts and admissions remain zero even with early mode enabled in Test B.

Hardware snapshots: `ab-artifacts/native-model-registry-20261010/device-r3a-test-a-complete/` and `device-r3a-test-b-results/`. Each performance CSV has 300 rows; the GXM frame-index windows differ (A 372-831, B 164-617), so FPS comparisons are not controlled or paired.

| Metric | A prepared=1, early=0 | B prepared=1, early=1 |
| --- | ---: | ---: |
| FPS from mean performance frame time | 11.34 | 7.07 |
| Mean frame time (us) | 88172 | 141449 |
| Median frame time (us) | 76997 | 81336 |
| Draws per frame | 286.44 | 253.55 |
| Mean frontend time (us) | 39880 | 86258 |
| Native attempts (counter delta) | 56813 | 60103 |
| Native hits (counter delta) | 34065 | 30394 |
| Consumer missing (counter delta) | 20947 | 28516 |
| R3-A early attempts/admitted (delta) | 0 / 0 | **0 / 0** |
| FPS for final 50 samples | 13.19 | 13.44 |
| Draws/frame for final 50 samples | 294.34 | 272.82 |

Test B has an extreme outlier at frame 253: `frame_us=9287906`, of which `pipeline_resolve_us=9148368`; a second frame 276 has `frame_us=1409466` and `pipeline_resolve_us=1254497`. The pipeline telemetry includes `Renderer::create_pipeline` on a cache miss. Geometry and vertex-decode spikes also occur. These events strongly skew average FPS; the final 50 samples are both near 13 FPS, with different draw counts. The two tests do not establish a regression caused by the early path because that path had **zero admissions**.

The frame recorder produced 300 live-play frames for each condition (A ~10.95 FPS, B ~6.86 FPS). Its final sample shows unusually large post-tasks time (A 883 ms, B 1237 ms). Main-loop performance CSV output runs before frame capture ends, so capture I/O is a plausible contaminant, not a GPU display-queue measurement. Device display-queue telemetry remains ~32-34 us with zero blocked samples and CPU/frontend-bound classification. No new `psp2core` file is listed, but graphics correctness and long-run safety are not confirmed by logs.

The source-level counter placement explains why zero early attempts is not sufficient to decide whether the runtime flag was ignored: `glx_SendFrame_cb` invokes `dispatch_prepared` only **after** `native_model_early_admissible` passes, but `ProducerEarlyAttempt` is counted **inside** `dispatch_prepared`. The predicate currently combines exact flags `0x800`, stable UnsortedPerspective view, 3d_unlit program, identity matrix, live-state matches, clean GX and no modifiers or userdata. Earlier hardware census measured ~0.69% UnsortedPerspective callbacks versus ~85% Characters/Shadowed. No individual early-gate rejection histogram was collected.

Next engineering priorities: (1) opt-in cheap producer pre-gate counters/rejection reason masks, without changing admission behavior; (2) investigate and prewarm/persist expensive GXM pipeline creation; (3) extend support only for model families with verified GX state/lifetime contracts, then repeat paired quiet A/B after early admissions become nonzero. Do not enable streamed GPU vertex geometry or relax skinning/TEV/texture guards merely for higher counts. No device INI/binary change, Vita build or Git push was performed during this inspection.

## Quiet GX/GXM control attempted (no completed capture yet)

The device was switched to a safe, read-back-verified **INI-only** baseline:
`gxm_native_model_draw/cache/uniform_build/prepared/early/metrics/census=0`,
`diagnostics=0`, `fps_overlay=0`, `gxm_native_gpu_static=1`, and
`gxm_streamed_vertex_gpu=0`. Native GXM and the R3-A executable, including
the atomic async reference-count fix, are unchanged. The planned quiet
capture is `ux0:data/strikersVita/r3a_quiet_gx.csv`, restricted to live
play and writing **only after 600 eligible warmup frames plus 300 samples**.

On the next user-reported run, the Vita FTP listing and an explicit `RETR`
check reported the target CSV **absent (FTP 550)**. The INI's first values
still match quiet GX, and the Vita regenerated its managed default block on
launch, confirming that the config file was processed. No new coredump was
listed beyond the earlier 2026-10-10 13:40 crash. `diagnostics=0` suppresses
the normal runtime log and consumer performance CSV, so there is no
alternative current-run FPS measurement. Code inspection confirms the
frame recorder itself is independent of diagnostics/overlay and is invoked
by the main loop, but it does not persist partial data; the result cannot
distinguish fewer than 900 qualifying play frames, a gameplay-state gating
problem, or file creation failure. **Do not report this run as recovered
or regressed FPS.**

The previously measured, matched quiet A/B remains the strongest evidence:
20.265 FPS for ordinary GX/GXM versus 15.098 FPS for native recipes/cache
with the uniform builder (same prior SELF, 600 warmup + 300 samples and
444/222 MHz clocks). User-observed ~25 FPS from the earlier build was not a
paired FPS capture. On any repeat baseline run, ensure 900 qualifying
live-play frames (not just launch/menu/presents) or deliberately choose a
shorter 60+300 capture, clearly marking different warmup conditions. No
further device INI/source/SELF changes were performed during this audit.
