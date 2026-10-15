#pragma once

// CPU-side identity of immutable GLG display-list resources. The renderer owns
// only GPU mesh indices; this table has no Aurora or VitaSDK dependencies.
#include "port/native_match_gpu_assets.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace port::directmatch {

inline constexpr uint32_t kNoNativeMesh = ~0u;
inline constexpr size_t kMaxNativeSourceBindings = 8192;

struct SourceLocation {
    uintptr_t displayList = 0;
    uintptr_t listData = 0;
    uintptr_t position = 0;
    uintptr_t color = 0;
    uint32_t listBytes = 0;
    uint32_t positionBytes = 0;
    uint32_t colorBytes = 0;
    uint32_t drawVertices = 0;
    uint32_t primitive = 0;
    uint16_t streamCount = 0;
    uint16_t pnIndex = 0;

    bool operator==(const SourceLocation&) const = default;
};

struct NativeSourceBinding {
    SourceLocation location{};
    SourceIdentity identity{};
    uint32_t resourceLevel = 0;
    uint32_t meshIndex = kNoNativeMesh;
    uint64_t generation = 0;
    uint64_t listRevision = 0;
    uint64_t positionRevision = 0;
    uint64_t colorRevision = 0;
    uint64_t validatedEpoch = 0;
    bool invalid = false;
};

class NativeSourceBindings {
public:
    bool register_source(const SourceLocation& location,
                         const SourceIdentity& identity,
                         uint32_t resourceLevel,
                         uint64_t listRevision,
                         uint64_t positionRevision,
                         uint64_t colorRevision,
                         uint64_t epoch,
                         const PreparedGpuAssets* assets = nullptr) {
        if (!location.displayList || !location.listData || !location.position ||
            !location.listBytes || !location.positionBytes ||
            location.drawVertices < 3)
            return false;
        auto it = bindings_.find(location.displayList);
        if (it == bindings_.end() && bindings_.size() >= kMaxNativeSourceBindings)
            return false;
        const uint32_t mesh = assets ? assets->find(identity) : kNoNativeMesh;
        // The generation changes even when a pooled address is reused.
        // Re-registration therefore never preserves an old identity or mesh.
        bindings_[location.displayList] = {
            location, identity, resourceLevel, mesh, ++generation_,
            listRevision, positionRevision, colorRevision, epoch, false
        };
        return true;
    }

    void resolve(const PreparedGpuAssets& assets) noexcept {
        for (auto& [_, entry] : bindings_)
            entry.meshIndex = entry.invalid ? kNoNativeMesh : assets.find(entry.identity);
    }

    void release_after(uint32_t resourceLevel) noexcept {
        for (auto it = bindings_.begin(); it != bindings_.end();) {
            if (it->second.resourceLevel > resourceLevel)
                it = bindings_.erase(it);
            else
                ++it;
        }
        ++generation_;
    }

    NativeSourceBinding* lookup(uintptr_t handle,
                                const SourceLocation& current) noexcept {
        auto it = bindings_.find(handle);
        if (it == bindings_.end() || it->second.invalid)
            return nullptr;
        if (it->second.location != current) {
            // Pointer reuse without a matching new registration is terminal:
            // never allow the old GPU mesh ID to reappear if fields later
            // happen to match an earlier occupant (ABA).
            it->second.invalid = true;
            it->second.meshIndex = kNoNativeMesh;
            return nullptr;
        }
        return &it->second;
    }

    void invalidate(uintptr_t handle) noexcept {
        auto it = bindings_.find(handle);
        if (it != bindings_.end()) {
            it->second.invalid = true;
            it->second.meshIndex = kNoNativeMesh;
        }
    }

    void clear() noexcept {
        bindings_.clear();
        ++generation_;
    }

    size_t size() const noexcept { return bindings_.size(); }
    uint64_t generation() const noexcept { return generation_; }

private:
    std::unordered_map<uintptr_t, NativeSourceBinding> bindings_;
    uint64_t generation_ = 0;
};

// A cheap global write epoch skips all source hashing in steady state.
// Changed source pages are checked by the caller and only then rehashed;
// content mismatches invalidate the binding permanently (no GX fallback).
// The epoch is sampled again to reject concurrent writes during validation.
template<class ReadEpoch,class RangesMatch,class RehashSource>
bool validate_source_revisions(NativeSourceBinding& entry,
                               ReadEpoch&& readEpoch,
                               RangesMatch&& rangesMatch,
                               RehashSource&& rehashSource) noexcept {
    if(entry.invalid)return false;
    const uint64_t before=readEpoch();
    if(before==entry.validatedEpoch)return true;
    if(!rangesMatch(entry) && !rehashSource(entry)) {
        entry.invalid=true;
        entry.meshIndex=kNoNativeMesh;
        return false;
    }
    if(readEpoch()!=before)return false;
    entry.validatedEpoch=before;
    return true;
}

} // namespace port::directmatch
