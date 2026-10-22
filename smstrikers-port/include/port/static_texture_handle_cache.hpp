#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

class PlatTexture;

namespace port
{
// Frontend-owned lookup hints, never GPU IDs or texture contents. All catalogue
// mutations must invalidate before storage can be released or reused.
class StaticTextureHandleCache
{
public:
    bool enabled() const noexcept { return enabled_; }

    void set_enabled(bool enabled) noexcept
    {
        if (enabled_ != enabled)
        {
            invalidate();
            enabled_ = enabled;
        }
    }

    void invalidate() noexcept
    {
        // Generation zero is empty. A full clear prevents old entries from
        // becoming live again after the finite generation counter wraps.
        if (generation_ == UINT16_MAX)
        {
            for (auto& entry : entries_)
                entry.generation = 0;
            generation_ = 1;
        }
        else
            ++generation_;
    }

    PlatTexture* find(std::uintptr_t handle) const noexcept
    {
        if (!enabled_)
            return nullptr;
        const auto& entry = entries_[slot(handle)];
        return entry.generation == generation_ && entry.handle == handle
                   ? entry.texture : nullptr;
    }

    void remember(std::uintptr_t handle, PlatTexture* texture) noexcept
    {
        if (enabled_ && texture)
            entries_[slot(handle)] = { handle, texture, generation_ };
    }

private:
    struct Entry
    {
        std::uintptr_t handle = 0;
        PlatTexture* texture = nullptr;
        std::uint16_t generation = 0;
    };

    static constexpr std::size_t slot(std::uintptr_t handle) noexcept
    {
        return (handle ^ (handle >> 16)) & 255u;
    }

    std::array<Entry, 256> entries_{};
    std::uint16_t generation_ = 1;
    bool enabled_ = false;
};
} // namespace port
