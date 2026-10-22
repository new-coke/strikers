# Vita: immutable frame queue, readbacks and render-only frameskip

## Scope and reference

The VitaGL provider is built from this repository, not the global SDK archive.
`CMakeLists.txt` already defines `READBACKS_SPEEDHACK=1`, `DRAW_SPEEDHACK=1`,
`HAVE_SHADER_CACHE=1`, and `NO_DEBUG=1`. The provider has the
`READBACKS_SPEEDHACK` guard in `extern/vitaGL/source/framebuffers.c`.
Verify the *linked* archive and installed EBOOT before attributing any
improvement to these definitions.

The relevant RE4 reference is `.tmp/re4-reference-20261005/`, especially
`platform/vita/gx_backend.inc` and `src/game/main.cpp`. RE4 uses an
eight-slot, 2 MiB/slot immutable command ring and a separate GPU owner. A
complete frame is a **sequence of immutable published slots**; it need not be
copied into one monolithic allocation.

## Frame ownership

Strikers already copied dynamic vertices, indices, matrices and materials into
bounded command slots, published after 16 draws and executed them on the owner
thread. However, every `begin_frame` and `end_frame` was synchronous, so the
frontend could not prepare the following frame while the owner rendered.

With quiet `vita_async_renderer=1`, both frame boundaries are now ordered
commands. The producer publishes the final immutable slot and records its
retirement sequence. At most **two complete frames** may be in flight; before
a third the producer waits for the oldest frame's completion. No render
command is allowed to reuse a slot still owned by the backend.

Resource creation, oversized borrowed-pointer commands, bitmap presentation,
and screen readback keep synchronous calls. Diagnostics, finite frame capture
and packet timelines force synchronous frame boundaries so their storage and
timestamps remain valid. An owner command failure latches a fatal state and
stops further quiet frame production. Nonfatal resource-admission errors do
not poison that latch. The GPU worker remains the sole VitaGL context owner.

## Copy path

Ordinary EFB-to-texture color copies use a GPU-owned textured quad, the same
target/restore and source-rectangle path as intensity/alpha copies, rather
than `glBlitFramebuffer`. Host conformance checks verify raw/converted
source corners, destination regions, failure cleanup and exact target restore.

**Two blits remain intentionally**: current scanout presentation and front
display loading-background capture. They have different framebuffer
orientation/lifetime contracts and must be replaced only with a separately
validated direct-GXM or textured-present experiment. Do not suppress scene
resolves or add `glFinish` / `glFlush` to hide image corruption.

## Frameskip modes

Set in `ux0:data/strikersVita/strikers.ini`:

| Option | Meaning |
| --- | --- |
| `vita_frameskip=0` or `OFF` | Default: render every logical frame |
| `vita_frameskip=1` | Alternate render/skip when live gameplay is eligible |
| `vita_frameskip=2` or `AUTO` | Bounded wall-time debt, maximum one consecutive renderer skip |
| `vita_async_renderer=1` | Default: queue up to two immutable frames across frontend/owner |
| `vita_async_renderer=0` | Synchronous A/B control for the original frame boundary behavior |

Only stable live gameplay is eligible for frameskip; menus/transitions, finite
captures and deterministic timing are excluded. Game tasks and audio still
execute, and `glSendFrame` still resets render lists and advances its counter.
Skipped frames suppress GPU packet submission and swap, leaving the prior
scanout visible. The normal FPS/game loop counter is **not** the number of
newly rendered images when skipping is active.

## Verification and remaining risks

- Host `vitagl_renderer_driver_test`: passed with blitless color-copy
  assertions and raw/converted corner comparison.
- Host `deferred_renderer_driver_test`: passed including a stalled GPU-owner
  test with two queued complete frames and 64-frame backpressure stress.
- Host `frameskip_scheduler_test`: passed OFF / 1 / AUTO, PAL, time regression,
  suspend reset and maximum one consecutive skip.
- `git diff --check`: passed.

## Real-device validation: 2026-10-21

Release VitaSDK build completed, and VPK resources were verified. The installed
SELF was read back byte-identically from `ux0:/app/SMSVITA01/eboot.bin`.

| Item | SHA-256 |
| --- | --- |
| Previous installed EBOOT (backup retained) | `286953f7bef881565a831f6d34e4822f3ec8527deadca0168f325a9fd6cfc04c` |
| New installed SELF | `5f7779bfb573a5a32212f3eaa721fa571259a6a9dfaae298a72c13d5378214c8` |
| New generated VPK | `16c97e226da19e76de1700c6853d7262e19e0ae6c0c2535341e0ec59f4ddf427` |

Three quiet runs on retail hardware used 300 consecutive live-play logical
frames 600-899, CPU/GPU/bus/Xbar 444/222/222/166 MHz, diagnostics/overlay OFF,
fixed_dt=0, and uniform/affine/vertex-fog options OFF.

| Build | Renderer async | Mean FPS | Mean frame | P95 | P99 |
| --- | --- | ---: | ---: | ---: | ---: |
| Previous EBOOT control | OFF | 26.200 | 38.168 ms | 49.699 ms | 54.686 ms |
| New candidate | ON | 16.918 | 59.107 ms | 66.778 ms | 76.737 ms |
| New candidate | OFF | 16.897 | 59.181 ms | 66.743 ms | 68.257 ms |

**This is a 35.4% regression from the previous installed EBOOT, not a
performance improvement.** Async ON versus OFF has negligible mean FPS impact
in this controlled sample; it does not explain the 9.28 FPS difference. Many
other code paths differ from the previous binary. Do not attribute the
regression to one specific change without further isolation.

The run configuration requested `vita_frameskip=AUTO`, but the finite frame
capture disables actual render skipping while collecting samples. These FPS
figures compare the full-render pipeline; they do **not** measure visual
smoothness or game pacing with AUTO enabled during ordinary play. Normal
startup with persistent `vita_frameskip=AUTO` and `vita_async_renderer=1`
was also confirmed. The managed defaults block contains `vita_frameskip = 0`,
but the first occurrence *above* the block is `AUTO` and takes precedence.
The normal-start INI readback remained stable, SHA-256
`12e92cbc996ea3c83f19e339ed3bde991aa08cc78f194a8137b47bda3d323279`.

After testing, synthetic gameplay/capture settings were removed, the app was
stopped, and installed EBOOT/INI plus both Card A save files were rehashed.
They match the expected bytes; Card B remains empty. The current candidate is
left installed, as explicitly requested, despite the regression. The prior
installed EBOOT and INI backup are retained under
`.tmp/strikers-async-frameskip-hw-20261021/baseline/` and the exact prior
installed EBOOT is also in `install/eboot.bin.before`.

Evidence (small text and CSV):
`.tmp/strikers-async-frameskip-hw-20261021/` with `control/`, `candidate/`,
`async-off/`, `candidate-comparison.json`, `final-device-state.json` and
the build/installation receipts. No full-gamepad human playthrough, post-goal
progression, visual verification or long-duration stability acceptance was
performed. Investigate this source-wide regression before promoting the new
binary as the performance baseline.
