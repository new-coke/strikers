# Aurora-Vita R2 coredump: Vita libstdc++ gthread contract

Date: 2026-10-10. Source-only fix plan; no Vita build, deployment or push.

## Evidence and reproduction context

- Installed SELF SHA-256: `612ddb81ef6106479e6502d9a98d02ef60589f7742515edc762050dab39d28c6`. Exact debug ELF: `smstrikers-port/build-vita-gxm-latest/strikers`.
- Core downloaded read-only from `ux0:/data/psp2core-1791639634-0x0000792761-eboot.bin.psp2dmp.tmp`, SHA-256 `9767f641c714b57e03ee98f706984f97fe83155ec6cfd4d7163bb6b8c5d43376`. Saved in `ab-artifacts/native-model-registry-20261010/crash-r2-investigation/`.
- The dump is gzip-compressed Vita-specific ELF; generic GDB cannot interpret its `THREAD_REG_INFO` note.
- Crashed thread `melee_gx_frontend` (UID `0x40060197`), stop reason `0x30004` data abort, PC `0x812729b4`, LR `0x8127d97b`, DFAR `0x00001642`, DFSR `0xc7`.
- Actual app text base is `0x8104c000`, while debug ELF text starts at `0x81000000`: subtract `0x4c000` before symbolizing. Correct ELF PC `0x812269b4` is within `aurora::gx::fifo::vita_worker_main`, not `glx_ShadowTextureGrab`.
- At the fault, `ldr r3, [r3, #8]` dereferences `r3=0x163a`. The neighboring instructions decrement a `shared_ptr` control block use count and call a virtual deleter when it hits zero. This corresponds to the worker cleanup of queued display-list `PinnedBytes` references. Core memory at `r7=0x84ad1c48` has invalid vtable `0x163a` instead of a libstdc++ address. This is consistent with a reference-count race, dangling control block or memory overwrite.

## Toolchain root-cause hypothesis: high confidence

VitaSDK GCC 15.2.0 `ext/atomicity.h` calls `__gthread_active_p()` to decide between plain and atomic reference increments/decrements. VitaSDK's `bits/gthr-default.h` chooses a weak `pthread_cancel` as the active-thread proxy on non-glibc/non-bionic targets. The exact installed SELF has strong `pthread_create` but *weak undefined* `pthread_cancel`. The disassembly at `vita_worker_main` contains a constant-false threaded branch and non-atomic `ldr/sub/str` for a `shared_ptr` use count, with the `ldrex/strex` path present but bypassed. Concurrent producer/cache copies and worker releases make this incorrect.

Although this is a concrete compiler/runtime mismatch, the core alone cannot exclude another earlier heap overwrite as a second contributor. Do not classify the repeating `scene_budget_exceeded scenes=6 budget=5` warning as the root cause without separate evidence.

## Implementation plan (reviewed before changes)

1. At the Aurora Vita GXM backend target, **only when asynchronous GX is enabled on Vita**, set `_GLIBCXX_GTHREAD_USE_WEAK=0` as a **PUBLIC** compilation contract on `aurora_vita_common`, which the GXM backend links PUBLIC. This propagates to all consumer C++ TUs (including Strikers via `port_flags`), makes `__gthread_active_p()` return true for non-glibc VitaSDK and forces libstdc++ atomic reference counts. Avoid a fake `pthread_cancel` symbol.
2. Add a compile-time guard in Aurora GX FIFO (Vita + async only) so a future build cannot silently regress to the unsafe weak-thread probe. This has no impact on the standard synchronous GX fallback, GPU queue order, renderer, GPU vertex streaming or shader.
3. Validate on the **host only**: existing native registry/cache/dispatch contract tests, relevant submission tests, code diff checks, and inspect CMake PUBLIC dependency propagation. No Vita cross-compile or installation.
4. Hardware validation, to be executed later by the user via manual build/install only: verify ELF disassembly now takes atomic `ldrex/strex` for worker shared_ptr decrements; test identical runtime/INI and longer match, check crash-free operation and visuals. If emergency rollback is required before the fixed build, disable only `gxm_native_model_cache` and `gxm_native_model_uniform_build` in the INI; do not change the GXM backend or streamed vertex ban.

Scope does **not** claim persistent R2 geometry/material native compilation is complete.

## Implementation performed (source-only)

- `smstrikers-port/extern/aurora-vita/cmake/aurora_vita_gxm.cmake`:
  in the GXM-only path, when `AURORA_VITA_ASYNC_GX` is enabled,
  `target_compile_definitions(aurora_vita_common PUBLIC
  _GLIBCXX_GTHREAD_USE_WEAK=0)`. The common target is PUBLIC-linked by
  `aurora_vita_gxm_backend`, whose `aurora::vita_backend` alias is
  consumed by Strikers' `port_flags`. This is only a C++ atomics policy;
  no new GPU synchronization, framebuffer/resource changes or GX command
  reordering.
- `smstrikers-port/extern/aurora-vita/lib/gx/fifo.cpp`: compile-time guard
  rejects `__vita__ && AURORA_VITA_ASYNC_GX` unless the macro equals zero.
  It does not affect the synchronous GX path or host/VitaGL compilation.

## Validation obtained without a Vita build

- Rebuilt the three native model registry, model cache and submission host
  binaries successfully. Compiler warnings were from existing robin_hood
  deprecated builtins, not this change.
- Targeted `ctest`: **8/8 PASS** for
  `vita_native_model_textured`,
  `vita_native_model_textured_cache`,
  `vita_native_model_textured_uniforms`,
  `vita_native_model_cache`,
  `vita_native_model_registry_cache`,
  `vita_native_model_registry_dispatch`,
  `vita_native_model_registry_dispatch_cache`, and
  `vita_submission`. Detailed logs in
  `ab-artifacts/native-model-registry-20261010/crash-r2-investigation/host-ctest.log`.
- A small **configuration-only** host CMake fixture includes the real GXM
  target fragment, creates a dummy port consumer and checks generated
  compile commands (no binary built). In `AURORA_VITA_ASYNC_GX=ON`, both
  GXM backend and its consuming port get
  `-D_GLIBCXX_GTHREAD_USE_WEAK=0`; with `OFF`, neither does. Fixture:
  `ab-artifacts/native-model-registry-20261010/crash-r2-investigation/cmake-contract/`.
- Existing git modifications unrelated to the fix were preserved. Hardware
  validation and a real Vita cross-compile are **pending** at user request.

## Next manual hardware validation

```sh
cd /Users/robin994/Documents/Code/strikersVita/smstrikers-port
STRIKERS_GX_THREAD=ON ./build-vita-native-gxm.sh
```

Before installing, compare the new SELF against the old digest above and
inspect the freshly linked **ELF's** `vita_worker_main` disassembly. The
shared_ptr decrement/release path must take atomic `ldrex/strex` or an
equivalent atomic sequence, not the previous constant-false threaded
branch followed by `ldr/sub/str`. The new ELF may relocate differently,
so do **not** reuse crash offsets from R1 without recalculating load bases.

The user must request installation explicitly. Validate complete long-running
matches with cache+uniform on, observe geometry/material textures/characters
and shadows, then compare performance and crash-free behavior against a
strictly matched GX fallback run. A missing crash in a short run is not proof
that all previous memory safety faults are gone.
