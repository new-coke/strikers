# Midterm: rendered cutscenes at 30 FPS and native MP4 playback

## Acceptance and scope

The target is **stable 30 newly rendered images/s**, followed by 60 FPS.
Priority scenes, confirmed by the user: **goal/replay, stadium entrance,
and trophy presentations**. Preserve the original scripts, animation timing,
camera, effects, sound and transitions. Render skipping and a logical-loop
FPS counter do not establish this target.

Evaluate each scene independently: quiet runs, identical installed SELF,
archive/INI/clocks, complete scene coverage, median/p95/p99/worst frame time
and the fraction within **33.333 ms**, followed by the 16.667-ms budget.
Repeat controls/candidates before promoting a renderer optimization.
The 30/60 FPS gates are **open**; this change does not claim a renderer gain.

## Existing baseline

The main worktree contains substantial staged and unstaged C20/async work.
It was preserved. The shared Vita cache was already bound to
`.tmp/strikers-c9-frameskip-razor-source-20261022/smstrikers-port`, base
`512e9cc5a`, with existing opt-in frameskip changes. This source binding was
retained. Only the MP4/scene-capture feature was overlaid for the device build;
the C9 shaders, VitaGL provider and renderer ownership were retained.

The installed control was read back before testing:

- EBOOT `7c8c6cd8d9f9857173cd51e22507654764e60d1c29b3d9d1b493be53225f3d6e`.
- INI `84d6485e6ad729e01a35784a1bdaa2057e9e217c4a87312fe2c717d4205e7576`.

The prior Razor intro report is at the staged C9 source's
`performance/RAZOR_INTRO_C9_PLAN_2026-10-22.md`. Its 19.12-Hz flip interval
and CPU aggregates describe that older capture, not this candidate. Geometry,
material, texture resolution and owner-side state submission remain the
measured optimization priorities. No new workers or speculative shader
changes were enabled here.

## MP4 conversion and archive

`prepare_strikers_vita.py` now defaults to `--video-format mp4`.
`--video-format wavc` preserves the previous conversion for older builds.
FFmpeg/libx264 and ffprobe are required. Video retains the original dimensions,
frame count, rate and full-range YUV420; H.264 Baseline uses no B frames.
Audio is decoded with the port's 64-bit DSP arithmetic, validated against
THP sample extents/counts, then encoded as AAC-LC stereo at 48 kHz.
The audio codec is lossy. ffprobe checks the output and FFmpeg decodes it
completely before archive publication.

The real European ISO produced **17/17** verified clips:

| Content | Bytes |
| --- | ---: |
| Original THP | 161,989,120 |
| Converted MP4 | 59,498,731 |
| Complete native game PSARC | 1,733,103,570 |

Complete archive:
`.tmp/asset-setup-validation/sms-mp4.psarc`

SHA-256:
`a8c56615600e8f700807cb460f4f12df2d9aae5d0bc28d9e90bca9464f481105`.

Its adjacent preparation report and an independent `--verify` run confirm
17 MP4 entries, 17 `STMP4001` descriptors and **zero THP entries**. Native
graphics/audio and archive/source round-trip verification also passed.
Generated game assets are local and are not committed.

MP4 entries live at `native/v2/video/<original path>.mp4`. The descriptor
stores width, height, frame count, rational rate and audio presence in 32
little-endian bytes. Native graphics packs remove a corresponding original
THP only after its successful conversion. Movie payload blocks are stored
without additional zlib compression for positional streaming.

The smaller movies-only PSARC used for bounded device testing is
`.tmp/strikers-midterm-20261022/movies-mp4.psarc`, 59,508,733 bytes,
SHA-256 `3735b859ea2717a73fe224bad6af74e9667638dd7a13b9e4438931d24e3154e3`.
Its installed copy `ux0:data/strikersVita/movies-mp4-midterm.psarc` was read
back completely with the same digest. The existing game PSARC was not replaced.

Normal configuration for the complete archive:

```ini
asset_archive = ux0:data/strikersVita/sms.psarc
vita_native_video = 1
vita_video_player = avplayer
vita_video_archive =
```

An optional `vita_video_archive` selects a separate movies-only PSARC.
This is used for the test overlay above. `vita_video_player = thp` selects
the old WAVC consumer. A missing MP4 retains the previous movie-open path;
an MP4-only complete pack requires the new consumer.

## Runtime implementation

- SceAvPlayer reads the PSARC entry through positional file callbacks. It
  does not extract a temporary MP4 or load the complete clip into RAM.
- The game retains its `MovieStart`/`MoviePlay`/`MovieStop` calls, named movie
  textures, UI geometry, skip and loop behavior, plus Strikers 101 announcer
  pause/resume.
- Delivered frames become three owned, linear Y/U/V planes. The GPU owner
  uploads one byte/pixel with `GL_LUMINANCE`, avoiding GX I8 tiling,
  untile/redecode and CPU expansion of those planes to RGBA. This is a copied
  upload, not a claim of zero-copy decoder textures.
- AAC output has a separate BGM port and worker. Two owned PCM blocks keep
  AvPlayer's borrowed frame pointers out of AudioOut's retained-buffer lifetime.
  The original volume curve, mute and fade commands are forwarded.
- Decoder output is copied before the next video poll. Textures are admitted
  synchronously on the existing graphics owner and retired by its ordered
  release path. Movie teardown joins audio before closing the player/archive.

## Cutscene capture

The finite quiet capture can now select original presentation phases and
write a joined `.scenes.csv` sidecar without per-frame file I/O. It records
task state, previous state, presentation function and scene instance/frame.
The base frame CSV schema is unchanged.

Example for a bounded entrance sample, with one occurrence of every key:

```ini
diagnostics = 0
fps_overlay = 0
fixed_dt = 0
vita_frameskip = 0
frame_capture = ux0:data/strikersVita/entrance.csv
frame_capture_frames = 90
frame_capture_match_only = 0
frame_capture_play_only = 0
frame_capture_scene = intro
frame_capture_skip = 0
```

Other selectors: `goal`, `replay`, `trophy`, `presentation`, `cutscene`, `all`.
The sample count must fit the scene being exercised; completion of a short
finite sample does not prove full-scene coverage or stable FPS.

```sh
python3 smstrikers-port/tools/analyze_cutscenes.py entrance.csv --out entrance.json
```

The analyzer checks sample identities and scene continuity, rejects diagnostic
captures as quiet FPS, and reports per-presentation/instance frame budgets.
The synthetic test's 28.57-FPS numbers are **fixture data**, not Vita results.

## First device attempt and crash preservation

The first candidate was installed and read back exactly:
SELF `902441fabb8491518665502c66dda8276d1cc1d4946793fd8e7957048d2a0dc3`.
It produced a coredump before any `[mp4] selected`/picture evidence.
The startup marker and initial 60-frame diagnostic capture are **not**
valid playback, stability or performance acceptance.

Crash:
`psp2core-1792699578-0x000f4b2d9d-eboot.bin.psp2dmp`, captured locally in
`.tmp/strikers-midterm-20261022/`.

The dump identifies `avPlayer Controller`, exception `0x30003` (prefetch
abort), PC `0x8fd30eb0`, LR `0x8fd2b8a1`. Its PC is outside the dump's listed
modules. These addresses alone do not prove the failing SDK call. Stack-word
symbolization against the exact relocated app ELF identifies the movie-open
path, but stack-word candidates are not an unwound call stack.

The exact crash ELF, map, VELF, SELF and VPK are preserved as the single
`.previous` set in `smstrikers-port/build-vita-gxm-latest/`.
No duplicate complete build set was created in `ab-artifacts`.

After the user explicitly confirmed Home recovery, the original C9 EBOOT
and exact INI were restored and read back. Card A's `.sav`/`.stat` hashes
remained `34367251ec96e1adde5fa3f85eb05054ef66e84ea73ba08e5a95537c9e531800`
and `4d04275d4e91cfc693e3a29989e17d70229ce6f26b79724c5edeaec42ef49c3f`;
Card B remained empty.

A subsequent device run exposed a concrete integration error:
`sceAvPlayerInit` returned **`0x81f05ec0`**, a valid user-memory handle.
VitaSDK typedefs the opaque handle as signed `int`, so the original
`handle < 0` check incorrectly selected the failure/unload path. The runtime
now separates handle validity from its sign and rejects null, `-1` and SCE
error results in the `0x80xxxxxx` namespace. The host mock returns the exact
observed signed handle, so initialization, polling, volume and close exercise
this case under ASan/UBSan.

AVPLAYER remains mapped until process exit and the callback init table is
owned by the player. Per-movie instances/frame buffers still close. Explicit
init/source/audio/EOF failure-stage logging and two output frames are used.
The unload following handle misclassification is consistent with the
controller thread's unmapped PC in the first dump; the dump alone does not
prove a complete call stack.

After this correction, hardware playback opened `intromovie.mp4` through the
PSARC callbacks. A direct renderer readback shows the full-color movie in its
original FE geometry, and the user confirmed **fluid video and audio**.
The first two successful runs were manually skipped, as confirmed by the
user: 344 delivered pictures / 19,586-ms last PTS / 20,277-ms wall time, then
841 pictures / 36,269-ms PTS / 36,949-ms wall time / error zero. These are
**partial diagnostic runs**, not full-speed or all-frames acceptance.
No additional coredump appeared in the dump inventory after these runs.

The latest candidate for the complete playback check is SELF
`e34d2496d0a5e46d43d085ed5422ccd46415e451eed0a0b68475bbedca62ac1b`,
VPK `94fd5227d861b73b4242445dc2f17ebea8ab6fc41032a6fde6f30842af5490f0`.
The installed SELF was read back exactly. Clock policy is verified at
444/222/222/166 MHz; renderer skipping is OFF and `fixed_dt=0`.

## Complete MP4 run and first quiet entrance sample

The subsequent unskipped intro reached normal player EOF without a crash:
**1,502 delivered pictures**, last timestamp **64,998 ms**, wall time
**65,745 ms**, player error **zero**. The converted source has 1,949 pictures
at 29.97 Hz. This proves complete timeline playback and successful teardown,
**not delivery of every authored picture or full-frame-rate acceptance**.
No screenshots were requested during this movie run. Polling/upload cadence
still needs attribution; the user's smoothness report does not supersede the
missing-picture count.

A separate quiet run used the same SELF, 444/222/222/166 MHz, diagnostics and
FPS overlay OFF, `fixed_dt=0`, render skipping OFF, INI-selected test match,
and `frame_capture_scene=intro`. Its **90 contiguous `GameBegin` samples**
reported **17.029 FPS**, median **55.059 ms**, p95 **63.576 ms**, and worst
**311.137 ms**. Two initial samples straddle the transition into presentation;
the following 88 presentation-state samples average **17.769 FPS**, p95
**63.471 ms**. Only **2/90** frames fit 33.333 ms. This is a bounded first
entrance sample, with startup effects and without full-scene visual coverage,
not a paired renderer A/B or a complete scene result.

CSV SHA-256: `4bb4bae2f568fbccf105c9825cb18fad650382af338ad011b7df131243c9d88a`.
Scene sidecar SHA-256: `7a32a6daff9e4058df0f68d187c555b047f3fc636b79e68e155beac37e5f4827`.
Raw CSVs, config/install receipts and analysis are in the `entrance-quiet/`
subdirectory of the evidence directory. Goal/replay and trophy measurements
remain pending. There is no demonstrated rendered-cutscene FPS gain here.

## PlayStation glyph option and tester handoff

The native prompt path now honors `vita_playstation_glyphs`: **1 by default**,
0 keeps original GameCube glyphs; apply changes at process restart. Existing
A/B/X/Y mapping becomes Cross/Circle/Square/Triangle, with original font metrics
and original L/R cells. Supported native legends and controller badges use the
same art. Movie pixels and literal button names are outside this substitution.
Host tests cover Western/Japanese layouts, default/on/off, unchanged metrics,
failed admission and teardown after an option change. A device readback of the
main menu shows Circle Back and Cross Continue with unchanged original layout.

Glyph-test SELF: `ac2d4e66fafcfc76391271d6f70630d58aeeefccef30995e9cf97db92b837a0b`.
The device generated `vita_playstation_glyphs = 1` without an override. After
this test the original INI was restored exactly (original SHA above), and the
app was stopped. This is C9 feature evidence, separate from the subsequent
Release candidate built from the source commit pushed to `main`.

The local tester build workflow is documented in `TESTING_RC.md`; its manifest
binds the exact source commit, VPK/SELF and the quiet tester INI. VPKs and frozen
retail video probe inputs stay local. Build profiler, autostart and the frozen
AVC probe are disabled. No C9 result is transferred to the new release artifact.

## Evidence and remaining gates

Small receipts, conversion reports, host logs, device identities and parsed
crash/stack evidence are under `.tmp/strikers-midterm-20261022/`.
The build cache retains the C9 staging source binding; inspect
`CMAKE_HOME_DIRECTORY` before a future build. Profiler and autostart are OFF.

Host evidence includes ASan/UBSan player lifecycle/I/O/owned-frame/audio tests,
single-channel renderer uploads and existing renderer contracts, DSP output
against the compiled production decoder, real ISO conversion/archive
verification, finite capture/retry tests, scene analysis, default INI checks
and movie shader UV/color tests. A successful build is not device acceptance.

Open device gates: MP4 delivery of all authored frames at the authored rate, repeatable correct color
and aspect, audio listening/synchronization, loop/skip/reopen behavior,
all 17 movie paths, and repeated quiet captures of the three rendered
cutscene categories. **Stable 30 FPS and 60 FPS remain unachieved.**
