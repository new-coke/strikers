// The lockstep engine: input logs, local input, confirmation, hashes and link statistics.

#ifndef PORT_ONLINE_ENGINE_H
#define PORT_ONLINE_ENGINE_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "wire.h"

namespace online
{
const uint32_t kTickUs = ONLINE_TICK_MS * 1000;
const uint32_t kNoTick = 0xFFFFFFFFu;

// The furthest a machine runs past every machine's input; at most PORT_ROLLBACK_TICKS - 2.
const uint32_t kMaxAhead = 14;

// Auto delay leaves this much of the worst path to guessing and covers the rest with delay.
const uint32_t kAutoGuessTicks = 2;

// The ticks before the intro's freeze: neutral input on every machine, never guessed.
const uint32_t kNeutralTicks = 3;

// Time from `then` to `now`, 0 when `then` was read later on another path.
inline uint64_t Since(uint64_t now, uint64_t then) { return now > then ? now - then : 0; }

// Round trip, jitter and loss of one link.
class LinkStats
{
  public:
    LinkStats() { Reset(); }
    void Reset();

    // Every authenticated datagram's sequence number.
    void OnSeq(uint64_t seq, uint64_t nowUs);

    // A round-trip sample from a TIMING echo.
    void OnRtt(uint32_t rttUs, uint64_t nowUs);

    bool HasRtt() const { return m_samples > 0; }
    uint32_t SrttUs() const { return m_srtt; }

    // The one-way delay's standard deviation over 10 s: the round trip's over the square root of 2.
    uint32_t JitterUs(uint64_t nowUs) const;

    int LossPercent(uint64_t nowUs) const;

    // Mean round trip and one-way deviation over the last `seconds`; false with no samples.
    bool Window(uint64_t nowUs, int seconds, double* rttUs, double* oneWaySdUs) const;

  private:
    struct RttBucket
    {
        uint64_t second;
        uint32_t count;
        double sum;
        double sumSq;
    };
    struct SeqBucket
    {
        uint64_t second;
        uint32_t count;
        uint64_t minSeq;
        uint64_t maxSeq;
    };

    void Pool(uint64_t nowUs, int seconds, uint32_t* count, double* mean, double* sd) const;

    RttBucket m_rtt[60];
    SeqBucket m_seq[10];
    uint32_t m_srtt;
    uint32_t m_rttvar;
    uint32_t m_samples;
};

// One slot's input for a tick, as the session installs it.
struct SlotInput
{
    uint8_t kind;   // ONLINE_SLOT_*
    uint8_t connected;
    PadBytes pad;
};

class Lockstep
{
  public:
    Lockstep() { Clear(); }

    // No match.
    void Clear();

    // A match between `machineMask`'s machines, with slots owned as `slotMachine` says.
    void Begin(uint32_t serial, int self, uint8_t machineMask,
               const int8_t slotMachine[ONLINE_MAX_SLOTS]);

    bool Active() const { return m_active; }
    uint32_t Serial() const { return m_serial; }
    int Self() const { return m_self; }
    uint8_t Machines() const { return m_machines; }
    bool InMatch(int machine) const
    {
        return machine >= 0 && machine < ONLINE_MAX_MACHINES && (m_machines >> machine & 1);
    }

    // Humans of `machine` in the match, and their slots in ascending order.
    int HumansOf(int machine) const;
    int SlotOf(int machine, int human) const;
    int OwnerOf(int slot) const
    {
        return slot >= 0 && slot < ONLINE_MAX_SLOTS ? m_slotMachine[slot] : -1;
    }

    // The newest sample of this machine's `human`; buttons keep every press since the last record,
    // or none while `taps` is false.
    void Sample(int human, const PadBytes& pad, bool connected, bool taps);

    void SetDelay(int ticks) { m_delay = ticks; }
    int Delay() const { return m_delay; }

    // Ticks 0 .. delay - 1 get neutral records with the current connected bits.
    void StartGeneration();

    // Records through tick `through` from the newest sample; the first new tick, or kNoTick.
    uint32_t Generate(uint32_t through);

    // Records of `machine` from `firstTick`: how many were new, or -1 for a broken rule.
    int Add(int machine, uint32_t firstTick, int count, const uint8_t (*records)[kRecordSize]);

    // The first tick of `machine` not held.
    uint32_t Have(int machine) const { return m_have[machine]; }
    const uint8_t* Record(int machine, uint32_t tick) const;

    int Conflicts() const { return m_conflicts; }

    bool Ready(uint32_t tick) const;
    uint32_t Confirmed() const;
    int Missing(uint32_t tick) const;
    void Read(uint32_t tick, SlotInput out[ONLINE_MAX_SLOTS]) const;

    // Read with missing records guessed as the newest; a later record that differs marks the tick.
    void ReadPredicted(uint32_t tick, SlotInput out[ONLINE_MAX_SLOTS]);

    // The earliest tick run on a wrong guess, or kNoTick; taking it clears it.
    uint32_t TakeMispredicted();
    uint32_t Mispredicted() const { return m_mispredicted; }
    int Mispredictions() const { return m_mispredictions; }

    void Drop(int machine, uint32_t tick);
    bool Dropped(int machine) const { return m_dropTick[machine] != kNoTick; }
    uint32_t DropTick(int machine) const { return m_dropTick[machine]; }

    // True, with the tick, when this makes a hash of `machine` differ from this machine's.
    bool SetHash(int machine, uint32_t tick, uint64_t hash, uint32_t* mismatchTick);
    bool HashOf(int machine, uint32_t tick, uint64_t* out) const;
    uint32_t NewestHash(int machine) const { return m_newestHash[machine]; }

    struct Components
    {
        uint32_t tick;
        int count;
        uint64_t hash[kMaxComponents];
        const char* const* names;
    };
    void SetComponents(int machine, uint32_t tick, const uint64_t* hashes, int count,
                       const char* const* names);
    const Components* ComponentsAt(int machine,
                                   int index) const;   // newest first, NULL past the ring
    static const int kComponentRing = 20;
    static const int kHashRing = 512;

  private:
    struct HashEntry
    {
        uint32_t tick;
        uint64_t hash;
    };

    bool m_active;
    uint32_t m_serial;
    int m_self;
    uint8_t m_machines;
    int8_t m_slotMachine[ONLINE_MAX_SLOTS];
    int m_delay;

    PadBytes m_latest[ONLINE_MAX_LOCAL_HUMANS];
    uint16_t m_taps[ONLINE_MAX_LOCAL_HUMANS];
    bool m_connected[ONLINE_MAX_LOCAL_HUMANS];

    std::vector<uint8_t> m_log[ONLINE_MAX_MACHINES];
    uint32_t m_have[ONLINE_MAX_MACHINES];

    // What each tick read for a machine's missing record, by tick modulo the ring.
    static const int kPredictRing = 256;
    struct Prediction
    {
        uint32_t tick;
        uint8_t rec[kRecordSize];
    };
    Prediction m_predicted[ONLINE_MAX_MACHINES][kPredictRing];
    uint32_t m_mispredicted;
    int m_mispredictions;
    uint32_t m_dropTick[ONLINE_MAX_MACHINES];
    int m_conflicts;

    HashEntry m_hash[ONLINE_MAX_MACHINES][kHashRing];
    uint32_t m_newestHash[ONLINE_MAX_MACHINES];
    uint32_t m_lastAgreed;

    Components m_components[ONLINE_MAX_MACHINES][kComponentRing];
    int m_componentsHead[ONLINE_MAX_MACHINES];
};
}   // namespace online

#endif   // PORT_ONLINE_ENGINE_H
