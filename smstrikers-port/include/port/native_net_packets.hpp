#pragma once
#include "NL/gl/glUserData.h"
#include <array>
#include <cstdint>

namespace port::directmatch::detail
{
// Producer-only provenance. Storage is bounded, cleared without a scan at
// begin_frame, and never reaches the renderer owner or survives shutdown.
class NetPacketRegistry
{
  public:
    enum class Add { Added, Duplicate, Invalid, Overflow };
    struct Match
    {
        unsigned source = 2;
        bool descriptorMismatch = false;
    };
    void begin(uint32_t frame) noexcept
    {
        currentFrame = frame;
        used = 0;
        active = true;
        overflowed = false;
    }
    void clear() noexcept { active = false; used = 0; overflowed = false; }
    Add add(uint32_t frame, const glModelPacket* packet, unsigned source) noexcept
    {
        if (!active || frame != currentFrame || !packet || source >= 2 ||
            packet->numStreams != 3 || !packet->streams)
            return Add::Invalid;
        if (overflowed)
            return Add::Overflow;
        for (unsigned i = 0; i < used; ++i)
            if (entries[i].packet == packet)
            {
                if (entries[i].source == source && entries[i].description.matches(*packet))
                    return Add::Duplicate;
                entries[i].source = 2; // Never reuse ambiguous provenance.
                return Add::Invalid;
            }
        if (used == entries.size())
        {
            overflowed = true; // The whole frame uses the generic path.
            return Add::Overflow;
        }
        auto& entry = entries[used++];
        entry.packet = packet;
        entry.source = source;
        entry.description.assign(*packet);
        return Add::Added;
    }
    Match find(uint32_t frame, const glModelPacket* packet) const noexcept
    {
        if (!active || overflowed || frame != currentFrame || !packet)
            return {};
        for (unsigned i = 0; i < used; ++i)
            if (entries[i].packet == packet)
            {
                if (entries[i].source >= 2 || !entries[i].description.matches(*packet))
                    return { 2, true };
                return { entries[i].source, false };
            }
        return {};
    }

  private:
    struct Description
    {
        uintptr_t indexBuffer = 0;
        glModelStream* streams = nullptr;
        uint16_t count = 0;
        uint8_t primitive = 0;
        std::array<glModelStream, 3> arrays{};
        void assign(const glModelPacket& p) noexcept
        {
            indexBuffer = p.indexBuffer;
            streams = p.streams;
            count = p.numVertices;
            primitive = p.primType;
            for (unsigned i = 0; i < arrays.size(); ++i)
                arrays[i] = p.streams[i];
        }
        bool matches(const glModelPacket& p) const noexcept
        {
            if (p.indexBuffer != indexBuffer || p.streams != streams || p.numStreams != 3 ||
                !p.streams || p.numVertices != count || p.primType != primitive)
                return false;
            for (unsigned i = 0; i < arrays.size(); ++i)
            {
                const auto& a = arrays[i];
                const auto& b = p.streams[i];
                if (a.address != b.address || a.id != b.id || a.stride != b.stride ||
                    a.beData != b.beData || a.dataSize != b.dataSize)
                    return false;
            }
            return true;
        }
    };
    struct Entry
    {
        const glModelPacket* packet = nullptr;
        unsigned source = 2;
        Description description{};
    };
    std::array<Entry, 8> entries{};
    uint32_t currentFrame = 0;
    unsigned used = 0;
    bool active = false, overflowed = false;
};
} // namespace port::directmatch::detail
