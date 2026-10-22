#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace port
{
// Diagnostic-only protocol: one positive decimal ID per request file. A unique
// path belongs to one launch; IDs must increase within that launch. The file is
// never deleted or rewritten by the game. Consume an ID before readback so a
// failed capture cannot introduce a fence/write on every subsequent frame.
struct SnapshotRequestIo
{
    static std::FILE* open(const char* path) noexcept { return std::fopen(path, "rb"); }
    static size_t read(void* data, size_t bytes, std::FILE* file) noexcept
    {
        return std::fread(data, 1, bytes, file);
    }
    static int error(std::FILE* file) noexcept { return std::ferror(file); }
    static int close(std::FILE* file) noexcept { return std::fclose(file); }
};

template <class Io = SnapshotRequestIo> class BasicSnapshotRequest
{
  public:
    static constexpr uint32_t MaxId = 2147483647u;
    static constexpr size_t PathBytes = 256, MaxRequestBytes = 32;
    // Reserve '.' + ten ID digits + '.ppm' + NUL in the same bounded buffer.
    static constexpr size_t MaxPathLength = PathBytes - 16;
    static constexpr uint64_t PollIntervalNs = 500000000ull;
    using OutputPath = std::array<char, PathBytes>;

    bool configure(const char* path) noexcept
    {
        requestPath = {};
        lastId = 0;
        polled = false;
        lastPoll = 0;
        if (!path || !*path || !std::strcmp(path, "0")) return false;
        size_t length = 0;
        while (length <= MaxPathLength && path[length]) ++length;
        if (length > MaxPathLength) return false;
        std::memcpy(requestPath.data(), path, length + 1);
        return true;
    }

    bool enabled() const noexcept { return requestPath[0] != '\0'; }
    uint32_t last_id() const noexcept { return lastId; }

    static bool parse_id(const char* text, size_t size, uint32_t& id) noexcept
    {
        if (!text || !size || size > MaxRequestBytes) return false;
        size_t offset = 0;
        while (offset < size && space(text[offset])) ++offset;
        if (offset == size || text[offset] < '0' || text[offset] > '9') return false;
        uint32_t value = 0;
        while (offset < size && text[offset] >= '0' && text[offset] <= '9')
        {
            const auto digit = uint32_t(text[offset++] - '0');
            if (value > (MaxId - digit) / 10u) return false;
            value = value * 10u + digit;
        }
        while (offset < size && space(text[offset])) ++offset;
        if (offset != size || !value) return false;
        id = value;
        return true;
    }

    template <class Clock> uint32_t poll(Clock&& clock, OutputPath& output) noexcept
    {
        // Keeping the clock inside this guard makes the OFF path inert.
        if (!enabled()) return 0;
        const auto now = uint64_t(clock());
        if (polled && (now < lastPoll || now - lastPoll < PollIntervalNs)) return 0;
        polled = true;
        lastPoll = now;
        auto* file = Io::open(requestPath.data());
        if (!file) return 0;
        char text[MaxRequestBytes + 1];
        const auto size = Io::read(text, sizeof(text), file);
        const bool readFailed = Io::error(file) != 0;
        const bool closeFailed = Io::close(file) != 0;
        uint32_t id = 0;
        if (readFailed || closeFailed || !parse_id(text, size, id) || id <= lastId) return 0;
        const int length = std::snprintf(output.data(), output.size(), "%s.%u.ppm",
                                         requestPath.data(), unsigned(id));
        if (length < 0 || size_t(length) >= output.size()) return 0;
        lastId = id;
        return id;
    }

  private:
    static bool space(char c) noexcept
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
    }
    OutputPath requestPath{};
    uint32_t lastId = 0;
    uint64_t lastPoll = 0;
    bool polled = false;
};

using SnapshotRequest = BasicSnapshotRequest<>;
}   // namespace port
