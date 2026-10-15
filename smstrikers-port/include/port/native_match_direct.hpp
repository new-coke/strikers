#pragma once

// Strict, opt-in native model bridge for the *live* Strikers render list.
// Unsupported packets are counted/dropped and NEVER fall back to GX.
#if defined(PORT_VITA) && defined(STRIKERS_VITA_NATIVE_MATCH_DIRECT)
#include "NL/gl/glUserData.h"
#include "NL/gl/glView.h"
#include <vector>
#include <array>
namespace port::directmatch {
void note_ball_packets(const glModel* model) noexcept;
void enable(bool yes) noexcept;
bool enabled() noexcept;
bool uses_own_renderer() noexcept;
bool preload() noexcept;
bool begin_frame() noexcept;
bool end_frame() noexcept;
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
void discard_present() noexcept;
#endif
bool readback_presented_rgba8(std::vector<uint8_t>& pixels) noexcept;
void present_native() noexcept;
void on_packet(eGLView view, unsigned long flags, const glModelPacket* packet) noexcept;
// Game-managed permanent GLG resources. Bind once on creation, invalidate
// before the resource arena is rewound; no per-draw GX display-list decoding.
void on_display_list_created(const glModelPacket* packet,
                             uintptr_t displayList, uint32_t markerLevel) noexcept;
void on_stitch_selectors(const glModelPacket* packet,
                         const uint8_t* selectors, uint32_t count) noexcept;
void on_resource_release(uint32_t markerLevel) noexcept;
void shutdown() noexcept;
void finish() noexcept;
void begin_view(eGLView view) noexcept;
void capture_texture(uintptr_t handle,unsigned x,unsigned y,unsigned width,unsigned height,bool intensity=false) noexcept;
void capture_shadow_atlas() noexcept;
void clear_scene(std::array<float,4> color) noexcept;
void draw_message_bitmap(unsigned width,unsigned height,const uint8_t* bits,uint32_t color,bool centered) noexcept;
void clear_depth() noexcept;
void set_fog(float start,float end,bool enabled,unsigned type,std::array<float,4> color) noexcept;
void on_texture_release(uint32_t level) noexcept;
void notify_write(const void* ptr,size_t bytes) noexcept;
}
#endif
