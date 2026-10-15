# VitaGL provider for Strikers

This is a source snapshot of the available local VitaGL source, preserved byte for byte in SOURCE_IDENTITY.json. Upstream: https://github.com/Rinnegatamante/vitaGL. The global SDK archive has a newer optional cache-path setter; this provider uses its built-in per-title cache directory. The application compiles against this provider's header to keep its public API consistent. The source files retain their LGPL-3.0-or-later notices and copyright.

The application builds this provider in its existing shared build directory, with SKIP_SPLASHSCREEN and HAVE_SCRATCH_MEMORY. The stock SDK archive starts a separate splash renderer; the first device launch crashed in that thread before any game rendering. Strikers owns its startup screen and GPU call order.

The requested provider flags are READBACKS_SPEEDHACK=1, HAVE_SHADER_CACHE=1, NO_DEBUG=1 and DRAW_SPEEDHACK=1. As in the upstream Makefile, NO_DEBUG maps to SKIP_ERROR_HANDLING. Application-side ownership, bounds, shader compile/link and framebuffer completeness checks remain active. The application issues no glFinish/glFlush calls, including diagnostics and shutdown. The READBACKS_SPEEDHACK guard was backported from upstream commit a85cb8d0a8d0ed5b9d8f6acdebab5b37cef8a9da in source/framebuffers.c; the available local snapshot predates that flag. No other speed hacks are enabled.

Do not replace it silently with a global prebuilt SDK archive. Both source and built archive identities are part of device evidence.

Local compatibility patch: the internal `_glDrawElements_CustomShadersIMPL` and `reload_ffp_shaders` declarations uses `SceGxmIndexSource`, matching their definitions, rather than the obsolete GLboolean parameter. This permits the application/provider to share the explicit 32-bit-enum ABI.

Circular-pool allocations now align to 16 bytes and stay bounded. An odd-sized U16 index upload previously left the following framebuffer-blit float data unaligned, causing an ARM data abort. The provider alignment test covers that sequence, overflow and pool exhaustion.
