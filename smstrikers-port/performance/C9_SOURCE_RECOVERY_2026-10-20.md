# Strikers Vita — C9 source recovery checkpoint (2026-10-20)

**Superseded later on 20 October 2026:** The user explicitly authorized the
Vita build and installation after this source-only checkpoint was written.
Reconstructed C9 has now been built and installed on Vita with verified SELF
SHA-256 `7c8c6cd8d9f9857173cd51e22507654764e60d1c29b3d9d1b493be53225f3d6e`
(the original historical C9 SELF is still unavailable), and a matched 300-frame
quiet hardware run measured **27.114365 FPS**. The new buildable-source branch
is `recovery/c9-hardware-27fps-20261020` at
`a5fe9b9422c49e50ef8f4fdf21cfde67329d7ab2`, with original dirty C20
source and binary fallback preserved. See the **current**
[C9 reconstructed hardware handover](C9_RECONSTRUCTED_HARDWARE_2026-10-20.md)
and [evidence manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate9-reconstructed-device-20261020/manifest.json).
The section below remains a historical record of the **earlier source-only state**.

## Current outcome: C9 source branch prepared, target NOT yet restored

The user's requested historical reference is **candidate C9**, not C11 or a later
candidate with similar frame rates. The original C9 executable/package were
not retained in the locally accessible artifact locations. Therefore, the
installed PS Vita application has **not** been replaced, and the C20 working
tree and the sole complete C18 `.previous` rollback set were not modified.

A separate Git branch has now been created without switching or cleaning the
dirty production checkout:

- Ref: `refs/heads/recovery/c9-source-pins-20261020`
- Source-recovery commit: `596779b242de3733fc914df45732ce34f581e362`
- Parent: `bc468b1843e74e9099ff3396ffb0eac0cc28ebd8`
- Frozen source receipt:
  `smstrikers-port/build-vita-gxm-latest/native-candidate9-build-result.json`
- Verified: **83/83 historical production/test source hashes** match this
  recovered Git tree. **25** active files differed and were recovered to an
  isolated overlay; **24** from independently hashed archived source copies,
  and **one** (`src/Game/main.cpp`) from the C7 watchdog before-image and
  a strictly isolated single-file watchdog patch. The reconstructed
  `main.cpp` hashes exactly to historical C9
  `7caf903a40d06fcdb14e65269a7500141c98f9c12fc2a2970d823f686743a3c4`.
- Source overlay: `.tmp/native-c9-recovery-20261020/overlay`
- Source hash manifest: `.tmp/native-c9-recovery-20261020/manifest.json`
- Branch/tree verification receipt:
  `.tmp/native-c9-recovery-20261020/git-checkpoint.json`

**Important limit:** the historic source gate covers 83 pinned files, not every
transitive include/build input. The branch is a SHA-verified **C9 source
reconstruction candidate**, not yet proof that a subsequent compile will
produce the identical historical ELF/SELF, graphics or measured FPS. Do not
present it as an installed/validated C9 binary.

## Historical executable identities and measured performance

Original C9 Release / VitaGL:

- **SELF**: 2,460,440 bytes, SHA-256
  `69a4bcf2a34b2d5163d9b61d715a37005c6a3bc7493c4cf24c0c837f49916c42`
- **VPK**: 2,994,217 bytes, SHA-256
  `0627c5ac77acab642ebce69f6547b076abda8f071338552d81e1a7384bb3817e`
- Build completed on 18 October 2026 at 22:36 local time.
- Hardware quiet `vitagl_uniform_cache=0`: **28.720436 / 27.626243 FPS**
  on two separate 300-frame windows.
- Hardware quiet `vitagl_uniform_cache=1`: **27.488988 / 27.032377 FPS**.
  The cache did **not** provide a repeatable speedup and default stayed OFF.
- C9 profiling-on observation: **29.520092 FPS**; this must not be
  compared as an equivalent diagnostics-free quiet benchmark.
- All historic quiet runs selected 300 live-play frames 600–899, fixed dt 0,
  and Vita clocks 444/222/222/166 MHz.

For comparison, C20's two matched quiet runs on Vita gave **16.739271 and
17.046823 FPS**. The regression primarily appeared between C12 and C15, with
an increase in owner-side draw time and renderer queue backpressure; C14's
more expensive fog-range fragment path is a candidate cause, **not proven**.

## Recovery and safety proof

The initial search found no file with the exact C9 SELF length among repository
files and no original C9 SELF/VPK among retained build/installation artifacts.
The console currently exposes `ux0:/app/SMSVITA01/eboot.bin` for the installed
version, and its `ux0:/data/strikersVita/strikers-update.vpk` contains an
embedded `eboot.bin` with SHA-256
`2168d5ecf0949f8b0ae888aaa7f01fafa4e0aee6357313f92662cc12f5f54cbb`,
**not C9**. Neither remote file was modified.

The two recovery scripts used temporary files and a Git alternate index:

- `.tmp/safe-script-c9-source-recovery-20261020.py`
- `.tmp/safe-script-c9-source-branch-20261020.py`

The second re-read the complete source-hash set from its generated Git tree,
validated C20's local SELF before/after, and checked that Git HEAD and the
working-tree porcelain listing did not change. It did **not** run
`git checkout`, `git switch`, `git reset`, `git stash`, a Vita build,
package or deployment. It added only a separate Git recovery reference.

Current active C20 SELF, both locally and at the last independently verified
device checkpoint:

`a9ef59c940ba70097695cd514aa03483f07aaa2558ec133d717477ccafb53751`

C20 remains the **installed** version pending an authorized, safe C9 build and
deployment. Original `strikers.ini`, game data and 41,076 bytes of retail
card files must not be replaced.

## Exact next operation: controlled rebuild and device A/B (NOT executed)

The user previously directed **no automatic Strikers Vita builds**. This
request to return to an older version authorized preparation and recovery, but
did not explicitly revoke that build constraint. Therefore the rebuild and
device replacement are **pending explicit build/deploy authorization**.

When authorized:

1. Establish the current device identity and independently back up C20 for a
   reliable rollback; preserve original saves/INI. Do not rotate or delete
   the existing complete C18 rollback without a safe single-slot migration.
2. Materialize the C9 source branch **outside the dirty main checkout**.
   An isolated source snapshot can be created with `git archive`, without
   checking out or overwriting the current branch. Do not call that snapshot
   a build or bypass the historical source-hash verification.
3. Reconfigure the **single existing**
   `smstrikers-port/build-vita-gxm-latest` build directory against this
   source only after the rollback artifacts and CMake cache/source identity
   have been protected. Do not leave profiler or LTO defaults inherited from
   C20 silently. Preserve the previous-build retention policy.
4. Build C9 explicitly and check the generated `eboot.bin`, VPK archive CRC,
   and the expected 2,460,440-byte historical SELF SHA. A different hash must
   be recorded as **reconstructed C9-like**, not presented as exact C9.
5. Only then install with complete remote-stage/canonical readback and
   rollback on mismatch. Leave the retail cards and original INI intact.
6. Run the same quiet 300 live frames 600–899 at fixed clocks, plus bounded
   visual/gameplay acceptance; compare against the original C9 quiet evidence
   and C20 capture, recognizing that frame indices do not freeze poses or
   simulation ticks.
7. If it fails a test, stop and restore the preserved installed version.

No build, installation, benchmark or FPS claim was produced by the present
source-recovery step.
