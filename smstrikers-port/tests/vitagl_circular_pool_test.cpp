#include "utils/circular_pool.h"
#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <cstdio>

int main() {
    alignas(16) std::array<uint8_t, 4096> pool{};
    uint8_t* cursor = pool.data();
    const auto end = pool.data() + pool.size();
    auto indices = vgl_pool_allocate(&cursor, end, 6); // An odd triangle count.
    assert(indices);
    std::memset(indices, 0x5a, 6);
    auto quad = vgl_pool_allocate(&cursor, end, 64);
    assert(quad && (uintptr_t(quad) & 15) == 0 && quad >= indices + 6);
    auto* floats = reinterpret_cast<float*>(quad);
    for (unsigned i = 0; i < 16; ++i) floats[i] = float(i);
    for (unsigned i = 0; i < 6; ++i) assert(indices[i] == 0x5a);
    for (unsigned size = 1; size <= 64; ++size) {
        const auto before = cursor;
        auto block = vgl_pool_allocate(&cursor, end, size);
        assert(block && block >= before && (uintptr_t(block) & 15) == 0);
        assert(block + size <= end && cursor >= block + size && cursor <= end);
        std::memset(block, int(size), size);
    }
    const auto before = cursor;
    assert(!vgl_pool_allocate(&cursor, end, UINT32_MAX) && cursor == before);
    cursor = end - 16;
    assert(vgl_pool_allocate(&cursor, end, 16) == end - 16 && cursor == end);
    assert(!vgl_pool_allocate(&cursor, end, 1) && cursor == end);
    cursor = pool.data() + 1;
    assert(vgl_pool_allocate(&cursor, end, 3) == pool.data() + 16);
    cursor = reinterpret_cast<uint8_t*>(UINTPTR_MAX - 7);
    assert(!vgl_pool_allocate(&cursor, reinterpret_cast<uint8_t*>(UINTPTR_MAX), 1));
    std::puts("VitaGL circular pool: float/GPU alignment after odd indices, non-overlap, bounds and overflow passed");
}
