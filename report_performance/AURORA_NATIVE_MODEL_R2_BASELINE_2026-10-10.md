# Aurora-Vita native models: R1 hardware baseline and R2 entry gate

Date: 2026-10-10. No Vita build, deployment or push performed.

## Hardware acquisition

Downloaded the R1 log, performance CSV and frame CSV directly from
`192.168.1.79:1337/ux0:/data/strikersVita/` into
`ab-artifacts/native-model-registry-20261010/device-r1-results/`.
The performance capture contains 300 sampled frames.

## Observed performance

Mean frame time: 74,673 us (13.39 FPS reciprocal of mean frame time).
Mean draws per sampled frame: 268.2.
Mean phase durations: draw frontend 31,263 us; state translation 6,682 us;
vertex decode 3,235 us; command build 2,887 us; geometry cache 2,125 us;
texture resolve 2,121 us; fixed uniform build 758 us.
Phases can overlap or nest and must not be summed as independent costs.

Native-model cumulative counters from first to final sample:

| Counter | First | Last | Delta |
| --- | ---: | ---: | ---: |
| attempts | 20,961 | 51,491 | 30,530 |
| native draws | 928 | 3,600 | 2,672 |
| fallback | 20,033 | 47,891 | 27,858 |
| compiled | 20,033 | 47,891 | 27,858 |

The native draw fraction of attempts over the sampled interval is 8.75%.
These are cumulative statistics, not per-frame measurements. `compiled`
tracks capture/fallback in this sample and must not be interpreted as
30,530 persistent GXM mesh compilations.

All `native_cache_*` counters were zero because the deployed build only
activates cache event accounting under `STRIKERS_GXM_NATIVE_MODEL_CENSUS=1`,
while the device INI had `gxm_native_model_census=0`. Zero means **no cache
telemetry**, not no cache hits. Thus cache-hit rate, miss breakdown and
producer/consumer rejection reasons are NOT measurable from this run.

## Controlled diagnostic correction

Aurora now accepts `STRIKERS_GXM_NATIVE_MODEL_METRICS=1` separately from
the mode-1 census, retaining the original census switch as an alias for
compatibility. Strikers exposes `gxm_native_model_metrics=0` in managed
defaults and its example INI. Both default to OFF; no render-state changes.
Counters take a mutex per event and must be used for diagnostic runs only,
not unbiased performance A/B.

## R2 design and admission gates

1. Compile immutable geometry (layout + source revision + index payload) and
   material/pipeline identity once, on the correct owner thread. Retain
   independently bounded GPU residency and invalidate on source-generation
   changes; never treat registry identity as proof of GXM residency.
2. The reusable recipe stores no camera, skin matrices, XF lights, scissor,
   render target, EFB-copy result or transient texture handles. Resolve
   dynamic texture/TLUT bindings and build/publish per-instance uniforms
   at submission time. Cache pipeline and immutable shader variants only.
3. On validation failure, execute pinned GX commands in the same FIFO slot.
   Preserve scene barriers, source pin lifetime, pipeline pin and async
   command ownership. Never enable streamed GPU vertex mode.
4. First improve the rejection distribution and actual native success rate
   on repeated static draws. CHARACTERS/SHADOWED need skin/TEV-aware
   equivalence tests before wider admission; do not loosen guards blindly.
5. Compare identical scene, clocks, warmup, sample interval and diagnostic
   settings against GX-only. Visual equivalence and crash-free hardware
   tests are mandatory before enabling native mode by default.

The existing `submit_native_model_recipe` already reuses a prepared packet,
geometry and pipeline key, but still translates current fixed-vertex state,
resolves live textures, builds/publishes uniforms and validates many guards.
Simply adding another producer cache cannot remove those costs. The measured
frontend time is not attributable exclusively to native recipes without an
equal-content GX baseline and cache-rejection breakdown.

Current change is a diagnostic prerequisite, **not R2 complete**.

## Hardware follow-up: associative cache + incremental uniforms

Acquired after the user's launch on 2026-10-10. Hardware CSV/log snapshots
are in `ab-artifacts/native-model-registry-20261010/device-r2-run/`.
Each performance capture contains 300 rows; `frame_us` below is the mean
consumer performance-capture duration, **not** the separate game frame CSV.

| Metric | R1 initial (ring) | R2 metrics-only INI | R2 cache+uniform |
| --- | ---: | ---: | ---: |
| Mean frame time (ms) | 74.67 | 76.08 | 80.34 |
| Reciprocal mean frame time (FPS) | 13.39 | 13.14 | 12.45 |
| Mean draws/frame | 268.2 | 281.9 | 294.2 |
| Native attempts (counter delta) | 30,530 | 63,605 | 34,525 |
| Native draws (counter delta) | 2,672 | 4,590 | 23,080 |
| Native draw/attempt (%) | 8.75 | 7.22 | 66.85 |
| Native fallback (counter delta) | 27,858 | 59,015 | 11,445 |
| Mean state translation (ms) | 6.68 | 6.92 | 5.91 |
| Mean command build (ms) | 2.89 | 2.98 | 2.17 |
| Mean geometry cache (ms) | 2.12 | 2.14 | 1.56 |
| Mean vertex decode (ms) | 3.23 | 5.74 | 6.08 |

The workload is **not paired** (different draw counts and global frame
positions), so FPS changes are descriptive only, not causal evidence of a
native-cache performance regression. Both R2 game-frame captures reported
`actual_cpu_mhz=444 actual_gpu_mhz=222` (requested CPU 500 MHz fell back to
444). `display_queue_us` is ~32 us, with CPU/frontend-bound classification.

In the cache+uniform sample, 344/354 geometry misses occurred within the
first 20 samples. Vertex decode averaged ~41.3 ms during that window, then
~3.6 ms after sample 20. FPS calculated from the remaining performance
frames was ~12.93. The cold-start/geometry-compilation behavior should be
isolated from steady-state comparisons; removing the first 40 samples also
gives ~12.93 FPS. Native draw admission increased dramatically without
eliminating per-instance texture/uniform/GLX work.

**Installed binary was verified directly on hardware** by streaming
`ux0:/app/SMSVITA01/eboot.bin` into SHA-256. Its digest is
`612ddb81ef6106479e6502d9a98d02ef60589f7742515edc762050dab39d28c6`,
exactly the earlier R1 build. It predates the locally added
`STRIKERS_GXM_NATIVE_MODEL_METRICS` switch, explaining why all
`native_cache_*` counters remain zero even though the INI says
`gxm_native_model_metrics=1`. Do not interpret zero events as zero hits.
The existing R1 binary can record those events only with
`gxm_native_model_census=1`, which adds mutex accounting and must be kept
separate from any quiet FPS comparison. Alternatively the user may manually
build the new source when ready.

The log contains no observed fatal error or allocator-failure diagnostics;
this does not establish image equivalence or long-run crash freedom.
Do not install/build/push without the user's explicit request.
