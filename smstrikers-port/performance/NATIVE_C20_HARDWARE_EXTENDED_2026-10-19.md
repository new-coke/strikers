# Strikers Vita C20 — extended on-device verification (19 October 2026)

## Status and scope

**Completed bounded hardware verification with one unresolved potential hang.** No source changes, Vita rebuild or binary redeployment were performed. The installed C20 SELF remained `a9ef59c940ba70097695cd514aa03483f07aaa2558ec133d717477ccafb53751` (2,651,942 bytes). The existing C20 VPK SHA-256 remains `40b73f8005b710749ced50254e36bee3b7d1c98338968af51e18c86a879fddcf`.

The console was reachable at `192.168.1.79` using VitaCompanion FTP 1337 and control 1338. Run-specific INIs were temporary; the ordinary INI and known original cards were verified after each closed run. The [SHA-256 evidence manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/manifest.json) binds 21 compact files (202,430 bytes total); the large raw logs remain under `.tmp/native-c20-hardware-tests-20261019`. The manifest itself is SHA-256 `0fef0458bc0771e83281394001cd2418abb062c9cbb70a3e122efb3fc3506c51`.

## 1. Automatic match stress / game simulation: PASS within the tested scope

C20 ran an automated match for **297.27 seconds** with diagnostics and the real `control_trace` observer enabled. There were **5,394 complete control records**, **1,158 world records**, and the final trace reached **tick 10,130** (no record-cap truncation). World state advanced and score transitions were observed:

`0–0 → 1–0 → 1–1 → 1–2 → 2–2`

The final sampled audio log recorded `outputs=26000`, `underruns=0`, `non_silent=1`, `error=00000000`. These are output/runtime indicators rather than acoustic proof. No PAD0/team0 player was associated with this **automatic** test mode. **Movement, pass and shot were NOT_EXERCISED** in this run. The executor completed and the remote SELF, ordinary INI and the original card contents passed postflight identity checks.

Evidence: [stress result](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/stress/result.json), [gzip runtime log](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/stress/runtime.log.gz).

## 2. Repeated quiet benchmark: PASS capture, 30/60 FPS FAIL

A second quiet C20 capture used the **same effective configuration as C19 and the previous C20 matched run**, except the unique output paths: no diagnostics, frame skipping, overlay, fog experiments or task/packet profiling, `fixed_dt=0`, seed `0x53545249`, frames **600–899** of live play. Reported actual clocks were CPU/GPU/bus/XBAR **444/222/222/166 MHz**. All three config dictionaries matched after excluding unique `frame_capture` and `log` values.

| Metric | C19 reference | C20 matched run 1 | C20 matched run 2 |
| --- | ---: | ---: | ---: |
| Mean FPS | 16.510175 | 16.739271 | **17.046823** |
| Mean frame time | 60.568710 ms | 59.739757 ms | **58.661957 ms** |
| Median frame time | 64.3835 ms | 62.5475 ms | **60.5055 ms** |
| p95 frame | 66.810 ms | 66.817 ms | **66.719 ms** |
| p99 frame | 68.897 ms | 67.542 ms | **66.990 ms** |
| Frames ≤33.333 ms | 0/300 | 0/300 | **1/300** |
| Frames ≤16.667 ms | 0/300 | 0/300 | 0/300 |

Run 2 averaged **+1.8373% versus C20 run 1**; the unweighted mean of the two C20 FPS values was **16.893047**. This provides a second finite sample, **not proof of stable causality or an optimization gain**. Frame content, clock state around startup and runtime scheduling can vary even when the INI matches.

Run-2 CSV: 8,864 bytes, SHA-256 `f53df83949493d9878c103a9874331e671406efc33572014c4d336de83330f48`. The returned result validated 300 sequential samples, first frame 600, last 899, then closed and reverified original state.

Evidence: [comparison and normalization](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/quiet-repeat/repetition-comparison.json), [CSV](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/quiet-repeat/native-c20-quiet600-repeat.csv), [run postflight](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/quiet-repeat/verified-result.json).

## 3. Manual UI / Free Practice input: SUSPECTED STALL, NOT ACCEPTED

A separate diagnostic C20 session used the normal `vita_test_match=0` path with `no_movies=1`, real control trace, snapshot requests, and VitaCompanion input. The first on-demand screenshot **was completed** and visibly showed the health-and-safety screen (request ID 1). A single acknowledged `cross` tap followed.

The subsequent runtime trace **did reach an actual controllable match**:

- `kind=player index=0 present=1 pad=0 team=0`, position `(0.53646, -8.2098, 0)`.
- `kind=world state=4` and advancing game simulation up to **tick 2390**.
- Last retained log: **379,066 bytes**, SHA-256 `f012931eed4dba1ffc18d24cdac2b47ac876f7c48ad0cefe0ce1f12bd9dadbf4`.

After on-demand snapshot **request ID 2** was published while the game session was active, no `request.2.ppm` appeared and no `snapshot_request id=2 ok=...` completion log was recorded. Several independent FTP reads returned the **same 379,066-byte log**; the trace remained at tick 2390. VitaCompanion acknowledged one controlled `left-stick 128 255` hold and the release, but the post-input log remained byte-identical. Thus **input dispatch was acknowledged by the companion, not verified at the gameplay layer**; the movement test is **NOT_ACCEPTED**, and no pass/shot input was attempted.

The missing picture and halted log are evidence of an **observed stall or process stop**. They do not prove an OS-level deadlock, its stack, or a C20-induced regression. A read-only postflight query confirmed that the `request` file contained ASCII `2\n` and that `request.2.ppm` was missing. No new crash dump filename appeared in the checked directory.

The visual request path is implemented in `src/Game/main.cpp`: `VitaCaptureFrameToPath` calls `sceGxmDisplayQueueFinish()` before querying the framebuffer in the VitaGL path. This is a **high-priority instrumentation point**, not a proven failure line. The first snapshot also encountered a transient partial FTP read in the controller; the fully written request-1 PPM was subsequently fetched and validated separately. Distinguish this host collector error from the distinct missing request-2 image.

The session was closed explicitly. Input release and a targeted app kill were acknowledged; remote C20 SELF, original INI and cards all passed final readback.

Evidence: [practice session journal](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/practice/state.json), [runtime log](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/practice/runtime.log.gz), [first hardware screenshot](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/practice/health-warning-frame-1.jpg), [request-2 postflight](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/practice/request2-after-stop.json).

## 4. Snapshot-disabled counterprobe: INCONCLUSIVE

A new, separate real-menu run disabled on-demand snapshots and attempted a single early health-screen confirmation. After **144.81 seconds** it had **not reached a match** and produced no `control_trace` world records. The controller was intentionally interrupted instead of sending speculative additional menu inputs. Its `finally` path still reaped the restore-config child and independently verified the C20 SELF, original INI and cards. Overall test status is **INCOMPLETE**, cleanup PASS. It **cannot** confirm that screenshot requests caused the earlier stall, because the controlled gameplay state was never reached.

Evidence: [bounded interrupted run](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/counterprobe/result.json), [restore receipt](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/counterprobe/restore-config-identity.json).

## 5. Final device state and next investigation

Final FTP postflight rehashed, without modifying:

- `ux0:/app/SMSVITA01/eboot.bin`: 2,651,942 bytes, SHA-256 `a9ef59c940ba70097695cd514aa03483f07aaa2558ec133d717477ccafb53751`.
- `ux0:/data/strikersVita/strikers.ini`: 760 bytes, SHA-256 `aac80bdf22392a11167a11dd9f713ffcff4088d3e9c898f9e6e50f5314620759`.
- Card A `4d6172696f536f63636572.sav`: 40,960 bytes, SHA-256 `34367251ec96e1adde5fa3f85eb05054ef66e84ea73ba08e5a95537c9e531800`.
- Card A `4d6172696f536f63636572.stat`: 116 bytes, SHA-256 `4d04275d4e91cfc693e3a29989e17d70229ce6f26b79724c5edeaec42ef49c3f`.
- Card B listing was empty. The `ux0:/data` directory had **16 existing `psp2core` names**, with **no new names timestamped within the current C20 session window**.

The source/build tree was not recompiled or deployed. Existing C18 `.previous` rollback remains untouched and the C19 executable backup from the earlier installation is preserved.

**Next tests required:** (1) establish a repeatable UI path to the same controlled player without on-demand screenshots; (2) trace before/after `sceGxmDisplayQueueFinish`, framebuffer acquisition and renderer pending/retired queue counters; (3) repeat the same input/snapshot sequence on C19 and C20 under fixed conditions before assigning regression causality; (4) once stable, exercise movement with raw pad trace, neutral settle, pass and shot. Full gameplay, stable 30 FPS and 60 FPS remain **OPEN**.

Final inventory: [postflight report](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/postflight/final-hardware-inventory.json).
