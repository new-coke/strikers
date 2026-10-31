// The online session: links, room and match, under one lock, with time and sockets from outside.

#ifndef PORT_ONLINE_SESSION_H
#define PORT_ONLINE_SESSION_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>
#include <string>

#include "engine.h"
#include "pipe.h"
#include "seal.h"
#include "wire.h"

namespace online
{
// A match's game loop stopped this long goes quiet, so the other machines drop it; through a
// shorter freeze its network side keeps talking and they wait.
const uint64_t kLoopGiveUpUs = 15000000;

// What a room compares before it admits a machine.
struct Identity
{
    uint32_t build;
    uint32_t dataFamily;
    uint8_t platform;       // ONLINE_PLATFORM_*
    uint8_t binaryFamily;   // FAMILY_*
    uint8_t dataRegion;     // 0 USA/Europe, 1 Japan
    char version[17];
};

enum
{
    PIPE_GAME = 0,    // the room's port, dual stack where the platform has IPv6
    PIPE_DISCOVERY,   // the host's shared LAN discovery listener, IPv4
    PIPE_SEARCH       // an ephemeral IPv4 socket that may broadcast
};

enum
{
    PORTMAP_NONE = -1,   // this platform has no port mapping
    PORTMAP_PENDING,
    PORTMAP_OPEN,     // the router forwards the port
    PORTMAP_SHARED,   // the router's own outside address is private or a carrier's
    PORTMAP_FAILED
};

class SessionEnv
{
  public:
    virtual ~SessionEnv() {}
    virtual uint64_t NowUs() = 0;
    virtual NetPipe* OpenPipe(int purpose, uint16_t port, int* error) = 0;   // PIPE_ERR_* in error
    virtual void ClosePipe(NetPipe* pipe) = 0;
    virtual uint16_t PipePort(NetPipe* pipe) = 0;
    // A name lookup off the calling thread; PollResolve is 0 while pending, 1 done, -1 failed.
    virtual void StartResolve(const char* host, uint16_t port) = 0;
    virtual int PollResolve(NetAddr* out) = 0;
    virtual int BroadcastTargets(NetAddr* out, int max, uint16_t port) = 0;
    virtual bool LanAddress(char* out, size_t size) = 0;
    // This machine's addresses after LanAddress's, such as a VPN's.
    virtual int OtherAddresses(char out[][16], int max)
    {
        (void)out;
        (void)max;
        return 0;
    }
    // A router forwarding UDP `port` here, off the calling thread; Poll gives its outside IPv4.
    virtual void StartPortMap(uint16_t port) { (void)port; }
    virtual int PollPortMap(char* wan, size_t size)
    {
        (void)wan;
        (void)size;
        return PORTMAP_NONE;
    }
    virtual void StopPortMap() {}
    // Cryptographic randomness from the system; false when the source failed.
    virtual bool Random(void* buf, size_t size) = 0;
    // A fresh desync report's path, its folder created; false when there is nowhere to write.
    virtual bool ReportPath(char* out, size_t size) = 0;
    virtual void Log(const char* line) = 0;
};

struct SessionConfig
{
    uint16_t discoveryPort;
    bool canHost;      // false where the platform never waits for connections
    bool findPublic;   // the router forwards the host port from launch; a host also asks STUN
};

// A device port's sample, as the session keeps it.
struct LocalPad
{
    PadBytes pad;
    bool connected;
};

class Session
{
  public:
    Session(SessionEnv* env, const Identity& identity, const SessionConfig& config);
    ~Session();

    void SetSettings(const OnlineSettings& settings);
    const OnlineSettings& Settings() const { return m_settings; }

    // Non-zero when accepted.
    int Host(uint16_t port, const char* nickname);
    int Join(const char* address, const char* nickname);
    int RefreshNearby();
    void Leave();
    int AddLocalPlayer(int localPort, const char* name);
    int RemoveLocalPlayer(int localPort);
    int SetDesc(const OnlineMatchDesc& desc);
    int SetReady(int ready);
    int Select(int op, int side, int team, int sidekick, int phase);
    int Start();
    int Vote(int slot, int vote);
    int AfterOpponentsLeft(int continueAgainstCpu);

    // Receives, runs timers and sends what is due.
    void Poll();
    // Sends what is due without receiving, for a thread that just queued something.
    void Flush();
    // When Poll next has work, never more than 20 ms ahead.
    uint64_t NextWakeUs();
    int Pipes(NetPipe** out, int max) const;
    // Goodbye to every link at once and every socket closed.
    void Shutdown();

    void Update(float timeScale);
    const OnlineState& State() const { return m_state; }
    bool Active() const;
    bool InMatch() const;
    void SampleLocalPads(const LocalPad ports[4]);
    // Per slot: kind, connected, a remote slot's confirmed input, a local slot's port.
    int RenderSlots(SlotInput out[ONLINE_MAX_SLOTS], int localPort[ONLINE_MAX_SLOTS]);
    int SlotKind(int slot) const;
    int SlotLocalPort(int slot) const;
    const char* SlotName(int slot) const;
    void LoadProgress(int percent);
    void MatchLoaded();
    int MayBeginMatch();
    void MatchEnded();
    int TickInput(uint32_t tick, SlotInput out[ONLINE_MAX_SLOTS]);

    // Rollback: the earliest tick that ran on a wrong guess, or kNoTick; asking clears it.
    uint32_t RollbackTo();

    // A replay stopped at a freeze: ticks from `tick` run again; their hashes do not count.
    void Rewind(uint32_t tick);

    // Every machine's input is in for the ticks before this one.
    uint32_t ConfirmedTick() const;

    // As ConfirmedTick, but not past a tick still waiting to run again on input that came since.
    uint32_t SettledTick() const;
    void TickHash(uint32_t tick, uint64_t hash);
    void TickComponents(uint32_t tick, const uint64_t* hashes, const char* const* names, int count);
    int TickActions(uint32_t tick, OnlineTickActions* out);
    void BarrierReached(int kind, uint32_t tick);
    void BarrierDone(int kind);
    int PresentationSkip(uint32_t presentation, bool pressed);
    int MayRunTick(uint32_t tick);
    float ClockRate();

    std::string TakeReport(uint32_t& serial);
    std::string SaveReport(const std::string& text) const;
    void ReportSaved(const std::string& path, uint32_t serial);

    // The match's input logs and hashes, read by the C API and the tests.
    const Lockstep& Engine() const { return m_lock; }

  private:
    struct Outgoing
    {
        uint8_t kind;
        uint16_t relSeq;
        uint16_t size;
        uint64_t lastSentUs;   // 0 until first sent
        uint64_t deadlineUs;   // GO and RESUME: the start moment a resend recomputes
        uint8_t body[kMaxMessageBody];
    };

    // Reliable messages a link holds ahead of the next in order; the 32-bit ack bitmap caps it.
    static const int kRelWindow = 32;
    struct Held
    {
        bool has;
        uint16_t seq;
        uint64_t atUs;   // its arrival, for a start time it carries
        Message m;
    };
    struct Link
    {
        bool used;
        bool authed;    // an authenticated datagram has arrived
        bool closing;   // refused: closes once its messages are acknowledged
        bool stuck;     // its reliable queue filled; the next timer pass drops it
        uint64_t closeAtUs;
        int machine;   // -1 until admitted; the host is 0
        NetAddr addr;
        NetAddr home;   // the address the handshake proved
        // Bytes from and to addr since it last changed. They bound the sends while it is not home,
        // where a peer's NAT may have moved it or a sender with this link's keys forged it.
        uint64_t awayRecv;
        uint64_t awaySent;
        uint32_t linkId;
        uint8_t nonce[16];
        uint8_t guestPublic[32];
        uint8_t hostPublic[32];
        uint8_t sendKey[32];
        uint8_t recvKey[32];
        uint64_t sendSeq;
        ReplayWindow replay;
        LinkStats stats;
        uint64_t createdUs;
        uint64_t lastHeardUs;
        uint64_t lastSentUs;
        bool echoValid;
        uint32_t echoUs;
        uint64_t echoAtUs;
        bool timingValid;
        uint32_t peerTick;
        uint32_t peerConfirmed;
        uint64_t timingAtUs;
        std::deque<Outgoing> out;
        uint16_t sendRelSeq;
        uint16_t recvNext;
        // Messages that arrived ahead of recvNext, by relSeq modulo kRelWindow.
        Held ahead[kRelWindow];
        uint32_t peerHas[ONLINE_MAX_MACHINES];
        int malformed;
        int componentsLeft;
        uint64_t matchGapUs;   // this match's longest silence from the peer
        bool sendNow;
        int byeReason;   // -1 until a BYE came
    };

    struct RoomHuman
    {
        bool used;
        int8_t slot;
        uint8_t vote;
        char name[ONLINE_NAME_MAX + 1];
    };

    struct RoomMachine
    {
        bool present;
        bool connected;
        bool ready;
        bool descAcked;
        uint8_t platform;
        uint8_t loadState;
        uint8_t loadPercent;
        uint8_t lossPercent;
        uint16_t pingMs;
        uint16_t jitterMs;
        char nickname[ONLINE_NAME_MAX + 1];
        RoomHuman human[ONLINE_MAX_LOCAL_HUMANS];   // by the machine's own local index
        int link;                                   // host: its link; -1 for the host itself
    };

    struct LocalHuman
    {
        bool used;
        int localPort;
        char name[ONLINE_NAME_MAX + 1];
    };

    struct StampedAction
    {
        uint32_t tick;
        OnlineTickActions actions;
    };

    struct Nearby
    {
        OnlineNearbyRoom room;
        NetAddr from;
        uint64_t heardUs;
    };

    struct Farewell
    {
        NetPipe* pipe;
        NetAddr addr;
        uint32_t linkId;
        uint8_t key[32];
        uint64_t seq;
        uint8_t reason;
        int left;
        uint64_t nextUs;
    };

    struct Stall
    {
        uint64_t startUs;
        uint32_t ms;
    };

    struct Event
    {
        char text[80];
    };

    void Reset(bool keepNearby);
    void Fail(int error, const char* sentence);
    void Notice(int notice, int machine);
    bool IsHost() const { return m_self == 0 && m_hosting; }
    int HumanCount() const;
    int MachineCount() const;
    int FreeSlot() const;
    void HostAddHuman(int machine, int localIndex, const char* name);
    void HostRemoveHuman(int machine, int localIndex);
    void HostRemoveMachine(int machine, int notice);
    void FillDescIdentity(OnlineMatchDesc* desc) const;
    void DescChanged();
    int ApplySelect(int from, const MsgSelect& select);
    int SelectionOwner(int side) const;
    void RoomChanged() { m_roomDirty = true; }
    void SendRoomState();
    void SendDesc(int machine);
    void BroadcastReliable(uint8_t kind, const Message& m);
    void SendReliableTo(int machine, uint8_t kind, const Message& m);
    void HostCheckStart();
    void HostCheckLoaded();
    void HostCheckVotes(uint64_t now);
    void BeginLoading(uint32_t serial);
    void RemoveDroppedMachines();
    void BackToRoom();
    void HandleMessage(Link& link, const Message& m, uint64_t now, uint64_t arrivedUs);
    void HostJoin(Link& link, const MsgJoin& j);
    void GuestRoomState(const MsgRoomState& r);
    void GuestDesc(Link& link, const MsgDesc& d);
    void RoomTimers(uint64_t now);
    uint8_t ExpectedDelay(uint64_t now);
    uint32_t WorstRttUs() const;
    void LinkGone(int machine, int reason, bool bye);
    void FillState(uint64_t now);
    void PublicTimers(uint64_t now);
    void NextStunServer(uint64_t now);
    void SettlePublic();
    void StopPublic();
    void MapPort(uint16_t port);
    void BuildError(int error, const char* detail);

    Link* NewLink(uint64_t now);
    Link* FindLink(uint32_t linkId);
    Link* MachineLink(int machine);
    void CloseLink(Link& link, bool sayBye, uint8_t reason);
    void Receive(uint64_t now);
    void OnDatagram(NetPipe* pipe, const NetAddr& from, const uint8_t* data, size_t size,
                    uint64_t now);
    void OnHello(const NetAddr& from, const uint8_t* data, size_t size, uint64_t now);
    void OnWelcome(const NetAddr& from, const Welcome& w, uint64_t now);
    void OnRefuse(const Refuse& r);
    void OnDiscover(NetPipe* pipe, const NetAddr& from, const Discover& d, uint64_t now);
    void OnAnnounce(const NetAddr& from, const Announce& a, uint64_t now);
    void OnSealed(const NetAddr& from, const uint8_t* data, size_t size, uint64_t now);
    void HandlePayload(Link& link, const Payload& p, uint64_t now);
    void Queue(Link& link, uint8_t kind, const Message& m, uint64_t deadlineUs);
    void SendDatagram(Link& link, uint64_t now);
    void SendDue(uint64_t now);
    bool LinkDue(const Link& link, uint64_t now) const;
    bool LinkMuted(const Link& link, uint64_t now) const;
    void LinkTimers(uint64_t now);
    void JoinTimers(uint64_t now);
    void SearchTimers(uint64_t now);
    void SendFarewells(uint64_t now);
    void ClosePipe(NetPipe*& pipe);
    void SendPlain(NetPipe* pipe, const NetAddr& to, const uint8_t* data, size_t size);
    void Deliver(Link& link, const Message& m, uint64_t now);
    uint32_t AheadBits(const Link& link) const;

    void BeginMatch(uint32_t startInUs, uint8_t delayTicks, uint64_t now);
    void EndMatchState();
    double SchedulePos(uint64_t now) const;
    void Reanchor(double pos, uint64_t now);
    void RunSchedule(uint64_t now);
    uint32_t Stamped(uint32_t tick, OnlineTickActions* out) const;
    void Stamp(uint32_t tick, uint32_t actions, int barrier, uint32_t seed, uint8_t cpuSlots);
    void OnInput(Link& link, const InputChunk& c);
    void OnHashes(Link& link, const HashChunk& c);
    void OnComponents(Link& link, const ComponentsChunk& c);
    void OnTiming(Link& link, const TimingChunk& t, uint64_t now);
    void HostCheckBarrier(uint64_t now);
    void ApplyResume(const MsgResume& r, uint64_t startUs);
    void ApplyDrop(int machine, uint32_t tick, uint8_t cpuSlots);
    void DropMachine(int machine);
    void RaiseDesync(uint32_t tick, int reporter, const uint64_t* hashes, bool fromPeer);
    uint32_t FirstDifference(uint32_t tick) const;
    void WriteReport();
    void MatchTimers(uint64_t now);
    void Record(const char* fmt, ...);
    uint8_t SlotsOf(int machine) const;
    void CheckOpponents();
    void UpdateProblem(uint64_t now);
    bool SoloMatch() const;
    void NoteStall(uint64_t now, bool stalled);
    uint32_t ReseedFor(uint32_t goals) const;

    SessionEnv* m_env;
    Identity m_id;
    SessionConfig m_config;
    OnlineSettings m_settings;
    OnlineState m_state;

    NetPipe* m_game;
    NetPipe* m_discovery;
    NetPipe* m_search;
    std::vector<NetPipe*> m_closedPipes;

    int m_phase;
    bool m_hosting;
    int m_self;
    int m_roomKind;
    char m_roomAddress[ONLINE_ADDRESS_MAX];
    char m_roomOthers[ONLINE_MAX_OTHER_ADDRESSES][24];
    int m_roomOtherCount;
    uint16_t m_roomPort;
    uint32_t m_roomId;
    uint32_t m_roomRevision;
    bool m_roomDirty;
    uint64_t m_roomSentUs;
    RoomMachine m_room[ONLINE_MAX_MACHINES];
    LocalHuman m_local[ONLINE_MAX_LOCAL_HUMANS];
    char m_nickname[ONLINE_NAME_MAX + 1];
    int m_error;
    char m_lastError[ONLINE_ERROR_MAX];
    uint32_t m_noticeSerial;
    int m_notice;
    int m_noticeMachine;
    char m_noticeName[ONLINE_NAME_MAX + 1];

    // The proposal between matches, the agreed match from START.
    OnlineMatchDesc m_desc;
    OnlineSelection m_selection;
    uint32_t m_descRevision;
    bool m_descValid;
    bool m_starting;   // host: waiting for every ack of the final descriptor
    bool m_rematch;
    uint32_t m_serial;
    uint64_t m_loadingSinceUs;
    int m_loadSentState;
    int m_loadSentPercent;
    uint8_t m_expectedDelay;
    uint64_t m_expectedAtUs;

    struct
    {
        bool active;
        int stage;
        bool resolving;
        NetAddr host;
        uint16_t port;
        KeyPair key;
        uint8_t nonce[16];
        uint64_t startUs;
        uint64_t helloUs;
        uint64_t firstHelloUs;
        char text[ONLINE_ADDRESS_MAX];
    } m_join;

    // A host's search for its outside address.
    struct
    {
        bool active;
        int map;   // PORTMAP_*
        char mapWan[16];
        int stunServer;   // into the server list; past its end once STUN is done
        bool stunResolving;
        NetAddr stunAddr;   // family 0 until resolved
        uint8_t txid[12];
        int stunSends;
        uint64_t stunStartUs;
        uint64_t stunSentUs;
        NetAddr mapped;   // family 0 until STUN answers
        int state;        // ONLINE_PUBLIC_*
        char address[ONLINE_ADDRESS_MAX];
    } m_public;
    uint16_t m_mapPort;   // the port the router was asked to forward, 0 before that

    uint32_t m_nextSeed;   // a seed SetDesc carried, for the next Start only

    std::vector<Link*> m_links;
    std::vector<Farewell> m_farewells;
    std::vector<NetPipe*> m_farewellPipes;
    uint64_t m_helloWindowUs;
    int m_helloCount;
    uint64_t m_discoverWindowUs;
    int m_discoverCount;

    bool m_searching;
    uint32_t m_queryId;
    int m_discoversLeft;
    uint64_t m_discoverNextUs;
    uint64_t m_searchEndUs;
    uint64_t m_searchCloseUs;
    std::vector<Nearby> m_nearby;

    Lockstep m_lock;
    bool m_goReceived;
    uint64_t m_startUs;
    bool m_matchOver;
    uint32_t m_endTick;
    int m_delay;
    bool m_delayAuto;
    uint64_t m_delayGrewUs;
    uint32_t m_loopNext;

    // Rollback: a tick's hashes count once its input is all in; a tick run again replaces them.
    struct PendingHash
    {
        uint32_t tick;
        uint64_t hash;
        int components;
        uint64_t component[kMaxComponents];
        const char* const* names;
    };
    static const int kPendingRing = 64;
    PendingHash m_pending[kPendingRing];
    uint32_t m_committed;   // the next tick whose hash is still pending
    void CommitConfirmed();
    double m_anchorPos;
    uint64_t m_anchorUs;
    double m_rate;   // ticks per microsecond
    float m_timeScale;
    float m_clockRate;
    uint64_t m_frameUs;   // the game's last Update
    bool m_scheduleRunning;
    bool m_stalled;
    uint64_t m_stallStartUs;
    std::vector<Stall> m_stalls;
    std::vector<uint64_t> m_stallStarts;   // host's auto-delay window
    double m_advance;
    uint64_t m_advanceUs;
    uint64_t m_slowSinceUs;
    int m_slowMachine;
    std::vector<StampedAction> m_stamps;
    struct
    {
        bool active;
        int kind;
        uint32_t tick;
        bool doneSent;
        bool resumeKnown;
        uint64_t resumeUs;
    } m_barrier;
    int m_barrierDoneKind[ONLINE_MAX_MACHINES];
    uint32_t m_barrierDoneTick[ONLINE_MAX_MACHINES];
    uint32_t m_goals;
    uint32_t m_skipPresentation;
    bool m_opponentsLeft;
    bool m_opponentsAnswered;
    int m_desync;
    uint32_t m_desyncTick;
    int m_desyncCount;
    char m_desyncComponent[32];
    char m_desyncReport[ONLINE_PATH_MAX];
    uint64_t m_desyncHashes[ONLINE_MAX_MACHINES];
    bool m_reportPending;
    std::string m_reportText;
    uint32_t m_reportSerial = 0;
    std::vector<Event> m_events;
    int m_componentsCount;
    uint32_t m_componentsTick;
    uint64_t m_componentsHash[kMaxComponents];
    LocalPad m_ports[4];
    uint16_t m_edgeButtons[4];
    bool m_edgePending;
    uint64_t m_edgeUs;
    uint32_t m_edgeTick;
    int m_inputToEffectMs;

    uint64_t m_voteDeadlineUs;

    int m_problem;
    int m_problemMachine;
    float m_problemSeconds;
    uint64_t m_problemHoldUs;

    Payload* m_payload;
};
}   // namespace online

#endif   // PORT_ONLINE_SESSION_H
