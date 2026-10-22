#include "port/snapshot_request.hpp"
#include <algorithm>
#include <cassert>
#include <string>

namespace
{
struct FakeIo
{
    static inline std::string bytes;
    static inline bool openFailure = false, readFailure = false, closeFailure = false;
    static inline unsigned opens = 0, reads = 0, closes = 0;
    static inline int token;
    static std::FILE* open(const char*)
    {
        ++opens;
        return openFailure ? nullptr : reinterpret_cast<std::FILE*>(&token);
    }
    static size_t read(void* target, size_t capacity, std::FILE*)
    {
        ++reads;
        const auto count = std::min(bytes.size(), capacity);
        std::memcpy(target, bytes.data(), count);
        return count;
    }
    static int error(std::FILE*) { return readFailure; }
    static int close(std::FILE*) { ++closes; return closeFailure ? -1 : 0; }
};
using Request = port::BasicSnapshotRequest<FakeIo>;
}

int main()
{
    for (const auto* input : { "1", "2147483647", "000001", " \t123\r\n" })
    {
        uint32_t id = 0;
        assert(Request::parse_id(input, std::strlen(input), id) && id > 0);
    }
    for (const auto* input : { "", " ", "0", "0000", "2147483648", "4294967296", "-1", "+1",
                               "1 2", "1.0", "1x", "0x10", "1\n2", "1 # note",
                               "999999999999999999999999999999999999" })
    {
        uint32_t id = 17;
        assert(!Request::parse_id(input, std::strlen(input), id) && id == 17);
    }
    uint32_t id = 9;
    assert(!Request::parse_id(nullptr, 1, id));
    const char embeddedNul[]{ '1', '\0', '2' };
    assert(!Request::parse_id(embeddedNul, sizeof(embeddedNul), id));
    const std::string bounded = std::string(31, ' ') + "1";
    assert(Request::parse_id(bounded.data(), bounded.size(), id) && id == 1);
    const std::string tooLong = bounded + " ";
    assert(!Request::parse_id(tooLong.data(), tooLong.size(), id));

    Request request;
    Request::OutputPath output{};
    unsigned clocks = 0;
    uint64_t now = 0;
    const auto clock = [&]() { ++clocks; return now; };
    for (const auto* path : { static_cast<const char*>(nullptr), "", "0" })
    {
        assert(!request.configure(path) && !request.enabled());
        assert(!request.poll(clock, output));
    }
    std::string longPath(Request::MaxPathLength + 1, 'a');
    assert(!request.configure(longPath.c_str()));
    assert(!request.poll(clock, output) && clocks == 0 && FakeIo::opens == 0);

    assert(request.configure("ux0:data/strikersVita/ui-run.request"));
    FakeIo::bytes = "1\n";
    assert(request.poll(clock, output) == 1 && request.last_id() == 1);
    assert(std::string(output.data()) == "ux0:data/strikersVita/ui-run.request.1.ppm");
    FakeIo::bytes = "2\n";
    now = Request::PollIntervalNs - 1;
    assert(!request.poll(clock, output) && FakeIo::opens == 1);
    ++now;
    assert(request.poll(clock, output) == 2 && FakeIo::opens == 2);
    now += Request::PollIntervalNs;
    assert(!request.poll(clock, output)); // Duplicate ID.
    FakeIo::bytes = "1";
    now += Request::PollIntervalNs;
    assert(!request.poll(clock, output) && request.last_id() == 2); // Stale ID.
    const auto priorOpens = FakeIo::opens;
    --now;
    assert(!request.poll(clock, output) && FakeIo::opens == priorOpens); // Reversed clock.
    now += Request::PollIntervalNs + 1;
    FakeIo::bytes = "3";
    FakeIo::openFailure = true;
    const auto priorReads = FakeIo::reads, priorCloses = FakeIo::closes;
    assert(!request.poll(clock, output) && FakeIo::reads == priorReads && FakeIo::closes == priorCloses);
    FakeIo::openFailure = false;
    assert(!request.poll(clock, output)); // Failed opens remain rate limited.
    now += Request::PollIntervalNs;
    FakeIo::readFailure = true;
    assert(!request.poll(clock, output) && request.last_id() == 2 && FakeIo::closes == priorCloses + 1);
    now += Request::PollIntervalNs;
    FakeIo::readFailure = false;
    FakeIo::closeFailure = true;
    assert(!request.poll(clock, output) && request.last_id() == 2);
    now += Request::PollIntervalNs;
    FakeIo::closeFailure = false;
    assert(request.poll(clock, output) == 3); // Failed I/O did not consume the ID.
    now += Request::PollIntervalNs;
    FakeIo::bytes = std::string(33, '1');
    assert(!request.poll(clock, output) && request.last_id() == 3);

    // The maximum ID and longest accepted base path fit including the NUL.
    std::string maxPath(Request::MaxPathLength, 'a');
    assert(request.configure(maxPath.c_str()));
    FakeIo::bytes = "2147483647\n";
    assert(request.poll(clock, output) == Request::MaxId);
    assert(std::strlen(output.data()) == output.size() - 1);
    assert(std::string(output.data()) == maxPath + ".2147483647.ppm");
    now += Request::PollIntervalNs;
    FakeIo::bytes = "2147483648\n";
    assert(!request.poll(clock, output) && request.last_id() == Request::MaxId);
    assert(FakeIo::reads == FakeIo::closes);

    std::puts("snapshot_request: parser, rate limit, OFF path, stale IDs and checked I/O passed");
}
