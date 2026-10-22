#pragma once

// Native model submission from the live Strikers render list.
// Unsupported packets are counted/dropped and NEVER fall back to GX.
#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "NL/gl/glUserData.h"
#include "NL/gl/glView.h"
#include <vector>
#include <array>
#include "port/native_texture_record.hpp"
class PlatTexture;
namespace port::loading { struct Request; }
namespace port::directmatch
{
void note_ball_packets(const glModel* model) noexcept;
void note_net_packet(const glModelPacket* packet, unsigned source) noexcept;
void enable(bool yes) noexcept;
bool enabled() noexcept;
bool uses_own_renderer() noexcept;
bool preload() noexcept;
bool begin_frame() noexcept;
bool end_frame() noexcept;
// Called after the benchmark commits its frame selection, outside frame timing.
void finish_packet_profile_frame() noexcept;
void discard_present() noexcept;
// Stage the retail VI black state; it is applied at the next successful present.
void set_black(bool black) noexcept;
// Synchronously arm/disarm the retrace-driven indicator on the GPU owner.
bool set_loading(const loading::Request& request) noexcept;
bool readback_presented_rgba8(std::vector<uint8_t>& pixels) noexcept;
void on_packet(eGLView view, unsigned long flags, const glModelPacket* packet) noexcept;
// Game-managed permanent GLG resources. Bind once on creation, invalidate
// before the resource arena is rewound; no per-draw GX display-list decoding.
void on_display_list_created(const glModelPacket* packet, uintptr_t displayList,
                             uint32_t markerLevel) noexcept;
void on_stitch_selectors(const glModelPacket* packet, const uint8_t* selectors,
                         uint32_t count) noexcept;
void on_resource_release(uint32_t markerLevel) noexcept;
void shutdown() noexcept;
void begin_view(eGLView view) noexcept;
void capture_texture(uintptr_t handle, unsigned x, unsigned y, unsigned width, unsigned height,
                     bool intensity = false, unsigned destinationWidth = 0,
                     unsigned destinationHeight = 0, bool clearSource = false) noexcept;
void capture_shadow_atlas() noexcept;
void clear_scene(std::array<float, 4> color) noexcept;
// Present a localized loading/error bitmap synchronously, also inside a frame.
void draw_message_bitmap(unsigned width, unsigned height, const uint8_t* bits, uint32_t color,
                         bool centered) noexcept;
void clear_depth() noexcept;
void set_fog(float start, float end, bool enabled, unsigned type,
             std::array<float, 4> color) noexcept;
// Separate GX range registers: base fog-off does not erase their owned table.
// On invalid enabled input, return false and discard the current frame.
bool set_fog_range(bool enabled, const std::array<float, 16>& gxViewProjection) noexcept;
void on_texture_release(uint32_t level) noexcept;
bool admit_texture(const PlatTexture* texture, const nativegxm::TextureUpload& image) noexcept;
void notify_write(const void* ptr, size_t bytes) noexcept;
}   // namespace port::directmatch
#endif
