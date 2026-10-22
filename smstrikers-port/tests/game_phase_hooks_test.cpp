// test_game_phase_hooks.py inserts the unchanged production function bodies.
#include "port/task_profile.hpp"
#include "port/native_control_trace.hpp"
#include "port/simulation_clock.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <unistd.h>

namespace port::control_trace {
bool enabled = false;
void after_fixed_update(float) noexcept { assert(false && "control trace is disabled in this fixture"); }
}

using P = port::TaskProfile;
using u32 = uint32_t;
using s32 = int32_t;
using s8 = int8_t;
static uint64_t now = 1000;
static unsigned clocks = 0, allocations = 0;
static bool denyAllocations = false;
static std::array<unsigned, P::DetailCount> workCalls{};
static unsigned reportedSteps = 0;
static float reportedDt = 0;

void* operator new(std::size_t size)
{
    ++allocations;
    if (denyAllocations) throw std::bad_alloc{};
    if (auto* result = std::malloc(size ? size : 1)) return result;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
extern "C" unsigned long long port_monotonic_ns() { ++clocks; return now; }
extern "C" void PortBenchGetCaptureStatus(PortBenchCaptureStatus* status) { *status = {}; }
static void work(P::Detail phase) { ++workCalls[phase]; now += 10; }

struct nlTaskManager
{
    int m_CurrState = 2, m_PrevState = 2;
    static nlTaskManager* m_pInstance;
};
static nlTaskManager manager;
nlTaskManager* nlTaskManager::m_pInstance = &manager;
static float g_fFixedUpdateTick = .02f, g_fSimulationTick = .02f;
static bool g_bRunSimAndRenderInLockStep = false;
struct FixedUpdateTask
{
    static float mAccumulatedDeltaT, mSimulationTime, mfFrameLockTime, mTimeScale;
    void Run(float dt);
    static void CallFixedUpdateTasks();
    static void AIUpdateTask(float) { work(P::FixedAi); }
    static void PrePhysicsAITask(float) { work(P::FixedPrePhysics); }
    static void PostPhysicsAITask(float) { work(P::FixedPostPhysics); }
    static void DecrementFrameLock(float dt) { mfFrameLockTime -= dt; work(P::RenderLighting); }
};
float FixedUpdateTask::mAccumulatedDeltaT = 0;
float FixedUpdateTask::mSimulationTime = 0;
float FixedUpdateTask::mfFrameLockTime = 0;
float FixedUpdateTask::mTimeScale = 1;
static void UseFixedUpdatePad() { work(P::FixedInput); }
static void UseDefaultPad() { work(P::FixedInput); }
static void UpdatePlatPad(float) { work(P::FixedInput); }
struct cPadManager { static void Update(float) { work(P::FixedInput); } };
struct FlickDetection { static void Update() { work(P::FixedInput); } };
static void ReportSimulationTime(float, float scaled, unsigned steps, uint64_t, bool, uint64_t)
{
    reportedSteps = steps;
    reportedDt = scaled;
    work(P::FixedControl);
}

struct nlVector3 { float x = 0, y = 0, z = 0; };
struct PhysicsWorld {};
struct PhysicsAIBall {};
static PhysicsWorld* g_PhysicsWorld = nullptr;
static void PhysicsUpdate(PhysicsWorld*, float) { work(P::FixedPhysics); }
struct Character
{
    nlVector3 m_v3Position;
    void UpdateBlinking(float) { work(P::RenderPreparation); }
};
static Character characters[10];
static Character* g_pCharacters[10];
struct Team { Character* GetGoalie() { return &characters[0]; } };
static Team teams[2];
static Team* g_pTeams[2]{&teams[0], &teams[1]};
struct cField { static float GetGoalLineX(unsigned) { return 10; } };
struct cBall { PhysicsAIBall* m_pPhysicsBall = nullptr; nlVector3 m_v3Position, m_v3PrevPosition; };
static cBall ball;
static cBall* g_pBall = &ball;
struct NetMesh
{
    static bool s_bAnimatedNetMeshEnabled;
    static NetMesh* spPositiveXNetMesh;
    static NetMesh* spNegativeXNetMesh;
    void Update(float, const nlVector3&, const nlVector3&, bool, PhysicsAIBall*) { work(P::FixedNet); }
};
static NetMesh nets[2];
bool NetMesh::s_bAnimatedNetMeshEnabled = true;
NetMesh* NetMesh::spPositiveXNetMesh = &nets[0];
NetMesh* NetMesh::spNegativeXNetMesh = &nets[1];
struct NpcManager { void UpdateAINPCs(float) { work(P::FixedNpc); } };
static NpcManager npcs;
struct BasicStadium
{
    NpcManager* mpNPCManager = &npcs;
    void* m_pShadowLight = nullptr;
    static BasicStadium* GetCurrentStadium();
    void* GetShadowLight(nlVector3, float) { work(P::RenderLighting); return nullptr; }
};
static BasicStadium stadium;
BasicStadium* BasicStadium::GetCurrentStadium() { return &stadium; }
struct SidelineExplodableManager { static void Update(float) { work(P::FixedSideline); } };
struct ReplayManager
{
    float mTime = 0;
    static ReplayManager* Instance() { static ReplayManager instance; return &instance; }
    void GrabSnapshot() { work(P::FixedSnapshot); }
    void RenderSnapshotAt(float) { work(P::RenderSnapshot); }
};

struct WorldManager
{
    static BasicStadium* s_World;
    static void RenderWorld() { work(P::RenderWorld); }
};
BasicStadium* WorldManager::s_World = &stadium;
struct GameRenderTask
{
    void Run(float dt);
    static void SetupConstants() { work(P::RenderPreparation); }
};
static bool g_bRenderWorld = true, g_bMemoryOnScreen = false, g_bWhiteDiffuse = false, g_bTexelDensity = false;
static bool g_TexDetail = true, g_TexShadow = true, g_TexSelfIllum = true, g_TexGloss = true;
static constexpr unsigned GLTT_Detail_bit = 2, GLTT_Shadow_bit = 4, GLTT_SelfIllum_bit = 8, GLTT_Gloss_bit = 16;
static constexpr unsigned MaxProjectedShadows = 3;
static void MemoryOnScreen(u32) { work(P::RenderLighting); }
static void UpdateGameObjectLighting() { work(P::RenderLighting); }
static void SetCharacterShadowUpdated(int, bool) { work(P::RenderLighting); }
struct Presentation
{
    static Presentation& Instance() { static Presentation instance; return instance; }
    void Update(float) { work(P::RenderPresentation); }
};
struct Jumbotron { static Jumbotron instance; void Update(float) { work(P::RenderPreparation); } };
Jumbotron Jumbotron::instance;
struct CrowdManager { static CrowdManager instance; void Update(float) { work(P::RenderPreparation); } };
CrowdManager CrowdManager::instance;
struct FlareHandler { static FlareHandler instance; void Render() { work(P::RenderPreparation); } };
FlareHandler FlareHandler::instance;
enum eGLView { GLV_FrontEnd };
enum { GLMod_DiffuseTex, GLMod_TextureMask, GLTT_Diffuse, GLTS_DiffuseWrap };
struct glPoly2
{
    void SetupRectangle(float, float, float, float, float) { work(P::RenderPreparation); }
    void Attach(eGLView, unsigned, unsigned, u32) { work(P::RenderPreparation); }
};
static u32 glGetTexture(const char*) { work(P::RenderPreparation); return 1; }
static void gl_ModifyAddMapping(unsigned, u32) { work(P::RenderPreparation); }
static bool glTextureLoad(u32) { return true; }
static unsigned glTextureGetWidth() { return 1; }
static unsigned glTextureGetHeight() { return 1; }
static void glSetDefaultState(bool) { work(P::RenderPreparation); }
static void glSetCurrentTexture(u32, unsigned) { work(P::RenderPreparation); }
static void glSetTextureState(unsigned, unsigned) { work(P::RenderPreparation); }
static unsigned glHandleizeTextureState() { return 0; }
static void glSetCurrentTextureState(unsigned) { work(P::RenderPreparation); }
static void gl_ModifyClearMappings() { work(P::RenderInventory); }
struct Inventory { void Update(float) { work(P::RenderInventory); } };
static Inventory glInventory;
struct Game { bool mbCaptainShotToScoreOn = false; };
static Game game;
static Game* g_pGame = &game;
static void UpdateAndRenderIndicators(float) { work(P::RenderInventory); }
struct WorldDarkening
{
    static WorldDarkening& Instance() { static WorldDarkening instance; return instance; }
    static void SetPolyColour(glPoly2&, int, int, int, int) { work(P::RenderPreparation); }
    void UpdateAndRender(float) { work(P::RenderInventory); }
};
struct ShootToScoreMeter { static ShootToScoreMeter instance; void UpdateAndRender(float) { work(P::RenderInventory); } };
ShootToScoreMeter ShootToScoreMeter::instance;
static u32 nlGetTicker() { return 1; }
static u32 nlGetTickerDifference(u32 a, u32 b) { return b - a; }
struct DepthOfFieldManager { static DepthOfFieldManager instance; void Update() { work(P::RenderEffects); } };
DepthOfFieldManager DepthOfFieldManager::instance;
struct BlurManager { static void Update(float) { work(P::RenderEffects); } };
static void WarbleTest() { work(P::RenderEffects); }
struct PhotoFlash { static void Render(float) { work(P::RenderEffects); } };
static void UpdateElectricFence(float) { work(P::RenderEffects); }

// INSERT_UNCHANGED_PRODUCTION_TASK_BODIES

static PortBenchCaptureStatus status(unsigned samples, unsigned warmup = 0)
{
    return {samples == 6 ? PORT_CAPTURE_COMPLETE : PORT_CAPTURE_COLLECTING,
            samples, 6, warmup, 0, samples ? 600 + samples - 1 : 0};
}
template<class T> static void run(T& task, const char* name, float dt)
{
    const auto start = port_monotonic_ns();
    task.Run(dt);
    const auto end = port_monotonic_ns();
    port::task_profile().record(&task, name, P::Run, end - start);
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    const std::string tasksPath = std::string(argv[1]) + ".tasks.csv";
    const std::string phasesPath = std::string(argv[1]) + ".phases.csv";
    for (unsigned i = 0; i < 10; ++i) g_pCharacters[i] = &characters[i];
    FixedUpdateTask fixed;
    GameRenderTask render;
    auto& profile = port::task_profile();

    const auto beforeOff = allocations;
    denyAllocations = true;
    profile.begin_frame(status(0));
    fixed.Run(.046f);
    render.Run(.046f);
    profile.end_tasks();
    profile.end_frame(status(0), 1);
    // FixedUpdateTask already had two diagnostic clocks. The phase hooks add none.
    assert(clocks == 2 && allocations == beforeOff && profile.samples().empty());
    assert(::access(tasksPath.c_str(), F_OK) && ::access(phasesPath.c_str(), F_OK));
    denyAllocations = false;
    assert(profile.configure(argv[1], 6));
    const auto reserved = allocations, beforeWarmup = clocks;
    denyAllocations = true;
    profile.begin_frame(status(0, 1));
    fixed.Run(.046f);
    render.Run(.046f);
    profile.end_tasks();
    profile.end_frame(status(0), 2);
    assert(clocks == beforeWarmup + 2 && allocations == reserved && profile.samples().empty());

    profile.begin_frame(status(0));
    run(fixed, "intro fixed", .05f);
    run(render, "intro render", .05f);
    profile.end_tasks();
    profile.end_frame(status(0), 99);
    assert(profile.samples().empty() && profile.task_count() == 0);

    struct Scenario { float dt, scale, debt; unsigned steps; bool carry, early, lockstep; int state; };
    const Scenario cases[]{
        {.046f, 1, .004f, 2, false, false, false, 2},
        {.002f, .5f, .001f, 0, false, true, false, 2},
        {.4f, 2, .01f, 16, false, false, false, 2},
        {0, 1, 0, 16, true, false, false, 2},
        {.07f, .5f, .02f, 1, false, false, true, 2},
        {.2f, 1, .07f, 0, false, false, false, 1}
    };
    for (unsigned index = 0; index < std::size(cases); ++index)
    {
        const auto& test = cases[index];
        manager.m_CurrState = test.state;
        g_bRunSimAndRenderInLockStep = test.lockstep;
        g_bRenderWorld = !test.early;
        FixedUpdateTask::mTimeScale = test.scale;
        if (!test.carry) FixedUpdateTask::mAccumulatedDeltaT = test.debt;
        const auto before = FixedUpdateTask::mAccumulatedDeltaT;
        workCalls = {};
        profile.begin_frame(status(index));
        run(fixed, "Game Fixed Update", test.dt);
        run(render, "Game Render", test.dt);
        profile.end_tasks();
        profile.end_frame(status(index + 1), 100 + index);
        assert(!profile.error_code() && profile.samples().size() == index + 1);
        const auto& frame = profile.samples().back();
        assert(frame.engineFrame == 100 + index && frame.liveFrame == 600 + index);
        assert(frame.simulation.present && frame.simulation.parentTask == 0);
        assert(frame.simulation.steps == test.steps && reportedSteps == test.steps);
        assert(frame.simulation.scaledDtSeconds == (test.state == 2 ? test.dt * test.scale : 0));
        assert(frame.simulation.scaledDtSeconds == reportedDt);
        assert(frame.simulation.debtBeforeSeconds == before);
        assert(frame.simulation.debtAfterSeconds == FixedUpdateTask::mAccumulatedDeltaT);
        assert(frame.simulation.tickSeconds == g_fFixedUpdateTick && frame.simulation.lockstep == test.lockstep);
        assert(workCalls[P::FixedAi] == test.steps && workCalls[P::FixedPhysics] == test.steps);
        assert(workCalls[P::FixedInput] == 4 * (test.steps + 1));
        uint64_t children[2]{};
        for (unsigned phase = 0; phase < P::DetailCount; ++phase)
        {
            const auto& detail = frame.details[phase];
            assert(detail.elapsedNs == 10ull * workCalls[phase]);
            const auto expectedCalls = phase < P::RenderLighting
                ? test.steps + unsigned(phase < P::FixedAi)
                : unsigned(!test.early || phase == P::RenderLighting);
            assert(detail.calls == expectedCalls);
            if (detail.calls)
            {
                assert(detail.parentTask == unsigned(phase >= P::RenderLighting));
                children[detail.parentTask] += detail.elapsedNs;
            }
        }
        assert(children[0] == frame.tasks[0].runNs && children[1] == frame.tasks[1].runNs);
        if (index < 5) assert(::access(tasksPath.c_str(), F_OK) && ::access(phasesPath.c_str(), F_OK));
    }
    assert(profile.complete() && allocations == reserved);
    const auto afterDeadline = clocks;
    profile.begin_frame(status(6));
    fixed.Run(.1f);
    render.Run(.1f);
    profile.end_tasks();
    profile.end_frame(status(6), 106);
    assert(clocks == afterDeadline + 2 && allocations == reserved);
    denyAllocations = false;
    assert(!::access(tasksPath.c_str(), F_OK) && !::access(phasesPath.c_str(), F_OK));
}
