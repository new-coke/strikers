# Vita optimisations

Gameplay went from 19.9 to 59.8-59.9 fps at 60 Hz vsync. A pad sample reaches the screen in about 41 ms. Intros and replays run at an even 30 fps.

## GPU

- Render at 72% of 960x544 and filter up at present. Fragment cost scales with pixel count, and 72% was the largest scale that held 60 fps.
- After a late gameplay frame, render at 64% for two seconds (`vitagl_dynamic_scale`). Late frames come in clusters, so a short drop absorbs them.
- Compile GX's per-pixel fog range adjustment out of the shaders (about 8 ms a frame). Use vertex fog and affine UVs where the result is exact (about 1.7 ms).
- Use a depth-only main target and never load or store the display's depth. No draw uses stencil, and the display only receives the upscale blit and 2D screens.
- Give the shadow atlas copies their own shader programs. The generic program made the atlas pass cost 3.3 ms; it now costs 0.45 ms.
- Upload opaque BC1 textures compressed instead of decoding them to RGBA8. They take an eighth of the memory and bandwidth.
- Stop the light loops at the packet's light count instead of running every slot.
- Turn depth of field off. Its blur was a full-frame copy and pass.

## CPU and frame pipeline

The game thread spent about 21 ms a frame, half of it converting GX packets.

- Convert and present each gameplay frame on core 3 while the next one simulates (async send). Some packets reuse the previous frame's allocations, so frame memory needs a third buffer.
- Skip resident packets whose bounds lie outside the view. About 50 drop out each frame.
- Copy only the bone palette rows a draw uses. The full palette was two thirds of every draw command.
- Cache texture handle lookups (2.25 -> 0.73 ms a frame) and read per-frame constants once per frame instead of per packet.
- Decode the CPU-skinned cinematic meshes through a lean path. Host tests check its output byte-for-byte against the old decoder.
- Use the VFP square root instead of an emulated PowerPC estimate. `-fno-math-errno` lets `sqrtf` compile to the instruction, and LTO is on.
- Create textures and models on the GPU owner thread through the command queue, so the game thread does not wait for the owner's backlog.

## Stutter

- Read files on the disc reader thread. A goal starts the crowd, jumbotron and cutscene loads, and reading them on the game thread caused the judder between the goal and the celebration.
- Skip reloading the crowd atlas when it is already loaded, and do not block on the jumbotron load. Each cost about 100 ms when a goal was scored.
- Raise the audio output thread above the other CPU0 threads. Busy frames were delaying audio blocks past the 10.7 ms one still playing.

## Cutscenes

Intros reach about 36 fps unlocked because the wide stadium shots exceed the frame budget on both CPU and GPU. An even 30 fps looks smoother than an uneven 36 fps.

- Present intros and replays every second vblank (`vita_cinematic_30`). Celebrations stay uncapped because they are close-ups that run near 60.
- Use the GPU-skinned gameplay model in replays. The cinematic model is skinned on the CPU every frame; presentations keep it for facial animation.

## Stability

- Link `pthread_cancel`. Without it libstdc++ treats threads as inactive, so `std::mutex` does not lock and `std::thread` throws.
- Wait for condition variables in the kernel. VitaSDK's pthreads poll every 500 us and delayed every signalled thread.
- Reopen and retry disc reads after standby. Standby can invalidate file descriptors.
- Pad the text segment so vita-elf-create does not overrun its buffer.
- Write the reason for an `nlBreak` abort to `ux0:data/strikersVita/nlbreak.log`. Logging is compiled out, so nothing else records it.
