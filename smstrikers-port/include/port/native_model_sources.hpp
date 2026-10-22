#pragma once

// Capture the engine's original host-order indices before dlMakeDisplayList
// replaces them with a GX display list. The live draw path never parses GX.
#include "port/scene_skinning.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace port::directmatch
{
static_assert(render::kSceneSkinBoneCount <= 32);

struct ModelSource
{
    uintptr_t list = 0;
    uintptr_t data = 0;
    uint32_t marker = 0, count = 0;
    uint8_t primitive = 0, streams = 0;
    bool hasPn = false;
    uint32_t skinBoneMask = 0;
    uint64_t generation = 0;
    std::vector<uint16_t> indices{};
    std::vector<uint8_t> selectors{};
};

class ModelSourceRegistry
{
  public:
    static constexpr size_t MaxEntries = 8192;
    static constexpr size_t MaxBytes = 8u * 1024u * 1024u;

    bool capture(uintptr_t list, uintptr_t data, uint32_t marker, uint8_t primitive,
                 uint8_t streams, std::span<const uint16_t> indices, bool hasPn)
    {
        if (!list || !data || !streams || primitive > 5 ||
            indices.size() < (primitive >= 4 ? 2u : 3u) || indices.size() > 64000 ||
            (primitive == 4 && indices.size() % 2))
            return false;
        const size_t bytes = indices.size() * (sizeof(uint16_t) + size_t(hasPn));
        const auto old = sources_.find(list);
        const size_t prior = old == sources_.end() ? 0 : occupied(*old);
        if ((old == sources_.end() && sources_.size() == MaxEntries) ||
            bytes > MaxBytes - (bytes_ - prior))
            return false;
        ModelSource entry;
        entry.list = list;
        entry.data = data;
        entry.marker = marker;
        entry.count = static_cast<uint32_t>(indices.size());
        entry.primitive = primitive;
        entry.streams = streams;
        entry.hasPn = hasPn;
        entry.generation = ++generation_;
        entry.indices.assign(indices.begin(), indices.end());
        // Re-registering a reused DL always invalidates the previous selector
        // set, even when the allocator returns the same address (ABA).
        sources_.insert_or_assign(list, std::move(entry));
        bytes_ = bytes_ - prior + bytes;
        return true;
    }

    bool attach_stitch(uintptr_t list, uintptr_t data, std::span<const uint8_t> stitch)
    {
        auto it = sources_.find(list);
        if (it == sources_.end() || it->second.data != data || !it->second.hasPn ||
            stitch.size() != it->second.count)
            return false;
        std::vector<uint8_t> selectors;
        selectors.reserve(stitch.size());
        uint32_t skinBoneMask = 0;
        for (const uint8_t index : stitch)
        {
            // Original StitchModel emits (bone-map index + 1) * 3. Slot zero
            // belongs to the model transform; retail meshes use up to 13 bones.
            // Keep the native 32-bone palette contract shared with the shader.
            if (index >= render::kSceneSkinBoneCount)
                return false;
            skinBoneMask |= uint32_t{1} << index;
            selectors.push_back(static_cast<uint8_t>((index + 1u) * 3u));
        }
        if (it->second.selectors != selectors)
        {
            it->second.selectors = std::move(selectors);
            // PN slots are stored in the resident vertices. A new stitching
            // map must not reuse a VBO compiled with the previous selectors.
            it->second.generation = ++generation_;
        }
        it->second.skinBoneMask = skinBoneMask;
        return true;
    }

    const ModelSource* find(uintptr_t list, uintptr_t data, uint32_t count, uint8_t primitive,
                            uint8_t streams, bool hasPn) const noexcept
    {
        const auto it = sources_.find(list);
        if (it == sources_.end())
            return nullptr;
        const auto& e = it->second;
        if (e.data != data || e.count != count || e.primitive != primitive ||
            e.streams != streams || e.hasPn != hasPn || (hasPn && e.selectors.size() != count))
            return nullptr;
        return &e;
    }

    void release_after(uint32_t marker) noexcept
    {
        for (auto it = sources_.begin(); it != sources_.end();)
        {
            if (it->second.marker > marker)
            {
                bytes_ -= occupied(*it);
                it = sources_.erase(it);
            }
            else
                ++it;
        }
    }
    void clear() noexcept
    {
        sources_.clear();
        bytes_ = 0;
    }
    size_t size() const noexcept { return sources_.size(); }
    size_t occupied_bytes() const noexcept { return bytes_; }

  private:
    using Entry = std::pair<const uintptr_t, ModelSource>;
    static size_t occupied(const Entry& pair) noexcept
    {
        return pair.second.count * (sizeof(uint16_t) + size_t(pair.second.hasPn));
    }
    std::unordered_map<uintptr_t, ModelSource> sources_{};
    size_t bytes_ = 0;
    uint64_t generation_ = 0;
};

}   // namespace port::directmatch
