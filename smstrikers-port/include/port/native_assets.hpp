#pragma once
#include <cstdint>
#include <vector>
namespace port {
void initialize_native_assets() noexcept;
void shutdown_native_assets() noexcept;
bool read_native_asset(const char* path,std::vector<uint8_t>& bytes,void*) noexcept;
}
