# Native GXM Fast Path — first static-geometry tranche (2026-10-08)

## Reason for the change

Strikers already packaged AVNR v1 geometry, but AVNR v1 stored 168-byte
`CanonicalVertex` arrays. On admission into Aurora's resident geometry cache,
the Vita still ran `pack_gpu_vertex_bytes` on each vertex.

The hardware profiling matters: the captured steady gameplay spent about
32.5 ms/frame in the broad GX processing scope and had about 229 geometry
cache hits per completed frame, **zero new geometry misses** in a representative
steady window. Converting geometry into final GPU format is useful on *cold
admission*; it cannot by itself remove the still-live GX game-side command
stream, material translation or submission in the steady frame.

## Implemented in this tranche

- Optional AVNR **v2** records, separate from AVNR v1. One record per exact
  source geometry contains source bytes **once**, shared indices, multiple
  explicitly serialized GPU attribute layouts and complete packed vertex
  arrays; output is bounded to 16 MiB of payload/32 MiB of total record.
- Host compiler `vita_compile_native_assets --gpu-static` builds both v1
  fallback and v2 geometry records from the same legacy GX request.
  `vita_prepare_assets.py --native-gxm-static` passes that mode through,
  storing v2 under `native/v2/geometry/<exact-source-hash>.avng`.
- The v2 profiles cover contiguous Tex0–Tex2, optionally Color0, Normal and
  indexed PN selector, **only if those source attributes exist**. Other layouts
  remain on the established path. The variant table is self-describing;
  runtime accepts an exact match only (component types, location,
  normalization, offset, stride, count).
- AVNR v2 source identity is NOT only a hash: the record repeats the original
  complete immutable source request, compares it byte-for-byte, validates
  the complete payload hash and record bounds, then verifies every index
  against the vertex count. Corrupt/mismatched records fall back.
- `StaticGeometryCache` can create GXM VB/IB directly from the chosen packed
  bytes without any intermediate `CanonicalVertex` array and without the
  `pack_gpu_vertex_bytes` CPU loop, **on a cache miss only**. Existing tracked
  source revisions, memory snapshots, admission budget, LRU, GPU retirement,
  scene/command ordering and matrix/lighting/TEV setup are unchanged.
- Default-off INI: `gxm_native_gpu_static=0`. This never enables streamed GPU
  vertex mode, disables a view/shader, or changes the game update timestep.
  `native_gpu_geometry_hits` and `native_gpu_geometry_attempts` are
  appended to the diagnostic consumer CSV as cumulative counters.

## Preconditions / meaningful expectations

1. `gxm_native_assets=1` without `native_asset_archive` falls back to
   `sms.psarc`. The original 595 MiB game PSARC contains **zero** native
   records. The old balanced native sidecar had 4,822 AVNR v1 geometry and
   763 native textures, **but no AVNR v2**.
2. Enabling the new flag without rebuilding/selecting a v2 sidecar will cause
   failed v2 lookups followed by the unchanged original code; it should
   not improve frame time.
3. This tranche is NOT direct GX draw-packet bypass. Game animation updates,
   TEV material state, producer GX FIFO and GXM submission still execute.
   No claim of 60 FPS or even measurable steady-state FPS improvement is made.
4. The v2 lookup is only attempted for stable display-list-backed geometry
   with no separate source index array, no point/line expansion, and an exact
   live GPU layout. Native GPU geometry is then managed by the same cache.

## Preparing the candidate sidecar

Compile the HOST asset tool (not a Vita build):

```sh
cd /Users/robin994/Documents/Code/strikersVita/smstrikers-port/extern/aurora-vita
cmake --preset vita-host-tests
cmake --build --preset vita-host-tests --target aurora_vita_compile_native_assets
```

One way to generate a new sidecar from a verified, personally owned source
`sms.psarc` while preserving original files is:

```sh
cd /Users/robin994/Documents/Code/strikersVita
python3 smstrikers-port/tools/vita_prepare_assets.py \
    --profile strikers \
    --source-archive ab-artifacts/asset-pipeline-20261005/sms.psarc \
    --native-only --native-textures all --native-gxm-static \
    --native-compiler smstrikers-port/extern/aurora-vita/build/vita-host-tests/aurora_vita_compile_native_assets \
    --output ab-artifacts/native-gxm-static-20261008/sms-native-gxm-static-only.psarc
```

A separate pack can retain the complete previously validated balanced sidecar
(media/shader/geometry/texture), adding only AVNR v2 geometry records. Source
archives and generated proprietary game assets remain local, ignored by Git.

### Completed all-resource candidate (local Mac, 2026-10-08)

The previously readback-verified balanced pack was upgraded without
re-encoding audio or video. The resulting PSARC and **all 12,687 records**
were verified against their materialized sources.

- Candidate: `ab-artifacts/native-gxm-static-20261008/sms-native-gxm-static.psarc`.
- SHA-256: `3d2d10cd625555b0208127aa60b8cd5507261909482b2842b4a8b44f92585c37`.
- 280,013,735 bytes (267.04 MiB) with 12,687 verified entries.
- All 7,865 entries from the prior balanced sidecar preserved; original
  SHA-256: `d39d0d4cf51ba0141fa61b77e37259d97cb4b7dacf16168946bba517679d0bb4`.
- New AVNR v2: 4,822/4,822 immutable GLG source geometries compiled,
  zero rejects/skips. 4,727 records have 8 GPU-layout variants, 66 have 12,
  and 29 have 16. Maximum record size 2.08 MiB, p95 123.3 KiB.
- **Coverage limitation:** none of these static GLG records has a per-vertex PN
  selector, so no indexed-PN layout (GXM attribute location 14) was generated.
  This does **not** precompile the dominant skinned CHARACTERS draw family;
  an animation-aware native draw-packet path will be needed separately.
- This pack was subsequently **installed on PS Vita** at
  `ux0:data/strikersVita/sms-native-gxm-static.psarc`, with a full 280,013,735
  byte FTP readback and SHA-256 identity check. The new SELF was installed
  (SHA-256 `a48acd5f7e434d2226665dddacd28f0fa41c47a624d61b4eecbeb2e3d604471a`).
  The Vita INI was checked after update: `gxm_native_gpu_static=1`, with
  the correct sidecar path, A3=0, A4=0, frameskip=0 and fixed_dt=0.
  The older binary/INI/sidecar remain backed up on the device.
- **Hardware result reported by the user:** played successfully, FPS
  remained essentially unchanged (~25). This is not a controlled OFF/ON
  steady-frame benchmark. No 60-FPS or gameplay perf gain is claimed.
- AVNR v2 source/code changes in the local working tree have **not been
  committed or pushed**; deploying local generated data is not a git push.

## Hardware gate: first validate the baseline

The Vita binary must be rebuilt **manually by the user**:

```sh
cd /Users/robin994/Documents/Code/strikersVita/smstrikers-port
STRIKERS_GX_THREAD=ON ./build-vita-native-gxm.sh
```

For the first boot, use the same SELF with `gxm_native_gpu_static=0`, then
try `=1`. Set `native_asset_archive` to the new installed candidate
PSARC, keep `asset_archive` pointing at the original `sms.psarc`, and use:

```ini
asset_archive=ux0:data/strikersVita/sms.psarc
native_asset_archive=ux0:data/strikersVita/sms-native-gxm-static.psarc
gxm_native_assets=1
gxm_native_gpu_static=0
gxm_uniform_delta_upload=0
gxm_a4_fragment_opt=0
vita_frameskip=0
fixed_dt=0
gxm_streamed_vertex_gpu=0
```

Do not change the stable `gxm_disable=0x8` until its reason is separately
reviewed. First check startup, gameplay, characters, shadows, colors, depth,
goal/replay, no crash and shader correctness. Repeat with `=1`, full restart
and the *same* pack and SELF. For diagnostics, collect cumulative
`native_gpu_geometry_hits` and attempts **across startup and level load**.
For frame timing, disable diagnostics and use paired 600-frame warmup +
1,200-frame gameplay windows, repeated three times.

A zero v2 hit count cannot validate the optimization or justify enabling it
by default. To measure cold decode savings, separately instrument the
geometry admission (native lookup, decode, pack and VB/IB creation).

## Next tranches for the 25-FPS bottleneck

Precompute immutable GX *draw recipes* per GLG object and translate their
state to GXM once, retaining live per-draw matrices/lighting and correct
shadow/alpha/target order. This is the step expected to remove recurrent
producer/consumer state work, not just one-time geometry preparation.
It requires its own trace oracle and opt-in hardware tests before rollout.
The preliminary A5 state-revision/elision and one-draw prepared-DL candidate
is documented in `A5_GX_STATE_AND_DL_2026-10-08.md`. A5 is a conservative
first step; a true reusable **per-material GXM DrawPacket** is not yet built.
