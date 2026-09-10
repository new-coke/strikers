// The software mixer that stands in for the GameCube DSP.

#include "port/audio.h"
#include "port/host.h"

#if defined(PORT_USE_AURORA)

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// dspvoice.h has no extern "C" of its own and declares `dspStudio` and `dspVoice` at namespace
// scope.
extern "C" {
#include "musyx/adsr.h"
#include "musyx/dspvoice.h"
#include "musyx/hardware.h"
#include "musyx/musyx.h"
#include "musyx/sal.h"
#include "musyx/stream.h"
#include "musyx/synth.h"

u32 adsrSetup(ADSR_VARS* adsr);

// hardware.c. salFrame picks which of a studio's two main buffers is written this tick, salAuxFrame
// which of the three aux buffers; snd_handle_irq advances both after this mixer runs.
extern u8 salFrame;
extern u8 salAuxFrame;

// src/platform/musyx_aram.c
void* PortAramResolve(size_t aram);
}

// Where a voice's sample bytes actually are.
static const u8* sampleBytes(const SAMPLE_INFO& smp) {
    if (smp.compType == 4 || smp.compType == 5 || smp.compType == 6)
        return (const u8*)PortAramResolve((size_t)smp.addr);
    return (const u8*)smp.addr;
}

namespace {

constexpr int kSampleRate = 32000;    // what salInitAi reports through *outFreq
constexpr int kChannels = 2;
constexpr int kFramesPerTick = 160;   // 0x280 bytes / 4 = one AI DMA buffer
constexpr int kSubFrames = 5;         // snd_handle_irq steps the synth 5x
constexpr int kFramesPerSub = kFramesPerTick / kSubFrames;   // 32

// Sample formats, from SAMPLE_INFO::compType.
constexpr u8 kFmtAdpcm = 0;      // and 1, 4, 5
constexpr u8 kFmtPcm16 = 2;      // and 6
constexpr u8 kFmtPcm8 = 3;

bool isAdpcm(u8 t) { return t == 0 || t == 1 || t == 4 || t == 5; }
bool isPcm16(u8 t) { return t == 2 || t == 6; }

// DSP-ADPCM: 8-byte blocks of one header byte and 14 four-bit samples.
constexpr u32 kAdpcmSamplesPerBlock = 14;
constexpr u32 kAdpcmBytesPerBlock = 8;

// The nine buses a studio mixes into, in the order `DSPhostDPop` and `_PB::mix` name them.
enum Bus { kL, kR, kS, kAL, kAR, kAS, kBL, kBR, kBS, kBuses };

// Per-voice decoder state. Parallel to dspVoice[] and indexed the same way; MusyX's own DSPvoice
// has nowhere to keep this because on console it lived in the hardware's parameter block.
struct VoiceMix {
    bool active;
    u32 pos;          // integer sample index into the sample
    u32 frac;         // 16.16 fraction between pos and pos+1

    // The four samples the interpolator sits between: pos-1, pos, pos+1, pos+2.
    s32 h[4];

    // ADPCM decoder. Decoding is sequential by construction, each block's output depends on the two
    // samples before it, so a block is decoded once and held while playback walks through it.
    s32 blockIndex;   // which block `decoded` holds, -1 for none
    s16 decoded[kAdpcmSamplesPerBlock];
    s16 yn1, yn2;     // carried across block boundaries

    // The per-voice low-pass. `_PBLPF` is one pole: y = (a0*x + b0*y1) >> 15, with the coefficients
    // built by hwLowPassFrqToCoef and carried in DSPvoice::filter.
    s32 lpY1;
    u8 lpOn;

    // What this voice last contributed to each bus.
    s32 last[kBuses];
};

VoiceMix* s_mix = nullptr;
u8 s_mixVoices = 0;

// One studio's accumulators, reused for each studio in turn. s32 so that many voices can sum
// without wrapping before the single clamp at the end; the DSP had 24-bit accumulators for the same
// reason.
s32 s_acc[kBuses][kFramesPerTick];

// The device mix: the master studios summed.
s32 s_outL[kFramesPerTick];
s32 s_outR[kFramesPerTick];
s32 s_outS[kFramesPerTick];

extern "C" void PortAudioMixFinishDump(void);

// STRIKERS_AUDIO_DUMP=<path> writes exactly what goes to the device, as a WAV.
std::FILE* s_dump = nullptr;
unsigned long s_dumpFrames = 0;
int s_dumpTried = 0;
int s_everRendered = 0;
s32 s_lastBusPeak = 0;
s32 s_lastRawPeak = 0;
// High-water marks since the last log line, not per-tick: a reverb send is bursty and a per-tick
// sample of it lands on silence most of the time.
s32 s_lastAuxSend = 0;
s32 s_lastAuxReturn = 0;

// Open the dump. The 44-byte canonical WAV header goes in with the sizes left at zero;
// PortAudioMixFinishDump fills them in, which is the only point the length is known.
bool dump_open(const char* path) {
    if (s_dump != nullptr || path == nullptr || *path == '\0')
        return false;
    s_dump = std::fopen(path, "wb");
    if (s_dump == nullptr)
        return false;
    s_dumpFrames = 0;
    unsigned char hdr[44];
    std::memset(hdr, 0, sizeof hdr);
    std::memcpy(hdr + 0, "RIFF", 4);
    std::memcpy(hdr + 8, "WAVEfmt ", 8);
    hdr[16] = 16;                       // fmt chunk size
    hdr[20] = 1;                        // PCM
    hdr[22] = (unsigned char)kChannels;
    hdr[24] = (unsigned char)(kSampleRate & 0xFF);
    hdr[25] = (unsigned char)((kSampleRate >> 8) & 0xFF);
    hdr[26] = (unsigned char)((kSampleRate >> 16) & 0xFF);
    const int byteRate = kSampleRate * kChannels * 2;
    hdr[28] = (unsigned char)(byteRate & 0xFF);
    hdr[29] = (unsigned char)((byteRate >> 8) & 0xFF);
    hdr[30] = (unsigned char)((byteRate >> 16) & 0xFF);
    hdr[32] = (unsigned char)(kChannels * 2);
    hdr[34] = 16;                       // bits per sample
    std::memcpy(hdr + 36, "data", 4);
    std::fwrite(hdr, 1, sizeof hdr, s_dump);
    static bool s_atexit = false;
    if (!s_atexit) {
        s_atexit = true;
        std::atexit(PortAudioMixFinishDump);
    }
    return true;
}

int s_logging = -1;
bool logging() {
    if (s_logging < 0) {
        const char* e = getenv("STRIKERS_LOG_AUDIO");
        s_logging = (e != nullptr && *e != '\0') ? 1 : 0;
    }
    return s_logging != 0;
}

inline s16 clamp16(s32 v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (s16)v;
}

// A float read from the environment once, with a default and a range.
float envFloat(const char* name, float dflt, float lo, float hi) {
    const char* e = getenv(name);
    if (e == nullptr || *e == '\0')
        return dflt;
    char* end = nullptr;
    const double v = strtod(e, &end);
    if (end == e)
        return dflt;
    if (v < lo) return lo;
    if (v > hi) return hi;
    return (float)v;
}

// --- the master stage ------------------------------------------------------- The console had
// neither of these.
float s_masterGain = -1.0f;   // STRIKERS_AUDIO_VOLUME, 0..2
float s_limitGain = 1.0f;     // the limiter's current gain, kept across ticks
float s_limitEnv = 0.0f;      // its peak follower

// Where the limiter starts working and where the soft clip does, in units of full scale. 0.89
// leaves a little over a decibel of headroom, which is enough for the release to be inaudible
// without the limiter riding the whole match.
constexpr float kLimitCeiling = 0.89f * 32767.0f;
constexpr float kSoftKnee = 0.80f * 32767.0f;

// One tick is 5 ms. Attack over roughly a millisecond, release over 250, both as per-sample
// one-pole coefficients at 32 kHz.
constexpr float kLimitAttack = 0.03f;
constexpr float kLimitRelease = 0.000125f;

// Above the knee the response bends instead of breaking. tanh is only evaluated for samples that
// are actually up there, which in a normal match is none of them.
inline float softClip(float x) {
    const float a = x < 0.0f ? -x : x;
    if (a <= kSoftKnee)
        return x;
    const float span = 32767.0f - kSoftKnee;
    const float y = kSoftKnee + span * std::tanh((a - kSoftKnee) / span);
    return x < 0.0f ? -y : y;
}

// The surround bus, folded into stereo. salCalcVolume still fills volS from the span and the
// console's last command was SET_OPPOSITE_LR, so surround goes out of phase between the channels.
const float s_surroundMix = 0.7071f;

// Decode one 8-byte ADPCM block into vm.decoded, carrying yn1/yn2 in.
void decodeBlock(VoiceMix& vm, const u8* data, const SNDADPCMinfo* info, u32 block) {
    const u8* blk = data + (size_t)block * kAdpcmBytesPerBlock;
    const u8 ps = blk[0];
    const int scale = 1 << (ps & 0x0F);
    const int idx = (ps >> 4) & 0x07;
    const s32 c1 = info->coefTab[idx][0];
    const s32 c2 = info->coefTab[idx][1];

    s32 yn1 = vm.yn1;
    s32 yn2 = vm.yn2;
    for (u32 i = 0; i < kAdpcmSamplesPerBlock; ++i) {
        const u8 byte = blk[1 + i / 2];
        s32 nib = (i & 1) ? (byte & 0x0F) : (byte >> 4);
        if (nib > 7)
            nib -= 16;                       // sign-extend the 4-bit sample
        s32 out = ((nib * scale) << 11) + 1024 + c1 * yn1 + c2 * yn2;
        out >>= 11;
        out = clamp16(out);
        vm.decoded[i] = (s16)out;
        yn2 = yn1;
        yn1 = out;
    }
    vm.yn1 = (s16)yn1;
    vm.yn2 = (s16)yn2;
    vm.blockIndex = (s32)block;
}

// Streams are compType 4, 5 and 6: a ring buffer the game keeps refilling, which MusyX plays as a
// sample whose loop spans the whole buffer.
bool isStream(u8 t) { return t == 4 || t == 5 || t == 6; }

// One sample from a voice, by index. ADPCM decoding is sequential, so walking forward a block at a
// time is not an optimisation but a correctness requirement: skipping a block loses the yn1/yn2 the
// next one is built on.
s32 sampleAt(VoiceMix& vm, const SAMPLE_INFO& smp, u32 index) {
    const u8* data = sampleBytes(smp);
    if (data == nullptr)
        return 0;

    // Never read past what was allocated for this sample.
    if (!isStream(smp.compType) && smp.length != 0 && index >= smp.length)
        index = smp.length - 1;

    if (isPcm16(smp.compType)) {
        // Big-endian on disc, like everything else the console read directly.
        const u8* p = data + (size_t)index * 2;
        return (s16)((p[0] << 8) | p[1]);
    }
    if (smp.compType == kFmtPcm8)
        return (s32)(s8)data[index] << 8;
    const SNDADPCMinfo* info = (const SNDADPCMinfo*)smp.extraData;
    if (info == nullptr)
        return 0;
    const u32 block = index / kAdpcmSamplesPerBlock;
    if (vm.blockIndex != (s32)block) {
        // Walk forward when we are behind, so yn1/yn2 stay continuous.
        u32 from = (vm.blockIndex >= 0 && (u32)vm.blockIndex < block)
                       ? (u32)vm.blockIndex + 1
                       : block;
        for (u32 b = from; b <= block; ++b)
            decodeBlock(vm, data, info, b);
    }
    return vm.decoded[index % kAdpcmSamplesPerBlock];
}

// Reset a voice's decoder to the start of the sample.
void startVoice(VoiceMix& vm, const SAMPLE_INFO& smp) {
    std::memset(&vm, 0, sizeof(vm));
    vm.active = true;
    vm.blockIndex = -1;
    if (isAdpcm(smp.compType)) {
        const SNDADPCMinfo* info = (const SNDADPCMinfo*)smp.extraData;
        if (info != nullptr) {
            vm.yn1 = 0;
            vm.yn2 = 0;
        }
    }
    // h[] straddles the cursor: h[1] is the sample at pos, h[2] the one after, h[0] the one before
    // and h[3] the one after that.
    vm.h[0] = 0;
    vm.h[1] = sampleAt(vm, smp, 0);
    vm.h[2] = sampleAt(vm, smp, 1);
    vm.h[3] = sampleAt(vm, smp, 2);
}

// Jump to the loop point. For a one-shot with a loop point, restore the decoder state the sample
// was authored with: without it an ADPCM loop clicks on every repeat, because the first block after
// the jump is decoded against the wrong two samples.
void loopVoice(VoiceMix& vm, const SAMPLE_INFO& smp) {
    vm.pos = smp.loop;
    vm.blockIndex = -1;
    if (isAdpcm(smp.compType) && !isStream(smp.compType)) {
        const SNDADPCMinfo* info = (const SNDADPCMinfo*)smp.extraData;
        if (info != nullptr) {
            vm.yn1 = info->loopY1;
            vm.yn2 = info->loopY0;
        }
    }
}

// Interpolate between the four samples in vm.h at a 16.16 fraction.
inline s32 interpolate(const VoiceMix& vm, u16 srcSelect, u32 frac) {
    if (srcSelect == 2)
        return vm.h[1];
    const float t = (float)frac * (1.0f / 65536.0f);
    if (srcSelect == 1)
        return (s32)((float)vm.h[1] + ((float)vm.h[2] - (float)vm.h[1]) * t);
    const float a = (float)vm.h[0];
    const float b = (float)vm.h[1];
    const float c = (float)vm.h[2];
    const float d = (float)vm.h[3];
    const float c0 = b;
    const float c1 = 0.5f * (c - a);
    const float c2 = a - 2.5f * b + 2.0f * c - 0.5f * d;
    const float c3 = 0.5f * (d - a) + 1.5f * (b - c);
    return (s32)(((c3 * t + c2) * t + c1) * t + c0);
}

// The per-voice low-pass that `_PBLPF` describes.
inline s32 lowPass(VoiceMix& vm, const FILTERInfo& f, s32 x) {
    if (f.on == 0) {
        vm.lpY1 = 0;
        return x;
    }
    const s32 y = ((s32)f.coefA * x + (s32)f.coefB * vm.lpY1) >> 15;
    vm.lpY1 = y;
    return y;
}

// The depop ramp, as `AddDpop` and `DoDepopFade` in hw_dspctrl.c; both `static` there and so not
// callable from here.
void addDpop(s32* sum, s32 delta) {
    *sum += delta;
    if (*sum > 0x7fffff) *sum = 0x7fffff;
    if (*sum < -0x7fffff) *sum = -0x7fffff;
}

// One aux bus out and back, which is what MIX_AUXA and MIX_AUXB were.
void auxRoute(s32* dst, const s32* src, int base, bool withSurround) {
    if (dst != nullptr) {
        for (int f = 0; f < kFramesPerTick; ++f) {
            dst[f] = s_acc[base + 0][f];
            dst[kFramesPerTick + f] = s_acc[base + 1][f];
            if (withSurround)
                dst[2 * kFramesPerTick + f] = s_acc[base + 2][f];
        }
    }
    if (src != nullptr) {
        for (int f = 0; f < kFramesPerTick; ++f) {
            s_acc[kL][f] += src[f];
            s_acc[kR][f] += src[kFramesPerTick + f];
            if (withSurround)
                s_acc[kS][f] += src[2 * kFramesPerTick + f];
        }
    }
}

void doDepopFade(s32* start, s32* delta, s32* hostSum) {
    if (*hostSum <= -160)
        *delta = (*hostSum <= -3200) ? 0x14 : (-*hostSum / 160);
    else if (*hostSum >= 160)
        *delta = (*hostSum >= 3200) ? -0x14 : (-*hostSum / 160);
    else
        *delta = 0;
    *start = *hostSum;
    *hostSum += *delta * 160;
}

// Publish the play cursor where MusyX reads it back.
void publishCursor(DSPvoice* v, const SAMPLE_INFO& smp, u32 pos) {
    const u32 addr = (u32)(uintptr_t)smp.addr;
    if (isPcm16(smp.compType)) {
        v->currentAddr = (addr >> 1) + pos;
    } else if (smp.compType == kFmtPcm8) {
        v->currentAddr = addr + pos;
    } else {
        v->currentAddr = addr * 2 + (pos / kAdpcmSamplesPerBlock) * 16 + 2
                         + (pos % kAdpcmSamplesPerBlock);
    }
}

} // namespace

extern "C" void PortAudioMixRender(s16* dest) {
    if (dest == nullptr)
        return;

    if (s_masterGain < 0.0f) {
        s_masterGain = envFloat("STRIKERS_AUDIO_VOLUME", 1.0f, 0.0f, 2.0f);
    }

    std::memset(s_outL, 0, sizeof(s_outL));
    std::memset(s_outR, 0, sizeof(s_outR));
    std::memset(s_outS, 0, sizeof(s_outS));

    if (s_mix == nullptr || s_mixVoices != salNumVoices) {
        std::free(s_mix);
        s_mixVoices = salNumVoices;
        s_mix = (VoiceMix*)std::calloc(s_mixVoices ? s_mixVoices : 1, sizeof(VoiceMix));
        if (s_mix == nullptr)
            return;
        for (u8 i = 0; i < s_mixVoices; ++i)
            s_mix[i].blockIndex = -1;
        if (logging())
            std::fprintf(stderr, "[port] audio: mixer sized for %u voices\n", s_mixVoices);
    }

    for (u8 st = 0; st < salMaxStudioNum; ++st) {
        DSPstudioinfo* stp = &dspStudio[st];
        if (stp->state != 1)
            continue;

        std::memset(s_acc, 0, sizeof(s_acc));

        // DSPhostDPop is nine s32 in bus order, the offsets are written into its declaration, so
        // the nine ramps are one loop rather than nine named lines.
        s32* const dpopSum = reinterpret_cast<s32*>(&stp->hostDPopSum);

        // The studio's inputs: DL_AND_VOL_MIX, one command per input.
        for (u8 in = 0; in < stp->numInputs; ++in) {
            const DSPinput& di = stp->in[in];
            const s32* src = dspStudio[di.studio].main[salFrame ^ 1];
            if (src == nullptr)
                continue;
            for (int f = 0; f < kFramesPerTick; ++f) {
                const s32 l = src[f];
                const s32 r = src[kFramesPerTick + f];
                const s32 s = src[2 * kFramesPerTick + f];
                s_acc[kL][f] += (l * (s32)di.vol) >> 15;
                s_acc[kR][f] += (r * (s32)di.vol) >> 15;
                s_acc[kS][f] += (s * (s32)di.vol) >> 15;
                s_acc[kAL][f] += (l * (s32)di.volA) >> 15;
                s_acc[kAR][f] += (r * (s32)di.volA) >> 15;
                s_acc[kAS][f] += (s * (s32)di.volA) >> 15;
                s_acc[kBL][f] += (l * (s32)di.volB) >> 15;
                s_acc[kBR][f] += (r * (s32)di.volB) >> 15;
                s_acc[kBS][f] += (s * (s32)di.volB) >> 15;
            }
        }

        DSPvoice* next = nullptr;
        for (DSPvoice* v = stp->voiceRoot; v != nullptr; v = next) {
            next = v->next;                       // v may be deactivated below
            if (v->state == 0)
                continue;

            const size_t vi = (size_t)(v - dspVoice);
            if (vi >= s_mixVoices)
                continue;
            VoiceMix& vm = s_mix[vi];
            SAMPLE_INFO& smp = v->smp_info;

            // A break. `hwBreak`, reached from every stop the synth or streaming layer makes, sets
            // bit 0x20 in the change mask and nothing else; acting on it was the command list's
            // job.
            if (v->postBreak != 0 || (v->changed[0] & 0x20) != 0) {
                if (v->state != 1 || v->startupBreak != 0) {
                    for (int b = 0; b < kBuses; ++b)
                        addDpop(&dpopSum[b], vm.last[b]);
                    v->startupBreak = 0;
                    v->postBreak = 0;
                    vm.active = false;
                    salDeactivateVoice(v);
                    continue;
                }
            }

            if (v->state == 1) {
                // Freshly activated. This is the work the command list's `if (dsp_vptr->state ==
                // 1)` block did.
                if (adsrSetup(&v->adsr) != 0) {
                    salSynthSendMessage(v, 0);
                    salDeactivateVoice(v);
                    continue;
                }
                startVoice(vm, smp);
                publishCursor(v, smp, 0);
                v->state = 2;
                if (logging()) {
                    static unsigned long s_started;
                    if (s_started++ < 400)   // a match's worth, not a boot's
                    {
                        // port_monotonic_ns, not clock_gettime: CLOCK_MONOTONIC is POSIX and the
                        // UCRT has neither. port/host.h exists for this exact family and names
                        // clock_gettime in its own header comment as the reason.
                        std::fprintf(stderr,
                                     "[port] mix: start voice %u smp_id=%u type=%u addr=%p "
                                     "len=%u loop=%u loopLen=%u pitch=0x%x @%lums\n",
                                     (unsigned)vi, v->smp_id, smp.compType, smp.addr,
                                     smp.length, smp.loop, smp.loopLength, v->pitch[0],
                                     (unsigned long)(port_monotonic_ns() / 1000000ull));
                    }
                }
            }
            if (!vm.active || sampleBytes(smp) == nullptr)
                continue;

            const bool loops = smp.loopLength != 0;
            const u32 endSample = loops ? (smp.loop + smp.loopLength) : smp.length;

            // Name the samples whose loop runs past their own length, the ones sampleAt has to
            // clamp.
            if (logging() && loops && !isStream(smp.compType) && endSample > smp.length) {
                static unsigned long s_warned;
                if (s_warned++ < 20)
                    std::fprintf(stderr,
                                 "[port] mix: sample %u loops past its length: loop=%u + %u > %u "
                                 "(clamped)\n",
                                 v->smp_id, smp.loop, smp.loopLength, smp.length);
            }
            u32 pitch = v->pitch[0];
            bool done = false;

            for (int sub = 0; sub < kSubFrames && !done; ++sub) {
                if (v->changed[sub] & 0x20) {
                    // The break's fast fade; the voice goes at the next tick.
                    adsrStartRelease(&v->adsr, 10);
                    v->postBreak = 1;
                } else if (v->postBreak == 0) {
                    if (v->changed[sub] & 0x40)
                        adsrRelease(&v->adsr);
                    if (v->changed[sub] & 0x08)
                        pitch = v->pitch[sub];
                }

                // The envelope is published once per sub-frame as a starting value and a per-sample
                // delta, which is exactly how the DSP's volume envelope consumed it.
                u16 env = 0, envDelta = 0;
                const u32 voiceDone = adsrHandle(&v->adsr, &env, &envDelta);

                // The nine pan volumes, gathered once. >> 15 and not >> 16 because they are 15-bit
                // unity: `hwSetVolume` builds each as `32767.f * v`, and the envelope adsrHandle
                // publishes tops out at 0x7FFF too.
                const s32 vol[kBuses] = { v->volL,  v->volR,  v->volS,
                                          v->volLa, v->volRa, v->volSa,
                                          v->volLb, v->volRb, v->volSb };
                int busN = 0;
                int busIdx[kBuses];
                s32 busVol[kBuses];
                for (int b = 0; b < kBuses; ++b) {
                    vm.last[b] = 0;
                    if (vol[b] != 0) {
                        busIdx[busN] = b;
                        busVol[busN] = vol[b];
                        ++busN;
                    }
                }

                const u16 srcSelect = v->srcTypeSelect;
                if (srcSelect == 2)
                    pitch = 0x10000;   // "no SRC": one sample in, one out

                // The low-pass history is cleared on the off-to-on edge, which is where the command
                // list wrote `pb->lpf.yn1 = 0`.
                if (v->filter.on != 0 && vm.lpOn == 0)
                    vm.lpY1 = 0;
                vm.lpOn = v->filter.on;

                s32 e = env;

                for (int i = 0; i < kFramesPerSub; ++i) {
                    const int f = sub * kFramesPerSub + i;

                    s32 s = interpolate(vm, srcSelect, vm.frac);
                    s = lowPass(vm, v->filter, s);

                    const s32 enveloped = (s * e) >> 15;
                    for (int j = 0; j < busN; ++j) {
                        const s32 c = (enveloped * busVol[j]) >> 15;
                        s_acc[busIdx[j]][f] += c;
                        vm.last[busIdx[j]] = c;
                    }

                    e += envDelta;
                    if (e < 0) e = 0;
                    if (e > 0x7FFF) e = 0x7FFF;

                    vm.frac += pitch;
                    while (vm.frac >= 0x10000) {
                        vm.frac -= 0x10000;
                        ++vm.pos;
                        if (vm.pos >= endSample) {
                            if (loops) {
                                loopVoice(vm, smp);
                            } else {
                                vm.active = false;
                                done = true;
                                break;
                            }
                        }
                        vm.h[0] = vm.h[1];
                        vm.h[1] = vm.h[2];
                        vm.h[2] = vm.h[3];
                        vm.h[3] = sampleAt(vm, smp, vm.pos + 2 < endSample
                                                        ? vm.pos + 2
                                                        : (loops ? smp.loop : vm.pos));
                    }
                    if (done)
                        break;
                }

                if (voiceDone != 0) {
                    vm.active = false;
                    done = true;
                }
            }

            // What sal_update_hostplayinfo did, which is `static` in hw_dspctrl.c and so not
            // callable from here.
            v->playInfo.pitch = pitch;
            if (smp.loopLength == 0) {
                v->playInfo.posHi = vm.pos;
                v->playInfo.posLo = vm.frac << 16;
            }
            publishCursor(v, smp, vm.pos);

            if (!vm.active) {
                // The console did not depop here: a one-shot that ran off its end kept reading the
                // ARAM zero buffer, so the DSP's last sample really was zero and there was no step
                // to hide.
                for (int b = 0; b < kBuses; ++b)
                    addDpop(&dpopSum[b], vm.last[b]);
                salSynthSendMessage(v, 0);
                salDeactivateVoice(v);
            }
        }

        // The depop ramps, one per bus. `DoDepopFade` publishes a start value and a per-sample
        // delta into the studio parameter block and the DSP added `start + n * delta` across the
        // buffer; that is this loop.
        for (int b = 0; b < kBuses; ++b) {
            s32 start = 0, delta = 0;
            doDepopFade(&start, &delta, &dpopSum[b]);
            if (start == 0 && delta == 0)
                continue;
            for (int f = 0; f < kFramesPerTick; ++f)
                s_acc[b][f] += start + delta * f;
        }

        // The aux sends and returns, triple-buffered: the send written this tick is processed by
        // `salHandleAuxProcessing` on the next and mixed back on the one after, which is the
        // reverb's 10 ms.
        if (stp->auxAHandler != nullptr) {
            // Send and return, separately: they are two different failures.
            for (int f = 0; f < kFramesPerTick; ++f) {
                const s32 m = s_acc[kAL][f] < 0 ? -s_acc[kAL][f] : s_acc[kAL][f];
                if (m > s_lastAuxSend) s_lastAuxSend = m;
            }
            auxRoute(stp->auxA[salAuxFrame], stp->auxA[(salAuxFrame + 1) % 3], kAL, true);
            const s32* ret = stp->auxA[(salAuxFrame + 1) % 3];
            if (ret != nullptr) {
                for (int f = 0; f < kFramesPerTick; ++f) {
                    const s32 m = ret[f] < 0 ? -ret[f] : ret[f];
                    if (m > s_lastAuxReturn) s_lastAuxReturn = m;
                }
            }
        }

        if (stp->type == SND_STUDIO_TYPE_STD) {
            if (stp->auxBHandler != nullptr)
                auxRoute(stp->auxB[salAuxFrame], stp->auxB[(salAuxFrame + 1) % 3], kBL, true);
        } else {
            // A DPL2 studio's aux B is not an effect send at all: it carries the rear pair, and
            // MIX_AUXB_LR folds it into the front through the Pro Logic II encode matrix, which
            // lives in the DSP microcode and is not in this tree.
            auxRoute(stp->auxB[salFrame], nullptr, kBL, false);
        }

        // UPLOAD_LRS: publish this studio's main output where its consumers read it next tick, and
        // add it to the device mix if it is a master.
        s32* const mainOut = stp->main[salFrame];
        if (mainOut != nullptr) {
            for (int f = 0; f < kFramesPerTick; ++f) {
                mainOut[f] = s_acc[kL][f];
                mainOut[kFramesPerTick + f] = s_acc[kR][f];
                mainOut[2 * kFramesPerTick + f] = s_acc[kS][f];
            }
        }
        if (stp->isMaster != 0) {
            for (int f = 0; f < kFramesPerTick; ++f) {
                s_outL[f] += s_acc[kL][f];
                s_outR[f] += s_acc[kR][f];
                s_outS[f] += s_acc[kS][f];
            }
        }
    }

    // The master stage: surround fold, master gain, limiter, soft clip.
    {
        s32 peak = 0;
        s32 raw = 0;
        for (int f = 0; f < kFramesPerTick; ++f) {
            {
                const s32 m = s_outL[f] < 0 ? -s_outL[f] : s_outL[f];
                if (m > raw) raw = m;
            }
            const float sur = (float)s_outS[f] * s_surroundMix;
            float l = ((float)s_outL[f] + sur) * s_masterGain;
            float r = ((float)s_outR[f] - sur) * s_masterGain;

            const float a = (l < 0.0f ? -l : l) > (r < 0.0f ? -r : r) ? (l < 0.0f ? -l : l)
                                                                      : (r < 0.0f ? -r : r);
            // Peak follower: jump to a new peak, fall away from an old one.
            s_limitEnv = a > s_limitEnv ? a
                                        : s_limitEnv + (a - s_limitEnv) * kLimitRelease;
            const float want = s_limitEnv > kLimitCeiling ? kLimitCeiling / s_limitEnv : 1.0f;
            s_limitGain += (want - s_limitGain)
                           * (want < s_limitGain ? kLimitAttack : kLimitRelease);

            l = softClip(l * s_limitGain);
            r = softClip(r * s_limitGain);

            const s32 li = (s32)l;
            const s32 ri = (s32)r;
            dest[f * 2 + 0] = clamp16(li);
            dest[f * 2 + 1] = clamp16(ri);
            const s32 m = li < 0 ? -li : li;
            if (m > peak) peak = m;
        }
        s_lastBusPeak = peak;
        s_lastRawPeak = raw;
    }

    s_everRendered = 1;

    if (logging()) {
        static unsigned long s_tick;
        if ((s_tick++ % 400) == 0) {   // every 2 seconds of audio
            int studios = 0, voices = 0, withAddr = 0;
            u32 peakEnv = 0, peakVol = 0;
            for (u8 st = 0; st < salMaxStudioNum; ++st) {
                if (dspStudio[st].state != 1)
                    continue;
                ++studios;
                for (DSPvoice* v = dspStudio[st].voiceRoot; v != nullptr; v = v->next) {
                    ++voices;
                    if (sampleBytes(v->smp_info) != nullptr)
                        ++withAddr;
                    u32 e = (u32)(v->adsr.currentVolume >> 16);
                    if (e > peakEnv) peakEnv = e;
                    if (v->volL > peakVol) peakVol = v->volL;
                    if (v->volR > peakVol) peakVol = v->volR;
                }
            }
            s32 rawPeak = 0, surPeak = 0;
            for (int f = 0; f < kFramesPerTick; ++f) {
                s32 m = s_outL[f] < 0 ? -s_outL[f] : s_outL[f];
                if (m > rawPeak) rawPeak = m;
                m = s_outS[f] < 0 ? -s_outS[f] : s_outS[f];
                if (m > surPeak) surPeak = m;
            }
            std::fprintf(stderr,
                         "[port] mix: studios=%d voices=%d withSample=%d "
                         "peakEnv=0x%04x peakVol=0x%04x busPeak=%d rawPeak=%d surPeak=%d "
                         "auxSend=%d auxRet=%d limGain=%.3f mixFrq=%u\n",
                         studios, voices, withAddr, peakEnv, peakVol, s_lastBusPeak, rawPeak,
                         surPeak, s_lastAuxSend, s_lastAuxReturn, (double)s_limitGain,
                         (unsigned)synthInfo.mixFrq);
            s_lastAuxSend = 0;
            s_lastAuxReturn = 0;
            for (u8 st = 0; st < salMaxStudioNum && voices; ++st) {
                if (dspStudio[st].state != 1)
                    continue;
                DSPvoice* v = dspStudio[st].voiceRoot;
                if (v == nullptr)
                    continue;
                std::fprintf(stderr,
                             "[port] mix:   voice smp_id=%u addr=%p extra=%p len=%u "
                             "loop=%u loopLen=%u type=%u off=%u state=%u pitch=0x%x pos=%u cur=0x%x "
                             "lpf=%u aux=0x%04x\n",
                             v->smp_id, v->smp_info.addr, v->smp_info.extraData,
                             v->smp_info.length, v->smp_info.loop, v->smp_info.loopLength,
                             v->smp_info.compType, v->smp_info.offset, v->state,
                             (unsigned)v->pitch[0], (unsigned)s_mix[v - dspVoice].pos,
                             (unsigned)v->currentAddr, v->filter.on,
                             (unsigned)(v->volLa > v->volRa ? v->volLa : v->volRa));
                break;
            }
        }
    }
}

extern "C" int PortAudioMixStats(PortAudioMixInfo* out) {
    if (out == nullptr)
        return 0;
    std::memset(out, 0, sizeof *out);
    if (!s_everRendered)
        return 0;
    for (u8 st = 0; st < salMaxStudioNum; ++st) {
        if (dspStudio[st].state != 1)
            continue;
        ++out->studios;
        for (DSPvoice* v = dspStudio[st].voiceRoot; v != nullptr; v = v->next) {
            ++out->voices;
            if (sampleBytes(v->smp_info) != nullptr)
                ++out->withSample;
            u32 e = (u32)(v->adsr.currentVolume >> 16);
            if (e > out->peakEnv) out->peakEnv = e;
            if (v->volL > out->peakVol) out->peakVol = v->volL;
            if (v->volR > out->peakVol) out->peakVol = v->volR;
        }
    }
    out->busPeak = (int)s_lastBusPeak;
    out->mixFrq = (unsigned)synthInfo.mixFrq;
    out->dumping = s_dump != nullptr;
    out->dumpFrames = s_dumpFrames;
    out->rawPeak = (int)s_lastRawPeak;
    out->limitGain = s_limitGain;
    out->masterGain = s_masterGain < 0.0f ? 1.0f : s_masterGain;
    return 1;
}

// The device's own bytes, handed over by the transport rather than taken at the end of the mix.
extern "C" void PortAudioDumpWrite(const void* pcm, unsigned int frames) {
    if (s_dumpTried == 0) {
        s_dumpTried = 1;
        dump_open(getenv("STRIKERS_AUDIO_DUMP"));
    }
    if (s_dump == nullptr || pcm == nullptr)
        return;
    std::fwrite(pcm, 2, (size_t)frames * kChannels, s_dump);
    s_dumpFrames += frames;
}

extern "C" int PortAudioDumpStart(const char* path) {
    s_dumpTried = 1;   // the menu's choice wins over the env's
    return dump_open(path) ? 1 : 0;
}

extern "C" void PortAudioDumpStop(void) { PortAudioMixFinishDump(); }

extern "C" void PortAudioMixFinishDump(void) {
    if (s_dump == nullptr)
        return;
    const unsigned long dataBytes = s_dumpFrames * kChannels * 2;
    const unsigned long riffBytes = dataBytes + 36;
    unsigned char v[4];
    v[0] = (unsigned char)(riffBytes & 0xFF);
    v[1] = (unsigned char)((riffBytes >> 8) & 0xFF);
    v[2] = (unsigned char)((riffBytes >> 16) & 0xFF);
    v[3] = (unsigned char)((riffBytes >> 24) & 0xFF);
    std::fseek(s_dump, 4, SEEK_SET);
    std::fwrite(v, 1, 4, s_dump);
    v[0] = (unsigned char)(dataBytes & 0xFF);
    v[1] = (unsigned char)((dataBytes >> 8) & 0xFF);
    v[2] = (unsigned char)((dataBytes >> 16) & 0xFF);
    v[3] = (unsigned char)((dataBytes >> 24) & 0xFF);
    std::fseek(s_dump, 40, SEEK_SET);
    std::fwrite(v, 1, 4, s_dump);
    std::fclose(s_dump);
    s_dump = nullptr;
    if (logging())
        std::fprintf(stderr, "[port] audio: wrote %lu frames of dump\n", s_dumpFrames);
}

#else // !PORT_USE_AURORA

extern "C" void PortAudioMixRender(short* dest) { (void)dest; }
extern "C" void PortAudioMixFinishDump(void) {}
extern "C" void PortAudioDumpWrite(const void* pcm, unsigned int frames) {
    (void)pcm;
    (void)frames;
}

#endif
