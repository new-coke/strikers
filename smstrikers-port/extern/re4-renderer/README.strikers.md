# RE4 command-buffer source used by Strikers

Repository: https://github.com/Rinnegatamante/re4
Pinned commit: 24c03d65b2a5896ef6d56dab1d8e9110ae19e0ae
Original file: platform/renderer/gx_command_buffer.h
License: CC0-1.0, included as LICENSE.

The command-buffer header is copied unchanged. The Strikers frontend in src/platform/deferred_renderer.cpp adapts the bounded ring, value-command ownership, synchronized pointer/result calls and sixteen-draw publication policy from platform/vita/gx_backend.h and gx_backend.inc. Commands contain Strikers typed rendering data; the GPU owner uses VitaGL exclusively. No GX frontend, raw GXM renderer, glFinish or glFlush call is imported. The renderer worker uses pthread_create so VitaSDK TLS is initialized.

The CPU affinity policy follows platform/vita/gx_backend.inc: frontend CPU2, VitaGL owner CPU1, helper geometry CPU0. The native audio transport adapts the aligned alternating output blocks and checked AudioOut errors in platform/vita/audio.c. The existing Strikers MusyX mixer is retained and enabled for the Aurora-free native runtime.
