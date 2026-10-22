#include "port/packet_profile.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <unistd.h>

using P = port::render::PacketProfile;
// The host libc accepts length modifiers unsupported by the Vita formatter.
// Run the real exporter through its supported integer/string format contract.
extern "C" int fprintf(FILE* file, const char* format, ...)
{
    for (const char* p = format; *p; ++p)
    {
        if (*p != '%') continue;
        ++p;
        if (*p == '%') continue;
        while (*p && std::strchr("diouxXfFeEgGaAcspn%", *p) == nullptr)
        {
            assert(*p != 'z' && *p != 'j' && *p != 't');
            ++p;
        }
        assert(*p);
    }
    va_list args;
    va_start(args, format);
    const int result = std::vfprintf(file, format, args);
    va_end(args);
    return result;
}
static unsigned calls;
static uint64_t ns = 1000, waits;
extern "C" unsigned long long port_monotonic_ns() { ++calls; return ns; }
static uint64_t wait_us() { return waits; }
static PortBenchCaptureStatus capture(size_t samples, size_t skip = 0)
{
    return {PORT_CAPTURE_COLLECTING, samples, 2, skip, 0, 600 + samples - 1};
}
#include "frame_timeline_profile_cases.inc"
int main(int argc, char** argv)
{
    const auto dir = argc == 2 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path() / ("strikers-packet-profile-" + std::to_string(getpid()));
    std::filesystem::create_directory(dir);
    const auto base = (dir / "run.csv").string();
    P p;
    { P::Scope scope(p, 11, 3, wait_us); scope.next(P::Geometry); scope.submitted(0, 3, 3); }
    assert(!calls && !p.enabled());
    assert(!p.configure(nullptr, 2) && !p.configure(base.c_str(), 8193));
    assert(p.configure(base.c_str(), 2));
    p.begin_frame(capture(0, 1));
    { P::Scope scope(p, 11, 3, wait_us); }
    assert(!calls); // No packet clocks or scope storage during warm-up/OFF.
    p.end_frame(capture(0), 10, false, {});
    p.begin_frame(capture(0));
    {
        P::Scope scope(p, 11, 6, wait_us);
        ns += 100; scope.next(P::Arrays);
        ns += 200; scope.next(P::Skin);
        ns += 300; scope.next(P::Material);
        ns += 400; scope.next(P::Geometry);
        ns += 500; scope.next(P::Submit);
        ns += 600; scope.submitted(0, 4, 6); scope.next(P::Cleanup);
        ns += 700; waits += 1;
    }
    p.end_frame(capture(0), 11, false, {}); // Intro/replay: benchmark did not select it.
    assert(p.samples().empty());
    p.begin_frame(capture(0));
    {
        P::Scope scope(p, 11, 6, wait_us);
        ns += 100; scope.next(P::Geometry);
        ns += 500; scope.next(P::Submit);
        ns += 600; scope.submitted(0, 4, 6); scope.next(P::Cleanup);
        ns += 700; waits += 1;
    }
    { P::Scope rejected(p, 99, 3, wait_us); ns += 50; }
    p.end_frame(capture(1), 20, false, {});
    assert(p.samples().size() == 1 && p.samples()[0].liveFrame == 600);
    auto s = p.samples()[0].producer;
    assert(s.ns[P::Invalidation] == 150 && s.ns[P::Geometry] == 500 && s.ns[P::Submit] == 600 && s.ns[P::Cleanup] == 700);
    assert(s.counts[P::Accepted] == 1 && s.counts[P::Rejected] == 1 && s.counts[P::Attempts] == 2);
    assert(s.counts[P::InputReferences] == 9 && s.counts[P::DecodedVertices] == 4);
    assert(s.counts[P::VertexBytes] == 4 * sizeof(port::render::ModelVertex) && s.counts[P::IndexBytes] == 12);
    assert(s.counts[P::PacketWaitNs] == 1000);
    assert(p.views()[11].counts[P::DynamicDraws] == 1 && p.views()[34].counts[P::Rejected] == 1);
    assert(!std::filesystem::exists(base + ".packets.csv")); // No per-frame output.
    p.begin_frame(capture(1));
    { P::Scope resident(p, 11, 3, wait_us); resident.submitted(42, 0, 0); ns += 10; }
    { P::Scope built(p, 3, 3, wait_us); built.submitted(43, 3, 3); ns += 20; }
    auto complete = capture(2); complete.state = PORT_CAPTURE_COMPLETE;
    p.end_frame(complete, 21, true, {});
    assert(!p.output_error() && p.samples().size() == 2);
    assert(p.views()[11].counts[P::ResidentHits] == 1 && p.views()[3].counts[P::ResidentBuilds] == 1);
    std::ifstream file(base + ".packets.csv");
    std::string line; unsigned lines = 0;
    while (std::getline(file, line)) ++lines;
    assert(lines == 4 && std::filesystem::exists(base + ".views.csv"));
    assert(!std::filesystem::exists(base + ".uniforms.csv")); // Legacy captures stay valid.
    const auto clocks = calls;
    p.begin_frame(complete); { P::Scope scope(p, 11, 3, wait_us); }
    assert(calls == clocks && !p.recording());
    P failed;
    const auto missing = dir / "missing";
    assert(failed.configure((missing / "run.csv").c_str(), 1));
    failed.begin_frame(capture(0)); failed.end_frame(capture(1), 30, false, {});
    assert(failed.output_error() && failed.samples().size() == 1);
    std::filesystem::create_directory(missing);
    assert(failed.write() && !failed.output_error()); // Failed writes retain samples for retry.
    P skipped;
    assert(skipped.configure((dir / "skipped").c_str(), 2));
    skipped.begin_frame(capture(1)); skipped.end_frame(capture(2), 40, false, {});
    assert(skipped.output_error() && skipped.samples().empty());
    // The optional owner counters use the same selected frame identity and
    // remain buffered until the finite capture is complete.
    P uniform;
    const auto uniformBase = (dir / "uniform.csv").string();
    assert(uniform.configure(uniformBase.c_str(), 2));
    port::render::Renderer::FrameTiming timing{};
    timing.uniforms.valid = timing.uniforms.enabled = true;
    timing.uniforms.cacheBytes = 4096;
    timing.uniforms.cachePeakBytes = 8192;
    const uint64_t counts[]{3, 48, 2, 256, 2, 256, 7, 112};
    std::copy(std::begin(counts), std::end(counts), timing.uniforms.stage[0]);
    timing.uniforms.stage[1][0] = 1;
    timing.uniforms.stage[1][1] = 16;
    timing.uniforms.stage[1][2] = 1;
    timing.uniforms.stage[1][3] = 64;
    timing.uniforms.fallbackUploads = 1;
    uniform.begin_frame(capture(0));
    uniform.end_frame(capture(0), 19, false, timing); // Excluded frame.
    uniform.begin_frame(capture(0));
    uniform.end_frame(capture(1), 20, false, timing);
    assert(!std::filesystem::exists(uniformBase + ".uniforms.csv"));
    // Force the sidecar rename to fail after the main files succeeded.
    std::filesystem::create_directory(uniformBase + ".uniforms.csv");
    uniform.begin_frame(capture(1));
    uniform.end_frame(complete, 21, true, timing);
    assert(uniform.output_error() && uniform.samples().size() == 2);
    std::filesystem::remove(uniformBase + ".uniforms.csv");
    assert(uniform.write() && !uniform.output_error());
    std::ifstream uniforms(uniformBase + ".uniforms.csv");
    std::getline(uniforms, line);
    assert(line == "# strikers-uniform-profile-v1 quantities_only=1");
    std::getline(uniforms, line);
    assert(line.find("vs_pack_calls,vs_pack_bytes") != std::string::npos);
    std::getline(uniforms, line);
    assert(line == "0,600,20,0,1,3,48,2,256,2,256,7,112,1,16,1,64,0,0,0,0,1,0,4096,8192");
    std::getline(uniforms, line);
    assert(line.starts_with("1,601,21,1,1,"));
    assert(!std::getline(uniforms, line));
    P inconsistent;
    assert(inconsistent.configure((dir / "mixed.csv").c_str(), 2));
    inconsistent.begin_frame(capture(0)); inconsistent.end_frame(capture(1), 20, false, timing);
    inconsistent.begin_frame(capture(1)); inconsistent.end_frame(complete, 21, true, {});
    assert(inconsistent.output_error() && inconsistent.samples().size() == 2 && !inconsistent.write());
    assert(calls == clocks); // Neither quantities nor export take a clock.
    check_frame_timeline_profile(dir);
    if (argc != 2) std::filesystem::remove_all(dir);
    std::puts("Packet profile: exact selection, phase partition, waits, views, failures, bounded output and quiet OFF passed");
}
