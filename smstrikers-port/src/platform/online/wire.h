// The online wire format: datagrams, chunks, messages and a checking parser.

#ifndef PORT_ONLINE_WIRE_H
#define PORT_ONLINE_WIRE_H

#include <cstddef>
#include <cstdint>

#include "port/online.h"
#include "port/lockstep.h"

namespace online
{
const size_t kMaxDatagram = 1200;
const size_t kSealedHeader = 16;
const size_t kTagSize = 16;
const size_t kMaxPayload = kMaxDatagram - kSealedHeader - kTagSize;

const size_t kRecordSize = 17;
const int kMaxRecordsPerChunk = 32;
const int kMaxHashesPerChunk = 8;
const int kMaxComponents = 32;
const int kMaxChunksPerMachine = ONLINE_MAX_MACHINES;
const int kMaxMessagesPerPayload = 32;
const size_t kMaxMessageBody = 276;

// The first byte of a datagram.
enum : uint8_t
{
    DG_DISCOVER = 0x01,
    DG_ANNOUNCE = 0x02,
    DG_HELLO = 0x10,
    DG_WELCOME = 0x11,
    DG_REFUSE = 0x12,
    DG_SEALED = 0x20
};

const size_t kDiscoverSize = 48;
const size_t kAnnounceSize = 48;
const size_t kHelloSize = 128;
const size_t kWelcomeSize = 64;
const size_t kRefuseSize = 32;

// Chunk kinds inside a sealed payload; 0x40 and up are reliable messages.
enum : uint8_t
{
    CH_TIMING = 0x01,
    CH_RELACK = 0x02,
    CH_INPUT = 0x03,
    CH_INPUT_ACK = 0x04,
    CH_HASH = 0x05,
    CH_COMPONENTS = 0x06,
    CH_BYE = 0x07,

    MSG_JOIN = 0x40,
    MSG_JOIN_ACCEPT = 0x41,
    MSG_JOIN_REFUSE = 0x42,
    MSG_ROOM_STATE = 0x43,
    MSG_DESC = 0x44,
    MSG_DESC_ACK = 0x45,
    MSG_LOCAL_PLAYER = 0x46,
    MSG_READY = 0x47,
    MSG_START = 0x48,
    MSG_LOAD_PROGRESS = 0x49,
    MSG_GO = 0x4A,
    MSG_BARRIER_DONE = 0x4B,
    MSG_RESUME = 0x4C,
    MSG_DROP = 0x4D,
    MSG_DESYNC = 0x4E,
    MSG_VOTE = 0x4F,
    MSG_VOTE_RESULT = 0x50,
    MSG_ABORT = 0x51,
    MSG_DELAY = 0x52,
    MSG_SKIP = 0x53,
    MSG_SELECT = 0x54
};

// JOIN's binary family: which compiler and target produced the build.
enum
{
    FAMILY_MACOS_ARM64 = 0,
    FAMILY_WINDOWS_X64 = 1,
    FAMILY_LINUX_X64 = 2,
    FAMILY_LINUX_ARM64 = 3,
    FAMILY_SWITCH = 4,
    FAMILY_XBOX = 5,
    FAMILY_OTHER = 6
};

// ABORT reasons.
enum
{
    ABORT_SETUP_MISMATCH = 1,
    ABORT_DESYNC = 2,
    ABORT_LEFT_WHILE_LOADING = 3,
    ABORT_HOST_ENDED = 4
};

inline void Put16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

inline void Put32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}

inline void Put64(uint8_t* p, uint64_t v)
{
    for (int i = 0; i < 8; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}

inline uint16_t Get16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

inline uint32_t Get32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

inline uint64_t Get64(const uint8_t* p)
{
    return (uint64_t)Get32(p) | ((uint64_t)Get32(p + 4) << 32);
}

// A name16 field: `name` cleaned, NUL-padded to 16 bytes.
void PutName(uint8_t* p, const char* name);

// Reads a name16 field; false unless it is NUL-padded and already clean.
bool GetName(const uint8_t* p, char out[ONLINE_NAME_MAX + 1]);

// A 16-byte printable ASCII field, NUL-padded.
void PutVersion(uint8_t* p, const char* version);
bool GetVersion(const uint8_t* p, char out[17]);

// One human's eight bytes: buttons, sticks, triggers.
struct PadBytes
{
    uint16_t buttons;
    int8_t stickX;
    int8_t stickY;
    int8_t substickX;
    int8_t substickY;
    uint8_t triggerL;
    uint8_t triggerR;
};

void RecordPut(uint8_t rec[kRecordSize], int human, const PadBytes& pad);
PadBytes RecordGet(const uint8_t rec[kRecordSize], int human);

// Checks what a record alone can show; `humans` is how many the sending machine has.
bool RecordValid(const uint8_t rec[kRecordSize], int humans);

struct Discover
{
    uint32_t queryId;
};

struct Announce
{
    uint32_t queryId;
    uint16_t gamePort;
    uint8_t platform;
    uint8_t machines;
    uint8_t humans;
    uint8_t flags;   // bit0 in a match or loading, bit1 full
    uint32_t build;
    uint32_t dataFamily;
    char hostName[ONLINE_NAME_MAX + 1];
};

struct Hello
{
    uint8_t publicKey[32];
    uint8_t nonce[16];
};

struct Welcome
{
    uint8_t publicKey[32];
    uint8_t nonce[16];
    uint32_t linkId;
};

struct Refuse
{
    uint16_t protocol;   // the host's
    uint8_t nonce[16];
    uint8_t reason;   // ONLINE_ERR_VERSION, _ROOM_FULL or _IN_MATCH
};

// The class of a well-formed plaintext datagram, else 0; a HELLO or REFUSE of any protocol passes.
int PlainClass(const uint8_t* p, size_t n, uint16_t* protocol);

size_t EncodeDiscover(const Discover& d, uint8_t out[kDiscoverSize]);
size_t EncodeAnnounce(const Announce& a, uint8_t out[kAnnounceSize]);
size_t EncodeHello(const Hello& h, uint8_t out[kHelloSize]);
size_t EncodeWelcome(const Welcome& w, uint8_t out[kWelcomeSize]);
size_t EncodeRefuse(const Refuse& r, uint8_t out[kRefuseSize]);

bool DecodeDiscover(const uint8_t* p, size_t n, Discover* out);
bool DecodeAnnounce(const uint8_t* p, size_t n, Announce* out);
bool DecodeHello(const uint8_t* p, size_t n, Hello* out);
bool DecodeWelcome(const uint8_t* p, size_t n, Welcome* out);
bool DecodeRefuse(const uint8_t* p, size_t n, Refuse* out);

struct SealedHeader
{
    uint32_t linkId;
    uint64_t seq;
};

void EncodeSealedHeader(const SealedHeader& h, uint8_t out[kSealedHeader]);

// Class, reserved bytes and minimum size; the tag is checked by the caller.
bool DecodeSealedHeader(const uint8_t* p, size_t n, SealedHeader* out);

struct TimingChunk
{
    uint32_t sendUs;
    uint32_t echoUs;
    uint32_t echoHeldUs;
    uint32_t tick;
    uint32_t confirmedTick;
};

struct RelAckChunk
{
    uint16_t nextRelSeq;
    uint32_t bitmap;
};

struct InputChunk
{
    uint32_t matchSerial;
    uint8_t machine;
    uint8_t count;
    uint32_t firstTick;
    uint8_t records[kMaxRecordsPerChunk][kRecordSize];
};

struct InputAckChunk
{
    uint32_t matchSerial;
    uint8_t machine;
    uint32_t nextTick;
};

// `machine`'s combined hashes of ticks firstTick .. firstTick + count - 1.
struct HashChunk
{
    uint32_t matchSerial;
    uint8_t machine;
    uint32_t firstTick;
    uint8_t count;
    uint64_t hash[kMaxHashesPerChunk];
};

struct ComponentsChunk
{
    uint32_t matchSerial;
    uint8_t machine;
    uint32_t tick;
    uint8_t count;
    uint64_t hash[kMaxComponents];
};

struct WireHuman
{
    int8_t slot;   // -1 for an unused entry
    uint8_t vote;
    char name[ONLINE_NAME_MAX + 1];
};

struct WireMachine
{
    uint8_t present;
    uint8_t platform;
    uint8_t flags;   // bit0 connected, bit1 ready, bit2 reserved, bit3 descAcked
    uint8_t loadState;
    uint8_t loadPercent;
    uint8_t lossPercent;
    uint16_t pingMs;
    uint16_t jitterMs;
    char nickname[ONLINE_NAME_MAX + 1];
    uint8_t humanCount;
    uint8_t delayTicks;                         // its setting: 0 Auto, else ticks
    WireHuman human[ONLINE_MAX_LOCAL_HUMANS];   // by the machine's own local index
};

enum
{
    MACHINE_CONNECTED = 1 << 0,
    MACHINE_READY = 1 << 1,
    MACHINE_DESC_ACKED = 1 << 3
};

struct MsgJoin
{
    uint16_t protocol;
    uint8_t platform;
    uint8_t binaryFamily;
    uint32_t build;
    uint32_t dataFamily;
    uint8_t delayTicks;   // the joining machine's setting: 0 Auto, else ticks
    char version[17];
    char nickname[ONLINE_NAME_MAX + 1];
    uint8_t humanCount;
    char human[ONLINE_MAX_LOCAL_HUMANS][ONLINE_NAME_MAX + 1];
};

struct MsgJoinAccept
{
    uint8_t machine;
    uint32_t roomId;
};

struct MsgJoinRefuse
{
    uint8_t reason;
    uint8_t hostPlatform;
    uint8_t hostDataRegion;   // 0 USA/Europe, 1 Japan
    char hostVersion[17];
};

struct MsgRoomState
{
    uint32_t revision;
    uint8_t phase;
    uint8_t host;
    uint8_t delayTicks;
    uint8_t voteSeconds;
    uint32_t matchSerial;
    OnlineSelection selection;
    WireMachine machine[ONLINE_MAX_MACHINES];
};

struct MsgDesc
{
    uint32_t revision;
    uint8_t desc[ONLINE_DESC_BYTES];   // decoded by PortOnlineDescDecode only
};

struct MsgDescAck
{
    uint32_t revision;
    uint64_t hash;
    uint8_t accepted;
    uint8_t reason;   // 0, 1 a field out of range, 2 identity differs
};

struct MsgLocalPlayer
{
    uint8_t op;   // 1 add, 2 remove, 3 rename a human, 4 rename the machine
    uint8_t localIndex;
    char name[ONLINE_NAME_MAX + 1];
};

struct MsgStart
{
    uint32_t matchSerial;
    uint32_t descRevision;
    uint64_t descHash;
};

struct MsgLoadProgress
{
    uint32_t matchSerial;
    uint8_t loadState;
    uint8_t percent;
};

struct MsgGo
{
    uint32_t matchSerial;
    uint32_t startInUs;
    uint8_t delayTicks;
};

struct MsgBarrierDone
{
    uint32_t matchSerial;
    uint8_t kind;
    uint32_t tick;
};

struct MsgResume
{
    uint32_t matchSerial;
    uint8_t kind;
    uint8_t delayTicks;
    uint8_t actions;
    uint32_t tick;
    uint32_t startInUs;
    uint32_t seed;
};

struct MsgDrop
{
    uint32_t matchSerial;
    uint8_t machine;
    uint8_t cpuSlots;
    uint32_t tick;
};

struct MsgDesync
{
    uint32_t matchSerial;
    uint32_t tick;
    uint8_t reporter;
    uint64_t hash[ONLINE_MAX_MACHINES];
};

struct MsgVote
{
    uint32_t matchSerial;
    int8_t slot;
    uint8_t vote;
};

struct MsgSelect
{
    uint32_t epoch;
    uint8_t op, side, team, sidekick, phase;
};

struct MsgSkip
{
    uint32_t matchSerial;
    uint32_t presentation;
};

struct MsgSerialByte
{
    uint32_t matchSerial;
    uint8_t value;   // VOTE_RESULT outcome, ABORT reason or DELAY ticks
};

struct Message
{
    uint8_t kind;
    uint16_t relSeq;
    union
    {
        MsgJoin join;
        MsgJoinAccept joinAccept;
        MsgJoinRefuse joinRefuse;
        MsgRoomState roomState;
        MsgDesc desc;
        MsgDescAck descAck;
        MsgLocalPlayer localPlayer;
        uint8_t ready;
        MsgStart start;
        MsgLoadProgress loadProgress;
        MsgGo go;
        MsgSkip skip;
        MsgSelect select;
        MsgBarrierDone barrierDone;
        MsgResume resume;
        MsgDrop drop;
        MsgDesync desync;
        MsgVote vote;
        MsgSerialByte serialByte;
    } u;
};

// The body after relSeq; returns its size, or 0 for an unknown kind.
size_t EncodeMessageBody(const Message& m, uint8_t out[kMaxMessageBody]);

// Checks the length and every field of a message body.
bool DecodeMessageBody(uint8_t kind, const uint8_t* p, size_t n, Message* out);

// Offset of startInUs inside GO and RESUME bodies, which a resend rewrites.
const size_t kGoStartOffset = 4;
const size_t kResumeStartOffset = 12;

struct Payload
{
    bool hasTiming;
    TimingChunk timing;
    bool hasRelAck;
    RelAckChunk relAck;
    int inputCount;
    InputChunk input[kMaxChunksPerMachine];
    int inputAckCount;
    InputAckChunk inputAck[kMaxChunksPerMachine];
    int hashCount;
    HashChunk hash[kMaxChunksPerMachine];
    int componentsCount;
    ComponentsChunk components[kMaxChunksPerMachine];
    bool hasBye;
    uint8_t byeReason;
    int messageCount;
    Message messages[kMaxMessagesPerPayload];
};

// Parses a decrypted payload; false, with `out` unspecified, when anything is malformed.
bool ParsePayload(const uint8_t* p, size_t n, Payload* out);

// Appends chunks to a payload buffer; each call fails without writing when the chunk does not fit.
class ChunkWriter
{
  public:
    ChunkWriter(uint8_t* buf, size_t cap) : m_buf(buf), m_cap(cap), m_size(0) {}

    size_t Size() const { return m_size; }
    size_t Room() const { return m_cap - m_size; }

    bool Timing(const TimingChunk& t);
    bool RelAck(uint16_t nextRelSeq, uint32_t bitmap);
    bool Input(uint32_t serial, uint8_t machine, uint32_t firstTick, int count,
               const uint8_t* records);
    bool InputAck(uint32_t serial, uint8_t machine, uint32_t nextTick);
    bool Hash(uint32_t serial, uint8_t machine, uint32_t firstTick, int count,
              const uint64_t* hashes);
    bool Components(uint32_t serial, uint8_t machine, uint32_t tick, int count,
                    const uint64_t* hashes);
    bool Bye(uint8_t reason);
    bool Reliable(uint8_t kind, uint16_t relSeq, const uint8_t* body, size_t size);

  private:
    uint8_t* Begin(uint8_t kind, size_t length);

    uint8_t* m_buf;
    size_t m_cap;
    size_t m_size;
};
}   // namespace online

#endif   // PORT_ONLINE_WIRE_H
