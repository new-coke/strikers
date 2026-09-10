#pragma once

#include "types.hpp"

namespace aurora::gfx {
inline constexpr bool UseTextureBuffer = true;
// smstrikers-port: sized from this game's measured peaks rather than upstream's
// defaults (uniform 24, storage 8 MiB). Measured worst case is 1.92, 1.95, 0.52,
// 8.10 and 19.45 MiB in the order below; overflow of the first four is a bare
// abort(), so each carries 2-4x margin. Storage is grown, not shrunk.
inline constexpr uint64_t UniformBufferSize = 8388608;   // 8 MiB
inline constexpr uint64_t VertexBufferSize = 5242880;    // 5 MiB
inline constexpr uint64_t IndexBufferSize = 2097152;     // 2 MiB
inline constexpr uint64_t StorageBufferSize = 16777216;  // 16 MiB
inline constexpr uint64_t TextureUploadSize = 25165824;  // 24 MiB

namespace detail {
struct Resources {
  wgpu::Buffer vertexBuffer;
  wgpu::Buffer uniformBuffer;
  wgpu::Buffer indexBuffer;
  wgpu::Buffer storageBuffer;
  wgpu::BindGroupLayout staticBindGroupLayout;
  wgpu::BindGroup staticBindGroup;
  wgpu::BindGroupLayout uniformBindGroupLayout;
  wgpu::BindGroup uniformBindGroup;
  wgpu::Limits limits;
  AuroraStats stats{};
};

Resources& resources() noexcept;
} // namespace detail
} // namespace aurora::gfx
