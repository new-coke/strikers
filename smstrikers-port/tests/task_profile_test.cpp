#include "port/task_profile.hpp"
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <new>
#include <unistd.h>

using P = port::TaskProfile;
namespace
{
uint64_t now = 1000;
unsigned clocks = 0;
size_t allocations = 0;
bool denyAllocations = false, failOutputWrite = false, failPhaseWrite = false;
PortBenchCaptureStatus runtimeStatus{};
PortBenchCaptureStatus status(size_t samples, size_t goal = 2, size_t warmup = 0)
{
    return { samples == goal ? PORT_CAPTURE_COMPLETE : PORT_CAPTURE_COLLECTING,
             samples, goal, warmup, 0, samples ? 600 + samples - 1 : 0 };
}
void task(P& profile, const void* key, const char* name, P::Phase phase, uint64_t ns)
{
    now += ns;
    profile.record(key, name, phase, ns);
}

void fixed_task(P& profile, const void* key, const char* name, unsigned steps, uint64_t ns)
{
    const auto start = now;
    const float before = .004f, tick = .02f, scaled = float(steps) * tick + .002f;
    float debt = before + scaled;
    {
        P::PhaseScope scope(profile, key, P::FixedControl);
        now += 10;
        for (unsigned step = 0; step < steps; ++step)
        {
            profile.next_phase(P::FixedInput);
            now += 10;
            for (unsigned phase = P::FixedAi; phase <= P::FixedSnapshot; ++phase)
            {
                profile.next_phase(P::Detail(phase));
                now += 10;
            }
            profile.next_phase(P::FixedControl);
            now += 10;
            debt -= tick;
        }
        profile.simulation_steps(steps, scaled, before, debt, tick, false);
        profile.next_phase(P::FixedInput);
        assert(now <= start + ns);
        now = start + ns;
    }
    profile.record(key, name, P::Run, ns);
}

void render_task(P& profile, const void* key, const char* name, uint64_t ns, bool early = false)
{
    const auto start = now;
    {
        P::PhaseScope scope(profile, key, P::RenderLighting);
        now += ns / 2;
        if (!early) profile.next_phase(P::RenderWorld);
        now = start + ns;
    }
    profile.record(key, name, P::Run, ns);
}
}

void* operator new(std::size_t bytes)
{
    ++allocations;
    if (denyAllocations) throw std::bad_alloc{};
    if (auto* p = std::malloc(bytes ? bytes : 1)) return p;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }

// Enforce the actual Vita integer formatter, and inject a real exporter error
// after fopen so fclose/remove/retry are exercised as well as open failure.
extern "C" int fprintf(FILE* file, const char* format, ...)
{
    for (const char* p = format; *p; ++p)
    {
        if (*p != '%') continue;
        ++p;
        if (*p == '%') continue;
        while (*p && !std::strchr("diouxXfFeEgGaAcspn%", *p))
        {
            assert(*p != 'z' && *p != 'j' && *p != 't');
            ++p;
        }
        assert(*p);
    }
    if (failOutputWrite && file != stderr) { errno = EIO; return -1; }
    if (failPhaseWrite && std::strstr(format, "# strikers-task-phases-v1"))
    {
        errno = EIO;
        return -1;
    }
    va_list args;
    va_start(args, format);
    const int result = std::vfprintf(file, format, args);
    va_end(args);
    return result;
}
extern "C" unsigned long long port_monotonic_ns() { ++clocks; return now; }
extern "C" void PortBenchGetCaptureStatus(PortBenchCaptureStatus* out) { *out = runtimeStatus; }

int main(int argc, char** argv)
{
    namespace fs = std::filesystem;
    const auto directory = argc == 2 ? fs::path(argv[1]) : fs::temp_directory_path() /
        ("strikers-task-profile-" + std::to_string(getpid()));
    fs::create_directories(directory);
    const auto base = (directory / "run.csv").string();
    const auto output = base + ".tasks.csv";
    const auto phaseOutput = base + ".phases.csv";
    int keys[P::MaxTasks + 1]{};
    P profile;
    const auto initialAllocations = allocations;
    denyAllocations = true;
    profile.begin_frame(status(0));
    {
        P::PhaseScope off(profile, &keys[0], P::FixedControl);
        profile.next_phase(P::FixedInput);
        profile.simulation_steps(0, 0, 0, 0, .02f, false);
    }
    profile.record(&keys[0], "off", P::Run, 5);
    profile.end_tasks();
    profile.end_frame(status(0), 1);
    assert(!profile.write());
    denyAllocations = false;
    assert(!clocks && allocations == initialAllocations && !profile.enabled());
    unsetenv("STRIKERS_VITAGL_PACKET_PROFILE");
    assert(!port::configure_task_profile() && !port::task_profile().enabled());
    setenv("STRIKERS_VITAGL_PACKET_PROFILE", "0", 1);
    assert(!port::configure_task_profile() && !clocks);
    assert(!profile.configure(nullptr, 2) && !profile.configure(base.c_str(), 0));
    assert(!profile.configure(base.c_str(), P::MaxFrames + 1));
    assert(profile.configure(base.c_str(), 2));
    const auto reservedAllocations = allocations;
    denyAllocations = true;
    profile.begin_frame(status(0, 2, 1));
    {
        P::PhaseScope warmup(profile, &keys[0], P::RenderLighting);
        profile.next_phase(P::RenderWorld);
    }
    profile.record(&keys[0], "warmup", P::Run, 5);
    profile.end_tasks();
    profile.end_frame(status(0), 10);
    assert(!clocks && !profile.recording());
    profile.begin_frame(status(0));
    render_task(profile, &keys[2], "intro-only", 900);
    profile.end_tasks();
    profile.end_frame(status(0), 19);
    assert(profile.samples().empty() && !profile.task_count());

    char ownedName[] = "Game,\"main\"\n#phase";
    profile.begin_frame(status(0));
    task(profile, &keys[0], ownedName, P::Transition, 1000);
    const auto firstPhaseClocks = clocks;
    fixed_task(profile, &keys[0], ownedName, 2, 5000);
    assert(clocks == firstPhaseClocks + 23); // Start, 21 shared boundaries, close.
    render_task(profile, &keys[1], "HUD", 2000);
    assert(clocks == firstPhaseClocks + 26);
    now += 100;
    profile.end_tasks();
    profile.end_frame(status(1), 20);
    ownedName[0] = 'X';
    assert(profile.samples().size() == 1 && profile.task_count() == 2);
    assert(!std::strcmp(profile.tasks()[0].name.data(), "Game,\"main\"\n#phase"));
    const auto& first = profile.samples()[0];
    assert(first.sample == 0 && first.liveFrame == 600 && first.engineFrame == 20);
    assert(first.managerNs == 8100 && first.tasks[0].runNs == 5000 && first.tasks[0].transitionNs == 1000);
    assert(first.tasks[0].runCalls == 1 && first.tasks[0].transitionCalls == 1);
    assert(first.simulation.present && first.simulation.steps == 2 && first.simulation.parentTask == 0);
    assert(first.details[P::FixedAi].calls == 2 && first.details[P::FixedAi].elapsedNs == 20);
    assert(first.details[P::FixedInput].calls == 3 && first.details[P::RenderWorld].parentTask == 1);
    assert(::access(output.c_str(), F_OK) != 0);
    assert(::access(phaseOutput.c_str(), F_OK) != 0);
    profile.begin_frame(status(1));
    fixed_task(profile, &keys[0], "Game,\"main\"\n#phase", 16, 4000);
    task(profile, &keys[1], "HUD", P::Transition, 500);
    const auto earlyPhaseClocks = clocks;
    render_task(profile, &keys[1], "HUD", 1000, true);
    assert(clocks == earlyPhaseClocks + 2);
    now += 50;
    profile.end_tasks();
    profile.end_frame(status(2), 21);
    assert(profile.complete() && !profile.error_code() && profile.samples().size() == 2);
    assert(profile.samples()[1].sample == 1 && profile.samples()[1].liveFrame == 601 &&
           profile.samples()[1].engineFrame == 21 && profile.samples()[1].managerNs == 5550);
    assert(profile.samples()[1].simulation.steps == 16 && profile.samples()[1].details[P::FixedAi].calls == 16);
    assert(!profile.samples()[1].details[P::RenderWorld].calls);
    const auto finishedClocks = clocks;
    profile.begin_frame(status(2));
    {
        P::PhaseScope finished(profile, &keys[0], P::FixedControl);
        profile.next_phase(P::FixedInput);
        profile.simulation_steps(0, 0, 0, 0, .02f, false);
    }
    profile.record(&keys[2], "after-deadline", P::Run, 1);
    profile.end_tasks();
    profile.end_frame(status(2), 22);
    denyAllocations = false;
    assert(clocks == finishedClocks && allocations == reservedAllocations);
    assert(fs::exists(output) && !fs::exists(output + ".tmp"));
    assert(fs::exists(phaseOutput) && !fs::exists(phaseOutput + ".tmp"));

    P missing;
    const auto missingDirectory = directory / "missing";
    assert(missing.configure((missingDirectory / "run.csv").c_str(), 1));
    missing.begin_frame(status(0, 1));
    task(missing, &keys[0], "retry", P::Run, 1);
    missing.end_tasks();
    missing.end_frame(status(1, 1), 30);
    assert(missing.complete() && missing.error_code() && missing.samples().size() == 1);
    fs::create_directory(missingDirectory);
    assert(missing.write() && !missing.error_code() && missing.samples()[0].engineFrame == 30);
    P writeFailure;
    const auto writeBase = (directory / "write-failure.csv").string();
    assert(writeFailure.configure(writeBase.c_str(), 1));
    writeFailure.begin_frame(status(0, 1));
    task(writeFailure, &keys[0], "write retry", P::Run, 2);
    writeFailure.end_tasks();
    failOutputWrite = true;
    writeFailure.end_frame(status(1, 1), 31);
    failOutputWrite = false;
    assert(writeFailure.error_code() == EIO && writeFailure.complete());
    assert(!fs::exists(writeBase + ".tasks.csv") && !fs::exists(writeBase + ".tasks.csv.tmp"));
    assert(writeFailure.write() && !writeFailure.error_code());

    P phaseFailure;
    const auto phaseBase = (directory / "phase-failure.csv").string();
    assert(phaseFailure.configure(phaseBase.c_str(), 1));
    phaseFailure.begin_frame(status(0, 1));
    fixed_task(phaseFailure, &keys[0], "fixed retry", 0, 100);
    phaseFailure.end_tasks();
    failPhaseWrite = true;
    phaseFailure.end_frame(status(1, 1), 32);
    failPhaseWrite = false;
    assert(phaseFailure.complete() && phaseFailure.error_code() == EIO);
    assert(phaseFailure.samples()[0].simulation.present && !phaseFailure.samples()[0].simulation.steps);
    assert(phaseFailure.samples()[0].details[P::FixedInput].calls == 1);
    assert(!fs::exists(phaseBase + ".tasks.csv") && !fs::exists(phaseBase + ".tasks.csv.tmp"));
    assert(!fs::exists(phaseBase + ".phases.csv") && !fs::exists(phaseBase + ".phases.csv.tmp"));
    // A failed phase rename must also leave the task commit unpublished.
    fs::create_directory(phaseBase + ".phases.csv");
    assert(!phaseFailure.write() && phaseFailure.error_code());
    assert(!fs::exists(phaseBase + ".tasks.csv") && phaseFailure.samples().size() == 1);
    fs::remove(phaseBase + ".phases.csv");
    assert(phaseFailure.write() && !phaseFailure.error_code());
    fs::remove(phaseBase + ".tasks.csv");
    fs::create_directory(phaseBase + ".tasks.csv");
    assert(!phaseFailure.write() && phaseFailure.error_code());
    assert(fs::exists(phaseBase + ".phases.csv") && phaseFailure.samples().size() == 1);
    fs::remove(phaseBase + ".tasks.csv");
    assert(phaseFailure.write() && !phaseFailure.error_code());

    P nested;
    assert(nested.configure((directory / "nested.csv").c_str(), 1));
    nested.begin_frame(status(0, 1));
    {
        P::PhaseScope outer(nested, &keys[0], P::RenderLighting);
        const auto beforeNested = clocks;
        P::PhaseScope inner(nested, &keys[1], P::RenderWorld);
        assert(clocks == beforeNested); // An invalid nested scope takes no clock.
    }
    task(nested, &keys[0], "outer", P::Run, 10);
    nested.end_tasks();
    nested.end_frame(status(1, 1), 33);
    assert(nested.error_code() == EINVAL && nested.samples().empty());

    P orphan;
    assert(orphan.configure((directory / "orphan.csv").c_str(), 1));
    orphan.begin_frame(status(0, 1));
    {
        P::PhaseScope child(orphan, &keys[1], P::RenderLighting);
        now += 10;
    }
    orphan.record(&keys[0], "other", P::Run, 10);
    orphan.end_tasks();
    orphan.end_frame(status(1, 1), 34);
    assert(orphan.error_code() == EINVAL && orphan.samples().empty());

    P exceedsParent;
    assert(exceedsParent.configure((directory / "parent-bound.csv").c_str(), 1));
    exceedsParent.begin_frame(status(0, 1));
    {
        P::PhaseScope child(exceedsParent, &keys[0], P::RenderLighting);
        now += 20;
    }
    exceedsParent.record(&keys[0], "parent", P::Run, 10);
    exceedsParent.end_tasks();
    exceedsParent.end_frame(status(1, 1), 35);
    assert(exceedsParent.error_code() == EINVAL && exceedsParent.samples().empty());

    P missingSimulation;
    assert(missingSimulation.configure((directory / "missing-simulation.csv").c_str(), 1));
    missingSimulation.begin_frame(status(0, 1));
    {
        P::PhaseScope child(missingSimulation, &keys[0], P::FixedControl);
        now += 20;
    }
    missingSimulation.record(&keys[0], "fixed", P::Run, 20);
    missingSimulation.end_tasks();
    missingSimulation.end_frame(status(1, 1), 36);
    assert(missingSimulation.error_code() == EINVAL && missingSimulation.samples().empty());

    P tooManySteps;
    assert(tooManySteps.configure((directory / "step-bound.csv").c_str(), 1));
    tooManySteps.begin_frame(status(0, 1));
    fixed_task(tooManySteps, &keys[0], "fixed", 17, 4000);
    tooManySteps.end_tasks();
    tooManySteps.end_frame(status(1, 1), 37);
    assert(tooManySteps.error_code() == EINVAL && tooManySteps.samples().empty());

    P wrongStepCount;
    assert(wrongStepCount.configure((directory / "step-count.csv").c_str(), 1));
    wrongStepCount.begin_frame(status(0, 1));
    {
        P::PhaseScope child(wrongStepCount, &keys[0], P::FixedControl);
        now += 10;
        wrongStepCount.simulation_steps(1, .02f, 0, 0, .02f, false);
        wrongStepCount.next_phase(P::FixedInput);
        now += 10;
    }
    wrongStepCount.record(&keys[0], "fixed", P::Run, 20);
    wrongStepCount.end_tasks();
    wrongStepCount.end_frame(status(1, 1), 38);
    assert(wrongStepCount.error_code() == EINVAL && wrongStepCount.samples().empty());

    // Invalid phase staging on a nonselected intro must not poison gameplay.
    P introPhases;
    assert(introPhases.configure((directory / "intro-phases.csv").c_str(), 1));
    introPhases.begin_frame(status(0, 1));
    {
        P::PhaseScope child(introPhases, &keys[0], P::FixedControl);
        introPhases.simulation_steps(2, .04f, 0, 0, .02f, true);
    }
    introPhases.end_tasks();
    introPhases.end_frame(status(0, 1), 38);
    assert(!introPhases.error_code() && introPhases.samples().empty());
    introPhases.begin_frame(status(0, 1));
    fixed_task(introPhases, &keys[0], "selected", 0, 100);
    introPhases.end_tasks();
    introPhases.end_frame(status(1, 1), 39);
    assert(introPhases.complete() && introPhases.samples()[0].engineFrame == 39);

    P skipped;
    assert(skipped.configure((directory / "skipped.csv").c_str(), 2));
    const auto skippedClocks = clocks;
    skipped.begin_frame(status(1));
    assert(skipped.error_code() == EINVAL && skipped.samples().empty() && clocks == skippedClocks);
    P jumped;
    assert(jumped.configure((directory / "jumped.csv").c_str(), 2));
    jumped.begin_frame(status(0));
    jumped.end_tasks();
    jumped.end_frame(status(2), 40);
    assert(jumped.error_code() == EINVAL && jumped.samples().empty());
    P unclosed;
    assert(unclosed.configure((directory / "unclosed.csv").c_str(), 1));
    unclosed.begin_frame(status(0, 1));
    unclosed.end_frame(status(1, 1), 41);
    assert(unclosed.error_code() == EINVAL && unclosed.samples().empty());

    // Nonselected staging, including its overflow, cannot consume dictionary
    // entries or substitute for the next selected gameplay frame.
    P introOverflow;
    assert(introOverflow.configure((directory / "intro-overflow.csv").c_str(), 1));
    introOverflow.begin_frame(status(0, 1));
    for (auto& key : keys) task(introOverflow, &key, "intro", P::Run, 1);
    introOverflow.end_tasks();
    introOverflow.end_frame(status(0, 1), 42);
    assert(!introOverflow.error_code() && !introOverflow.task_count());
    introOverflow.begin_frame(status(0, 1));
    task(introOverflow, &keys[0], "selected", P::Run, 1);
    introOverflow.end_tasks();
    introOverflow.end_frame(status(1, 1), 43);
    assert(introOverflow.complete() && introOverflow.task_count() == 1);
    P overflow;
    assert(overflow.configure((directory / "overflow.csv").c_str(), 2));
    overflow.begin_frame(status(0));
    for (unsigned i = 0; i < P::MaxTasks; ++i) task(overflow, &keys[i], "task", P::Run, 1);
    overflow.end_tasks();
    overflow.end_frame(status(1), 44);
    assert(overflow.task_count() == P::MaxTasks);
    overflow.begin_frame(status(1));
    task(overflow, &keys[P::MaxTasks], "extra", P::Run, 1);
    overflow.end_tasks();
    overflow.end_frame(status(2), 45);
    assert(overflow.error_code() == EOVERFLOW && overflow.samples().size() == 1 && !overflow.complete());
    assert(!fs::exists((directory / "overflow.csv.tasks.csv")));
    P tooLong;
    assert(tooLong.configure((directory / "long.csv").c_str(), 1));
    char longName[P::NameBytes + 1];
    std::memset(longName, 'x', P::NameBytes);
    longName[P::NameBytes] = 0;
    tooLong.begin_frame(status(0, 1));
    task(tooLong, &keys[0], longName, P::Run, 1);
    tooLong.end_tasks();
    tooLong.end_frame(status(1, 1), 46);
    assert(tooLong.error_code() == ENAMETOOLONG && tooLong.samples().empty());
    P counterOverflow;
    assert(counterOverflow.configure((directory / "counter.csv").c_str(), 1));
    counterOverflow.begin_frame(status(0, 1));
    counterOverflow.record(&keys[0], "counter", P::Run, UINT64_MAX);
    counterOverflow.record(&keys[0], "counter", P::Run, 1);
    counterOverflow.end_tasks();
    counterOverflow.end_frame(status(1, 1), 47);
    assert(counterOverflow.error_code() == EOVERFLOW);
    P allocationFailure;
    denyAllocations = true;
    assert(!allocationFailure.configure(base.c_str(), 1));
    denyAllocations = false;
    assert(allocationFailure.error_code() == ENOMEM && !allocationFailure.enabled());

    if (argc != 2) fs::remove_all(directory);
    std::puts("Task profile: selected identities, quiet warmup/OFF, owned CSV names, reserved collection, task/clock bounds and output retry passed");
}
