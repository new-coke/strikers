# Functional coverage audit - 2026-10-18

This is an implementation and evidence inventory for the current standalone
VitaGL port. It does not certify 100% whole-game coverage or 60 FPS.

## Current hardware checkpoint — 20 October 2026

The installed application is now a **reconstructed C9-like VitaGL SELF**, not C20 or the unrecoverable bit-identical historic C9 binary. SELF SHA-256 `7c8c6cd8d9f9857173cd51e22507654764e60d1c29b3d9d1b493be53225f3d6e` was confirmed by FTP readback. The 300-frame synthetic-match quiet run on retail PS Vita covers live-play frame indices 600–899 with the historical C9 settings and fixed clocks, returning **27.114365 FPS**, median **34.384 ms**, p95 **47.686 ms**. **72/300** frames fit the 30 FPS budget and **0/300** fit 60 FPS. This is a verified finite boot/game loop/frame-capture test only: there is **no new human movement, pass, shot, goal UI, full gameplay progression, audio listening, persistence stress or suspend/resume acceptance** on reconstructed C9. Original `strikers.ini` and both Card A files were SHA-verified unchanged; Card B is empty. The active 2026-10-20 source checkpoint is local Git branch `recovery/c9-hardware-27fps-20261020` at `a5fe9b9422c49e50ef8f4fdf21cfde67329d7ab2`, and the primary dirty C20 checkout remains intact. See [reconstructed C9 hardware report](C9_RECONSTRUCTED_HARDWARE_2026-10-20.md) and [evidence manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate9-reconstructed-device-20261020/manifest.json). C20 evidence below describes the earlier installed state only.

## Current checkpoint — 19 October 2026

C20 **extended real-hardware evidence**: an automatic match ran 297.27 s with an advancing world trace up to tick 10,130, but **no PAD0 player** was assigned in that mode. A second matched 300-frame benchmark measured 17.046823 FPS, and 299/300 frames exceeded the 33.333 ms budget. A distinct normal-menu run did enter a live match with PAD0/team0, but stalled at tick 2390 near a dynamic screenshot request; a later acknowledged analog input generated no new gameplay trace. **Human movement, pass, shot and post-goal UI remain NOT_ACCEPTED on C20.** A control run without screenshot requests failed to reach the same scene, so the stall's cause is not established. Original cards and INI were restored and verified, with no new dump filename in the checked window. Refer to the [latest C20 hardware report](NATIVE_C20_HARDWARE_EXTENDED_2026-10-19.md) and [evidence manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-hardware-extended/manifest.json).

C20 **completed device follow-up**: Release build, successful Vita installation and hash-verified SELF readback, a quick non-comparable quiet capture (15.702046 FPS on frames 120-419), a **C19-equivalent quiet capture** (16.739271 FPS on frames 600-899, versus C19 16.510175 FPS; 0/300 frames meeting 30 or 60 FPS), and a **960x544 play-frame-120 screenshot identical pixel by pixel to C19**. The first visual collector hit a 135-second deadline, cleaned up safely, and the longer retry completed. C20 remains installed, the original INI is restored, and the two original Card A files remain byte-identical. This supports bounded boot, game-rendering and audio-activity observations only, not human input, extended stability, save progression or full-game coverage. See [C20 device validation](NATIVE_C20_DEVICE_2026-10-19.md) and its [evidence manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-device/manifest.json).

C20 began as a **source-only deferred-ring continuation** and then underwent finite device validation. The production owner/producer queue passes five new deterministic host cases (610 draws and 73,731 copy records), plus the existing driver suite, under ASan/UBSan. The preimage fails on its premature draw-128 publication wait after explicit release/join, with no timeout used as the oracle. The [C20 source checkpoint](NATIVE_C20_DEFERRED_SOURCE_2026-10-19.md) records the reviewed ownership contract and the [root host evidence](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate20-source-root-review.json).

C20's finite Vita test does **not** add a whole-game acceptance PASS or establish an improvement beyond single observed windows. C19 is the prior device checkpoint and C18 the sole complete `.previous` rollback set; C19's executable is separately retained in the local install backup. The previous pitch/subframe/lifetime and uncached vertex-program fixes are already integrated. The separate mono EOF-offset finding is latent in the direct-call contract; the normal caller reduces the examined zero-fill length to zero. `GCStream.cpp` is unchanged. Movement, other human actions, progression, listening, movie coverage, suspend/resume and extended stability retain their existing evidence status.

C13's ordinary D1 CREATE/manual LOAD fixture is complete: CREATE and LOAD
Card A checkpoints match byte for byte, Card B stays empty, normal shutdown
is observed, original cards are restored and verified, and the fixture lease
is retired. The [D1 root closure](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate13-d1-root-closure-review.json)
binds those observations. OOM recovery and progression autosaves remain open.

C14 fog-range/cache changes are applied; the cache defaults to OFF. The separately
reviewed incremental build attempt2 completes with return code 0 following the
stopped orphan of attempt1. Root verifies 3194 source pins, 70 gate references,
all five artifacts and the sole C13 previous set. C14 was installed with complete
staged/canonical SELF readbacks and unchanged INI; its single bounded installer
finishes in 14.085817 s with exit 0 and the child reaped. The
[installation review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate14-install-root-final-review.json)
binds the journal and retained C13 rollback bytes. C14 controls-1 subsequently
reaches actual Free Practice with P1 on team 0. Thirteen guided entry inputs
complete; five entry requests and the only movement request are refused before
send. All fourteen gameplay cases remain NOT_EXERCISED. Native exit at frame
25389 and all four worker exits are recorded in the complete 1181-record watchdog
epoch. Closing identity, no new core and all 41076 original card bytes unchanged
are verified by the [C14 controls root review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate14-controls1-root-final-review.json).

Snapshot fusion, THP wrap correction and D13 selective OOM pass their finite
host gates. The additional [legacy copy/shift gate](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-legacy-copy-shift-host-root-final-review.json)
passes 93 cases under fatal ASan/UBSan: 69 string, 16 key and 8 outline cases.
All four changes are now part of the built and installed C15. The AVC continuity
probe remains OFF; its target linkage is not a decoder or movie-speed result.
The [composition receipt](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate15-source-application.json)
preserves all twenty full-file readbacks.

The first C15 attempt is preserved: its 3282-pin/74-reference source gate,
fresh C14 device checkpoint and single previous rotation remain recorded.
Configuration completed in 3.855194 s; the target compiled the fourteen
script-key consumers and AVC, then failed after 199.749909 s at final link on
unavailable `posix_memalign`. The nine invariants were verified and both
children reaped; no C15 ELF/SELF was accepted from that attempt.

The one-file [Vita allocation repair](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-vita-aligned-new-root-application.json)
then supplies the SDK's aligned-allocation contract. The [attempt2 build review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate15-attempt2-build-root-review.json)
verifies the complete receipt `5aa4438f1e8ce6914ebc2899f6259007c890d1eac6214668cd95a6fc9b8017c4`:
configuration 2.943822 s, build/package 20.582346 s, 3309 source pins, 74 gate
references and all ten final invariants. Only the allocation operator is
recompiled; 30 first-attempt object/dependency entries are reused with exact
hashes, mtimes and actual header bindings. C14 remains the sole previous set,
without another rotation.

C15 SELF `6206560fa7789022e7e0a5f1c89e355e88bb0ca13aa50d155d251d288531c492`
(2631871 bytes) is installed with complete readbacks. The [install review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate15-install-root-review.json)
records 15.922332 s, 80 events, 25 FTP commands and unchanged INI 2eb0746f...64a0b.
The [prelaunch post-install result](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate15-postinstall-1/result.json)
closes in 4.512198 s with two children reaped, exact current identity, no new
core and all 41076 original card bytes unchanged.

C15 controls-1 has closed. Root viewed all 25 completed images; attributed
+X calibration, -X and +Y movement and return to neutral are observed.
-Y was not exercised in that C15 run: movement is PARTIAL, with no new gameplay PASS.
The [independent partial review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate15-controls1-independent-partial-review.json)
records 18 completed inputs and two refusals before send. A ui-log timeout
triggers closure, followed by native Start+Select exit at frame 26027. The
[postflight result](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate15-controls-1-postflight-guardian-1/result.json)
verifies exact identity, no new core and all 41076 original card bytes unchanged.
The timeout and SIGKILL EPERM remain recorded without an assigned cause.

The finite asset result is 83/83 required paths; it is not a percentage of
whole-game behavior. One gameplay control case is PARTIAL and thirteen remain
NOT_EXERCISED. The [C15 quiet root review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate15-quiet-1-root-review.json)
records 14.348838 FPS over 300 live-play frames, mean 69.692053 ms, p95 83.444 ms
and p99 92.755 ms, with no frame meeting 16.667 ms. Historical C12-r1 measured
27.404856 FPS / mean 36.489883 ms. The completed C15 profile localizes the
increase in owner drawing and renderer waits; snapshot and packet preparation
excluding its own wait remain similar. This is not a GPU timestamp or proof
of a single cause. fixed_dt=0 does not keep pose, tick or draw counts identical
across builds.

C17 quiet-1 is complete. The [device review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate17-device-root-review.json)
binds SELF `e159f524e42243dba06a8c28850eceaf6924b604e61e90e3ec399125fde293b7`
and 300 consecutive live rendered frames 600–899: 16.7807938 FPS,
mean 59.5919367 ms, p95 66.809 ms, p99 77.395 ms and 0/300 at or below 16.667 ms.
This is +16.9488% observed FPS versus C15, still below historical C12-r1 and 60 FPS.
All three quiet runs use clocks 444/222/222/166 MHz with diagnostics, overlays,
frame skipping and sleeping OFF. C17's INI matches C15 after only the run label.
There is one window per candidate; fixed_dt=0 does not freeze simulation or
visibility, and C17 needed acknowledged launch-confirm input. The measurement
applies to the combined changes and does not isolate fog lookup causality.
The separate visual run compiles 82 scene programs. Root viewed play-frame 120
with pitch, players, shadows, HUD and stadium boundary present and no obvious
defect in that scene; this establishes neither pixel parity nor all-scenes coverage.
Diagnostic visual FPS is excluded from the quiet comparison.

The [C17 controls review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate17-controls-root-review.json)
supports the bounded -Y movement attempt 2, controller sequence 155. At ticks
4410–4500 the same PAD0/team0 actor, index 2/class 8, moves from
(-2.0256, 6.1866) to (-1.8908, 2.3236). The game records raw left input (0, -56),
axes (0, -1) and desired speed 5.9; neutral input at tick 4460 precedes desired
speed 0 and a stable position with zero or negligible speed at ticks 4470–4500.
Root viewed images 23/24 before and after, with the settle away from boundaries.
That direction is supported on C17; +X/-X/+Y remain historical C15 observations.
Movement stays PARTIAL and thirteen other cases remain NOT_EXERCISED.
Movement attempt 1 and ground-pass attempt 1 are refused before action input;
the second ground-pass request is never dispatched because of the close reserve,
and no shot is requested. Later unattributed PAD activity is excluded from acceptance.

The controls executor closes with actual exit 0 and native exit at frame 28562.
All four serial postflight helpers exit 0; 1190 watchdog records have contiguous
sequences 0–1189, all four worker roles exited and frontend shutdown_runtime.
Exact identity, no new unrelated dump and all 41076 original card bytes unchanged
are verified. Ordinary INI `aac80bdf22392a11167a11dd9f713ffcff4088d3e9c898f9e6e50f5314620759`
is restored. The close-reserve refusal remains recorded; native and worker exit
do not prove OS process absence. The separate quiet/visual release/kill closures
are retained with full identity checkpoints and do not supply graceful-exit proof.

Remaining movie, audio, graphics, progression, sustained stability and 60 FPS
criteria are still open. C16 finished packaging but was not installed: its broad
LTO diagnostics exposed real memory defects and unresolved type mismatches.
C17 is built and installed with C15's original compilation policy. Its build-time
review preserves the then-current C15 rollback set. Build and installation
complete with actual exit 0; the first installation failure and subsequent
normal-reboot recovery are retained. Canonical camera layouts, 23 bounded text
copies, memberwise DSP-header conversion and shader-key identity checks pass
their focused host regressions. The integrated fog lookup passes 266336
bit-identical host comparisons, with the finite C17 device results above.
These source and host results add no gameplay PASS by themselves.

C18's fog-disabled shader specialization is integrated in two files,
experimental and default OFF. Its [build review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate18-build-root-review.json)
verifies the 77.570-second incremental build, actual exit 0, reaped child,
fifteen product identities, five artifacts and VPK/SELF. Only the renderer is
recompiled. The [install review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate18-install-root-review.json)
binds the single successful 41.637974-second transaction and full readbacks.
At that C18 checkpoint, the installed 2645642-byte SELF is
`850cc77b6fac6fccdeaf478e484e79d93204d7d6f6ae80223cf9b3089f8cc339`.
The [C18 device review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate18-device-root-review.json)
accepts the finite same-SELF quiet and visual pairs; the [independent review](../../.tmp/native-c18-resume-20261019/device-independent-review.json)
binds their local evidence and final closure checks.

| C18 quiet flag | Mean FPS | Mean frame, ms | Median, ms | p95, ms | p99, ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| fog-off specialization 0 | 16.2266205 | 61.627127 | 65.218 | 69.767 | 79.900 |
| fog-off specialization 1 | 16.5609864 | 60.382877 | 63.337 | 66.755 | 66.854 |

Both windows contain 300 consecutive live rendered frames 600–899, with 0/300
within either 16.667 or 33.333 ms. FPS is 1000/mean frame milliseconds;
p95/p99 use nearest ranks 285/297. Clocks 444/222/222/166 MHz, seed 0x53545249,
fixed_dt=0 and started_before_input=true match. Only the tested flag and run
names differ after INI normalization. The observed +2.0606% FPS comes from one
sequential pair, with repeatability and stable causal effect unproven; variable
stepping does not freeze simulation workload. The default remains OFF.

The separate visual pair uses fixed_dt=16.666666667, diagnostics=1,
overlay/timing=0, other profilers OFF, affine UV=0 and vertex fog=0. Both record
120 live frames 90–209 and a play-frame-120 snapshot, with equivalent normalized
INI settings and acknowledged press/release startup branches. Root viewed both
960×544 PNGs. PPM RGB comparison finds 0/522240 differing pixels; both PPM and PNG
files are byte-identical. The shared PPM SHA is
`5dc7bc75ac6ec9929fcff56590ac6f466d827532103a6f4697d226e06b15674b`.
Logs show 82/162 programs, fog_off_specialize=0/1 and no compile/link/reflection
failure. All 212/220 audio records show error=00000000 and underruns=0, which is
not a listening test. Equivalence is limited to this one scene/frame; diagnostic
fixed-step snapshot timings add no quiet FPS or all-scenes coverage.

All four C18 captures close with acknowledged release/kill commands, unchanged
full SELF/INI readbacks and complete checkpoints without new unrelated dumps.
The real device root review records collected executor exit 0. The [card manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/cards-after-native-candidate18-closed/manifest.json)
confirms original CardA SAV/STAT and empty CardB unchanged, total 41076 bytes.
The [restore receipt](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate18-restore-ordinary-1/restore-config-identity.json)
restores the ordinary 760-byte INI
`aac80bdf22392a11167a11dd9f713ffcff4088d3e9c898f9e6e50f5314620759`.
Command ACKs do not prove OS process absence; the last full SELF readback precedes
the INI restore. These bounded checks add no new gameplay-control PASS.

The historical [C18 rotation review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate18-rotation-root-review.json)
records C17 replacing C15. After C18 acceptance, the [C19 rotation review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate19-rotation-root-review.json)
verifies the complete C18 set as the sole current `.previous`, replacing C17.
C19 is now installed with the ordinary INI restored. Its default-OFF type-4 curve
specialization retains generic fallback: the finite host gate passes and six
exact product/test postimages are [integrated](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate19-source-root-review.json).
The [C19 build review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate19-build-root-review.json)
verifies completion in 48.337 seconds with actual exit 0 and the child reaped.
The 2645700-byte SELF is
`90d5e6cc6f36ab0f472f65ffcf203c9973e60fabcf26fc7d6c040a88b4fe54a7`.
The [install review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate19-install-root-review.json)
verifies the single 17.535743584-second transaction, actual executor exit 0,
reaped child, full stage/canonical/swap readbacks and unchanged ordinary INI.
The [C19 device review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate19-device-root-review.json)
binds the finite quiet/visual results and final closure checks below.

The [independent quiet review](../../.tmp/native-c19-resume-20261019/quiet-independent-review.json),
SHA `7c2dfbae301db955d1e402701cb09ff0005a713309a86b92f58abab700a32a6a`,
verifies both CSVs and 300 consecutive live rendered frames 600–899 on that same SELF.

| C19 quiet flag | Mean FPS | Mean frame, ms | Median, ms | p95, ms | p99, ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| type-4 curve specialization 0 | 16.5101750 | 60.5687100 | 64.3835 | 66.810 | 68.897 |
| type-4 curve specialization 1 | 18.2731919 | 54.7249767 | 50.7850 | 66.674 | 66.751 |

Each window has 0/300 frames within either 16.667 or 33.333 ms. Clocks are
444/222/222/166 MHz, seed is 0x53545249 and fixed_dt=0, with diagnostics,
overlays, profilers, frame skipping and sleeping OFF. Fog-disabled specialization
stays OFF in both runs; normalized INIs differ only in curve4 and run names.
FPS is 1000/mean frame milliseconds; p95/p99 use nearest ranks 285/297.
Flag 0 has started_before_input=false and acknowledged launch-confirm
press/release; flag 1 has started_before_input=true without those confirmation
inputs. The +10.6784% observed FPS and 5.8437333 ms lower mean are descriptive
results from one sequential pair. Different startup branches and variable
stepping leave isolated causality, repeatability and sustained performance
unproven. Both results remain below historical C12-r1 and the 60 FPS target.

The separate C19 visual pair uses fixed_dt=16.666666667, 120 live frames 90–209
and one successful play-frame-120 snapshot per run. Both startups have
started_before_input=false and acknowledged press/release. Root viewed both
960×544 PNGs. The PPM comparison finds 0/522240 differing pixels, zero differing
channels and maximum delta 0; both PPM and PNG files are byte-identical.
The shared PPM SHA is `5dc7bc75ac6ec9929fcff56590ac6f466d827532103a6f4697d226e06b15674b`;
PNG SHA is `c71fff18c69fe2ee4ade5358f73e8d7feb271e3b6ef6fe8ce1c68a5ff69e5031`.
Both logs record 82 scene programs with curve4=0/1. All 199/197 audio records
have underruns=0 and error=00000000, with non_silent=1 observed; no listening
test is claimed. Coverage is one scene/frame, without a per-draw census of
specialized-path use. Diagnostic timings add no quiet FPS or all-scenes result.

All four C19 captures have complete release/kill closures and checkpoints,
with actual executor exit 0 collected and no new unrelated dump. The
[final visual-1 checkpoint](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate19-curve4-1-visual-1/checkpoint-1792437325861929000/manifest.json) is complete.
The [final card manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/cards-after-native-candidate19-closed/manifest.json)
confirms original CardA SAV/STAT and empty CardB unchanged, total 41076 bytes.
The [restore receipt](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate19-restore-ordinary-1/restore-config-identity.json)
restores the ordinary INI
`aac80bdf22392a11167a11dd9f713ffcff4088d3e9c898f9e6e50f5314620759`.
Root rechecks fifteen product sources, four test postimages, five current
products and all five C18 `.previous` artifacts unchanged. ACKs do not prove
OS process absence; the last full SELF readback precedes the INI restore.
Curve4 and fog-disabled specialization remain default OFF;
these finite checks add no new gameplay-control PASS. Movement stays PARTIAL,
the other thirteen cases remain NOT_EXERCISED, and the remaining movie, audio,
graphics, progression, sustained stability and 60 FPS criteria remain open.
Details and evidence links are in [Native completion](NATIVE_COMPLETION_2026-10-18.md).

## Evidence chronology

The source checkpoint inspected was `main` at `bc468b18`, followed by the
coordinated working-tree changes described below. Hardware results identify
their executable separately. Candidate6 adds a complete intro, a separate
movie skip, successful current-card manual LOAD and two quiet live-match
windows. Candidate7's first-return audio crash is reproduced and diagnosed.
Candidate8 then completes Quit, automatic LOAD, intro, Title and Main menu,
followed by a manually selected second match with P1 assigned and Pause.
Candidate9 builds and installs successfully with idle-timer, audio-option and
PAD-error corrections. Its uninterrupted ten-minute diagnostic run reaches
3,385,537 independent uniform-packing comparisons, a natural match result and
an orderly exit. Audio-option menu/restart and physical PAD cases remain open.
Candidate10's reviewed controller, prompt and vertex-cache changes are integrated,
built and byte-verified on the console. Its first smoke run develops severe
audio-path slowdown during the introduction; it exits deliberately at frame 67
with no new core. A same-configuration restart progresses normally, verifies
Pause, Vita Circle/Cross prompts, Choose Sides, P1 assignment and resume, then
closes with all workers stopped at frame 16,123 and no new core. The slowdown
is intermittent and remains unresolved. Candidate10's separate vertex profile
verifies 94,536 exact-descriptor hits, one visible live scene and orderly exit.
Candidate11 stream, DPL2, Japanese/literal prompt, scroll-boundary and audio
position diagnostic changes are integrated after their isolated host gates;
the source gate binds 511 inputs and 59 evidence gates before the Vita build.
The Release package now builds and its full SELF is read back from the console.
The actual ARM bridge and OFF-path TLS guards are reviewed. Candidate11's first
diagnostic run verifies live play, Pause, scrolling Choose Sides, neutral-side
validation and P1 assignment, then exits at frame 34,872 with all workers stopped
and no new core. Unjournalled pad inputs intervene before the intended resume
check, so that controlled sequence remains open. Complete acceptance remains open.

Candidate11 subsequently completes the isolated C/S/L/A/R save-data sequence:
CREATE data, manual SAVE, manual LOAD, options autosave and startup reload.
L equals S and R equals A byte for byte; the original cards are restored and
read back unchanged. Controlled CREATE confirmations remain unattributed,
and two early Cross acknowledgements in A lack sampled input; the later
setting/apply/autosave sequence is attributed. The audit also finds an actual
8,256-byte icon allocation passed to a 16,384-byte aligned write. Valid payload
CRCs and successful reloads do not establish the safety of that earlier code.
The first capacity correction still allocates after CREATE and can leave an
orphan file on allocation failure. The separate preflight/UI recovery revision
passes 58 cases in 57 groups under each A5/zero initialization state, including
real storage paths with modeled frontend collaborators. The actual-font review
then corrects paragraph separators and an absent Japanese glyph. Text1 stops
after compile because the SDK ASan ignorelist was not inventoried; text2 finds
invalid intermediate pointer formation in nlFont::Load. Both failures remain
preserved. After the two-line font correction, text3 passes all 16 positive
cases and 22 negative examples in four families, using nine actual fonts and
the same independent oracle. It checks 1,832 glyph metadata records and 1,038
quads without invalid lookups, substitutions or degenerate geometry.
The four exact save/font postimages are now composed in active source for C13.
They reserve and zero physical icon capacity before CREATE and supply selective
allocation-failure Retry/Continue behavior. C13 now builds with 3,135 source
pins and 66 gate receipts, and all seven final invariants verified. The exact
2,501,440-byte SELF 8fc35138...a9c7 is installed with full readbacks; original
Card A and empty Card B remain unchanged. The targeted ARM SAVE/FONT review
is complete, with capacity prepared before CREATE, emitted zero/copy/owner
swap and preserved8704-byte icon reads. Controls4 now observes ordinary
startup and a successful18944-byte existing-card autoload on C13, followed
by native exit at frame3325 and all four worker roles exited. The later
D1 CREATE/manual LOAD fixture is now accepted in the root closure above.
Selective OOM recovery and GPU prompt rendering remain pending; the earlier
startup/autoload observation does not close those criteria.
These results do not close Cup/league progression or post-match autosave.

All seven content cases complete their diagnostic windows and worker shutdown
without new cores observed at their closing checks. The independently computed
union verifies 83/83 required IO paths across seven stadiums, nine captain
identities and four ordinary sidekick types. This is the finite matrix's asset
IO result, not an overall game-coverage percentage. Strict rows remain open:
continuous animation is unobserved; introductions are missing in cases 1, 2, 3
and 5; captain identification in the case 6/7 intro, sidekick colors in case 4,
the third Hammer Bro in case 6 and the third Toad in case 7 are unattested.
Cases 2 and 5 exceed the closing deadline, and case 2 also misses the first-live
deadline. The empty case 6 observation is retained/excluded, as is stale global
renderer timing. The original saves are read back unchanged after the matrix.
None of these diagnostic windows establishes quiet performance. The detailed
latest checkpoint, frozen matrix audit and isolated next-candidate work are in
[Native completion](NATIVE_COMPLETION_2026-10-18.md).

Candidate12 integrates ten scoped corrections/instruments. Its first Vita build
fails at link because the controller observer requests an external `padStatus`
that production keeps private; the earlier host stub concealed this boundary.
That failed attempt remains preserved. A three-file const-getter correction
passes the independent-TU regression, preserving the seven original groups and
adding two groups. The separate retry binds 2,629 source/evidence files and 63
gates and completes the Vita build. Final ARM review finds no blocker in the
targeted DOF, PB and control-trace scope, with its ABI/FP limits stated.
Candidate12-r1 SELF `ec9402c592ff8f03e2d77e3a206b0a654af8f4ac83848e93578af2fefe35eae3`
was installed with full readback after an acknowledged console reboot recovered
FTP staging; original cards and the preceding INI are verified unchanged across
that recovery. Its first timeline capture completes with nine coherent CSVs,
no recorded draw/provider failures, one reviewed live image and a deliberate
worker shutdown at frame 5,258. The closing identity/core check finds no new
core; frontend termination and perceptual audio quality are not inferred from
worker counters. A separate quiet capture measures 27.404856 FPS, mean
36.489883 ms, p95 47.570 ms and 0/300 within 16.667 ms. This single window
does not establish a cross-build gain or full-speed operation. The second profile
with general diagnostics OFF completes with continuous engine samples and no
large periodic Geometry spikes. Its mean is 37.105370 ms, median 35.9215 ms and
0/300 frames within 16.667 ms; the different workload and instrumented scopes
do not establish a quiet speedup. The first controls run stays in intro/title,
produces no gameplay control trace and exceeds its 420-second limit before
deliberate exit at frame 10,949. All 14 action cases remain NOT_EXERCISED;
worker shutdown, no new core at closing check and unchanged cards are observed.
Controls2 closes within its limit at +167.867153 seconds but fails image-age
admission before sending Start. Its 14 cases remain NOT_EXERCISED; native exit
at frame 2,579, one 152-record watchdog epoch, full identities, no new core and
unchanged original cards are verified. C13 controls3 fails unconfirmed startup
and closes at40.828443 seconds. Controls4 enables the display and delays its
single startup confirmation; it reaches Health, completes one observed Cross,
and then closes after a ten-second UI-fetch process timeout. Its partial-read
stdout, TERM return and subsequent SIGKILL EPERM remain preserved. Native
exit completes at94.784102 seconds with an82-record watchdog epoch, no new
core and unchanged cards. Image2 is retrieved/viewed only after closure.
Controls5 raises only the fetch child budget to20 seconds and preserves all
image/trace and global limits. It verifies11 images through the ordinary intro,
Title, Main, Strikers101 and Mario/Toad selection UI, with10 service-acknowledged
inputs. The last Toad confirmation is not observed in the game log. Image12
never completes, and two subsequent logs plus postflight retain identical
83843-byte contents. The485 watchdog records advance through seq484, but end
without a terminal marker. Graceful close has no native exit line; the verified
fallback reports cannot kill. Closure ends at624.682144 seconds, within budget.
The closing checkpoint finds no new core and original cards remain unchanged.
No-sleep is already enabled; a separate display-on observation leaves both log
and watchdog unchanged. Cause remains unproved. All14 gameplay cases remain
NOT_EXERCISED, with no gameplay P1 assignment or D1 terminal inferred. The
[controls5 root review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate13-controls5-root-final-review.json)
is SHA63522d44edc478c1c4286db17baf94c91453153e9cc4a388d43b4e8139937c1a.
Fog now has six accepted host suites: numeric786, frontend97, renderer155,
deferred baseline81, deferred proposal81 and runtime/real-queue66. The first
two fixture failures and the third runner's incorrect unsuffixed report-path
check remain preserved. Root independently validates the actual five suffixed
PacketProfile CSVs without rerunning the drivers. A separate R1 candidate
correctly selects the generic fragment path for both rigid/skinned range-ON
plus vertex-fog-ON cases, preserving the other ten shader-source cases.
These are host/model results; Cg/WPOS/reflection, Vita ABI and GPU fidelity
remain open. The four save/font corrections are now built and installed in C13;
fog remains isolated. A14-file fog/cache composition has been reviewed against
all accepted postimages, including two explicit full-file merges; its renderer
preserves R1 and cache remains OFF. It is staged for the next build, not active.
Installation alone adds no device-case acceptance.
The isolated texture-handle cache passes 39 bounded host operations: identical
194-row baseline/OFF/ON semantics, 49 configuration rows and four detected
negative invalidation controls. Root verifies 250 outputs and 1,004 canonical
dependency paths. The original compile failure remains preserved. This is
LP64 resolver/cache coverage with declared modeled providers; the proposal
remains outside C13 and default OFF, with no device or FPS acceptance.
Full identities, separate receipts and qualifications are recorded in
[Native completion](NATIVE_COMPLETION_2026-10-18.md).

## What the coverage claim must count

Coverage and performance have separate acceptance criteria:

| Coverage layer | What can close a row | What is currently established |
| --- | --- | --- |
| Prepared asset conversion | Every input in the identified pack has a supported native representation, with container validation and zero required-format fallbacks | A full local pack was prepared and verified; counts below |
| Native implementation and host contracts | Every reachable packet/state/platform operation has correct behavior, and its relevant success/failure contract is exercised | Post-skin integration has 59/59 effective passing checks after two fixture corrections; snapshot 1/1, black/bitmap 4/4, the 11-case loading/media/phase gate with one corrected host adapter, and CARD/bootstrap 2/2 have their separate passing evidence below. |
| End-to-end game behavior on Vita | An identified executable and asset pack complete every specified game scenario with correct graphics, sound, input and persistence | Candidate8 SELFbbdf completes one first-return round trip through Quit, LOAD, intro, Title and Main, then manual P1 assignment, The Palace, a second match and Pause. Its 33-image review qualifies the observed path and input delivery; every individual human action is not yet established. Original saves remain unchanged after LOAD. Candidate6 also completes a natural 1,949-picture intro, a separate skip and manual LOAD. The older renderer crash, repeated stability, all 17 movies with synchronization, save/write/progression and whole-game acceptance remain open. |
| Full-speed operation | Real-time live gameplay meets the 16.667 ms frame budget across the accepted scenarios, without frame skipping | Candidate12-r1's first quiet window measures 27.404856 FPS and 0/300 within 16.667 ms. Candidate11's exploratory quiet stream comparison and the earlier repeated candidate9 uniform/candidate10 vertex-cache comparisons establish no benefit; those defaults remain OFF. No full-speed gameplay acceptance follows. The approximately 60 FPS Health captures contain no gameplay. |

A test-suite pass percentage is the fraction of selected tests passing. It is
not a percentage of the game's functions, asset combinations, modes or branches.
Likewise, 34 defined render views and zero invalid packets in one scene would
not establish that all views or all game content were exercised.

The practical implementation gate is to close known reachable gaps, then use
device testing to find behavior and fidelity gaps that host contracts cannot
establish. Final coverage requires the device acceptance ledger below.

## Latest correction and device cycle

The detailed build and hardware ledger is in
[Native completion](NATIVE_COMPLETION_2026-10-18.md). Its latest evidence separates
the following independent acceptance rows:

| Area | Current evidence | Remaining acceptance |
| --- | --- | --- |
| First return from direct match | Candidate8 Pause/Resume, Quit confirmation, automatic LOAD, intro, Title and Main complete in one process; a second manually selected match loads and pauses | Repeated round trips, normal-start variants, long runs and audible FE transitions |
| Diagnostic watchdog | Candidate9 has 599 positive RUN records across 610.073932 seconds without game input or display intervention, with a maximum sample gap of 1.365966 seconds; its later orderly exit records the sampler-stop boundary | Repeated long runs and explicit suspend/resume |
| Idle behavior | Candidate9 restores the SDL-equivalent default idle tick at initialize/pump and completes the bounded-gap no-input interval above | Physical suspend/resume and longer stability scenarios |
| Human PAD0 | P1 assignment and player marker are visible; the earlier log proves Circle plus clamped analog 56,0 and release. Candidate10 repeats Pause, Choose Sides assignment and Circle resume in ten reviewed images | Independently demonstrate movement, pass/change, turbo, shot/charge, item and special actions; candidate10's analog/Cross/Square outcomes are ambiguous because goals and possession changes intervene |
| Audio options | Candidate9 host gate preserves current/saved Mono on Vita; 224 Vita/retail scenarios and 4,378 ASan/UBSan checks pass with three prepatch failures reproduced | Menu/revert/LOAD/restart and audible output on hardware |
| PAD error handling | Candidate9 host gate passes 13 groups and 587,449 checks through real PAD/VBlank/FE bodies; normal mapping digest is unchanged over all 4,096 button and 65,536 axis combinations | Physical error, disconnection and recovery on hardware |
| Pose snapshot copy | Compatible buffers retain owned previous storage while avoiding a redundant copy; 509,745 checks and 216 matrix builds pass under ASan/UBSan | Same-executable scenario fidelity and measured cost |
| Uniform packing cache | Candidate9 logs 3,385,537 independent comparisons and a natural 4–2 result/highlight. Four quiet OFF/ON/OFF/ON windows show no speedup; default remains OFF. Both quantity/task profiles complete: VS pack calls 865,911→145,471 and 677,505 reused values with zero provider errors/fallbacks | Broader fidelity; differing draw/step counts and profiling overhead preclude deriving a speedup from the instrumented cadence |
| Vertex-program cache | Candidate10 profile has 94,536 indexed requests/hits, zero create/error/fallback counts and 17,928-byte CPU metadata residency; 94,979 accepted packet draws, zero invalid/GPU errors, one reviewed live image and orderly exit at frame 4,600. Four quiet OFF/ON/OFF/ON runs complete: 27.457338 aggregate FPS OFF versus 26.588964 ON, default OFF | Broader fidelity and repeatable measured benefit; instrumented counts do not establish a speedup |
| Interleaved vertex streams | Candidate11 ON profile reduces its 857,973 source streams to 91,484 actual binding calls with zero provider errors/fallbacks; two live images and deliberate diagnostic exits reviewed. One quiet pair measures OFF 28.093181 FPS versus ON 27.528295 FPS, all 0/300 within 16.667 ms; default remains OFF | Broader fidelity and repeated benefit; the single exploratory pair does not establish a speedup or statistical generalization |
| DPL2 rear return | Candidate11 restores the original one-tick L-add/R-subtract rear return; 43 cases / 164,777 checks and 2,560 interpreted original-opcode samples pass, with unchanged Stereo/Mono digests; Vita build and installation complete | Audible hardware behavior and option/reinitialization/restart |
| Japanese and literal Vita prompts | Candidate11 admits three Japanese text signatures, preserves original font metrics/Western pixels, and rewrites only 14 exact localized instruction pairs inside the final LOC allocation; font 70 cases / 3,196,023 checks and literal 42 cases / 237,027 checks pass | Device text fitting, Japanese scene availability and baked controller art; cup-cheater Stats+ uses fallback text and has unproven retail reachability |
| Scrolling text bounds | Candidate11 fixes the reproduced short-message heap overread and long-message converter overflow; the real-body ASan/UBSan gate passes 36 cases / 2,303 checks. Two device images show the Choose Sides ticker advancing; neutral-player validation, dismissal and P1 assignment are visible, followed by a clean terminal checkpoint | Other reachable scrolling consumers and controlled repeat of resume after intervening unjournalled input; no attribution of the old graphics crash |
| Audio slowdown localization | Candidate11's 17,081 host checks, fallback/negative gates and linked ARM guards pass. The device produces 148 AudioMix position samples across DSP, auxiliary, synth, stream, AI and sequencer phases; the final run has zero reported audio errors/underruns and all workers exit | Capture and interpret an actual slow interval; sampled position is not duration, audible quality or a fix |
| Candidate10 startup slowdown | First smoke remains near match introduction; mixer timing rises to 2.933 seconds per call, then orderly exit at frame 67. The same-configuration second smoke progresses, verifies menus/live play and exits at frame 16,123; both have no new core | Intermittent root cause and regression correction; one functioning restart does not establish stability |
| NV12 staging | Candidate8/9 include the default-OFF, exact-byte staging path and allocation fallback with eight-geometry host checks | Same-executable movie OFF/ON timing, colour, playback and synchronization |

The [candidate8 UI review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate8-watchdog-1/ui-review-through-33.json)
binds all 33 inspected images to an immutable input-journal copy. Its diagnostic
300-frame window measures 20.165759 FPS, but watchdog/logging and possible
snapshot overlap prevent using it as a quiet speed comparison. Candidate7's
two quiet attempts close without CSVs after 640.8 and 1,108.2 seconds; neither
adds a performance result. Candidate9's four quiet windows now provide the
current measured OFF/ON comparison, preserved in its
[quiet review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate9-uniform-quiet-review.json).

The [candidate9 source gate](../build-vita-gxm-latest/native-candidate9-source-gate.json)
binds 87 inputs and 48 gates; its Release SELF is
`69a4bcf2a34b2d5163d9b61d715a37005c6a3bc7493c4cf24c0c837f49916c42`.
Build success and the host gates above do not close the remaining device rows.

Two platform changes are integrated for **candidate10 and are not part of
candidate9**. Native prompts replace four face glyphs in six compatible text
fonts and four legend hashes; the 47-case host gate checks pixels, metrics,
pointer transport and marker ownership, with CPU OOM limits stated explicitly.
The earlier 16-font census was a filtered subset, not the entire pack. The
subsequent reachable-scene audit identifies Japanese text profiles and 14 literal
instruction/language pairs still requiring adaptation; 185 heading instances
across 51 default FENs contain no controller tokens in the nine audited LOCs.
Fourteen specialized Japanese FEN paths are absent from the prepared manifest;
only the title fallback is currently demonstrated. Font glyph support does not
establish complete Japanese scene coverage. The PAD backend now publishes up to four stable logical slots and
tracks external actuator ownership through source generations and failed-stop
debt. Its 38-group host gate and the unchanged 13-group legacy assertions pass.
Device multiplayer still requires two independent physical controller streams,
stable team assignment, hotplug and actual rumble. SDK availability and mocked
return values do not establish those observations; handheld hardware has no
internal actuator. Candidate10's source gate binds the reviewed integrations.

The [candidate10 ten-image review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate10-smoke-2/ui-review-through-10.json)
binds the visible Circle/Cross prompts, complete Choose Sides/P1 assignment,
back/resume flow and terminal log/watchdog to exact image and input hashes.
The remaining GameCube controller graphic and independent gameplay actions
are explicitly open. Its 23.401342 FPS diagnostic capture is not a quiet
performance result.

The older graphics crash remains unresolved. The candidate7 audio crash now
has a bounded, independently checked capsule tied to its exact core and ELF;
the new successful round trip is retained as separate hardware evidence rather
than rewriting that historical diagnosis. Both crash capsules are revalidated
before artifact rotation.

## Prepared pack examined

Local artifact: `.tmp/asset-setup-validation/sms.psarc`.

- Size: 1,786,972,393 bytes.
- SHA-256: `e978b9978e4ced523c127ce2eedba5e19a4a5741cfd4523bb0be10571de6b09b`.
- 24,386 payload entries, 24,387 TOC records including the manifest, and 37,824 blocks.
- Full payload/container verification succeeded.
- Native graphics ABI: `strikers-native-graphics-v1`.

| Asset component | Recorded preparation result | Qualification still required |
| --- | --- | --- |
| Models | 149 model files converted | Exercise their packet/material combinations in game |
| Texture bundles | 164 bundles, 1,738 bundle texture entries | Appearance, mip sampling, animated and render-target interactions |
| Embedded resources | 38 resource bundles converted, 770 embedded textures | Exercise the resource users |
| Compiled textures | 1,809 unique compiled textures | Correctness of every referenced material combination |
| Audio | 1,202 requests, 1,351 occurrences/sources, 22,977 records, 918,389,780 uncompressed bytes; zero preparation fallbacks | Audible playback, mixing and transition behavior on device |
| Movies | All 17 movie paths converted, zero preparation fallbacks; H.264 baseline/all-intra payloads in the native container | The report explicitly has `hardware_validated: false`; all 17 need device playback qualification |

Preparation sources:
[pack verification](../../.tmp/asset-setup-verify.json),
[pack manifest](../../.tmp/asset-setup-validation/sms.psarc.json),
[graphics converter](../tools/asset_pipeline/native_graphics.py),
[PSARC parser](../tools/asset_pipeline/psarc.py).

The console's observed archive size is 1,791,284,489 bytes. This matches the local
`ab-artifacts/unified-native-20261015/sms.psarc`, whose SHA-256 was recomputed
during this audit as
`699670609b952510615d869b344c18bda6b7b1f0cce8868e0248b9204cec220d`.
Its [upgrade manifest](../../ab-artifacts/unified-native-20261015/sms.psarc.json)
records the native InGameUI/MainUI replacements and two obsolete source payloads
zeroed in place. Their combined 4,312,096 bytes exactly explain the physical
size difference from the compact audit archive.

An independent streamed comparison of all **24,386 live payloads** in these two
local archives found identical paths, sizes and SHA-256 hashes: 1,862,468,339
logical bytes and **zero differences**. Therefore the graphics, audio, movie,
transition and texture-mask census below also applies to that unified archive.
See [logical equivalence evidence](2026-10-18-pack-logical-equivalence.json).
The logical-payload ledger SHA-256 is
`e051da1b4ca186c83fdef5f7a34347c9108f81acc8a88bfa386c7a36461f9418`;
the JSON specifies its path/size/payload-digest encoding. A streamed complete
remote-archive hash attempt timed out after 600 seconds. The remote archive has
size compatibility with the unified local archive, but its complete SHA-256
identity remains unverified; the 24,386-payload equivalence result applies to
the two local archives. Prepared native audio also has a runtime decoding
fallback for individual cache misses; a cold miss by itself is not evidence of
missing sound.

The material audit also enumerated all 7,128 packets in the 149 prepared NSMD
model files. This is an asset census, not a runtime coverage result:

| Stored texture configuration | Packet count |
| --- | ---: |
| `0x01` | 4,566 |
| `0x03` | 476 |
| `0x05` | 57 |
| `0x07` | 33 |
| `0x09` | 44 |
| `0x11` | 803 |
| `0x15` | 11 |
| `0x19` | 751 |
| `0x31` | 24 |
| `0x39` | 78 |
| `0xff` | 285 |

The 11 `0x15` packets are in `The_Palace.glg`; `0x31` occurs in
`Yoshi_Stadium.glg`, and `0x39` occurs in `Forbidden_Dome.glg`,
`Super_Stadium.glg` and `Yoshi_Stadium.glg`. The absence of a stored mask does
not exclude a runtime material/program conversion that produces it. In
particular, `0xff` is a source/material-override state and must not automatically
be counted as an unsupported draw. The census informed the material
inherited-stage and alpha-state tests.

## Concrete implementation work in this change set

### Vertex streams and live line primitives

The raw stream mapping is defined in
[GLMeshWriter.h](../include/Game/GL/GLMeshWriter.h): position is 0, skin weights
are 13, and Position4 is 14. The mesh writer and native converter preserve those
IDs; there is no intervening 13-to-14 remap.

The native packet decoder and native bounds path incorrectly treated ID 13 as
position. Both now use `GLStream_Position` and `GLStream_Position4`:
[native_match_geometry.cpp](../src/platform/native_match_geometry.cpp) and
[DrawableModel.cpp](../src/Game/Drawable/DrawableModel.cpp). This admits genuine
four-component position streams and prevents weights from replacing positions.

Line support is a reachable retail requirement. The prepared archive contains:

| Transition sequence | Model and effect | Source |
| --- | --- | --- |
| `in_ball_breaks_glass` | `broken_glass`, screen grab, white half-alpha outline, `target/grab_texture` | `files/art/transitions/transitions.fx`, lines 147-153 |
| `break_glass` | The same outlined screen-grab model | The same file, lines 162-168 |

[ModelTransition.cpp](../src/Game/Transitions/ModelTransition.cpp) parses
`outline` and emits `GLP_LineStrip` in `RenderOutline`. Earlier native code
accepted only primitive types 0-3 and submitted triangles. Coordinated changes
now extend the source registry, geometry decoder and renderer for line lists
and strips. Transition fidelity still requires a device capture of those
sequences.

The implementation now reuses dynamic decoding storage, corrects render-view
target/depth-clear handling and EFB copy/sampling/lifetime behavior, including
mixed batches, and preserves inherited material state across draws. These
changes passed the integrated host gate below. Their visible output and
runtime behavior still require qualification on the matching Vita executable.

### Rigid skin capacity: 17 affected retail packet records

Investigation of the integrated smoke's `skin_selectors` status found a
concrete capacity mismatch. The source registry accepted only stitching
indices 0-8, `prepare_skin` accepted registers -96 through -72, and the shader
stored ten matrix slots including slot zero. The native userdata contract
already permitted up to 32 bones, while the examined retail assets require up
to **13** in one rigid packet.

The full local NSMD scan covered **149 files, 89 skin containers, 1,076
stitching sections and 324,885 selector bytes**. Of these, **801 packet records
use the rigid/solid-offset path**; **17 records in 16 files** reference indices
greater than eight. Every rigid stitching length matches its packet's vertex
count. Each affected packet's BoneMap contains exactly the consecutive indices
0 through its maximum selector index. A file named `_blend` can still contain
rigid packet records, so the classification follows packet state.

Paths below are relative to `files/art/` in the identified local packs;
packet indices are zero-based:

| Model path | Packet index | Vertex/index count | Required rigid bones |
| --- | ---: | ---: | ---: |
| `characters/Birdo/Birdo_rigid.glg` | 9 | 758 | 13 |
| `characters/Chainchomp/Chainchomp.glg` | 0 | 1,616 | 10 |
| `characters/Chainchomp/Chainchomp_blend.glg` | 0 | 1,616 | 10 |
| `characters/DonkeyKong/DonkeyKong_rigid.glg` | 5 | 534 | 11 |
| `characters/Goalie/Goalie.glg` | 0 | 363 | 10 |
| `characters/HammerBro/HammerBro.glg` | 5 | 523 | 10 |
| `characters/HammerBro/HammerBro.glg` | 8 | 507 | 10 |
| `characters/Koopa/Koopa_rigid.glg` | 9 | 573 | 11 |
| `characters/MarioSidekickB/MarioSidekickB.glg` | 5 | 876 | 10 |
| `characters/SuperTeam/SuperTeam.glg` | 10 | 947 | 10 |
| `characters/SuperTeam/SuperTeam_blend.glg` | 11 | 1,275 | 10 |
| `characters/Toad/Toad_rigid.glg` | 11 | 331 | 10 |
| `characters/Yoshi/Yoshi.glg` | 1 | 1,172 | 12 |
| `characters/Yoshi/Yoshi_rigid.glg` | 6 | 305 | 11 |
| `characters/YoshiBrown/YoshiBrown.glg` | 6 | 305 | 11 |
| `characters/YoshiRed/YoshiRed.glg` | 9 | 305 | 11 |
| `Goalie/Goalie.glg` | 35 | 449 | 10 |

All 16 corresponding GLG files from the original archive identified in the
warble audit were converted again **in memory** with `native_model`; every
result was byte-identical to its prepared NSMD payload. No archive was changed.
For two reproducible examples:

- Birdo rigid original SHA-256:
  `00370c983771b27367b1ccfc7f5ab39f7346e7b92084d22850f356edc41cac22`;
  native SHA-256:
  `071d8792369c546da7cc3a626e804648ef8d26734912ad8bfef204b34d8e17cb`.
- Chainchomp original SHA-256:
  `591fbf39eccc76bf1688994b05fb1daecdc9f6908800ce66d8f727c90eee203b`;
  native SHA-256:
  `e575f0c20af192d041f33e7788e3fd2b0144c3ee2c03e15ebf045bd755517dea`.

The original [StitchModel](../src/Game/GL/gluSkinMesh.cpp) writes
`3 * (index + 1)`. [MakeUserData](../src/Game/GL/ShaderSkinMesh.cpp) stores
`reg = 3 * index - 96`; [glud_Skin](../src/NL/glx/glxSend.cpp) loads both position
and normal matrices at `reg + 99`. Thus the extra indices are real additional
matrix slots, not encoded aliases that can be clamped or wrapped:

| BoneMap/stitch index | Stored register | GX first row | Native matrix slot |
| ---: | ---: | ---: | ---: |
| 0 | -96 | 3 | 1 |
| 8 | -72 | 27 | 9 |
| 9 | -69 | 30 | 10 |
| 12 | -60 | 39 | 13 |
| 31 | -3 | 96 | 32 |

The correction shares an explicit **32-bone native contract** through
[scene_skinning.hpp](../include/port/scene_skinning.hpp): 33 slots including
slot zero, 99 `float4` rows. This is the bounded native API capacity, not an
assertion about a maximum imposed by original GX hardware. Registry,
userdata validation, temporary decode palette, scene material, shader,
uniform storage and shader reflection use the same capacity.

Every draw now checks register range, duplicate registers and finite matrix
values, including draws reusing a resident VBO. A mask of the bone indices
required by each source is recorded when stitching changes. Resident draws
check their fresh palette against that mask, so omitting or substituting a
previously required slot cannot silently use an identity transform.

Focused regressions cover all 256 possible stitching bytes (indices 0-31 are
valid), original CPU/GPU mapping at 13 and 32 bones, production draws with
10/11/12/13/32 bones, changing high-slot poses, immutable queued materials,
and invalid/sparse palette rejection and recovery on resident hits. These
changes passed the subsequent host integration and are included in SELF EE7c,
whose completed smoke has zero cumulative invalid packets. The single
registration error in the older 9a smoke was not attributed to a particular
asset; the clean new run does not establish device coverage of every one of
the 17 affected rigid packets or the entire roster.

### Integrated host validation

The post-skin integration rebuilt **42 configured test targets**. Its first
CTest invocation passed 57/59; two fixtures were corrected and both targeted
reruns passed, leaving **59/59 effective passing checks**. The subsequently
added snapshot-request regression passed **1/1 in 0.29 seconds**. These results
must not be described as one 60/60 CTest invocation.

The next [black/bitmap host gate](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/black-bitmap-host-gate.json)
rebuilt and passed **4/4** targeted checks: the real glx/frontend bridge,
renderer, deferred queue and startup-screen driver. The gate command completed
in 4.322 seconds. It covers the new synchronous presentation contracts and the
material program-lookup regression; it predates loading-indicator service work.

The subsequent [loading/media/phase gate](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/spinner-host-gate.json)
rebuilt nine selected targets. Ten of 11 CTests passed initially; the host-only
Cg adapter was corrected and its exact command passed separately. The final
pure-black startup-retirement correction was then rebuilt and passed in the
renderer driver. These are **11 effectively passing checks across the recorded
commands**, not one clean 11/11 CTest invocation. They cover the actual loading
frontend/backend, retrace helper, shader oracle, deferred failure/clock paths,
task/phase instrumentation and native media records.

The later [CARD/bootstrap gate](2026-10-18-card-bootstrap-host-gate.json) passed
**2/2 selected CTests in 9.58 seconds** after rebuilding the CARD driver. Its
production MemCard/SaveLoad callback test runs under ASan and UBSan with isolated
temporary cards. None of these host counts is a whole-game coverage percentage.

The earlier gate passed **55/55 in 6.36 seconds**, including production
texture-state validation. Its predecessor rebuilt all 38 targets then
configured and passed 54/54 in 8.81 seconds. Target and CTest case counts are
separate because one target can support multiple registered checks. These
results cover their selected geometry, renderer, material, capture,
resource-failure and persistence contracts, not a whole-game coverage percentage.

The material oracle completed **229,008 RGBA channel comparisons**, including
**57,252 alpha comparisons**, with zero failures. It compares the independent
original TEV model with the shipped Cg shader algebra, covering all 20 standard
masks, dirt/crowd paths, movie-selector transitions, predecessor pairs, a
10,000-draw persistent sequence, constant-colour cleanup and forced rebuilds.
It uses constant texture samples and real-number TEV algebra with quantized K
inputs; it does not certify GX per-stage rounding, GPU sampling or device
appearance.

A profiler-writer portability correction replaces `%zu` formatting
that is unsafe for the target newlib path. Its **2/2 targeted regression checks
passed in 0.84 seconds**. This result is recorded separately from the earlier
54-case gate. See [packet_profile_test.cpp](../tests/packet_profile_test.cpp)
and [host test registration](../tests/CMakeLists.txt).

### Texture-state packing and clear masks

[glState.cpp](../src/NL/gl/glState.cpp) used 32-bit clear-mask expressions for
a 54-bit texture-state value, and its reference setter assigned high/low words
in the original big-endian order. The port now combines numeric 64-bit fields
and uses 64-bit shifts/masks. Live users include character dirt/detail,
projected-shadow rendering and movie texture attachment.

The `gl_texture_state` regression executes the production setters, both public
APIs and the method/default helper. It completed **5,186 checks with zero
failures under UBSan**. Before the correction it detected wrong word placement
and an undefined shift by 32. This closes the tested state-packing contract;
visible movie, character and shadow output remains part of device acceptance.

### Transition blanking, immediate messages and loading animation

The native `glxSwapSetBlack` no-op in
[glxSwap.cpp](../src/NL/glx/glxSwap.cpp) was a live retail gap. Quit, demo and frontend
transitions call it. The corrected bridge stages black state without an
immediate swap, matching the original setter's lack of `VIFlush`. Successful
frame presentation consumes the original three-frame countdown before choosing
the display image: two black frames, then a visible third. Discarded or failed
frames do not consume it. Main and persistent render targets remain intact.

Localized loading/error bitmaps now use a synchronous display-only operation,
including when a game frame is already open. This makes the message visible
before a blocking frontend load returns, without promoting an incomplete main
frame. A successful bitmap clears displayed black state without resetting the
separate retail countdown. The **4/4 host gate** above covers these contracts,
startup-screen retirement, target preservation and failure behavior. They are
later working-tree changes and have not yet been qualified on the EE7c device
executable measured below.

The three-icon loading indicator is a separate reachable retail feature:
transitions to match/frontend, frontend reloads and option resource changes
arm it. Its implementation now advances from real display retraces on the
renderer owner while the game thread blocks, retains the first-call two-second
delay, and performs no ongoing clock/retrace queries when disabled. Save/Restore
preserves its flag and visible-background semantics. The loading host gate
above passes; its device appearance and lifecycle still require qualification.

The original icon data cannot be treated as ordinary alpha-blended RGBA. Both
32x32 images contain 194 visible pixels and 14 pairs with different visibility
between their even/odd pixels. The odd visible pixel writes shared YUV422
chroma for both pixels. The new frontend emits explicit luma/chroma operations
from those original arrays; its independent pixel oracle and renderer service
tests passed. Reconstruction of shared chroma from the
native RGBA background is an explicit BT.601 adaptation, not a claim of
bit-identical GX display-copy conversion.
See [loading_indicator.hpp](../include/port/loading_indicator.hpp) and its
[independent XFB oracle](../tests/loading_indicator_test.cpp).

The service captures the completed display into its own background surface and
does not promote an unfinished game frame. Failure disarms the timer and
releases its resources on the owner thread while preserving the first error;
OFF also remains effective after a latched failure. Host tests inject failed
clock queries, waits and GPU ticks. A completed black-only presentation keeps
the startup screen resources alive until the first real application content.

### Save operations now propagate actual write failures

[card_vita.cpp](../src/platform/card_vita.cpp) previously discarded metadata
open/write errors, data flush/close errors, and directory/removal failures.
Consequently the CARD API could report success after an unsuccessful save.

The correction:

- Checks complete writes, flush and close before success is reported.
- Uses exclusive creation for a new save, so an existing save cannot be
  truncated by a create race.
- Propagates metadata failures through create, write and status updates.
- Commits cached status only after its replacement is published.
- Increments transferred-byte counters only for successful operations.
- Checks directory enumeration, close and removal during format, and sidecar
  removal during delete.
- Preserves synchronous completion while distinguishing submission from
  completion: an accepted asynchronous operation returns READY and invokes its
  callback once with the actual result; immediate rejection invokes no callback.
- Leaves unrelated files outside the CARD sidecar suffixes alone.

Vita metadata replacement cannot assume POSIX atomic-overwrite semantics.
VitaSDK's current `_rename_r` deletes an existing destination before calling
`sceIoRename`. The implementation therefore uses the native non-overwriting
rename operation and one bounded `.stat.bak`, with recovery:

| Replacement state or failure | Preserved state and next load |
| --- | --- |
| Temporary open, short write, flush or close fails | Existing primary and backup remain; temporary replacement is discarded when possible |
| Moving primary to backup fails | Existing primary remains; the replacement fails |
| Publishing the closed temporary file fails | The backend attempts to restore the backup to the primary name |
| Publication and rollback both fail | The previous complete metadata remains in `.stat.bak`; load uses it |
| Valid primary and older backup both exist | Load prefers the primary; a later save removes the stale backup only after validating the primary |
| Invalid/missing primary and valid backup | Load uses the backup; a later save restores it before replacement |
| Both existing metadata copies are invalid | Open reports an I/O error and retains both files |
| Legacy save with neither metadata file | The previous default-metadata behavior remains supported |

Here, validation means a complete record with the expected magic/version;
this change does not add a payload checksum. It also does not make writes to
the `.sav` data itself transactional or promise power-loss durability of the
filesystem. A failed partial data write is reported, rather than disguised as
success.

Primary platform references:
[VitaSDK rename implementation](https://github.com/vitasdk/newlib/blob/vita/newlib/libc/sys/vita/syscalls.c),
[rename behavior issue and resolution](https://github.com/vitasdk/newlib/issues/91),
[exclusive fopen mode parsing](https://github.com/vitasdk/newlib/blob/vita/newlib/libc/stdio/flags.c).

### Save host validation

The new [card_vita_driver_test.cpp](../tests/card_vita_driver_test.cpp) compiles
the production backend with actual SDK declarations, isolated temporary host
files and compile-time-only failure injection. The mock rename explicitly
refuses an existing destination, matching the native constraint instead of
inheriting host POSIX replacement behavior.

The driver checks:

- Successful create, write, readback, metadata update and reopen.
- Refusal to recreate an existing save without changing its contents.
- Data open/short-write/flush/close failures.
- Metadata open/short-write/flush/close and both rename phases.
- Failure of rollback, recovery through backup, and stale/invalid backup cases.
- Invalid primary plus invalid backup.
- Metadata failure after a data write.
- Delete and format failures, missing optional sidecars, channel isolation and
  preservation of unrelated files.
- Callback timing/count/result, transferred bytes, and closed resources.
- Accepted read failures versus immediate rejection, nested callbacks and a
  later independent operation, without duplicate completion or stale counters.

Validation performed:

```sh
cmake --build smstrikers-port/.tmp/native-gxm-host --target card_vita_driver_test -j8
ctest --test-dir smstrikers-port/.tmp/native-gxm-host -R '^(card_vita_driver|save_load_vita)$' --output-on-failure
```

The original persistence-only gate passed 1/1 in 0.22 seconds. After the
submission/completion and bootstrap corrections below, the current gate passed
**2/2 in 9.58 seconds**: CARD driver 0.25 seconds and production callback test
9.33 seconds. Scoped `git diff --check` passed. These tests used only temporary
host cards; they are not on-device persistence certification.

### Original bootstrap restored, with a single completion contract

The original CARD implementations distinguish accepted work from immediate
submission failure: see [CARDRead.c](../src/Dolphin/card/CARDRead.c),
[CARDCreate.c](../src/Dolphin/card/CARDCreate.c) and
[CARDStat.c](../src/Dolphin/card/CARDStat.c). The Vita backend previously called
the callback with an I/O error and returned that same error. Its live
[MemCard caller](../src/Game/Sys/gcmemcard.cpp) interpreted the return value as a
failed submission and could complete or clean up the same operation again.

Accepted Vita CARD operations now return READY after the inline completion;
only their callback carries an I/O failure. Invalid submissions return their
error without a callback. No global counter, new public callback, save-format
change or asynchronous worker was introduced. Reads also check close errors.

MemCard now rejects failed status queries before interpreting metadata and
rejects files shorter than their computed icon/banner header. A shadowed write
result in `SetStatusDone` no longer swallows an immediate submission error.
[SaveLoad.cpp](../src/Game/DB/SaveLoad.cpp) checks read completion errors before
CRC or deserialization, validates data/icon read lengths against their buffers,
and commits cleanup state before the user callback. Thus a completion callback
can safely start another load without the previous call overwriting its state.

The Vita-only bypass in
[TransitionTask.cpp](../src/Game/TransitionTask.cpp) has been removed. The first
ordinary frontend initialization again enters `SCENE_SHOULD_LOAD_OR_SAVE` when
save I/O is enabled and retains the original legal-screen fallback when it is
disabled. SELF 447fe subsequently completed this bootstrap/autoload path on the
device with the preserved current save, as recorded below. The earlier EE7c UI
recordings predate this correction.

[test_save_load_vita.py](../tools/test_save_load_vita.py) extracts the current
unchanged production callback bodies and compiles them with the full MemCard
implementation and Vita CARD backend. Its [host fixture](../tests/save_load_vita_test.cpp)
adapts platform services and GameInfo serialization, then exercises the real
exists-to-load sequence, nested successful/failed completions, no-callback
submission errors, status/read errors, missing and truncated files, oversized
icon metadata, invalid sizes and both CRCs. ASan/UBSan found no errors; valid
temporary save and metadata bytes remained unchanged. This is callback and
I/O-contract coverage, not validation of the popup state machine or device UI.

### Configuration migration preserves the previous complete file

The managed-defaults refresh in [config.c](../src/platform/config.c) had the
same Vita `rename` replacement risk. It also treated failed input opens as
missing files and did not check input read/close failures before replacement.
The correction is scoped to Vita migration and I/O errors; the managed
defaults table, revision, profiling choices and first-value-wins parser are
unchanged.

- An exact current managed block at EOF still causes no write.
- Existing nonregular or unreadable primary files cause an error and remain
  intact; only an absent file permits fresh generation.
- Every migration checks input reads and close, output writes, flush and close
  before publishing the replacement.
- A closed temporary file is published with native non-overwriting renames,
  retaining at most one complete original at `.vita-defaults.bak`.
- A failed publication attempts rollback. If rollback fails, the preserved
  backup is loaded read-only for that launch and `PortConfigPath()` reports the
  actual path used.
- A missing primary recovers a fully readable backup before migration. If its
  native restore fails, that backup is likewise loaded read-only. An unreadable
  or nonregular backup is preserved rather than overwritten.
- A later migration removes a stale backup only after checking that it can be
  read completely and after the replacement's I/O has completed.
- The Vita loader reports final read and close failures. Normal non-Vita
  loading behavior is unchanged.

The new [vita_config_io_test.c](../tests/vita_config_io_test.c) exercises 29
migration, failure and recovery cases through compile-time-only I/O hooks in
[vita_config_test_io.h](../tests/vita_config_test_io.h). They cover both input
passes, output failures, publication/rollback, read-only backup fallback,
stale/invalid backups, nonregular primary paths, current-file no-write behavior,
explicit-path load errors and closed resources. Fixtures are isolated temporary
files and never the user's configuration. The existing
[vitagl_default_config_test.c](../tests/vitagl_default_config_test.c) continues
to check the actual defaults and override ordering.

```sh
cmake --build smstrikers-port/.tmp/native-gxm-host \
  --target vitagl_default_config_test vita_config_io_test diagnostics_config_test -j8
ctest --test-dir smstrikers-port/.tmp/native-gxm-host \
  -R '^(vitagl_default_config|vita_config_io|diagnostics_config)$' --output-on-failure
```

Result: **3/3 selected tests passed**, 1.15 seconds, after regenerating the
existing host cache. Scoped `git diff --check` passed. These checks establish
host I/O contracts, not device filesystem power-loss durability. The loader
still applies environment settings as it reads; a late input failure is
reported, but this change does not introduce an atomic environment transaction.

## Paths that must not be misclassified as live coverage holes

| Item | Evidence and classification |
| --- | --- |
| `target/warble`, `target/colour`, `target/offset` | The examined original retail executable and original assets already leave this path inactive. The conclusion rests on original binary/map, loader/registration and pre-conversion asset evidence below, not solely on missing converted texture IDs. |
| Runtime target creation | [glxTarget.cpp](../src/NL/glx/glxTarget.cpp) creates grab, DOF and ten projected-shadow textures. Original retail `UpdateWarble` is only `blr`; `ColourGrab` and `OffsetGrab` only call `nlBreak`. These three warble resources are not omitted working runtime targets in the examined retail build. |
| Filters 1-5 and 7 | The enum names GaussianX/Y, Box, Glow, LargeBox and luminance blit. Current source assigns only None or Blt. Both the original and native view loops act on Blt (6); `ScreenBlur::ApplyModifier` is already empty upstream. No additional live use was found. See [glPlat.cpp](../src/NL/gl/glPlat.cpp) and [ScriptedTransition.cpp](../src/Game/Transitions/ScriptedTransition.cpp). |
| Shadow volume mode | `g_bShadowVolumes` is zero-initialized and controlled by a debug/pause toggle; shadow-blend views are disabled in normal setup. This is a dormant debug path, distinct from ordinary projected/planar shadows that need qualification. |
| Co-planar shadows | Original upstream geometry/pass preparation flattens or repeats the relevant shadows with depth comparison passes. The lack of a direct native GX co-planar flag alone does not prove a missing visible feature. Fidelity is still a device acceptance item. |
| Generated `dSolveLCP` stub | The generated stub has a host-style mangled symbol. A real implementation is present in [lcp.cpp](../src/ode/lcp.cpp). The inspected Vita map places the generated stub section at address zero among discarded sections. |
| Generated StatsGatherer/TMAnimController stubs | Their generated host-style symbols and `port_missing` are likewise discarded in the inspected Vita map; they are not reachable linked Vita implementations. |
| Generated GX Peek/Poke stubs | The generated `_GXPeekARGB`/`_GXPokeARGB` sections are also discarded. Their textual presence is not evidence of a reachable native render fallback. |

Link evidence: `smstrikers-port/build-vita-gxm-latest/strikers.map`, inspected
discarded sections around lines 26061-26105, plus
[stubs_generated.c](../src/platform/stubs_generated.c) and the dead-strip
configuration in [CMakeLists.txt](../CMakeLists.txt). This is evidence for the
inspected build; recheck the matching map if link/source composition changes.

### Why the warble path is inactive in the examined retail build

The colour and offset textures are clearly intended as intermediate inputs to
an indirect warp effect. Their absence from a static texture dictionary alone
would not show that the effect is unreachable. The audit therefore followed
runtime registration and the original executable:

- [main.cpp](../src/Game/main.cpp), `SetupViews`, disables `GLV_Warble` and
  `GLV_WarbleBlend` when `glTextureLoad(glGetTexture("target/warble"))` fails.
  The same guard is present in upstream `decomp/main` at commit
  `d1f357ffeb1d8e89311edebf9a4b304b05ece7bb`; no later live re-enable was found.
- `glGetTexture` hashes the name. `glplatTextureLoad` in
  [glxTexture.cpp](../src/NL/glx/glxTexture.cpp) only succeeds for an already
  registered texture; neither it nor `glx_GetTex` creates a `target/*`
  resource. The latter also resolves texture-animation aliases, which were
  included in the original GLG scan.
- `glxInitTargets` creates a 320x224 RGB565 grab texture, a 320x224 RGB565 DOF
  texture and ten 80x80 A8 projected-shadow textures. Other registration paths
  create movie planes, crowd/Jumbotron textures, font/resource textures and
  lighting ramps; none registers the three warble IDs.
- `WarbleTest` in [GameRenderTask.cpp](../src/Game/GameRenderTask.cpp) returns
  before submitting its preview/blend packets when the warble texture is
  missing. Warble matrices and a guarded sender branch do not create or enable
  the missing path.

The pre-conversion original archive
`ab-artifacts/asset-pipeline-20261005/sms.psarc` has SHA-256
`79c9db029380164e9c9bc393e3b2511579362960eb63f95501d15d968ebba65c`.
Its **722 relevant original files** were searched for the text names and both
byte orders of the three IDs: `8d1f22b2` (warble), `5f723b69` (colour), and
`7aca0b1c` (offset). There were **zero occurrences**, including all 149 GLG
models/animation dictionaries, 164 GLT bundles, resource and effect files.
The corresponding native-pack dictionary scan also found no matching IDs in
164 NSTB bundles and 54 resource dictionaries.

The original `sys/main.dol` and `files/mariosoccerz.dol` are byte-identical,
SHA-256 `0e4850754d44c3819b32d5e0cf640490ff96904e2384b21f300924bc78703f91`.
Addresses were resolved with that archive's own `files/MarioSoccerZ.MAP` and
the DOL section table, rather than addresses for another regional build:

| Original retail symbol | Virtual address | Size | Verified binary behavior |
| --- | --- | ---: | --- |
| `UpdateWarble` | `0x801c1fd0` | 4 bytes | Exactly `4e800020`, the PowerPC return instruction `blr` |
| `ColourGrab` | `0x801c24b4` | 32 bytes | Function prologue/epilogue and one call to `nlBreak` at `0x801ce118`; no capture |
| `OffsetGrab` | `0x801c2494` | 32 bytes | Function prologue/epilogue and one call to the same `nlBreak`; no capture |
| `InitTargets` | `0x801c2548` | 492 bytes | Texture-allocation/registration calls match grab, DOF and the projected-shadow loop |
| `SetupViews` | `0x80173168` | 304 bytes | Texture lookup/load followed by two view-enable calls matches the original missing-warble guard |

This establishes dormant scaffolding in the examined retail build, rather than
a feature removed by native asset conversion. It does **not** establish an
implemented warble effect. Enabling that path in a future asset/configuration
would require complete target creation, capture and indirect-warp behavior.

## Current integrated device smoke

### SELF EE7c: clean recorded match smoke

The [candidate2 smoke summary](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate2-smoke-1/summary.json)
records all **120 requested live-play samples**, frames120-239, for SELF
SHA-256 `ee7c9a84d5d97b85d3a30fe206f74dd40b0cdb59e1b8e7703fad7ea68ec5abd5`.
The image shows the field, players, shadows, HUD and textures; no fresh crash
dump was observed. The cumulative [native status](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate2-smoke-1/vitagl_match_status.csv)
at frame900 reports **300,218 draws, invalid=0, gpu_error=0, operation=none,
failure=none**, with 446 native records, nine decoded records, 693 resident
entries and 45,869 dynamic draws. This closes the clean-smoke criterion for
this run. It does not certify every roster, stadium, feature or long session.

The run used diagnostics and visual capture, so its timing is not a quiet FPS
measurement. INI SHA-256 is
`5bd9778f7589f44bd4680e3f337a79cdff8f818bf6404637c7f8be65839c0f96`;
CSV SHA-256 is
`d76c7b2f0265232cce3ac4dd7823467fb136ffc9f4625904e4ad7daffa7dc92d`.

### Earlier SELF 9a: diagnostic registration failure retained in the record

The recovered earlier diagnostic run used SELF SHA-256
`9a4375c147edf8777a395ea59734578c9acb538404a8f7537c6f0813d951393d` and completed
all **120 requested profile samples**, without fresh crash dumps. Its snapshot
shows the field, characters, shadows, HUD and textures without magenta missing
texture output. This is a diagnostic run, **not a quiet performance run**;
new timing metrics are not assigned until its analysis is recorded.

The 120-sample window reports **39,054 accepted draws and zero rejected draws**.
However, the cumulative `vitagl_match_status.csv` at frame 663 reports 230,189
draws, **one invalid packet**, 429 native records, eight native decoded records,
682 resident entries, 34,375 dynamic draws, `gpu_error=0`, operation `none` and
failure **`skin_selectors`**. This counter is incremented during skin
registration, whereas profile rejections count draw callbacks; zero rejected
draws alone does not timestamp the registration failure. The cumulative CSV
does not identify its asset or frame. This historical run must not be
reclassified as an entirely clean functional pass. The subsequent capacity and
provider-lifetime corrections are included in the EE7c smoke above; broader
content and repeated-transition acceptance remain necessary.

Evidence for this executable and run is maintained under
`docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18`. Completion of
one exhibition run does not close the remaining scenario families below.

### Ordinary frontend and movie observations on EE7c

The ordinary UI runs use `vita_test_match=0`, `no_movies=0`, native video and
audio enabled. On-demand screenshots confirmed Health and Safety and later
Team Select. The [second UI log](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate2-ui-2/ui-log-1792339780102393000.log),
with `log_scenes=1`, records scene51 (Health), 39 (Legal), 53 (Intro), 2 (Title),
3/1 (Main Menu/Mario background), then scene8 (Team Select) with input. This
establishes that the ordinary frontend path is reachable. It does not certify
the entire menu flow or explain all earlier unobserved transitions.

The movie path has a concrete failure: `intromovie.thp` is selected and the
hardware decoder initializes, then the log reports
`decode/picture failed=0x00000000; movie skipped`. A title or menu reached after
this failure is not successful movie playback. The decoder correction and
hardware recheck are outstanding.

The first UI run later stopped producing the requested screenshot and log
updates after Start. No fresh dump was observed; its cause is not established.
The native quick-exit path requires Start+Select held for two seconds, whereas
the recorded input was Start alone. Pause/resume remains an acceptance item.

The second UI run reached Options and the Save/Load menu. After selecting LOAD,
request9 was absent and there was no additional scene entry or `[card] start`.
The [later UI2 log](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate2-ui-2/ui-log-1792340506602574000.log)
remained identical after another 98.27 seconds: 129,614 bytes, SHA-256
`e17c1d338db0856c727ba5781324c85a3429b091b1fce8632bb85899630f2eca`, last audio
counter 73,300. No fresh dump was observed. An earlier log collection does not
establish continued audio after this stop. The shared popup/frontend path is
being diagnosed; the CARD fixes above do not establish its cause or resolution.

All three examined local packs contain identical `common.ini` (4,133 bytes,
SHA-256 `7d684186b2c5ddec25e273552027a6388ece7048786c45970a72f4b10293d835`)
and `user.ini` (72 bytes,
`f9bb771d85e0190d9990521195a7302a47bbbe9ae8851b4a40d08bb14ff2fc07`). Neither
sets `skipfe`; `platform.ini` and `locale.ini` are absent. Thus an earlier
unobserved jump into a match cannot be attributed to a proven disc `skipfe`
override. Scene logging is the existing diagnostic control; adding arbitrary
`skipfe` to the port INI would not change the game's separate global config.

The [pre-UI save manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/cards-before-ui/manifest.json)
preserves cardA's 40,960-byte save and 116-byte metadata; cardB was empty.
Read-only inspection of those preserved copies verified the manifest hashes:

- Save SHA-256:
  `34367251ec96e1adde5fa3f85eb05054ef66e84ea73ba08e5a95537c9e531800`.
- Metadata SHA-256:
  `4d04275d4e91cfc693e3a29989e17d70229ce6f26b79724c5edeaec42ef49c3f`.

The metadata magic/version and length are valid. Its 8,256-byte icon/banner
header places the save record at the 16,384-byte sector boundary. Stored record
size is **18,537 bytes**; independently recomputed data CRC `AB02EE5E` and icon
CRC `4413ADE4` match the stored fields. The current ARM
`GameInfoManager::GetMemoryCardDataSize` returns the same 18,537-byte size:
16,178 plus the custom-tournament 2,359-byte record. Thus the examined backup
has a valid current native size and both CRCs; its format is not a demonstrated
explanation for the pre-I/O UI stop. This inspection changed no backup or
original save and does not establish successful on-device load behavior.

The subsequent [post-UI2 readback manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/cards-after-candidate2-ui-2/manifest.json)
confirms that both cardA files remain byte-identical to the pre-UI copies and
cardB remains empty. That navigation produced no observed save-file change.

Both recorded EE7c UI launches still use the earlier Vita bootstrap bypass.
The source correction and production callback gate described above restore
that path. Its later on-device autoload result and the still separate manual
popup acceptance are recorded below.

### Restored bootstrap/autoload on SELF 447fe

The [UI3-2 run identity](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate3-ui-2/run-identity.json)
records SELF
`447fe1129f8c3d7618ff8e9dce221356a0f3d89bb8f3f0c5e332e6f115856001`,
INI SHA-256 `6af98c53bb8c51bbc34df45684570e19ceb0b81c3500b863a681414b0a3c49d5`,
`vita_test_match=0`, movies/audio enabled, real-time simulation, diagnostics and
scene logging enabled, with renderer timing and profiling disabled. No input
was sent during this bootstrap, and no screenshot was requested in the first
60 seconds. The first request was published about 80.8 seconds after launch.

The [first screenshot](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate3-ui-2/native-candidate3-ui-2.request.1.png)
shows the normal **Loading from Memory Card Slot A** screen. The
[recorded log](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate3-ui-2/ui-log-1792342261675945000.log)
then proves the current-save load: exists callback result0, `MarioSoccer` read
of 18,944 bytes with result0, deserialized `[user] after-load` state with save
ID1372292338 and CRC `3a46d6bf`, load callback result0 and finished success1.
The 18,944-byte read is the aligned read buffer for the 18,537-byte record
validated above. The scene sequence reaches Health51, bootstrap35, Legal39,
Intro53 and Title2. Thus **autoload of this existing save is confirmed on the
device**. No save command was issued in this sequence; create, overwrite,
manual LOAD, error UI and progression persistence retain their separate tests.

The same log still reports a movie failure: hardware initialization succeeds,
but two decode attempts and a drain produce zero pictures, after which the
intro is skipped. Reaching Title after that fallback is not movie acceptance.
Audio output reaches 9,300 buffers with non-silent output and error0 in this
collection; this does not establish listening quality or long-session audio.

The separate [UI3-1 recording](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate3-ui-1/ui-log-1792341924163263000.log)
stopped advancing after Health51 and an earlier successful snapshot showing
the static LOADING message. Its final audio counter is2,600. The successful
UI3-2 bootstrap does not erase that intermittent stop, and the timing of the
screenshot requests alone does not establish its cause.

A source/asset audit found no unmatched loading ON/OFF in this initial route.
`main` presents the static LOADING bitmap before the first task transition;
`StateTransition(0x10000,4)` enables the indicator, initializes the fast Health
frontend and synchronously disables the indicator before returning. A completed
main-loop snapshot follows that return. Health itself changes neither loading
nor black state. The native FEN has two texture resources, `288d277b` and
`ecce7182`, both present in `BootUI.Res`; the later localized AsyncImage reads
are separate. An invalid scene can retain the static bitmap via `glDiscardFrame`,
but that is a possible symptom path rather than a demonstrated cause of UI3-1.
The FEN/resource/AsyncImage implementation was not changed in this integration.
References: [main.cpp](../src/Game/main.cpp),
[TransitionTask.cpp](../src/Game/TransitionTask.cpp),
[SHHealthWarning.cpp](../src/Game/SH/SHHealthWarning.cpp),
[feResourceManager.cpp](../src/Game/FE/feResourceManager.cpp),
[FrontEndTask.cpp](../src/Game/FrontEndTask.cpp) and
[vitagl_renderer.cpp](../src/platform/vitagl_renderer.cpp).

### UI3-2: confirmed renderer data abort after Pause/resume

The later UI3-2 failure now has a real core dump,
`psp2core-1792342444-0x0001d226b9-eboot.bin.psp2dmp`, SHA-256
`66c907fcc679f742ef366ce06efbabee2b92c3e42a8c74caedb5ba262f32a389`.
It belongs to SELF447fe above and the matching unstripped ELF with SHA-256
`3cdeba9bf3900287026089ca516a2b6a796753b0f246ee0b1a8f3dc14a92dfcf`.
The [offline diagnosis](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate3-ui-2/core-diagnosis.md),
[register/module inventory](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate3-ui-2/core-offline-inventory.json)
and [checked native call chain](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate3-ui-2/core-call-chain-and-allocation.json)
preserve the evidence and its limits.

The input diary records paired Start press/releases at 1792342354.78 and
1792342424.49. Screenshot3 shows Pause57; screenshot4 shows its fade after the
second Start. The dump timestamp is 1792342444, about 20 seconds later. The
last two logs have identical SHA-256
`cad2b54a0e49acf599fccdb97ec08598211de8718353507b9f476c282e750150`
and 54,908 bytes despite being collected 24.74 seconds apart; request5 was not
completed. This establishes a crash in this run, while the earlier UI3-1 and
EE7c stops retain their separate, unproven causes.

| Evidence | Finding and limit |
| --- | --- |
| Faulting thread | Renderer owner/pthread UID `0x40010151`, stop reason `0x30004` (data abort); PC `0xe0004934`, LR `0x01000000` unusable. The PC lies in the SceLibKernel allocation export interval. |
| Relocation | Application RX runtime `0x8102b000` maps to ELF `0x81000000`; RW runtime `0x81400000` maps to ELF `0x81350000`. The segments require different deltas. |
| Native call chain | Checked stack slots and disassembled prologues identify `gpu_alloc_mapped_aligned -> gpu_alloc_texture -> glTexImage2D -> Renderer::create_texture -> DeferredRenderer worker`. The kernel-to-native anchor is a stack candidate, not a complete unwound kernel frame. |
| Current request | An 80 by 80 RGBA8 runtime-generated texture, 25,600 bytes, alignment16. Allocation had not reached its pixel upload. The first captured 4 KiB of its source are zero. |
| Corrupt allocator data | R4 `0x64a40010` is inside the 48 MiB CDRAM mspace. A free-list field refers to `0x67608c28`; its captured page is almost entirely pixel-like word `0x40f4f3f4`, also in R2/R3. A raw tail value `0x40f4f3f8` is consistent with a fault address at R3+4, but the local primary parser does not label that tail, so it remains a qualified DFAR interpretation. |
| Loading state at fault | The captured renderer has loading inactive and loading resource IDs cleared. The failure occurred during normal material texture submission, after the earlier loading ON/OFF sequence. |

This proves allocator corruption at the observed allocation; it does not name
the earlier writer or prove which resource lifetime caused it. The texture-slot
table and faulting kernel instruction page are absent from the captured memory.
An independently identified provider lifetime gap was corrected in candidate4;
its startup/menu and first Pause/Resume observations below do not establish
sustained stability or identify the earlier corrupting writer. Repeated resume,
NIS-to-gameplay, Quit-to-frontend and resource reuse remain acceptance work. The source
trace indicates an intro/NIS transition here; a demo timeout or return-to-FE
must not be asserted as the cause of the dump.

### Candidate4: verified installation, autoload/menu progress and movie color defect

The [installation record](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/installs/6a78f252430d9e1c.json)
identifies the 2,427,336-byte SELF, verified by full readback, as SHA-256
`6a78f252430d9e1cab280d84c95ea63099b9f5cbbbf5f5013b9c8da20638e625`.
At installation, the previous executable was SELF447fe. The matching candidate4 ELF is
`ed42d349608d83427e9b6673d06514c62a252ef490819ac2f9a4e1ee5e8685d7`,
recorded in the [provider-options inventory](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-provider-options.json).

| Attempt | Verified outcome and evidence |
| --- | --- |
| `native-candidate4-ui-1` | `release all` received `Input released.` at 1792344435.427809. Cleanup then failed with FTP `550 Could not delete the file.` for an absent startup log. The [prelaunch inspection](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-1/prelaunch-inspection.json) explicitly records `launch_sent=false`; this is a workflow failure before game launch. Its [input-reset record](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-1/input-reset.json) and [traceback](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-1/launch-output.json) are preserved. |
| `native-candidate4-ui-2` | Reset received the same ACK at 1792344619.458542; launch returned `Launched.` and the fresh startup log was confirmed before fallback input. The [run identity](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-2/run-identity.json) records SELF6a78 and INI SHA-256 `72ae07336c735cac7b76a6ae129c71efd184bd8c77e0bd3045da60d7c2808a9d`. |
| `native-candidate4-ui-3` | Reset received the ACK at 1792346212.22509 and launch was confirmed by a fresh startup log before fallback input. The [run identity](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-3/run-identity.json) records the same SELF and INI SHA-256 `c34ac254d008905e3e55531e3aa13b5e747925cda66b6ee5669126795b28a042`, with `no_movies=1`. Its first Pause/Resume and later loss of progress are detailed below. |

The workflow now confirms absence with a bounded `SIZE` query before deciding
whether `DELE` is needed; a failed deletion is still an error. A separate,
offline rerun of `test_vita_workflow_device.py -v` passed **13/13 tests in
0.060 seconds**, with fake FTP/command implementations and isolated temporary
directories. Its [new gate JSON](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/workflow-delete-if-present-host-gate-candidate4.json)
and [log](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/workflow-delete-if-present-host-gate-candidate4.log)
retain source hashes; the earlier five-test gate is unchanged.

UI2 uses the ordinary path (`vita_test_match=0`, `no_movies=0`, native video
and audio enabled), diagnostics/scene logging and a unique snapshot request
path. Finite capture, packet profiling and GPU timing are off. The
[autoload log](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-2/ui-log-1792344674542765000.log)
records Health51, successful exists/load callbacks for Slot A, an 18,944-byte
`MarioSoccer` read with result0, coherent after-load state and then Legal39 and
Movie53. The [input/snapshot diary](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-2/ui-events.jsonl)
and [viewed screenshot5](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-2/native-candidate4-ui-2.request.5.png)
establish arrival at Main Menu. Screenshots6/7 subsequently record Options
selection and the Options page. The
[later scene/card log](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-2/ui-log-1792345198100934000.log)
then reaches the load-check path and scene27, with `callback op=askload result=0`;
[request12](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-2/native-candidate4-ui-2.request.12.png)
shows the confirmation popup. Manual LOAD had not been confirmed.

Before the next planned Down input, FTP INI verification returned
`550 Could not allocate memory`, followed by banner timeouts. The diary's bare
`tap` intent entry for ID13 does not represent a sent input: the
[transport record](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-2/transport-recovery-1792345321194850000.json)
explicitly records that request13 and its input were not sent. Companion still
answered `help`/`release all`, while targeted kill returned `cannot kill`.
[Recovery reboot](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-2/recovery-reboot-1792345348844113000.json)
was acknowledged; the first two FTP polls still timed out. The ACK itself
does not establish that a cold reset occurred.

The subsequent [read-only recovery](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-2/transport-recovered-evidence-1792346116426045000.json)
completed at 1792346117.607038. It recovered the unchanged 1,930-byte INI,
the existing request file and a 100,397-byte log, SHA-256
`851b5104cab4df958aca6ff24adbdb8d5f5b9e65f0bec0e7ce5f73d9510cf481`.
Only the known 447fe core appears in its dump list, with the same hash;
no new core was found at this checkpoint. The recovered log reaches audio
output 55,400, but its tail alone does not establish current process liveness.
This closes evidence collection after the transport interruption. It does not
diagnose that service failure or turn the unconfirmed manual LOAD into a pass.

The [later movie log](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-2/ui-log-1792344747945670000.log)
records cumulative decoded pictures **1, then 300**, with 512 by 416 output and
successful hardware decode/stop/flush return codes. This resolves the earlier
zero-picture observation for this intro, but does not establish playback of
all 1,949 intro frames, completion, synchronization or all 17 movies.

There is a demonstrated color defect. An offline search of source frames0-359
matches UI2 request2 to **frame183**: normalized grayscale comparison error
0.0513, versus 0.3851 for frame184 and 0.4332 for frame182. The
[matching record](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-video-colour-audit/frame-match.json)
preserves the screenshot/source hashes. The
[source frame](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-video-colour-audit/intromovie-frame183-original.png)
and [device crop](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-video-colour-audit/frame183-device-crop.png)
show corresponding scene content with visibly different colors.
[Single-frame color models](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-video-colour-audit/colour-models.json)
suggest a greatly reduced Cr contribution; a simple U/V swap fits worse than
the correct model. Source tracing subsequently confirms a coordinate mismatch:
the retail movie samples V from texture2 with coordinate1, while the native
shader used unsupplied coordinate2. The integrated host correction and its
limits are recorded below; hardware color equivalence is not yet established.

These are UI/functional observations, not a new FPS measurement. The recovered
dump inspection only establishes the state at its recorded collection time;
absence of a new core does not certify stable execution. The 447fe corruption
and repeated lifecycle paths remain open. The reset ACKs establish
a new synthetic-input initial condition and do not explain older uncommanded
frontend transitions. No whole-game 100% or 60 FPS claim follows.

### Candidate4 UI3: ordinary startup, one Pause/Resume and a later stop

The UI3 identity above records ordinary startup, native audio, real timestep,
diagnostics/scene logging, `vita_test_match=0`, `no_movies=1`, finite capture off
and packet/timing profiling off. The log again confirms current-save autoload,
then Health51/card35/Legal39/Movie53/Title2/Main3/1/Team8/Stadium9.
The [first log](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-3/ui-log-1792346271318305000.log)
and [input diary](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-3/ui-events.jsonl)
show these frontend transitions before the first scripted Start. No cause is
attributed to them: reset/fallback records do not exclude physical input or
prove a tested menu-selection sequence. With movies skipped and finite capture
disabled, this run supplies neither movie nor FPS qualification.

| UI3 observation | Recorded outcome |
| --- | --- |
| Snapshot1 | [Next Level Games legal screen](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-3/native-candidate4-ui-3.request.1.png), after successful autoload. |
| Snapshot2 | [Match presentation](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-3/native-candidate4-ui-3.request.2.png), with field, characters and goal. |
| First Pause | Start press/release at 1792346344.104227/1792346344.318889 reaches scene57; [snapshot3](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-3/native-candidate4-ui-3.request.3.png) visibly shows Pause. PPM SHA-256 `c2bbcb96ed6b26746a1b2750c48aab2efc89c6375349c1e9629773477ed2688b`. |
| First Resume | Paired Start at 1792346385.99399/1792346386.180161 is followed by advancing match state and [snapshot4](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-3/native-candidate4-ui-3.request.4.png), showing goal presentation without the Pause overlay. PPM SHA-256 `e4b594a0110459bc5cfd22fb0adcd94150fbfe299831c2a64de48c0467bcd71e`. |
| Temporary progression | Match time subsequently reaches 142.2/300.0, score 1-4, state 2, and audio output 31,100. This is not a completed match or repeated lifecycle acceptance. |
| Later loss of progress | The [log collected at 1792346584.299012](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-3/ui-log-1792346584299012000.log) and [repeat at 1792346704.847361](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-3/ui-log-1792346704847361000.log) are identical: 73,030 bytes, SHA-256 `7020a09e8c3d2ba0953bc73cd17e59d1a0093e0df4fd71c830c73f63520f0951`. Request5 published at 1792346644.929843 after another paired Start is absent. The log was already stopped before this input. |

The [dump check at 1792346713.691377](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate4-ui-3/dump-check.json)
finds only the known, hash-validated 447fe core. It supplies no new core for
diagnosing this stop. The first Pause/Resume is observed, while the later loss
of progress, Quit/rematch, repeated transitions and long-run stability stay open.
This is not a full regression pass for the earlier renderer corruption.

### Movie UV correction and video phase attribution: host evidence

The movie-only coordinate correction passes its [integrated 4/4 gate](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-movie-uv-gate/host-gate.json)
in 12.11 seconds, covering movie sampling, materials, shader variants and the
renderer driver. A retained negative control fails before the correction.
With the actual frame183 YUV fixture, [the shader oracle](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-movie-uv-gate/frame183-after.json)
passes 851,968 movie checks, 1,277,952 projective/invariance checks and 3,407,872
standard-material checks with zero errors. These arithmetic/source tests do
not certify Vita GPU precision, filtering or exact GX integer rounding.

All 160 standard fragment variants remain byte-identical across 20 masks times
alpha/affine-UV/vertex-fog alternatives. Their 814,384 concatenated source bytes
have SHA-256 `bac36777e9fd634d9d8de3dc098b147609c4431376f554d4922f67b677c03a09`;
see [source equivalence](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-movie-uv-gate/standard-source-equivalence.json).
The provider hashes the exact source with XXH3-64 in
[custom_shaders.c](../extern/vitaGL/source/custom_shaders.c). The read-only
generation audit derives generic cache names `ADFA0375446F97B9.gxp` before
and `360D4B4EE13F20B0.gxp` after the correction, from 5,324 and 5,346 source bytes.
Thus the changed generic source selects its own cache entry; unchanged standard
programs keep their existing entries. No cache deletion is needed for this change.

The separate [video-profile gate](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-video-profile-gate/cmake-gate.json)
passes 1/1 CTest in 3.73 seconds; its standalone UBSan driver passes in 0.845 seconds.
It checks gated prepare/init/Decode/Stop/Flush/tile attribution and absence of
extra timing queries when disabled. These component totals begin after
validation/locking and exclude their result/profile logs and teardown; cold
initialization includes its existing diagnostic. They are independent of
finite live-match captures. The [first fixture failure and corrected run](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-video-profile-gate/results.json)
remain recorded. This is measurement capability, not a measured speedup.

### Verified 447fe analysis retention and candidate5 identity

The original 447fe core above remains retained. Its [analysis manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate3-ui-2/core-retention-manifest.json)
has SHA-256 `689e5c31958025c7cdd83a6a87a2f0b6bd7e0e900dfc0d18e7b1d4e099cad6e8`
and pins 15 files totalling 454,158 bytes: the checked seven-native-frame chain,
identities, module/register/memory windows, relevant compiled layouts,
12 functions/12,722 instruction bytes and 12 EHABI entries/extab context.
The compiled bytes and entries were independently compared against the exact
447fe ELF before rotation.

The [retention gate](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate3-ui-2/core-retention-host-gate.json)
passes 15/15 tests in 0.511 seconds, covering full-artifact and explicit
analysis-capsule modes, altered/missing files, invalid schema/identity,
unsafe/symlink/nonregular paths and fresh core names/hashes, including the
launcher's dump-check function with fake transport. The baseline list is
unchanged. Capsule status remains `analyzed_hardware_unresolved`.
It preserves this bounded diagnosis; arbitrary new addresses outside its
extracts still require the exact old full ELF. The kernel instruction page and
runtime texture-slot table are absent, and the earlier corrupting writer is
unidentified. Retention is not a crash fix or a hardware regression pass.

The [rotation manifest](../build-vita-gxm-latest/native-candidate5-rotation.json)
records completion at 1792346614.662677 after capsule verification. At that
checkpoint, candidate4 SELF6a78 and its corresponding ELF/VELF/map/VPK became
the single complete `.previous` set; the older 447fe full set was retired with the bounded
evidence above. The [candidate5 build manifest](../build-vita-gxm-latest/native-candidate5-build-result.json)
records successful Release configuration in 5.121 seconds and build in 71.858 seconds,
completed at 1792346700.4777842, with profiler/autostart OFF and seven package entries.
SELF SHA-256 is `4f951a043386b51a32cd6bdd6105eda44924ae5edefc1a1b8b901b353ca4c6fb`
(2,432,279 bytes); matching ELF is
`b305ff78d4b6a6da9fab7ae52f7b14b1e78f22b874d2ca3f175710285c861762`.
The [installation record](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/installs/4f951a043386b51a.json)
verifies readback at 1792346766.5398688. The build/install checkpoint alone has
no hardware acceptance result. The following sections record the subsequent
candidate5 device work while retaining the earlier scenario/lifecycle limits.

### Candidate5 startup, movie colour and separate Health capture

`native-candidate5-video-1` uses SELF4f951 above and effective INI SHA-256
`8d59cf594d02a3214be67e22a34a158f2bd8365b052ee50d8e59ae6ad9f5eb58`.
The ordinary startup again autoloads the current save, reads 18,944 bytes and
completes with result0. PAD0 remains neutral on Health until the first scripted
Cross at 1792346908.167813; the log records the paired A-to-zero input. This
does not exercise manual LOAD from Options.

The [movie screenshot](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate5-video-1/native-candidate5-video-1.request.2.png)
at 1792346973.833271 is a positive visual check of the chroma correction:
Mario's clothing is red, the goalkeeper green, and skin tones are appropriate.
Its PPM SHA-256 is
`6b173d840cf97e161da9dfe5e606115d938c2515aec6eca2f6ac0a799cf47fbd`.
This establishes the observed picture, not numerical GPU equivalence or the
appearance of all movie frames.

The identity-checked [video profile](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate5-video-1/video-profile-analysis.json)
contains four disjoint component windows, closed by an explicit reset:
**1,127 components, 1,127 pictures, zero reported errors** in `intromovie.thp`.
Measured component-body time is 63.266866 seconds, or 56.137 ms/component;
Decode averages 23.556 ms, Stop 23.528 ms and Tile 8.489 ms. The total already
includes these children, and blocking call time is not GPU execution time.
The separate wall counter reaches 1,000 shown frames in 205.7 seconds. The
printed game-clock value 99.7 wraps and is not used as elapsed wall time.
These aggregates are not aligned to the finite frame-capture CSVs and do not
establish full-speed playback or synchronized sound.

Those five CSVs finished earlier on **Health (scene51)**, before the first
Cross: 300 non-match frames, 600 accepted packets, zero rejected packets and
zero simulation steps. Their **60.142 FPS / 16.627 ms mean** is a frontend
cadence, not gameplay or movie playback. The [Health attribution review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate5-video-1/health-attribution-review.json)
pins all five hashes and the scene log; the CSV alone cannot name the scene.
The analyzer's strict default rejects this capture as non-live. Its explicit
frontend scope passes the [2/2 CTest, 35-case host gate](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/non-live-profile-host-gate/results.json)
without relaxing identity, census or timing validation.

Cross at 1792347150.211692 skips the movie to Title and closes the decoder
profile. Later Start is acknowledged by Companion, but the preserved launch
segment has no corresponding PAD transition or snapshot4. Its 55,676-byte log,
SHA-256 `6998bcca14b66f505deb78b1374051e630c7e5fca24215e9079aa6df2490414e`,
stops advancing; no new core is recorded at the intervening check. This alone
does not distinguish crash, deadlock, suspension or termination.

The [foreground attempt](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate5-video-1/foreground-attempt-1792347318517793000.json)
preserves the earlier log and identity, then sends only `launch SMSVITA01`.
The next log has a new complete startup and reset counters. Its snapshot4
belongs to that new startup and does not validate the earlier Start or resume
the old evidence segment. Remote CSVs were rewritten; the already validated
first-segment local CSVs and summary remain preserved without another fetch.
All 17 movies, complete end/skip combinations, synchronization and repeatable
UI/lifecycle behavior remain open.

### Candidate5 live-match timing and 17-phase attribution

The following captures all identify SELF4f951, native audio, real timestep,
seed `0x53545249`, no movies, no frameskip and no screenshots. Each has 300
verified live-play samples600-899 at actual CPU444/GPU222 MHz. Quiet runs have
diagnostics and packet profiling OFF; phase runs enable packet attribution.
The serial experiment explicitly sets `vita_game_parallel=0`.

| Capture | FPS | Mean / median, ms | p95 / p99, ms | Frames within 16.667 ms |
| --- | ---: | ---: | ---: | ---: |
| [Default quiet](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate5-quiet-1/summary.json) | 26.168281 | 38.214203 / 36.8225 | 48.118 / 50.011 | 0/300 |
| [Default quiet repeat](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate5-quiet-2/summary.json) | 26.507865 | 37.724653 / 36.2735 | 46.359 / 49.073 | 0/300 |
| [Serial quiet](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate5-serial-quiet-1/summary.json) | 26.976447 | 37.069373 / 35.597 | 46.092 / 49.819 | 0/300 |
| [Default instrumented](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate5-phases-1/summary.json) | 25.728487 | 38.867423 / 37.334 | 50.950 / 59.729 | 0/300 |
| [Serial instrumented](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate5-serial-phases-1/summary.json) | 26.938417 | 37.121707 / 36.3445 | 43.862 / 46.340 | 0/300 |

The default quiet CSV SHA-256 is
`f997ef3678abfe0785a5545bd837443d74986c58e11b519d692eb723b8af0d40`,
INI `69e0bc311226e29e666b4b575715efdf289a63f55222cd64d62755649fb4115f`.
The serial quiet CSV is
`7a990a43a9bb84c271e02fc412e25bdd60a66d83fbc5f363f4d707fb73ff252b`,
INI `e3d98244fc44ce40978896301a878de2a600080a0777955c9eb20d8389dfa8f4`.
The default quiet repeat CSV is
`bd269e61534b00e7c748a7b9ef57e4962189f8702c64355023b12a50cb38e82a`,
INI `65cf4f899baf94d8071b963e0b5509b95dd4e81ddab9cccaff7c5b484fbe079d`.
The linked summaries bind each capture to its executable/INI and strict
live-play metadata. These are individual windows, not a stability distribution
or a demonstrated repeatable speedup over EE7c or between configurations.
The serial quiet result is only about 1.77% above this second default window;
the repeated baseline changes the comparison and prevents a general conclusion
from the first pair alone.

The [default phase summary](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate5-phases-1/task-summary.json)
and [serial phase summary](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate5-serial-phases-1/task-summary.json)
validate all 17 phases. Important costs use different denominators:

| Phase | Default | Serial | Denominator |
| --- | ---: | ---: | --- |
| Snapshot render | 5.208 ms | 4.694 ms | Per captured frame, 300 task runs |
| Pre-physics | 2.005 ms | 2.079 ms | Per simulation step, including worker/join work when enabled |
| Replay capture | 1.823 ms | 0.927 ms | Per simulation step |
| Simulation steps in 300 frames | 585 | 559 | Observed loop iterations |

Default simulation advances 11.700 seconds from 11.696 seconds supplied;
serial advances 11.180 from 11.172 seconds, with maximum remaining debt below
0.020 seconds in both windows. They have different workloads and step counts;
per-step normalization does not make the trajectories identical. Phase children
are subsets of their parent tasks and must not be added again. Snapshot render
means pose/replay rendering, not the optional diagnostic screenshot.
These data select optimization targets while gameplay, replay and rendering
semantics remain required. They do not close the 60 FPS or whole-game ledger.

### Candidate6: natural intro completion, repeat skip and manual LOAD

The [reviewed ordinary UI run](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate6-video-2/ui-review.json)
identifies SELF SHA-256
`582c69e8a6be2b136b5213be01dfb06f8ce7e0d6970be3bde6d8c96eb83b092d`
and INI `46eff6e64712a649cb425c1fd9f7ca7f44d77311f39c5d33ffde37f61bfaace8`.
Its launch time is 1792348774.015157. Fifteen reviewed snapshots and the
201,375-byte final UI log, SHA-256
`109ef15eb51d791e2467a40538710bc38140fe429b3dd504be82fbdcde742d38`,
support the following observed paths.

| Movie episode | Decoder result | Observed exit |
| --- | --- | --- |
| First `intromovie.thp` playback | 1,949 components, 1,949 pictures, zero errors | Natural completion to Title before the later explicit Cross input |
| Second playback of the same file, entered by Title's attract cycle | 1,814 components, 1,814 pictures, zero errors | Explicit Cross at 1792349277.199908; snapshot4 shows Title |

The [identity-checked video analysis](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate6-video-2/video-profile-analysis.json)
records these as two reset-closed epochs: 3,763 pictures in total, zero reported
decoder errors and no parser issues. Its SHA-256 is
`fd3f5e7796f79710813450fb9f4ee2907501ad44b79841999337dc5797e8d3ca`.
This is completion and skip coverage for **one of the 17 movie files**.
The first episode's wall counter reaches 1,000 shown frames in 106.9 seconds
and 1,500 in 150.0 seconds; the wrapped game-clock field is not elapsed wall
time. These diagnostic observations do not establish full-speed playback or
a repeatable speedup. Snapshot3 shows Mario's red shirt; snapshot2 contains
green/yellow effects and is not a neutral colour reference. Audio counters
record non-silent output and zero output errors/underruns, while listening
quality and A/V synchronization remain unqualified.

The subsequent observed route is Title -> Start -> Main -> Options ->
Save / Load -> Load -> confirmation -> Load -> Options. At
1792349904.6310139, the final Cross starts the manual Card A load; release is
recorded at 1792349904.847113. The operation reads **18,944 bytes**, returns
read result0 and callback result0, and finishes with `success=1`.
After-load state records save ID `1372292338` and CRC `3a46d6bf`.
Scene38 and snapshot15 confirm the return to the Options menu.

The [post-load card manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/cards-after-native-candidate6-manual-load/manifest.json),
completed at 1792350010.718722, verifies that the saved data is byte-identical:

| Card A file | Bytes | SHA-256 |
| --- | ---: | --- |
| `.sav` | 40,960 | `34367251ec96e1adde5fa3f85eb05054ef66e84ea73ba08e5a95537c9e531800` |
| `.stat` | 116 | `4d04275d4e91cfc693e3a29989e17d70229ce6f26b79724c5edeaec42ef49c3f` |

Card B remains empty. This closes one ordinary manual LOAD and return case
for the existing Card A data; no save, overwrite or delete was selected.
It does not qualify persistence after new progression, error popups or every
card state. No new core was found at the final retrieval. Earlier crashes
and unexplained losses of progress remain separate, unresolved observations.

The run's finite capture completed earlier on Health (scene51), before any
Cross: 300 samples with `match=0` and `match_frame=0`. Its approximately
60.1397 FPS is neither movie cadence nor gameplay. The distinct quiet
live-match result for this SELF is recorded in the hardware-speed section.

## End-to-end acceptance ledger still needed

Every device result should identify the executable hash, archive hash, INI,
scenario, outcome and evidence path. A success in an automatic exhibition
match does not close menu, tournament or persistence rows.

| Scenario family | Required acceptance | Current evidence limit |
| --- | --- | --- |
| Cold and warm startup | Asset verification, native loading, title and menu entry, no missing resources | Earlier startup/autoload/menu evidence is retained above. Candidate6 completes the intro to Title, observes the attract-cycle repeat/skip and reaches Main/Options. Repeated transitions remain open; the 447fe UI3-2 crash and later losses of progress remain separate observations. |
| Frontend/options | Every offered menu and option; correct text, icons and transitions | EE7c reaches Main Menu, Team Select, Options and Save/Load; 447fe displays Pause. Candidate6 reaches the manual LOAD confirmation, completes the Card A operation and returns to Options in snapshot15. This does not qualify every menu or repeated lifecycle stability. |
| Team and stage selection | Every offered captain, sidekick and stage loads correctly | Pack conversion is broad; scenario enumeration/results incomplete |
| Live match | Movement, switching, passing, shooting, tackling, items, charged/super attacks, AI and keeper behavior | Prior real gameplay timing exists; all behavior combinations not certified |
| Special presentation | Introduction, goals, replay, end-of-match, overlays, particles and crowd animation | Renderer changes require new visual comparison |
| Broken-glass transitions | Both named sequences above, with retained screenshot texture and visible line outlines | Confirmed reachable assets; device check pending |
| Shadows/DOF/material combinations | Correct ordering, alpha, depth, texture state, projected shadows and full-screen copies | Host contracts help; complete visual qualification pending |
| Pause and exit | Pause/resume, quit/rematch, return to frontend, relaunch and cleanup | Black/bitmap/loading host contracts pass. 447fe UI3-2 has a renderer allocation data abort. Candidate4 UI3 visibly pauses, resumes and advances the match, then loses progress later without a new core at the final check. Repeated resume, intro-to-play, Quit, rematch and cleanup remain open; this is not a full stability pass. |
| Cups/tournaments/progression | All offered competition flows, wins/losses, completion and unlock persistence | Game code is present; full progression run not recorded |
| Save/load/error UI | Bootstrap/autoload, create, overwrite, reopen after process restart, delete and displayed failure behavior | Earlier automatic exists/load cases and host CARD/callback 2/2 are recorded above. Candidate6 manually loads the existing Card A save: reads 18,944 bytes, result0, callback0, success1 and return to Options; `.sav`/`.stat` remain byte-identical and Card B empty. Save/overwrite/delete, other card/error states and new progression persistence remain pending. |
| Sound | Music, effects, commentary, transitions, simultaneous voices and long-run audio continuity | Native audio backend and preparation exist; full listening/underrun evidence absent |
| Movies | All 17 prepared movies start, play with synchronized sound, end and skip correctly | Candidate4's matched frame183 colour defect and candidate5's positive correction are retained. Candidate6 completes `intromovie.thp` naturally with 1,949 pictures and skips a separate repeat after 1,814; both report zero decoder errors. The first 1,000 shown frames still take 106.9 seconds diagnostically. This covers one file's observed completion/skip; all 17, synchronized sound and full-speed cadence remain open. |
| Input | Button/analog mapping, held/released transitions, overlay interaction | [pad.c](../src/platform/pad.c) maps only Vita port 0; slots 1-3 report no controller |
| Multiplayer scope | Define and implement the intended Vita multiplayer behavior if original multi-controller parity is required | It must not be silently counted as complete; current backend is one local controller |
| Suspend/resume and long sessions | Stable device lifecycle, resource limits and repeated match/menu transitions | Requires hardware testing |
| Real-time speed | No frame skip, real-time simulation and accepted live-match frame-time distribution | Candidate6 quiet: 25.74780 FPS, mean 38.83827 ms, 300 live-play samples 600-899 and 0/300 inside 16.667 ms. Candidate5 default repeats 26.168/26.508 and serial 26.976 are separate windows, not a demonstrated causal speedup. Health's approximately 60 FPS is not gameplay. |

This table is a proposed acceptance denominator, not a claim that every row
has been exercised. Specific content IDs and the number of cases within each
family must be recorded when executing the ledger.

### Smallest practical device matrix using the existing controls

The existing workflow accepts repeatable `--set KEY=VALUE` overrides. The
runtime explicitly bridges only `team1`, `team2`, `sidekick1`, `sidekick2` and
`stadium` into the game's global configuration in
[main.cpp](../src/Game/main.cpp); arbitrary disc-configuration keys placed in
the launcher INI do not become game settings automatically. For example,
`fe_demo_mode_time_out` is 60 seconds in the examined pack, but no equivalent
environment bridge was found.

| Scenario | Existing controls | Evidence to collect |
| --- | --- | --- |
| Ordinary frontend startup and movie completion | `--set vita_test_match=0 --set no_movies=0 --set vita_native_video=1 --set audio=1 --set log_scenes=1`; use `--set frame_capture_match_only=0 --set frame_capture_play_only=0` to include frontend phases. This removes the forced test-match path; ordinary disc settings still apply. | Fresh startup log, movie start/stop and title/menu progression, displayed frames and sound. Candidate4 displays intro pictures with an observed color defect; reaching the menu does not establish correct movie playback. |
| Movie skip and menu input | On an observed movie, issue `press cross`, then `release cross`; on the observed title/menu, use cross, circle and directional press/release pairs. | Movie teardown and correct next scene, accept/back/navigation feedback, no stale movie texture or audio. There is no direct INI selector for all 17 movies. |
| Content sweep | Run the seven explicit stadium/roster combinations below, with audio enabled and `vita_frameskip=0`. | Intro and live-play screenshots, packet/state errors and native health counters for each named content case. |
| Introduction, goal-area and ordinary match transitions | Set `frame_capture_play_only=0` for match phases; select `vita_snapshot_match_frame` for introduction or `vita_snapshot_play_frame` for play. Diagnostic `vita_snapshot_goal_x` delays a play snapshot until the ball reaches the chosen absolute X threshold. | Copy/depth/shadow/material fidelity and transition output. The threshold observes ball position; it does not force a goal or certify replay coverage. |
| Pause, resume, return to frontend and new match | During an observed running match, pair `press start`/`release start`; the ordinary pause menu accepts Start or Circle to resume. Navigate Quit/Choose Sides using observed menu state, then enter another match. | Pause texture/UI correctness, retained settings, resource reuse across match/menu transitions and continued frame production. |
| Human-controlled input | Enter a normal match through the frontend, or assign port 0 through the pause Choose Sides screen before applying analog/button input. | Movement, passing/shooting and release transitions. `vita_test_match=1` deliberately leaves both sides under AI, so gameplay buttons in that default run do not validate human control. |
| Process restart | The installed VitaCompanion 1.06 was queried directly and explicitly supports `kill SMSVITA01` and `launch SMSVITA01`. Keep press/release commands paired and observe startup confirmation. | Correct relaunch with the same identified binary/archive and restored baseline INI. Process kill is not a substitute for testing in-game resource teardown. |

Seven automatic content launches cover every stadium, all nine accepted
captain names and the four accepted sidekick names at least once. They do not
cover all pairwise combinations:

| World ID | `stadium` | `team1` | `team2` | `sidekick1` | `sidekick2` |
| ---: | --- | --- | --- | --- | --- |
| 1 | `The_Palace` | `mario` | `luigi` | `toad` | `koopa` |
| 4 | `Yoshi_Stadium` | `yoshi` | `daisy` | `hammerbro` | `birdo` |
| 6 | `Forbidden_Dome` | `peach` | `waluigi` | `koopa` | `hammerbro` |
| 5 | `Super_Stadium` | `mystery` | `mario` | `toad` | `koopa` |
| 2 | `DK_Daisy` | `donkeykong` | `wario` | `birdo` | `toad` |
| 0 | `Mario_Stadium` | `luigi` | `yoshi` | `hammerbro` | `koopa` |
| 3 | `Wario_Stadium` | `wario` | `waluigi` | `toad` | `birdo` |

Accepted names are defined in [feHelpFuncs.cpp](../src/Game/FE/feHelpFuncs.cpp)
and [WorldLoader.cpp](../src/Game/World/WorldLoader.cpp). The first four stadium
cases target the rare stored material masks identified by the census.

These exact INI values were rechecked against `OnPreGameState`, the team and
sidekick converters, and the world filename array. The comparisons are
case-insensitive. `stadium=The_Palace` is the asset-name selector; it is not the
separate `force_stadium=peach_toad` diagnostic selector. Unknown names return
invalid IDs rather than selecting a documented default, so keep the values
above unchanged. For `team1=mystery`, `GetSidekick(0)` always returns
`SK_MYSTERY`: expect the Super Team's robot companions, despite the accepted
but ignored `sidekick1=toad` value. Toad is exercised in other rows.

For each short visual content launch, apply all five row values with `--set`
and use the following common controls. The snapshot request path must be
unique to that run; the other snapshot selectors remain zero.

```ini
vita_test_match=1
fixed_dt=0
vita_frameskip=0
audio=1
vita_native_audio=1
vita_native_video=1
diagnostics=1
log_scenes=1
log_match=1
probe_dvd=1
vitagl_timing=0
vitagl_packet_profile=0
frame_capture_frames=0
```

Record the fresh launch identity, introduction with named teams/stadium, live
players/sidekicks, HUD, shadows and field. `probe_dvd=1` in
[dvd.c](../src/platform/dvd.c) prints `[port] dvd: open <path>` and read results;
retain the requested world and character/model paths and reject reported
short/failed reads. File-open evidence alone does not certify visible output.
Collect the fresh cumulative `vitagl_match_status.csv` through introduction
and live play: require `invalid=0`, `gpu_error=0` and no first failure, together
with the matching visual evidence. A fresh `[match]` line proves continued
simulation, but does not identify the roster by itself.

The native loaders reject unconverted models/textures rather than silently
using the old format. Search the launch log for `[native-model] converted model
required`, `[native-texture] ... invalid/failed`, malformed model/skin messages
and `[texload] ... not loaded`; any such message needs classification before
acceptance. Sources: [glxLoadModel.cpp](../src/NL/glx/glxLoadModel.cpp) and
[glxTexture.cpp](../src/NL/glx/glxTexture.cpp). There is no single current log
line acknowledging all five resolved selectors, so INI identity or zero error
counters alone are insufficient. Disable `probe_dvd` and these diagnostic
controls for the separate quiet speed measurement.

For an ordinary frontend visual run, enable diagnostics and set
`vita_snapshot_request=ux0:data/strikersVita/<unique-run>.request`. Each new
positive integer ID (1-2147483647) written to that file triggers one existing
frame readback to `<request-path>.<ID>.ppm`; stale/duplicate IDs are ignored.
Polling is limited to twice per second while explicitly enabled, and disabled
runs perform no request-file polling or additional polling-clock calls. The
trigger is host-tested and has produced the UI screenshots above.

Use `frame_capture_frames=0`, profiling off and both match/play snapshot
selectors zero for this UI workflow. The one-shot request does not consume a
future scheduled snapshot. The helper's `--visual` default is a match snapshot,
which cannot fire while remaining in the frontend. Screenshot/input traversal
runs remain separate from quiet FPS measurement.

Cup completion, save/error screens, lesson movie selection, original multiplayer
parity and suspend/resume are not closed by these launch flags. They require
observed UI/state traversal or a specific additional harness; no direct
supported scenario selector was found for them in the current launcher.

### Next retail UI and progression checks, in execution order

These are planned cases, not recorded passes. Current-save autoload and the
candidate6 manual Card A LOAD/return are recorded above. Next verify sustained
Pause/resume after the confirmed UI3-2 renderer crash, repeat manual loading
and startup for stability, and exercise the progression routes below.
Continue with `vita_test_match=0`,
`log_scenes=1`, `log_match=1`, native audio/video enabled, real-time simulation,
profiling off and the one-shot screenshot request. Remove the five content
overrides above before a new process launch: `OnPreGameState` reads them on
every match, and they can override or contaminate frontend choices. Leave
forced stadium and unlock-all controls inactive when testing progression.

Cross is retail A, Circle is B, Triangle is Y; pair every press with release.
Use observed labels and wait for the next fresh screenshot/scene entry before
sending the next action. Scene IDs below come from
[BaseGameSceneManager.h](../include/Game/BaseGameSceneManager.h).

| Priority | Shortest retail route and expected feedback | Save effects |
| --- | --- | --- |
| 1: read-only trophy round trip | Main Menu (3) -> Trophies (28). Open regular Cups (44), return with Circle; open Super Cups (44) and Milestones (45), then Circle back through 28 to 3. Verify earned/locked appearance, left/right trophy browsing, text/images and continued snapshot production. | Viewing these existing trophy pages does not issue a save. It does not establish earning or unlocking them. |
| 2: ordinary match/menu round trip | Main Menu -> Friendly -> Team Select (8): choose captains/sidekicks and assign/ready P1 in the same scene -> Stadium Select (9) -> loading -> live match. Exercise a visible pass/shot/item, Start -> Pause (57), resume, then Pause -> Quit confirmation -> Main Menu. Re-enter Friendly and start another match. | This checks the normal transition/resource path. A completed match changes in-memory statistics; preserve/read back the card when checking eventual persistence. |
| 3: shortest regular cup and continuation | Main Menu -> Cup Battles (13), Mushroom Cup -> initial options (40), choose 2 minutes and Rookie -> captain, sidekick and final ready confirmations (15) -> standings (17). Cross -> Choose Sides (5), ready P1 and start. After one completed game, return to animated standings (18); note `[cup] hub` round/game, then Circle -> confirm Main Menu. Re-enter Cup -> Mushroom -> Continue and verify the same standings. | The initial ready confirmation arms hub autosave, and return from each cup game arms it again. Expect `[cup] autosave`, `[card] start op=save`, one successful callback and `[card] finished ... success=1`. Main-menu return itself does not add a save. |
| 4: actual cup progression and trophy | Finish Mushroom's three rounds, recording scores/rank, then the end hub and its Cross continuation through trophy/unlock/brag pages (44/46, with possible milestone 45). Top three unlock Flower; first place is required for the championship trophy. Reopen Cup selection and Trophy Room, then relaunch/load and verify persisted progress. | Postgame hub autosaves; Cross leaving Bragging Rights (46) also pushes Save (31) before Main Menu. A trophy image alone is insufficient: compare `[user]` trophy bits/`flower`, rank, save callback and reopened UI. |
| 5: minimal custom league | Main Menu -> Custom Tournament (36): League, 3 teams, 1 game per opponent -> initial options (42), 2 minutes -> Team Setup (37). Triangle/Y autofills empty rows and tags one human team; verify its controller icon. Left/right on a row toggles human/CPU. Start -> animated standings (24), Cross -> Choose Sides (7). One human team plays two opponents across the three-round schedule. Verify a bye/CPU fixture, standings update, Circle/confirm Main Menu and Custom -> Continue. | Start at completed Team Setup arms autosave; each match return arms it again. If an old custom tournament exists, choose Continue or explicitly confirm New/overwrite; creating a new one replaces that stored tournament on the next save. |
| 6: minimal custom knockout | Start a new custom tournament, choose Knockout and 4 teams (the only sizes are 4 or 8), then the same 42 -> 37 setup, Triangle and Start -> bracket (23). Exercise semifinal/final progression and the end state; an early loss separately tests elimination. One human team winning the bracket requires two wins. | Same autosave points as custom league. A one-human end goes through the Tournament Over popup to Main Menu; multiple human teams use player awards (47). |

The two-minute option is the retail minimum, not a clock override. Overtime
and presentation can make elapsed time longer. A completed Mushroom run closes
only that initial progression path: Flower, Star, Bowser, all four Super Cups,
their later unlocks, higher custom sizes and milestone thresholds remain
separate acceptance cases. Flower requires a top-three Mushroom finish; Star
requires a top-three Flower finish; Bowser requires the three preceding regular
cup trophies; Super Cup mode requires the Bowser trophy. Do not enable unlock
cheats to count those paths as completed.

Additional save boundary: leaving Options after a changed `UserInfo` CRC pushes
Save; leaving unchanged Options does not. Confirming non-initial cup options
also pushes Save. Inspect the already preserved card snapshot before write
cases and record the resulting readback/CRCs afterward. The normal LOAD and
trophy-viewing cases should not be credited as save/overwrite tests.

Route/write references: [SHMainMenu.cpp](../src/Game/SH/SHMainMenu.cpp),
[SHChooseCup.cpp](../src/Game/SH/SHChooseCup.cpp),
[SHCupChooseCaptain.cpp](../src/Game/SH/SHCupChooseCaptain.cpp),
[SHCupHub.cpp](../src/Game/SH/SHCupHub.cpp),
[SHTournSetParams.cpp](../src/Game/SH/SHTournSetParams.cpp),
[SHTournTeamSetup.cpp](../src/Game/SH/SHTournTeamSetup.cpp),
[SHSpoils.cpp](../src/Game/SH/SHSpoils.cpp),
[SHCupOptions.cpp](../src/Game/SH/SHCupOptions.cpp),
[SHOptions.cpp](../src/Game/SH/SHOptions.cpp),
[BraggingRights.cpp](../src/Game/FE/BraggingRights.cpp), and
[GameInfo.cpp](../src/Game/GameInfo.cpp).

### Execution checklist after candidate4, using the existing helpers

Candidate4 SELF6a78's UI1/UI2/UI3 outcomes and candidate5/6 partial device
acceptance are recorded above. The commands below originated at the candidate5
checkpoint and remain recipes for open work, not additional results. Use the
actual installed candidate in the run label and replace `N` with an unused
label before launch. Preserve the verified 447fe analysis capsule/raw dump and
the single complete `.previous` set selected by the latest rotation manifest.
The helper records the installed SELF and exact INI; remote archive size alone
remains an identity limit. Use the preserved baseline, whose five roster/stadium
keys are absent.

**1. Ordinary UI, autoload, manual LOAD and transition reuse.**

```sh
python3 .tmp/safe-script-native-device-rgba-20261018.py launch \
  --label native-candidate5-ui-N --frames 1 --skip 0 --diagnostics \
  --set vita_test_match=0 --set no_movies=0 \
  --set audio=1 --set vita_native_audio=1 --set vita_native_video=1 \
  --set log_scenes=1 --set log_match=1 --set vitagl_timing=0 \
  --set frame_capture_frames=0 --set frame_capture_match_only=0 \
  --set frame_capture_play_only=0 \
  --set vita_snapshot_request=ux0:data/strikersVita/native-candidate5-ui-N.request
```

The final INI override disables finite capture despite the required positive
CLI `--frames` argument. The UI helper needs the launch's startup confirmation.
The launcher's `startup-log` confirmation may send one paired Cross if its
fresh marker is absent after three seconds; record whether it did so before
describing a run as input-free. UI3-2 has `started_before_input=true`, so that
fallback was not sent in the crash run.

The local VitaCompanion reference (`d46772344d926e6765943703adf1a1518c20e10e`)
keeps virtual input in kernel globals until release/reset; TCP disconnection
and application launch do not reset them. `release all` clears synthetic
buttons and analog/touch overrides and returns `Input released.` on success;
it does not alter physical input. This reference exposes `quit`, while the
installed1.06 advertises `kill`, so its code alone cannot certify the deployed
kill path. For candidate4, the temporary launch wrapper now sends `release all`
before launching, requires the exact ACK and preserves command/response/time
in `input-reset.json`, then `run-identity.json.synthetic_input_reset`. Syntax
validation passed, and the installed1.06 ACK is now recorded for candidate4
UI1, UI2 and UI3 in the evidence above. This is a new initial condition for
candidate4, not an explanation of older transitions. Existing UI helpers record tap ACKs in `ui-events.jsonl`
but do not provide a reset action. All 14 recorded inputs across UI2-1, UI2-2, UI3-1 and
UI3-2 have both press and release ACKs; all four run identities show startup
before fallback input. These records do not support attributing UI3-2's
pre-Start frontend progression to an unreleased synthetic button. References:
[kernel/main.c](../../.tmp/vitacompanion-reference/kernel/main.c),
[input_parse.c](../../.tmp/vitacompanion-reference/src/input_parse.c) and the
[UI helper](../../.tmp/safe-script-native-ui-20261018.py).

Use the request helper after observing scene progression; apply a bounded tap
only when the displayed state requires that action:

```sh
python3 .tmp/safe-script-native-ui-step-20261018.py request \
  --label native-candidate5-ui-N --id 1
python3 .tmp/safe-script-native-ui-step-20261018.py tap \
  --label native-candidate5-ui-N --id 2 --button cross \
  --duration-ms 80 --settle-ms 750
```

IDs must increase. A pending screenshot is fetched again with
`safe-script-native-ui-20261018.py fetch --label native-candidate5-ui-N --id ID`;
do not resend the input merely to obtain its screenshot. Finite-run `poll`
is not the UI collector when `frame_capture_frames=0`.

| Order | Observed route | Acceptance evidence |
| --- | --- | --- |
| A | Health -> automatic Slot A load -> Legal -> Intro -> Title/Main | Successful exists/load callbacks and coherent after-load state; actual decoded movie pictures/audio and completion; fresh displayed output. A decoder fallback does not pass the movie. |
| B | Main -> Options -> Save/Load -> LOAD -> confirm and return | Candidate6 records one successful existing-Card-A load, return to Options and unchanged card bytes above. Repeat for stability and separately cover other card/error states; popup entry alone is not CARD completion. |
| C | Main -> Friendly -> team/sidekick/stadium -> intro/NIS; Pause/resume there, then let gameplay begin | Correct choices and NIS, visible Pause/fade, sustained live match after loading OFF, no new dump or native failure. This repeats the transition adjacent to the 447fe fault. |
| D | Live match: human pass/shot/item -> Pause/resume -> Pause/Quit -> Main -> second Friendly | Continued input/rendering, complete menu round trip and reused resources; do not count process kill as Quit/cleanup. Also inspect loading and black output for stale main-frame promotion. |
| E | Main -> trophy pages -> Main; then the progression routes above | Read-only trophy round trip before writes. Cup/custom autosave, unlock and restart/load need their own before/after save evidence. |

The existing read-only card verifier can compare A/B with the preserved copy:
`python3 .tmp/safe-script-native-save-verify-20261018.py --label candidate5-ui-N`.
It preserves changed readbacks and reports a difference; after an intentional
SAVE or cup/custom autosave, review that difference as an expected write case
with record/CRC validation rather than calling it an unchanged-card pass.

**2. Attribute current frame cost across the 17 instrumented phases.**

```sh
python3 .tmp/safe-script-native-device-rgba-20261018.py launch \
  --label native-candidate4-phases-1 --frames 300 --skip 600 \
  --profile --tasks --phases \
  --set vita_test_match=1 --set fixed_dt=0 --set vita_frameskip=0 \
  --set audio=1 --set vita_native_audio=1 --set vitagl_timing=0
python3 .tmp/safe-script-native-device-rgba-20261018.py poll \
  --label native-candidate4-phases-1
```

`--tasks` requires `--profile`, and `--phases` requires `--tasks`. The current
runtime enables task/phase capture from packet profiling plus finite capture;
there is no additional phase-enable INI setting. Poll fetches frame, packet,
view, task and phase CSVs, then runs both analyzers, verifies executable/INI
identity and collects cumulative native status. Require all 300 selected rows
and aligned sample/live/engine frame identities. The phase report separates
fixed control/input, AI, physics, replay/lighting/presentation and render work
across its 17 names; inspect fixed-step counts/debt and unattributed parent
time. Instrumented phase coverage is a cost denominator, not functional game
coverage. The 300/skip600 window matches the principal EE7c baseline; 120 rows
remain appropriate for a short smoke test. This instrumented run is not the
quiet speed comparison.

**3. Execute all seven exact roster/stadium rows above.** For row1:

```sh
python3 .tmp/safe-script-native-device-rgba-20261018.py launch \
  --label native-candidate4-content-1 --frames 120 --skip 120 \
  --diagnostics --visual --set vitagl_timing=0 \
  --set log_scenes=1 --set log_match=1 --set probe_dvd=1 \
  --set audio=1 --set vita_native_audio=1 \
  --set stadium=The_Palace --set team1=mario --set team2=luigi \
  --set sidekick1=toad --set sidekick2=koopa
python3 .tmp/safe-script-native-device-rgba-20261018.py poll \
  --label native-candidate4-content-1
```

Repeat as `content-2` through `content-7`, replacing all five keys with the
corresponding row. The scheduled `--visual` snapshot is live-play frame120;
it does not prove the named introduction by itself. Add a run-specific request
path and observe intro/live screens where needed. Require the requested asset
paths plus visible expected content, no unclassified native-loader failures,
fresh cumulative invalid0/gpu_error0 and no new unreviewed dump. A successful
120-row window cannot erase an earlier failure outside that window.

**4. Quiet timing after functional stability.** Run a new label
`native-candidate4-quiet-1` with `--frames 300 --skip 600`, audio on and no
`--diagnostics`, `--profile`, `--tasks`, `--phases` or `--visual`. The wrapper
sets `fixed_dt=0`; verify that value, frameskip0 and observed clocks in the
record. Compare a completed valid distribution with EE7c's 24.681 FPS, then
repeat across accepted content. No current result establishes 60 FPS.

The A-D route plus seven launches still leaves the following blocks open:

| Remaining block | Why the proposed checks do not close it |
| --- | --- |
| Stable lifecycle | A single repaired transition does not qualify repeated cold/warm starts, long play, suspend/resume, resource pressure or the separate UI3-1 stop. |
| All movies and teaching | The ordinary intro covers one movie. Strikers101/lesson selection, its lesson movies, credits and all 17 prepared movie files need observed play/end/skip and sound. |
| Match presentation and effects | Goal celebrations/replay, end-match, super attacks, all items, rare broken-glass paths, shadows/DOF and dynamic captures need triggered visual evidence; automatic roster coverage cannot guarantee those states. |
| Saving and progression | Manual read is separate from create, overwrite, error UI, autosave, cup/custom completion, unlocks and persisted trophy/statistics. The minimal Mushroom/custom paths do not cover every competition or milestone. |
| Human controls and multiplayer | AI-only launches do not exercise held/released analog/buttons or human gameplay. Current PAD only supplies port0; original multi-controller parity remains a scope/implementation gap. |
| Sound | Non-silent buffers and error0 do not prove music/commentary/effect balance, simultaneous voices, A/V synchronization or long-session continuity. |
| Speed and identity | Seventeen measured phases diagnose cost; they do not meet the frame budget. Quiet results need identified binaries/content, and the complete remote archive SHA remains unverified. |

## Last measured hardware speed

### Candidate7 SELFce82: lifecycle result before quiet comparison

The [candidate7 UI review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate7-watchdog-1/ui-review.json)
identifies SELF `ce82e093e411deadbfc333cf72aa3c8992ca1563ca3aa947dd91b858ea6ae924`
and INI `5c9f3ac4628b0a275e906650329d51a307d9efbbdebef81d5bb6d716dc661403`.
All 82 shader programs initialize. Nine inspected images establish live
rendering, Pause, Resume, a later goal replay, a second Pause, Quit selection,
the Yes/No confirmation and the subsequent loading screen. The next image
request remains unanswered. Logs show automatic LOAD success before stopping.

A new title-matching core confirms a crash on the frontend: `PlayStream`
reads its state through a null track at address `0x64`, reached from
`PlayFETitleMusicWithFade` and `TitleScene::SceneCreated`. Core SHA is
`d0dc77803c5d131d2f4f60d5a4d0f5eea5fbb84a3e074f8fd80d06762f081ca2`;
the [root review](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate7-watchdog-1/root-crash-review.json)
requires its matching complete SELF and ELF to remain retained. The direct
match startup bypasses initial FE audio creation; its first return to FE does
not recreate the tracks destroyed by match teardown. The correction now passes
its host before/after regression: 32 configurations, 64 visits and 2,520 checks
under ASan/UBSan. The [freeze manifest](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-fe-audio-lifecycle-gate/freeze.json)
binds the real extracted functions and their simulated service boundaries.
A new hardware round trip is still required. This does not resolve the older
graphics-allocation crash.

| Candidate7 device contract | Observed outcome | Remaining acceptance |
| --- | --- | --- |
| Program initialization and first match rendering | 82 programs initialized; pitch, players, HUD, shadows and effects visible | Other shader/content combinations and pixel fidelity |
| Pause and Resume | Observed return to play and a later goal replay | Repeated cycles and other presentation states |
| Quit and return to front end | Failed with the preserved audio-track crash | Corrected build must reach title/menu and start another match |
| Save integrity after this attempt | Existing CardA files byte-identical on two reads; CardB empty | Save creation, overwrite, progression and error UI |
| Watchdog | Startup failed with illegal priority `0x80028023`; zero samples | Priority 191 host correction needs positive hardware heartbeats |

The finite diagnostic capture covers live frames 120–239 and finishes before the UI
requests. It contains cold geometry spikes and is not a quiet 600–899 comparison.
The inspected Pause/Resume does not close the stable-lifecycle row.

The first quiet candidate7 capture is [incomplete after 640.8 seconds](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate7-quiet-1/incomplete-review.json),
without a new core or requested CSV. Its effective INI equals the successful
candidate6 settings except for the CSV label. The ready marker is written
before runtime/game initialization, so it does not prove live-frame progress.
This attempt provides no FPS result and does not establish a cause for the
missing capture.

### Candidate6 SELF582c: two completed quiet live-match windows

The [candidate6 quiet summary](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate6-quiet-1/summary.json)
identifies SELF
`582c69e8a6be2b136b5213be01dfb06f8ce7e0d6970be3bde6d8c96eb83b092d`,
INI `15023340f1255d436c57ecd3618724f5f277e449a1f608043e086342b0ba0d1d`
and CSV `25ea8121cbd7c6f17535a7ba51d11cf44147247ecff1f4692d3c301efcec0017`.
All 300 samples are verified live-play frames 600–899. Diagnostics, overlay,
packet profiling, renderer timing and snapshots are disabled; the run uses
`fixed_dt=0`, `vita_frameskip=0` and seed `0x53545249`. Observed clocks are
444/222/222/166 MHz.

| Samples | Mean frame time | Elapsed FPS | Median | p95 | p99 | Frames meeting 60 FPS |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 300 | 38.83827 ms | 25.74780 | 37.804 ms | 48.431 ms | 49.981 ms | 0/300 |

Eight percent of frames meet the 30 FPS deadline in the first window.
The [second quiet window](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate6-quiet-2/summary.json)
measures 27.067450 FPS: mean 36.944743 ms, median 35.116 ms,
p95/p99 45.847/48.709 ms and 0/300 frames inside 16.667 ms. Its CSV SHA is
`5274207067ee826038afd356087b751a99d74d1b6177931761479962eb33dfb1`
and INI SHA is `081a9a9862a14d1f8da7a1b9602f12b1eaeb2a851b485588ec98de75b4cb549c`.
Both windows and the separate phase capture verify SELF/INI and report no
new core at retrieval. Retain both results as the baseline spread; they do
not establish a stable speedup over candidate5. Full-speed 60 FPS and
scenario-wide performance remain unmet.

### Integrated SELF EE7c: earlier completed quiet run

The [candidate2 quiet summary](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-candidate2-quiet-1/summary.json)
records **300 live-play samples**, frames600-899, for SELF
`ee7c9a84d5d97b85d3a30fe206f74dd40b0cdb59e1b8e7703fad7ea68ec5abd5`.
The run uses `fixed_dt=0`, `vita_frameskip=0`, seed `0x53545249`, diagnostics,
overlay, renderer timing, packet profiling and snapshot requests disabled,
with affine UV and vertex fog overrides off. Observed clocks were
444/222/222/166 MHz; no fresh crash dump was observed.

INI SHA-256 is
`c150239fe84f9569f1739d9daa1aa5501f28f95016887e839bfc94057d6e2607`;
CSV SHA-256 is
`f818988657b61c0db0f3d9eb8a2cefca98f478d569328140c49f625bf15e0699`.

| Samples | Mean frame time | Elapsed FPS | Median | p95 | p99 | Frames meeting 60 FPS |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 300 | 40.517 ms | 24.681 | 38.673 ms | 54.058 ms | 64.245 ms | 0/300 |

The 30 FPS deadline was met by 8.33% of frames. This is one completed run,
not a repeated comparison that isolates any individual source change. It
predates the later black/bitmap, loading-indicator and material-lookup work.
Full-speed 60 FPS remains unmet.

### Integrated SELF 9a: one completed quiet run

The [native-functional-quiet-1 summary](../../docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/native-functional-quiet-1/summary.json)
records **300 live-play samples**, after skipping 600, for SELF
`9a4375c147edf8777a395ea59734578c9acb538404a8f7537c6f0813d951393d`.
Its run identity specifies `fixed_dt=0`, `vita_frameskip=0`, diagnostics,
overlay, renderer timing and packet profiling all disabled. Observed clocks
were 444/222/222/166 MHz. INI SHA-256 is
`f793a6d5da6f57bcc1e803276d33001f4cde8edab5c006a5a1d241d1c6d6c4bd`;
CSV SHA-256 is
`4d377ee5b5db78d6433ba2af82fe122bd5b0509c2f52d119ccd5972ba8bf3cfe`.

| Samples | Mean frame time | Elapsed FPS | Median | p95 | p99 | Frames meeting 60 FPS |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 300 | 46.618 ms | 21.451 | 45.925 ms | 54.698 ms | 59.413 ms | 0/300 |

This is one completed run, not a repeated comparison establishing an improvement
over the historical control. It predates the subsequent skin-capacity and
resource-lifetime corrections. The diagnostic smoke's cumulative registration
failure is still an acceptance issue for this executable; a quiet timing
capture does not independently prove that all rendering paths are correct.

### Earlier repeated quiet baseline

The previous repeated-run summary, for the different executable identified below, is
[2026-10-18-quiet-summary.json](2026-10-18-quiet-summary.json), described in
[PERFORMANCE.md](../../PERFORMANCE.md).

Nine hardware runs: three independent runs per profile, 600 live warm-up
frames followed by 300 captured frames each. They use real-time simulation
(`fixed_dt=0`), no frame skipping, diagnostics/timing off, and effective
CPU/GPU/bus/crossbar clocks of 444/222/222/166 MHz.

Measured executable SHA-256:

`4cb243ebfdb96aa67bdea7ac88ae669d613c3025b1a1e8ceeb46eaa7add04026`

| Profile | Samples | Mean frame time | Pooled FPS | p95 | p99 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Control | 900 | 50.033 ms | 19.987 | 69.504 ms | 87.599 ms |
| Affine UV candidate | 900 | 48.789 ms | 20.496 | 63.823 ms | 78.552 ms |
| Vertex fog candidate | 900 | 47.766 ms | 20.935 | 58.279 ms | 66.609 ms |

None of the 2,700 captured frames met the 60 FPS deadline. The control needs
approximately 33.37 ms less mean frame time, or a 66.69% reduction. Run-to-run
variance overlaps the candidate changes; the evidence does not justify
promoting those flags as demonstrated gains. Pooled frame tails are descriptive
and are not independent run replications.

A successful build, newly supported draw type, or reduced allocation count is
not evidence of an FPS improvement. Measure the integrated executable again
after functional checks, and use
[PACKET_PROFILING.md](PACKET_PROFILING.md) for cost attribution separately from
quiet timing.

## Device workflow and current access

[tools/vita_workflow_device.py](../tools/vita_workflow_device.py) implements
bounded backup/install/run/fetch/restore operations for title `SMSVITA01`.
The latest explicit Strikers target found in current local scripts remains
`192.168.1.79`, FTP port 1337 and command/input port 1338. The first read-only
connection attempt returned `OSError 65: No route to host`, but a later check in
the same session succeeded in 0.238 seconds with `220 FTPVita Server ready`.
Access then resumed. SELF EE7c completed the clean diagnostic smoke,
quiet run and the ordinary-frontend observations described above. SELF447fe's
UI3-2 run confirms current-save autoload, followed later by the renderer data
abort documented above. Candidate4 SELF6a78 was installed with verified
readback; UI2 confirms autoload and Main, with decoded intro pictures and a
proven color defect. FTP allocation errors/timeouts interrupt manual LOAD
before confirmation; subsequent evidence recovery completes without a new core.
UI3 visibly pauses/resumes and advances the match, then loses progress later;
its final dump check also finds only the known 447fe core. This stop is not a
diagnosed candidate4 crash. Candidate5 SELF4f951 is built/installed with the
movie-coordinate correction and separate video phase attribution. Sustained
lifecycle, corrected video and remaining scenarios still require device results.
These changes are not part of the EE7c device timings.
The complete remote archive hash attempt timed out after 600 seconds;
remote size compatibility does not establish complete archive identity.
New device evidence belongs in
`docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18` at the repository
root. The old 9a cumulative `skin_selectors` rejection remains in its historical
record; EE7c has zero invalid packets in its smoke. Its successful smoke does
not establish whole-game acceptance, complete movie/pause behavior or 60 FPS.

Read-only collection of an existing run:

```sh
python3 smstrikers-port/tools/vita_workflow_device.py fetch \
  --host 192.168.1.79 --out .tmp/existing-run-evidence --label EXISTING_LABEL
```

This retrieves existing CSV/log/packet/view files, the INI and installed SELF
identity. This generic fetch command does not retrieve `vitagl_match_status.csv`;
it requires an additional read. The newer bounded `safe-script-native-device-rgba-20261018.py`
poll recipe above already collects it for diagnostic/profile runs. Its cumulative invalid/draw/texture/character
counters and first invalid-packet diagnostics can locate missing paths, but
they do not supply a whole-game coverage denominator.

For a future real-time performance run, override the helper's default fixed
step explicitly with `--set fixed_dt=0`; the default helper value is a
16.666666667 ms fixed step and must not be mistaken for real-time simulation.
Keep visual capture and detailed profiling separate from the quiet comparison.
Verify installed binary and asset identities before assigning new results.

The local persistence tests use isolated temporary cards. The coverage audit
read the preserved pre-UI backup without modifying it or the original saves;
it issued no controller input. Device results for the integrated executable
must be recorded in the evidence directory above as they complete.
