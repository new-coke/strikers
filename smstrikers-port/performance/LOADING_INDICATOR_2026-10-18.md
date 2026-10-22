# Native loading indicator: renderer and owner-thread contract

## Result and validation boundary

The native renderer now implements the live retail loading indicator on the
existing VitaGL owner thread, independently of game-frame production. It uses
the original icon data, shared YUV422 chroma writes, retrace delay and selection
order. It never publishes an incomplete main framebuffer.

The host gate compiled nine targets. Ten of the eleven selected CTests passed
on the first run; the new Cg oracle initially needed host-only syntax fixes
(`floor` import and Clang vector construction). Its identical test command then
passed independently: **1,579,652 channel comparisons**, including **237,024
unchanged pixels preserved exactly**. The maximum observed quantized RGB
difference against the rational/double reference was **one byte**.

The final pure-black startup-retirement condition was then checked by rebuilding
and running only `vitagl_renderer_driver_test`; it passed. This retained the
existing rule that blanking alone does not release the startup resource.

Full command lines, return codes, timings and the initial adapter failure are
retained in:

- `docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/spinner-host-gate.log`
- `docs/report_performance/evidence/NATIVE_COMPLETION_2026-10-18/spinner-host-gate.json`

These are host ownership, state, scheduling and algebra checks. The new Cg
program still requires compilation and visual validation on Vita. No hardware
performance improvement or completed 100% functional coverage is inferred from
this gate.

## Presentation and scheduling

`DeferredRenderer::set_loading` is synchronous: previous queued commands retire
before the owner consumes the borrowed uploads. The renderer waits for the
display queue before capturing `GL_FRONT` into a private background texture.
It captures the completed display even when `begin_frame` has already cleared
main or an unrelated offscreen target is open.

The callback clock in `loading_indicator_state.hpp` advances from unsigned
`sceDisplayGetVcount` differences. It folds missed retraces in constant time,
including counter wrap, without replaying missed GL work. The worker uses an
interruptible `CLOCK_MONOTONIC` condition deadline while loading is active. New
commands and stop wake the same condition. Darwin host tests use its relative
monotonic wait adapter. When loading is off, the original indefinite wait
remains: no extra clock reads, retrace queries or timed wakeups occur.

The original rules are retained:

| Event | Native behavior |
| --- | --- |
| First `glxSwapLoading`, including OFF | Consume the one-time two-second retrace delay. |
| Eligible retrace, no first-frame countdown | Draw the currently selected icon, then advance phase. Native 60 Hz uses 19 draws per selection. |
| Eligible retrace, countdown still positive | Decrement without presenting; the transition to zero clears the private backdrop and presents black. Icons begin on the next eligible retrace. |
| Late wake spanning black and icon phases | Clear the private backdrop and present only the latest due icon phase. |
| Ordinary frame ends while loading is active | Close the frame without promoting main or consuming the loading counter. |
| Save/restore with loading previously enabled | Rearm, reset icon phase/delay as requested by the frontend, and synchronously publish a black backdrop without consuming a retrace or modifying the staged black flag. |
| Bitmap/error message during loading | Complete the bitmap display, then replace the private backdrop with that completed image. It clears staged black and retains the first-frame countdown. |
| OFF | Disarm and release transient resources, without another presentation. |

Display transactions restore the active frame, semantic target, actual distinct
read/draw framebuffer bindings and frame timing. Fixed state, viewport,
scissor and vertex-attribute caches are invalidated; program/texture caches
continue to describe actual GL bindings. The immutable CPU command ring is
independent of the VitaGL scratch pools rotated by each swap.

All timer, draw, wait and completion failures disarm on the GL owner and release
the private loading resources. The original error remains latched. OFF cleanup
still executes after a previous deferred command failure. No tick survives a
completed shutdown. Pure black output records a completed display but retains
the startup resource until an icon, bitmap or ordinary scene replaces it.

## Icon pixels and native RGB backdrop

The frontend helper `port/loading_indicator.hpp` converts the two original
32-by-32 YUVA4444 images to operation textures. Alpha is a write mask, not a
blend weight. Each texel stores:

| Channel | Meaning |
| --- | --- |
| R | Expanded source Y byte. |
| G/B | Original three-tap, integer-truncated shared U/V bytes. |
| A | `85 * (writeY + 2 * writeUV)`, with `writeUV` taken from the odd source pixel. |

An odd visible pixel can therefore update the chroma of its transparent even
neighbour. An even visible pixel whose odd neighbour is transparent keeps the
backdrop chroma. The original assets contain fourteen mixed-mask pairs per
image; an ordinary RGBA alpha blend would not reproduce them. The separate
frontend oracle executes the original `StorePixel` order against pseudorandom
YUV422 backgrounds and checks the encoded operations byte for byte.

The dedicated loading Cg shader samples both operation and background textures
with nearest filtering. Its full-screen pass restores the same background on
each rotating display buffer before drawing the three indicators. Operation
bounds end before the next icon's 24-pixel pitch, so their padded 32-pixel
quads do not overwrite another icon's modified pixels.

Positions use the original fixed 640-wide XFB literals, independent of the
engine aspect setting: x = 288/312/336, y = 382 or 366 at native 60 Hz, with
32-pixel squares. The frontend maps these to the 960-by-544 native display.
The background copy reverses the FBO destination Y endpoints because the
provider's default-framebuffer source uses top-first texture V. A numeric
provider-coordinate oracle verifies the complete capture/overlay orientation.

The native backdrop is RGBA, so untouched pixels return its original sample
exactly. For touched pixels requiring backdrop Y or shared UV, the chosen
adaptation uses the local `OSFatal.c` limited-range BT.601 coefficients with
explicit byte rounding. Shared chroma uses the average RGB of the original
logical pixel pair. The inverse also rounds and clamps bytes explicitly.

This RGB-to-YUV adaptation is a documented native convention, not proof of the
GameCube EFB-to-XFB encoder's hardware rounding. The shader oracle executes the
actual production Cg functions and compares them with raw retail pixels plus
an independent rational/double conversion. It allows bounded float32
half-byte rounding differences; untouched pixels must be bit-identical.

## Resources and regression coverage

Loading allocates one private full-display RGBA color target, the provider's
independent hidden depth surface, two operation textures, four vertex buffers
and one shared index buffer. Color plus depth is approximately 4 MiB. A pair of
`vglMemFree(VGL_MEM_ALL)` readings logs the actual pool delta once per successful
arm; hardware measurement is pending. The dedicated shader is lazily compiled
and retained until renderer shutdown. OFF releases all other loading resources.
Ticks create no application GL objects or buffer uploads; gameplay uniforms,
bone palettes and material shaders are unchanged.

The production renderer driver verifies private capture, completed-front
ordering, state/cache restoration, no main/target writes, 120-retrace delay,
19-draw selection phases, black countdown, synchronous black restore, bitmap
replacement, skipped ordinary presents, resource lifetime and partial failures.
An independent retrace-by-retrace transcription checks the constant-time clock.

The deferred driver verifies synchronous borrowed uploads, progress while the
frontend is idle, queue fairness under sustained draws, cleanup after prior
failure, restart and shutdown. Host-only symbol substitution injects
`clock_gettime` and condition-wait failures and counts queries to prove the OFF
path has no timed work. It adds no hook or dispatch cost to the Vita binary.

The same gate also passed the native runtime frontend, boot screen, loading
operation helper, task profile writer/parser, game-phase hooks, simulation
clock and media record tests. CARD/save-flow changes are validated separately
by their owner.
