# C20 — deferred slot acquisition, source checkpoint

## Status

**C20 is implemented and passes the finite host acceptance checks.** Root and an independent renderer reviewer approved the ownership change and the final regression gate. The actual full candidate run exits 0; the source preimage fails with the required early-wait witness and completes cleanup. The [root source review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-source-root-review.json) binds the source identities, processes, logs and preserved C19/C18 artifacts.

**Device update, 19 October 2026:** C20 has now been built and installed. The shared Release build completed with code 0 using native LTO OFF, hot LTO ON, profiler OFF, profile autostart OFF. VPK CRC verified and embedded `eboot.bin` exactly equals `strikers_vita.self` (2,651,942 bytes, SHA-256 `a9ef59c940ba70097695cd514aa03483f07aaa2558ec133d717477ccafb53751`). The VPK SHA-256 is `40b73f8005b710749ced50254e36bee3b7d1c98338968af51e18c86a879fddcf`. FTP readback independently confirmed that C20 is installed. The former C19 SELF (2,645,700 bytes, SHA-256 `90d5e6cc6f36ab0f472f65ffcf203c9973e60fabcf26fc7d6c040a88b4fe54a7`) and original ordinary INI (760 bytes, SHA-256 `aac80bdf22392a11167a11dd9f713ffcff4088d3e9c898f9e6e50f5314620759`) were backed up in `.tmp/native-c20-device-20261019/backup` before installation.

The C20 device launch returned `Launched.` and produced its verified capture-start marker. One initial quiet run recorded **300 consecutive live-play frames 120-419**, CPU/GPU/bus/Xbar **444/222/222/166 MHz**, both experimental fog flags OFF, diagnostics OFF and FPS overlay OFF. Its CSV is `.tmp/native-c20-device-20261019/quiet/native-c20-quiet.csv` (8,879 bytes, SHA-256 `9ea33bd58390bd5c89a04cd8602cdbb99d89685c42832702be25e21ecd45211b`): **15.702046 FPS**, mean **63.685967 ms**, median **66.626 ms**, empirical p95 **66.879 ms**, 0/300 frames at >=30 FPS and 0/300 at >=60 FPS. That early quick run does **not** demonstrate an FPS gain or regression over C19's 16.510175 FPS OFF measurement because its frame window and `fixed_dt` differ. Broader gameplay/collision/input acceptance was not performed.

After the run, `restore-config` returned `Killed.` and restored the original 760-byte INI. A subsequent independent FTP readback confirmed C20's SELF and the original INI hashes exactly. Personal save cards and application assets were not modified by this workflow, and the existing C18 `.previous` rollback set remains in place. The prior sentence and local-build instructions below describe the **original source-only checkpoint at creation time**; they were subsequently executed in this device continuation.

**Completed device follow-up:** A second quiet run used C19-equivalent flags (except unique output paths) for the same 300 live-play frames **600-899** with `fixed_dt=0`: **C20 16.739271 FPS versus C19 16.510175 FPS** (+1.3876% observed; no causal claim), p95 66.817 ms, **0/300 frames at 30 or 60 FPS**. A diagnostic visual run completed and yielded a 960x544 screenshot at play frame 120 **pixel-identical to C19**. Its first 135-second collector attempt timed out but restored device state; the bounded retry completed after 216.2 seconds. The original two card files and ordinary INI were verified unchanged after both completed sessions. See the [C20 full device report](NATIVE_C20_DEVICE_2026-10-19.md) and [20-file evidence manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-device/manifest.json). Gameplay and stable-FPS acceptance remain open.

## Problem and decision

Before C20, `DeferredRenderer::Impl::publish()` published an immutable command batch, advanced `writeSlot`, then waited immediately if the next ring slot was still busy. At the 16-draw publication boundary this could block `draw_scene()` before the frontend prepared the next packet's material and geometry, even though no further command-buffer write had been requested yet.

The selected change delays only that acquisition. It does not reduce the renderer owner's work and does not establish an FPS improvement. Previous C12/C15 wall-time profiles made a broad frontend-cache rewrite a weak first choice: owner-draw wall time rose substantially while packet preparation excluding its own wait remained approximately unchanged. Those overlapping scopes are not GPU timestamps.

### Reviewed implementation contract

- A producer-only `writeSlotAcquired` flag records whether the current slot can be inspected or written by the producer.
- Publication checks the flag before reading the command buffer's size. An unacquired slot may still be executing on the owner and must not be read or published again.
- After advancing the slot, publication sets `writeSlotAcquired = !busy[writeSlot]` while holding the existing mutex. A free slot stays acquired without an additional lock on the normal path. A busy slot remains unacquired and publication returns.
- Both `append` and `emplace` acquire the slot before their first append and before a retry after a capacity-triggered publication. Acquisition waits for `busy == false` under the same mutex used by the owner after command execution and destruction.
- The ring stays at eight 2 MiB CPU buffers. The one GPU-owner thread, publication every 16 draws, immutable payload copies, FIFO order and explicit synchronous drains remain unchanged.
- `drain()` continues to publish owned pending commands and wait until `retired >= issued`. It does not acquire an empty next slot. Resource results, oversized borrowed payloads, end-frame, readback and shutdown preserve their existing lifetime barrier.
- Start and shutdown reset the ownership flag. Wait counters move to the acquisition point; the unprofiled path adds no clock reads. The wait may be attributed to the next command in diagnostics.

The independent review checked all buffer accesses in the translation unit and the command buffer's execute/reset ordering. The selected scope supersedes the old three-stage proposal for this iteration; it does not implement that wider design.

## Completed host evidence

The gate compiles the actual production deferred renderer translation unit, both current and archived preimage, against the same updated host driver. The driver holds the owner inside its first draw, and the producer explicitly observes that handshake after draw 16. Publication of the eighth batch then returns before the owner is released, allowing CPU preparation. The following append waits for the slot to be retired. A separate controller always releases the owner; its timeout is a cleanup bound and is not a success or regression oracle.

One full candidate run completed with exit **0 in 10.216020 seconds**. It includes the pre-existing immutable-payload, ring-reuse, synchronous oversized-command, failed-frame, bitmap, loading, restart and timeline cases, followed by these five new cases:

| New case | First wait observed at | Draws verified | Copy records verified | Result |
|---|---|---:|---:|---|
| Next draw after the full ring | Draw 129 | 129 | 0 | PASS |
| Initial `append` after the full ring | Next copy command, stage 129 | 128 | 3 | PASS |
| Shutdown without another append | Drain, stage 129 | 128 | 0 | PASS |
| `emplace` retry after capacity publication | Retry, stage 300 | 113 | 24,576 | PASS |
| `append` retry after capacity publication | Retry, stage 300 | 112 | 49,152 | PASS |
| **New gate total** | | **610** | **73,731** | **5 PASS** |

Stage 300 is a test label, not a draw count. All five cases verify FIFO order, exactly-once execution, immutable copied data, completed cleanup and no timeout. The initial append and emplace paths and both capacity retries are covered. The shutdown case exercises an unacquired slot without a subsequent append, so the ownership guard also protects empty publication and drain.

The archived preimage, linked to the same driver, completes with the expected exit **64 in 1.258830 seconds**, after releasing and joining the owner. Its exact witness is:

```text
FAIL premature_acquire_after_publish draw=128 first_wait=128 fifo=1 cleanup=1 timeout=0
```

All **seven child processes** were collected: three compilations, two links and two runs. The complete runner exits **0 in 51.801377 seconds**, and its final executor result was collected. Compilation/link stdout and stderr are empty. Runtime stderr contains only ordinary backend log lines; there are no AddressSanitizer, UndefinedBehaviorSanitizer or float-cast-overflow diagnostics. Leak detection was disabled for this host run.

Root independently verified **103 inputs** against their before/after identities, **1,081 distinct compiler dependencies**, all **14 recorded output streams**, the objects/binaries, the three current postimages and the archived source preimages. The system and SDK dependencies were recorded after compilation and rehashed during review; this is not a pre-build environment freeze.

### Source and evidence identities

Paths in the following table are relative to `smstrikers-port`:

| File | Bytes | SHA-256 |
|---|---:|---|
| `src/platform/deferred_renderer.cpp` | 30,496 | `b13bfa4682bbb2a7d5570acd2825fae73964fa4de78c5ad14fcfc7cc6b1e9d29` |
| `tests/deferred_renderer_driver_test.cpp` | 31,764 | `620d76aa407dfc0f2b9f6198ae8e2b1b1897cb4a234215d982a61b428610ffbb` |
| `tests/deferred_slot_acquire_cases.inc` | 10,438 | `c55c350a88b2c166cc2da00d42f58219f03ab89d16da404f9fa7c9715ef63de4` |

The original renderer preimage is 29,378 bytes, SHA-256 `197ae293902a964e95b589fcabd4923e25c16d33f38a4ec594b0b5e35e40ee1a`. The 10,252-byte [root review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-source-root-review.json) is SHA-256 `ae135f347ed3eede9be6bc6bf239d942141455e825ec9f9dbd9c548aae93f6bc`.

The full 862,761-byte host receipt has SHA-256 `acaa39cea0b8132886d7df2e1957e562748b07831869902a421d852e1c383407`. Its [174,188-byte gzip archive](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-host/results.json.gz) decompresses byte for byte to that original receipt. The [candidate output](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-host/candidate-full.stdout) and [preimage witness](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-host/preimage-witness.stdout) are retained beside it, with both runtime stderr files.

The host gate is part of the existing `deferred_renderer_driver_test` target. Its one-off comparison recipe and the preimages remain under `.tmp/native-c20-deferred-20261019`; the consumed recipe SHA-256 is `c7c1b822db86bf761c8873bd861f4330808f2a16b7bc1c230bc3c532403d9e76`. No additional test run was used to obtain the accepted result.

These finite host checks and the static ownership review do not certify GPU behavior, every possible schedule/race, Vita performance or the SDK driver's behavior.

## Other findings retained for follow-up

The optional balanced fog-range selector is a separate proposed experiment. It can select the same adjacent coefficients in three or four source-level comparisons, but Cg may compile the linear and balanced forms similarly. It needs a separate opt-in generator change, exact numeric/source checks and a device comparison before any performance conclusion. It is not part of this source candidate.

The mono-stream EOF zero-fill offset omission is inherited from the decompilation (`decomp/main` commit `d1f357ffeb1d8e89311edebf9a4b304b05ece7bb`, `GCStream.cpp` blob `45bbbd0a4ba9baf7ca2aa703f85834294be2bc82`). It is a latent direct-call contract mismatch: `memset` omits `MRAMOffset` while the callback includes it. However, the normal mono caller uses `GetUpdateReadLength()` to round the remaining file bytes to 32; in the examined EOF path `ReadASize` equals that request and `ReadBSize` becomes zero. Subsequent updates use the separate end-at-update branch with the correct offset. No ordinary game-path failure was established, and no retail-DOL comparison was performed. The audio line is therefore unchanged in this candidate. A synthetic direct call that exceeds the normal requested length must not be presented as a reproduced in-game audio defect.

The earlier pitch/subframe/lifetime mixer and uncached vertex-program creation fixes are already integrated: `audio_mix.cpp` SHA-256 `5645d74a5e963e9122681f0a4a38fe6cdcdc7fd41648afb2f43144259565580f` and `custom_shaders.c` SHA-256 `9851c5590f2a2d111d2a2da44c0c89703ebf94b919bfe8b86e8caaced3c2e7af` match the handoff postimages. They were not reapplied. The distinct `SAMPLE_INFO::offset` gap is not covered by that earlier subframe fix.

## Local Vita build commands (executed in the later device continuation)

The commands below were originally prepared for a local build, then executed for C20 successfully. They reuse the shared build directory and explicitly restore the ordinary compiler/profiler policy:

```sh
cd /Users/robin994/Documents/Code/strikersVita
export VITASDK=/usr/local/vitasdk
cmake -S smstrikers-port -B smstrikers-port/build-vita-gxm-latest \
  -DCMAKE_BUILD_TYPE=Release \
  -DSTRIKERS_VITA_NATIVE_LTO=OFF \
  -DSTRIKERS_VITA_HOT_LTO=ON \
  -DSTRIKERS_VITA_PROFILER=OFF \
  -DSTRIKERS_VITA_PROFILE_AUTOSTART=OFF
cmake --build smstrikers-port/build-vita-gxm-latest \
  --target strikers_vita.vpk-vpk --parallel 10
```

The existing complete C18 `.previous` set is the sole rollback set. The build commands themselves do not rotate it or deploy anything. The generated package is `smstrikers-port/build-vita-gxm-latest/strikers_vita.vpk`; subsequent installation, artifact identity, finite visual and quiet checks are documented separately in the linked C20 device report. Compilation alone is not device validation.

## Remaining target acceptance

The bounded C20 visual and quiet checks have now been completed on a recorded executable/configuration, but one run per version cannot establish repeatable improvement. Host scheduling regression success and one pixel-identical scene do not close the stable-30/60-FPS or full gameplay-coverage goals. C19's single quiet OFF/ON pair was 16.51/18.27 FPS and had startup/workload limitations; the C20 comparison intentionally uses the OFF=0 reference with the same frame interval and effective flags.
