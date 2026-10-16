#include "port/native_graphics.hpp"
#include <array>

namespace port::assets
{
bool validate_skin(std::span<const uint8_t> b) noexcept
{
    if (!magic(b, "NSSK0001"))
        return false;
    const auto* p = b.data() + 16;
    const auto* end = b.data() + b.size();
    uint32_t count = 0;
    while (p < end)
    {
        Section s;
        if (!section(p, end, s))
            return false;
        p = s.next;
        ++count;
        const auto data = s.payload;
        const auto* d = data.data();
        const size_t n = data.size();
        switch (s.type)
        {
        case BoneMatrices:
            if (n % 68)
                return false;
            break;
        case BoneMap:
            if (n % 8)
                return false;
            break;
        case SoftwareVertices:
            if (n % 32 || n / 32 > 4096)
                return false;
            break;
        case Pairs:
            if (n % 4)
                return false;
            break;
        case Stitching:
            if (n < 8 || !word(d) || word(d + 4) >= word(d))
                return false;
            break;
        case Morphs:
        {
            if (n < 12)
                return false;
            const size_t morphs = word(d), vertices = word(d + 4);
            if (morphs > 8 || vertices > 4096 || morphs * 8 + 12 > n)
                return false;
            const size_t at = 8 + morphs * 8, deltas = word(d + at);
            if (deltas > (n - at - 4) / 16)
                return false;
            uint64_t referenced = 0;
            for (size_t i = 0; i < morphs; ++i)
                referenced += word(d + 8 + morphs * 4 + i * 4);
            if (referenced > deltas)
                return false;
            for (size_t i = 0; i < deltas; ++i)
                if (word(d + at + 4 + i * 16 + 12) >= vertices)
                    return false;
            break;
        }
        default: return false;
        }
    }
    return p == end && count == word(b.data() + 12);
}
bool validate_model(std::span<const uint8_t> b) noexcept
{
    if (b.size() > 32u * 1024u * 1024u || !magic(b, "NSMD0001") || !word(b.data() + 12))
        return false;
    const auto* p = b.data() + 16;
    const auto* end = b.data() + b.size();
    uint32_t groups = 0;
    while (p < end)
    {
        Section g;
        if (!section(p, end, g) || g.type != Group)
            return false;
        p = g.next;
        ++groups;
        const auto* cursor = g.payload.data();
        const auto* groupEnd = cursor + g.payload.size();
        std::array<std::span<const uint8_t>, 12> blocks{};
        while (cursor < groupEnd)
        {
            Section s;
            if (!section(cursor, groupEnd, s) || s.type < References || s.type > Materials)
                return false;
            cursor = s.next;
            if (s.type <= Indices)
            {
                if (!blocks[s.type].empty())
                    return false;
                blocks[s.type] = s.payload;
            }
            if (s.type == Skin && !validate_skin(s.payload))
                return false;
            if (s.type == TextureAnimation)
            {
                if (s.payload.size() < 16 ||
                    word(s.payload.data() + 4) > (s.payload.size() - 16) / 8)
                    return false;
            }
            if (s.type == VertexAnimation)
            {
                if (s.payload.size() < 16)
                    return false;
                const uint64_t frames = word(s.payload.data() + 4),
                               vertices = word(s.payload.data() + 8);
                if (frames * vertices > (s.payload.size() - 16) / 12)
                    return false;
            }
            if (s.type == Materials &&
                (s.payload.size() < 8 || word(s.payload.data() + 4) > (s.payload.size() - 8) / 12))
                return false;
        }
        const auto models = blocks[Models], packets = blocks[Packets], streams = blocks[Streams];
        const auto vertices = blocks[Vertices], indices = blocks[Indices],
                   references = blocks[References];
        if (models.empty() || models.size() % 16 || packets.empty() || packets.size() % 80 ||
            streams.empty() || streams.size() % 12 || vertices.empty() || indices.empty() ||
            indices.size() % 2 || references.size() % 4)
            return false;
        for (size_t i = 0; i < models.size(); i += 16)
        {
            const size_t first = word(models.data() + i + 12), count = word(models.data() + i);
            if (first > packets.size() / 80 || count > packets.size() / 80 - first)
                return false;
        }
        for (size_t i = 0; i < streams.size(); i += 12)
        {
            const auto* s = streams.data() + i;
            const size_t at = word(s), n = word(s + 8);
            if (s[4] > 14 || !s[5] || s[6] || s[7] || !n || at > vertices.size() ||
                n > vertices.size() - at)
                return false;
        }
        for (size_t i = 0; i < packets.size(); i += 80)
        {
            const auto* packet = packets.data() + i;
            const size_t index = word(packet + 4), count = half(packet + 8),
                         first = word(packet + 12), num = packet[11];
            if (packet[10] > 5 || !num || first > streams.size() / 12 ||
                num > streams.size() / 12 - first || index % 2 || index > indices.size() ||
                count > (indices.size() - index) / 2 || word(packet + 36) > references.size() ||
                word(packet + 76))
                return false;
            for (size_t stream = 0; stream < num; ++stream)
            {
                const auto* s = streams.data() + (first + stream) * 12;
                const size_t stride = s[5], available = word(s + 8);
                for (size_t j = 0; j < count; ++j)
                    if (uint64_t(half(indices.data() + index + j * 2)) * stride + stride >
                        available)
                        return false;
            }
        }
    }
    return p == end && groups == word(b.data() + 12);
}
}   // namespace port::assets
