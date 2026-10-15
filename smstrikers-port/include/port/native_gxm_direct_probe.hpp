#pragma once

namespace port {
// Deliberately separate from the game/Aurora GX initialization sequence.
// Available only with STRIKERS_VITA_GXM_DIRECT_PROBE=ON and INI opt-in.
// 1 = original one-mesh fixture; 2 = offline-packed real static GLG meshes.
int run_native_gxm_direct_probe(unsigned mode = 1) noexcept;
}
