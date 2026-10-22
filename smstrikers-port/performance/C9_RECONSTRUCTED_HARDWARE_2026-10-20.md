# Strikers Vita C9 reconstructed rollback — Vita validation (2026-10-20)

## Result

**PASS for a bounded hardware-performance rollback.** The **reconstructed C9**
VitaGL SELF is installed on the retail PS Vita `192.168.1.79` as
`ux0:/app/SMSVITA01/eboot.bin`. Exact installed-byte readback and test-close
readback both match the built candidate. The 300-frame comparable quiet capture
measured **27.1143645938458 FPS**, recovering **10.0675416 FPS** from the most
recent C20 matched 300-frame result **17.046823 FPS** (one pair; not a
causal/repeatability proof). The real, original C9 quiet observations were
**28.720436 FPS** and **27.626243 FPS** with uniform cache OFF. The reconstruction
is therefore in the historical C9 range but **not byte-identical** to the
original C9 release.

No crash or runtime hang was evidenced by this bounded benchmark, but no manual
human-input/Free Practice, post-goal progression, audio listening, suspend,
long-duration stability or full-gameplay acceptance was performed on the newly
installed reconstructed candidate. **30 FPS stable and 60 FPS remain unachieved**.

### Binary and source identities

| Identity | SHA-256 | Bytes |
|---|---|---:|
| Historical original C9 SELF (NOT recovered) | `69a4bcf2a34b2d5163d9b61d715a37005c6a3bc7493c4cf24c0c837f49916c42` | 2,460,440 |
| Historical original C9 VPK (NOT recovered) | `0627c5ac77acab642ebce69f6547b076abda8f071338552d81e1a7384bb3817e` | 2,994,217 |
| **Reconstructed, installed C9 SELF** | `7c8c6cd8d9f9857173cd51e22507654764e60d1c29b3d9d1b493be53225f3d6e` | 2,455,894 |
| Reconstructed, local C9 VPK | `5d7714b779ec501f8ab46b769f4a6383e323ab49e82fcfa35cf17c4e410a6132` | 2,989,560 |
| Preserved C20 fallback SELF (NOT installed now) | `a9ef59c940ba70097695cd514aa03483f07aaa2558ec133d717477ccafb53751` | 2,651,942 |
| Preserved C20 fallback VPK | `40b73f8005b710749ced50254e36bee3b7d1c98338968af51e18c86a879fddcf` | 3,184,501 |
| Original full C18 `.previous` SELF | `850cc77b6fac6fccdeaf478e484e79d93204d7d6f6ae80223cf9b3089f8cc339` | 2,645,642 |

- Historical C9 SHA-verified source set: **83/83** from the original C9 build
  receipt, preserved as commit `596779b242de3733fc914df45732ce34f581e362`.
- Reproducible **reconstructed C9 source checkpoint**:
  `recovery/c9-hardware-27fps-20261020`, commit
  **`a5fe9b9422c49e50ef8f4fdf21cfde67329d7ab2`**, derived from C9
  source pins plus 19 preserved transitive source additions/edits. This ref was
  created with an alternate Git index and does not touch the active dirty
  C20 checkout. **No push** was executed.
- Built from the Git-archived C9 source at
  `.tmp/native-c9-isolated-20261020/source/smstrikers-port`; the missing
  pre-C12 headers and `task_profile.cpp` were restored to that isolated stage
  only. The standalone VitaGL and platform targets passed before the VPK link.
- The source-checkpoint commit excludes `.DS_Store` and packaging-only
  `template.xml` line-ending normalization. The original 83 source pins remain
  identical; the recovered 19 additional files are supported by pre-C12 snapshots
  or the originally untracked task profiler. This does **not** prove
  identical instructions to the lost original C9 SELF.

### Comparable quiet hardware capture

- **Rows:** 300 consecutive live match/play samples 0–299, logical frames
  **600–899**, `vita_test_match=1`, `fixed_dt=0`, `vita_frameskip=0`.
- **CPU/GPU/bus/Xbar clocks:** 444/222/222/166 MHz, verified in the CSV.
- **Diagnostics/FPS overlay:** OFF. `vitagl_uniform_cache=0`,
  `vitagl_vertex_fog=0`, `vitagl_affine_uv=0`,
  `vitagl_packet_profile=0`, `vitagl_timing=0`,
  `vita_watchdog=0`; other switches follow the original C9 quiet receipt.
- **Average:** 36,880.82 microseconds = **27.1143646 FPS**.
- **Median:** 34,384 microseconds; **p95:** 47,686 microseconds; **p99:**
  49,923 microseconds.
- **Within 33.333 ms (30 FPS):** 72/300 frames; **within 16.667 ms
  (60 FPS):** 0/300 frames. Both targets remain open.
- **CSV:** 8,823 bytes, SHA-256
  `bfdd256a9001bbcb1590494d86928c5c67ddba391b8d87187c000571a03d7ad3`.
  The complete downloaded CSV was independently SHA-checked against the remote
  capture and contains contiguous samples 0–299 and play frame IDs 600–899.

### Installer, cleanup and save safety

The bounded installer used `smstrikers-port/tools/vita_workflow_device.py` and
verified the installed SELF by FTP canonical-path readback. The hardware
benchmark tool verified its one-shot startup-ready marker before continuing;
the 300-frame CSV and downloaded readback SHA match. After measurement, the
test restored the exact original `strikers.ini`, stopped the app, released
synthetic controller state and independently verified the installed C9 SELF.
The fallback procedure would have restored C20 upon failed startup, missing
capture, FPS below the test gate or cleanup failure. It was **not** needed.

- Original `strikers.ini`: **760 bytes**, SHA-256
  `aac80bdf22392a11167a11dd9f713ffcff4088d3e9c898f9e6e50f5314620759`.
- Original retail Card A `.sav`: **40,960 bytes**, SHA-256
  `34367251ec96e1adde5fa3f85eb05054ef66e84ea73ba08e5a95537c9e531800`.
- Original Card A `.stat`: **116 bytes**, SHA-256
  `4d04275d4e91cfc693e3a29989e17d70229ce6f26b79724c5edeaec42ef49c3f`.
- Original Card B: **empty**, checked after the run. No save restoration was
  necessary (`cards_repaired=[]`). Local, private card backup bytes live
  under `.tmp/native-c9-isolated-20261020/card-original/`; the public
  evidence manifest stores hashes only, not save contents.
- C20 remote rollback backup with original INI:
  `.tmp/native-c9-device-20261020/backup/{eboot.bin,strikers.ini}`.
- C20 SELF/VPK local backup:
  `.tmp/native-c9-build-third-20261020/rollback-c20/`.
  The original complete C18 `.previous` set remains untouched.

### Frozen evidence (no binary or save duplication)

`docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate9-reconstructed-device-20261020/`

- `manifest.json` — receipts, source commit and all evidence SHA-256 checks.
- `quiet300.csv` — 300 measured play frames and device clocks.
- `run-result.json` and `run-identity.json` — verified metrics and exact
  INI overrides.
- `install-identity.json`, `fetch-identity.json`,
  `restore-identity.json` — bounded installation/download/INI recovery.
- `source-checkpoint.json` — new source-ref identities and 19 added/modified
  source SHA pins.
- `ready-marker.csv` and `build-result.json` — startup handshaking,
  generated executable/package checksum and sizes.

### Handover / important build-directory state

**IMPORTANT:** To preserve the uncommitted original C20 working tree without
creating additional Vita build directories, the **single existing**
`smstrikers-port/build-vita-gxm-latest` CMake cache currently points at
`.tmp/native-c9-isolated-20261020/source/smstrikers-port`, **not** the root
checkout's `smstrikers-port`. This is intentional while the installed build
is reconstructed C9. Do not silently assume the next `cmake --build` compiles
the active root C20 sources; do not delete the isolated stage while the cache
references it. Always check `CMAKE_HOME_DIRECTORY` before any subsequent
reconfiguration. If switching build source, restore the cache source binding
explicitly, first pinning the installed C9 binary and C20 fallback identities.

The new `recovery/c9-hardware-27fps-20261020` Git ref contains the
reconstructed, buildable C9 code without modifying the active dirty root tree.
It can be archived or checked out **only with a clean/isolated and approved
source-management transaction**; never use `git reset --hard` or overwrite the
dirty C20 source. The current C9 SELF/VPK live in the usual shared build
directory (not committed). The older C9 exact binary remains unavailable.

The natural next engineering target is still stable 30 FPS, followed by
60 FPS. Focus on owner-side VitaGL draw cost and shader complexity, with
controlled repeatable quiet 300-frame A/Bs against **this** installed baseline.
Do not add fragment-shader paths like the suspected C14 fog-range regression
without a measured GPU/CPU timing attribution. Functional human control,
pass, shot, saves/progression and extended stability are **not accepted** merely
because this quiet synthetic benchmark passed.
