# Strikers Vita - Native source binding by GLG resource lifetime
Date: 2026-10-13
Checkout: /Users/robin994/Documents/Code/strikersVita

## Goal and scope

Eliminate display-list/position/color FNV-1a content hashes and 1,800-mesh
search from the **steady live-game draw path**. Resolve native GPU IDs at
resource creation, with explicit invalidation for resource arena rewinds
and observable CPU source writes. Retain strict no-GX-fallback semantics.
This does NOT remove Aurora as GPU backend or supply missing Strikers
materials/PN skinning/EFB/HUD.

## Changes

- `src/NL/glx/glxDisplayList.cpp:88`: after building/flushing a
  **permanent** GLG DisplayList, send its packet, DL pointer and current
  GLX resource-marker level to the direct native bridge. Temporary
  frame-lifetime display lists are not admitted.
- `src/NL/glx/glxMemory.cpp:633`: before
  `glplatResourceRelease` rewinds the bump allocator, remove native
  bindings allocated at marker levels greater than the release target.
- `include/port/native_match_source_bindings.hpp`: bounded (8,192-entry)
  portable registry of DL handle, generation, resource marker, original
  source signature, stream metadata, CPU-write revisions and resident
  native mesh index. Fresh registration at a reused pointer always gets
  a new generation. A metadata mismatch poisons the old entry permanently.
  Lookup is average O(1); no Aurora or sceGxm types in this header.
- `src/platform/native_match_direct.cpp`: registration first checks
  resource-arena ownership (DL, GLG data, position/color arrays) and
  rejects direct PN-index lists. It hashes the original source **once at
  registration**, records write revisions and either binds immediately
  (late-loaded assets) or defers binding to `preload()`.
- Each admitted draw now checks the DL pointer and constant metadata,
  its mesh ID, and the Aurora CPU-write epoch. If the epoch is unchanged,
  **no source hash, GX command decode or GPU geometry repack** is executed.
  When global writes are observed, tracked source-range revisions are
  checked. Only a dirty range triggers rehash; mismatched content
  permanently invalidates the native binding. Coarse-page false
  positives can be re-validated with the original source signature.
- An epoch changing *during validation* rejects that draw and retries
  validation next time; it does not silently use an out-of-date source.
- `GLUD_EnvDiffuse` is explicitly rejected because the live
  `Fresnelify` code writes vertex color alpha without a tracked
  DCFlushRange. It must be moved to native fragment uniforms before
  rendering those models with static GPU colors.
- Expanded catalog parsing now releases the original file buffer before
  allocating packed CPU GPU upload arrays, avoiding a redundant ~6 MiB
  peak allocation. CPU upload buffers are dropped after synchronous upload.
- `tests/native_match_source_bindings_test.cpp` exercises deferred
  resolution, 1,800 real meshes, hash collisions, recycled pointer ABA,
  marker release, metadata poisoning, revision fast hits, dirty rehash,
  concurrent epoch retry and permanent invalidation.
- `tests/CMakeLists.txt`: registers this test and enables assertions
  even under Release. Native-only compile gates are also added to the
  relevant GLX display-list and resource-memory translation units.

## Evidence and validation

- Real expanded catalog: **1,800 meshes, 258,528 vertices,
  207,333 triangles**. File:
  `ab-artifacts/native-gxm-direct-20261013/gxm_native_match_meshes_expanded_preview.bin`
  SHA-256:
  `f23352c248dd4ed8a291af619fbb9eca89ff805c7931804482186ac2aff62326`.
- Portable registry contract against all real catalog records: PASS.
- ABA/reuse, marker unwinding, metadata mismatch, dirty revision,
  identity mismatch and race retry host checks: PASS.
- AddressSanitizer/UndefinedBehaviorSanitizer on real asset test: PASS
  (macOS leak-check instrumentation not supported).
- VitaSDK ARM C++ syntax-only: PASS for direct-match bridge,
  `glxDisplayList.cpp` and `glxMemory.cpp` in standard mode;
  both GLX files also PASS in Lean mode.
- CMake Vita *configure-only* PASSED in shared
  `smstrikers-port/build-vita-gxm-latest`. Confirmed native feature gate
  in all five relevant source compile recipes. **No Vita compiler/linker
  build or SELF/VPK conversion was launched.**
- Aurora host CTest regression suite: **38/38 PASS**.
- `git diff --check` in root and nested Aurora, shell syntax: PASS.
- No Vita FTP writes/INI changes, commits or pushes.

## Current hardware/validation gap

The device still has the old native-match v1 SELF, SHA-256
`06fb6568eefd7455139c6c1fa35c015cb11769f2577f84f39cf3bba4b8a326aa`
and the latest FTP report of that build had **zero queued/accepted native
draws** (1,003,335 packet rejections at its earlier material filter).
The expanded preview-material bridge, portable GPU asset store and this
new static source registration have NOT yet been tested as a complete
Vita executable. Do not report a black-screen fix or any FPS gain.

For the next manual test, compile with
`STRIKERS_NATIVE_MATCH_DIRECT=ON ./build-vita-native-gxm.sh`
from `smstrikers-port/`. This script rebuilds the shared Vita directory;
do not invoke automatically. The deployed device must receive the
matching expanded pack as
`ux0:data/strikersVita/gxm_native_match_meshes.bin`, replacing the old
640-mesh sidecar **with an explicit backup/verification**.

The runtime emits `ux0:data/strikersVita/gxm_native_match_status.csv`.
New fields: `source_registrations`, `source_releases`,
`source_hits`, `source_unbound`, `source_dirty_rehash`,
`source_invalid`, `source_misses`, and `source_unsafe`.
A promising run has nonzero `source_hits` and accepted native draws.
A run with all `source_unbound` indicates source binding coverage
needs work, not that native GXM is too slow.

## Caveats and next step

The current change assumes static engine resources notify writes through
DCFlush/Store or the existing memory revision system. Unannounced direct
writes are not provably tracked; dynamically mutated color/skin/morph
streams must stay excluded until their lifetime/dirty contracts are
explicit. A 64 KiB page false positive still incurs a hash *on that
write epoch*, not on each draw.

The renderer continues to use Aurora GXM allocation, shader compilation,
GPU owner worker and present path; this is an incremental source-identity
component for a future Aurora-free `StrikersNativeGXM`, **not** that
standalone renderer itself.

Priority after hardware coverage: convert texture/TEV and character PN
skinning to resident native materials/attributes and verify at least one
real match frame with accepted draws before measuring FPS.
