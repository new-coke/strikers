# Strikers Vita C20 — device installation and finite validation (2026-10-19)

## Extended hardware follow-up — see latest result

Additional device checks on the same installed C20 SELF include a 297-second automatic-match stress run with 10,130 simulated ticks, a second C19-equivalent quiet capture at **17.046823 FPS**, and a diagnostic Free Practice attempt with PAD0/team0 actually assigned. The Free Practice session exhibited a **potential stall at tick 2390**: an on-demand screenshot request did not complete, and the log stopped advancing before the acknowledged stick command; movement is **NOT_ACCEPTED**. A separate no-snapshot counterprobe did not reach the same gameplay state, so causality remains **UNRESOLVED**. Original saves/INI and installed SELF were reverified intact, and no new timestamped core dump was found in the checked directory. Full details, preserved receipts, the suspected synchronization readback path and limits are in [C20 extended hardware report](NATIVE_C20_HARDWARE_EXTENDED_2026-10-19.md).

## Result

**C20 is built, installed as the app executable, and validated in two finite on-device runs.** The ordinary configuration was restored, the installed SELF was re-read from the Vita, and the two original card files were independently re-read without a byte change. This closes the source-only build/device gap noted in [the C20 source checkpoint](NATIVE_C20_DEFERRED_SOURCE_2026-10-19.md). It does **not** close the 30/60 FPS performance goal or whole-game functional coverage.

The device was `192.168.1.79`, title ID `SMSVITA01`. Deployment replaced only `ux0:/app/SMSVITA01/eboot.bin` through the existing bound VitaCompanion FTP workflow, not the entire installed app or its data. The initial targeted `kill` returned `Error: cannot kill the app. Is the TITLEID correct?`; the installer nevertheless read the old C19 executable, wrote C20, and re-read the exact C20 bytes. Subsequent test teardown acknowledged `Killed.` The original `strikers.ini`, disc archive, and card data were not replaced by the installer.

### Build / installation identities

| Item | SHA-256 | Bytes |
| --- | --- | ---: |
| C20 production deferred source | `b13bfa4682bbb2a7d5570acd2825fae73964fa4de78c5ad14fcfc7cc6b1e9d29` | 30,496 |
| C20 SELF and final remote `eboot.bin` | `a9ef59c940ba70097695cd514aa03483f07aaa2558ec133d717477ccafb53751` | 2,651,942 |
| C20 VPK | `40b73f8005b710749ced50254e36bee3b7d1c98338968af51e18c86a879fddcf` | 3,184,501 |
| Pre-install C19 SELF, retained locally | `90d5e6cc6f36ab0f472f65ffcf203c9973e60fabcf26fc7d6c040a88b4fe54a7` | 2,645,700 |
| Preserved sole C18 `.previous` SELF | `850cc77b6fac6fccdeaf478e484e79d93204d7d6f6ae80223cf9b3089f8cc339` | 2,645,642 |
| Ordinary INI after every completed test | `aac80bdf22392a11167a11dd9f713ffcff4088d3e9c898f9e6e50f5314620759` | 760 |

The shared `smstrikers-port/build-vita-gxm-latest` directory was used. VPK archive CRC and embedded-`eboot.bin` byte identity against the SELF pass. The complete C18 `.previous` build set was not rotated. The C19 install backup remains under `.tmp/native-c20-device-20261019/backup`, and the installer also retained the C19 preimage under `install/eboot.bin.before` for local rollback.

## Quiet frame timing

The initially available C20 quick run collected **300 live-play frames 120–419**, with `fixed_dt=16.666666667`, warmup 120, diagnostics OFF: **15.702046 FPS**, mean 63.685967 ms, p95 66.879 ms. This is **not comparable** to historical C19 frames 600–899 with `fixed_dt=0`.

A second C20 run replicated C19's exact INI override values, except unique CSV/log output paths, and captured **300 consecutive live-play frames 600–899**, seed `0x53545249`, `fixed_dt=0`, no overlay, diagnostics, frame skip, task or packet profiling. Both experimental fog specializations were OFF; both runs reported actual clocks **CPU 444 / GPU 222 / bus 222 / XBAR 166 MHz**. The normalized configuration identity was compared programmatically before accepting the CSV.

| Metric | C19 curve4=0 reference | C20 matched run |
| --- | ---: | ---: |
| Mean FPS | 16.510175 | **16.739271** |
| Mean frame | 60.568710 ms | **59.739757 ms** |
| Median frame | 64.3835 ms | **62.5475 ms** |
| p95 frame | 66.810 ms | 66.817 ms |
| p99 frame | 68.897 ms | **67.542 ms** |
| Mean `tasks_us` | 55.3813 ms | **54.6347 ms** |
| Mean `present_us` | 5.1874 ms | **5.1050 ms** |
| Frames <= 33.333 ms | 0/300 | 0/300 |
| Frames <= 16.667 ms | 0/300 | 0/300 |

The observed increase is **+0.229096 FPS, +1.3876%**. It is one sequential comparison across different boots, source builds, and potentially variable real-time game state; it **does not establish a repeatable or causal renderer gain**. `tasks_us` and `present_us` are CPU-side phase metrics, not independent GPU timestamps. Neither the 30 FPS intermediate target nor the 60 FPS final target is met.

C20 matched CSV: `07f0b08e341250534eab1ee56000d64ab87fad78e696b330061201467eeddb99` (8,853 bytes). See the [comparison receipt](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-device/quiet600/comparison.json), [original C20 CSV](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-device/quiet600/native-c20-quiet600.csv), and [C19 reference](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate19-curve4-0-quiet-1/native-candidate19-curve4-0-quiet-1.csv).

## Visual and audio

The first visual collector used a **135-second timeout** and exited before a complete CSV/PPM was emitted. This was a collector deadline, **not an observed Vita crash**; its `finally` block restored the INI and verified C20 SELF and original cards. Historical C19 visual capture took considerably longer. The complete [failed-collector receipt](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-device/visual-timeout/verified-result.json) is retained and is not relabelled as PASS.

The new labelled attempt with a **330-second capture deadline** finished successfully in **216.2 seconds**. It used C19's exact visual overrides, except unique output file names: diagnostics ON, fixed dt 16.666666667 ms, 120 samples after skip 90, live-play snapshot frame 120, fog specializations OFF. Its 960x544 RGB screenshot is **pixel-identical** to the C19 curve4=0 snapshot: all pixel values match, RGB absolute difference `[0,0,0]`, no bounding box of differences. The C19 PPM reference hash is `5dc7bc75ac6ec9929fcff56590ac6f466d827532103a6f4697d226e06b15674b`; the C20 [PNG capture](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-device/visual120/debug_frame_play_120.png) has hash `c71fff18c69fe2ee4ade5358f73e8d7feb271e3b6ef6fe8ce1c68a5ff69e5031`.

The [C20 visual runtime log](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-device/visual120/native-c20-visual120-retry.log) confirms **82 VitaGL scene programs**, game match entry and at the last recorded audio sample `outputs=18200`, `underruns=0`, `non_silent=1`, `error=00000000`. No matching fatal diagnostic strings were found in this log. This is **not** an acoustic listening test, an all-scene check, or proof that crash dumps were absent from uninspected system locations.

## Restoration and original save integrity

After both accepted tests, the device was left with C20 still installed and the ordinary 760-byte INI restored. The visual retry's `restore-config` child returned zero and targeted kill was acknowledged. The before/after card bytes were unchanged and match the latest C19 closing manifest:

- `cardA/4d6172696f536f63636572.sav`: 40,960 bytes, `34367251ec96e1adde5fa3f85eb05054ef66e84ea73ba08e5a95537c9e531800`.
- `cardA/4d6172696f536f63636572.stat`: 116 bytes, `4d04275d4e91cfc693e3a29989e17d70229ce6f26b79724c5edeaec42ef49c3f`.
- `cardB` was empty in the retained C19 card manifest; the two known original files were the bytes explicitly re-read in C20 verification.

The full source of the local automation, raw capture PPM, host-only preimage and the single C19 executable backup remain under `.tmp/native-c20-device-20261019`. **No game save, disc data, VPK, SELF or entire build cache was copied into evidence.**

## Evidence and limits

The [SHA-indexed evidence manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-device/manifest.json) lists 20 small retained files and their SHA-256 values, plus the current build identities. Manifest SHA-256 at archival time: `7b56bc77844620b5f1fff160baf0f437592f5216385e0583faf0d6cca20fcff1`.

**Acceptance:** C20 compile/package PASS; installed executable identity PASS; bounded quiet CSV PASS; one matching visual scene PASS; ordinary INI and original card retention PASS. **Open:** sustained 30 FPS, 60 FPS, full gameplay, pass/shot/goal transition, all cutscenes, all audio paths, crash-free extended play. The result should be treated as a finite candidate validation and not a whole-game certification.
