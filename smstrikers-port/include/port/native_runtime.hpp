#pragma once
namespace port::native_runtime
{
bool initialize() noexcept;
bool pump() noexcept;
void shutdown() noexcept;
}   // namespace port::native_runtime
