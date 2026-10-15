#pragma once
#include <cstddef>
#include <cstdint>
namespace port::jobs {
using Task=bool(*)(void*,size_t,size_t,uint32_t) noexcept;
bool initialize() noexcept;
void shutdown() noexcept;
unsigned worker_threads() noexcept;
unsigned game_execution_lanes() noexcept;
bool parallel_for(size_t count,size_t grain,Task task,void* context) noexcept;
}
