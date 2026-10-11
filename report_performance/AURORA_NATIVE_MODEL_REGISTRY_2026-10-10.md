# Aurora-Vita native model registry, tranche R1
Date: 2026-10-10
Scope: source-only; Vita GXM build/install/push are explicitly excluded.

## Architectural decision

The producer-side native-model lookup, 64-entry ring and optional
set-associative cache previously lived in Strikers
`src/NL/glx/glxSend.cpp`. Aurora already owned the FIFO transport and
consumer-side GPU cache, but the producer decided whether a model signature
was valid. This duplicated policy and prevented reuse by other ports.

The new `lib/gx/native_model_registry.hpp` lives **inside Aurora-Vita** and
provides a typed, application-independent `NativeModelDescriptor`, a
`NativeModelRegistry`, and two explicit conservative admission contracts.
Strikers retains only a GLX adapter that extracts metadata and supplies
the original GX draw emitter. The existing default GX/GXM path is unchanged.

### Public Aurora contract

```cpp
aurora::gx::fifo::NativeModelDescriptor descriptor{};
// Populate display-list identity/length, render view and immutable material
// identity, texture handles, per-stream address/stride/size/semantic/endian.
// Optionally supply sourceGeneration/materialGeneration on asset reload.
auto& registry = aurora::gx::fifo::native_model_registry();

if (!registry.dispatch_or_capture(descriptor, originalDlData, originalBytes,
      [&](const void* data, uint32_t size) noexcept {
        // Original GXCallDisplayList, executed once for first admission.
      })) {
  // Unchanged GX emission at the original draw slot.
}
```

The descriptor **does not** cache per-instance camera, transforms, animated
skinning palette, dynamic lights, scissor, EFB texture state or GPU resource
handles. Each submission still validates/materializes those on the owner
thread. It includes all six texture IDs, material/raster/TEV identity fields,
and up to eight independently described vertex streams (normal/UV semantics
are representable without pretending they have already been translated).
Values are compared field by field, not with a hash or padded struct memcmp.

`NativeModelRegistry` supports:
- Bounded 64-slot producer ring (existing default for experimental mode);
- Existing opt-in 256-slot / four-way associative `NativeModelCache`;
- Exact semantic lookup with optional loader generations;
- Explicit `invalidate_source(displayList)` for future GLG unload/reload;
- Retention of in-flight recipes through queue-owned `shared_ptr`;
- Borrowed lookup handles on the single producer thread, avoiding an
  unnecessary shared_ptr atomic reference count on every registry hit;
- `dispatch_or_capture` to handle lookup, first GX recording, recipe
  admission and failure automatically inside Aurora.

A registry hit is **not** proof the GPU buffer is still valid. The existing
Aurora GXM consumer validates pinned display-list revision, current GX state,
shader/pipeline, indexed geometry residency, texture/TLUT/EFB bindings,
material flags and live matrices. At any failed guard the original pinned GX
display list is executed at the same ordered FIFO position.

### Admission and coverage

The static-unlit mode-1 admission decision now uses Aurora's own
`model_admission_rejections()` classifier; Strikers provides typed
`ModelAdmissionFacts`. The expanded mode-2 view/program and range checks
are evaluated by `native_model_runtime_admissible()` in Aurora; Strikers
only maps game-specific view/program enums and supplies stream metadata.

The previous hardware census admitted **0 / 168,635** callbacks for
mode 1: texconfig, texture references, normals/UV streams and real
CHARACTERS/SHADOWED views were excluded. Do **not** remove these gates
without a validated material/skin/TEV contract.

The mode-2 native recipe experiment already ran at ~15 FPS versus ~20 FPS
for the ordinary GX/GXM route, with a reported 34% increase in mean frame
time. Centralizing its registry does not eliminate its per-instance GX state,
dynamic texture resolution or uniform build, and **no FPS improvement has
been established by R1**.

### Opt-in and rollback

The effective default remains:

```ini
gxm_native_model_draw=0
gxm_native_model_cache=0
gxm_native_draw_replay=0
gxm_native_gpu_static=1
gxm_streamed_vertex_gpu=0
gxm_disable=0x8
```

No existing Vita setting was changed; do not enable mode 2 in the stable
build solely because this source refactor compiled on the host. To roll
back just the R1 source migration, revert only the new registry header,
the new test target/changes and the `glxSend.cpp` adapter patch; preserve
all unrelated RE4/VitaGL experimental work currently in the Aurora worktree.

## Validation

Two new standalone host test variants cover the registry dispatch:
- First capture executes the original GX list exactly once;
- Same-source lookup queues a native recipe without invoking Strikers again;
- Changed DL bytes cause the consumer to fall back to the original bytes;
- A changed material identity creates a separate registry entry;
- No worker returns false so the game retains its normal GX emitter;
- Set-associative cache mode (STRIKERS_GXM_NATIVE_MODEL_CACHE=1).

Extended `vita_native_model_cache_test` covers descriptor collisions,
texture/program/material/stream/endian differences, optional generations,
source invalidation, per-slot cache erasure, bounded occupancy and
queue-owned recipe lifetime.

Final host contract test results: **5/5 PASS**, including both cache modes,
capture-once/replay/changed-source fallback and admission-census oracles.
The ordinary Strikers first-run INI/default configuration test also **PASS**,
and both repository worktrees pass `git diff --check`.

Caveat: the existing desktop preset is `AURORA_VITA_RENDERER=VITAGL`.
Two **legacy** host tests, `vita_native_model_recipe` and
`vita_native_model_recipe_cache`, failed during this work with
`FAIL 20: packet`; their test source was not changed. The macOS host
configuration cannot select GXM without VitaSDK
(`cmake/aurora_vita_gxm.cmake` rejects that configuration).
These issues are not evidence of correct GXM rendering, nor evidence
that R1 fixes or causes a VitaGL runtime issue.
Full CTest: **33/35 PASS** on the existing *VitaGL-configured* host tree;
the only failures are those two legacy recipe tests. All four new registry
cases and `vita_native_model_textured` passed. The exact command output
was retained in `/tmp/strikers-native-registry-full-ctest.log`; the registry
contract output is in `/tmp/strikers-registry-guard-ctest.log`.
The isolated GXM-host configuration was attempted **without a Vita build**
and rejected intentionally by Aurora's CMake guard, so native GXM hardware
equivalence remains an explicit pending test.

Do not run an automatic Vita build: user manually builds with

```sh
cd /Users/robin994/Documents/Code/strikersVita/smstrikers-port
STRIKERS_GX_THREAD=ON ./build-vita-native-gxm.sh
```

## Next functional milestone

R2 should connect model load/unload generation IDs from the GLG asset
lifecycle to the registry; introduce an immutable GXM material/program
recipe that supports textured normal/UV layouts and TEV combinations.
Only then can the GXCallDisplayList entrypoint automatically route more
real game draws through the resident GXM path. The consumer must retain
the exact GX fallback, thread ordering, framebuffer/scene barriers, and
live skin matrices for CHARACTERS and SHADOWED. Measure admission count,
real native-dispatch count and quiet FPS with equal clocks and content.
