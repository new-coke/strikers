// The wire format, keys and addresses, then the decoders on truncated, mutated and random input.

#include "../src/platform/online/pipe.h"
#include "../src/platform/online/seal.h"
#include "../src/platform/online/stun.h"
#include "../src/platform/online/wire.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <set>
#include <vector>

using namespace online;

bool OnlineFuzzOne(const uint8_t* data, size_t size);
extern const char* g_onlineFuzzWhy;

namespace
{
int g_failures;

#define CHECK(cond)                                                     \
    do                                                                  \
    {                                                                   \
        if (!(cond))                                                    \
        {                                                               \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                               \
        }                                                               \
    } while (0)

std::mt19937_64 g_rng(20260922);

uint32_t r32() { return (uint32_t)g_rng(); }

// Every message kind with every field set to something in range.
std::vector<Message> SampleMessages()
{
    std::vector<Message> out;
    Message m;

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_JOIN;
    m.u.join.protocol = ONLINE_PROTOCOL_VERSION;
    m.u.join.platform = ONLINE_PLATFORM_LINUX;
    m.u.join.binaryFamily = FAMILY_LINUX_X64;
    m.u.join.build = 0xDEADBEEF;
    m.u.join.dataFamily = 0x01020304;
    std::snprintf(m.u.join.version, sizeof m.u.join.version, "1.6.0-3-gabcdef");
    std::snprintf(m.u.join.nickname, sizeof m.u.join.nickname, "Zoë");
    m.u.join.humanCount = 2;
    std::snprintf(m.u.join.human[0], sizeof m.u.join.human[0], "Zoë");
    std::snprintf(m.u.join.human[1], sizeof m.u.join.human[1], "Sam");
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_JOIN_ACCEPT;
    m.u.joinAccept.machine = 3;
    m.u.joinAccept.roomId = 0x11223344;
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_JOIN_REFUSE;
    m.u.joinRefuse.reason = ONLINE_ERR_DATA_FAMILY;
    m.u.joinRefuse.hostPlatform = ONLINE_PLATFORM_SWITCH;
    m.u.joinRefuse.hostDataRegion = 1;
    std::snprintf(m.u.joinRefuse.hostVersion, sizeof m.u.joinRefuse.hostVersion, "1.6.1");
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_ROOM_STATE;
    m.u.roomState.revision = 77;
    m.u.roomState.phase = ONLINE_PHASE_POST_MATCH;
    m.u.roomState.host = 0;
    m.u.roomState.delayTicks = 4;
    m.u.roomState.voteSeconds = 60;
    m.u.roomState.matchSerial = 9;
    m.u.roomState.selection.epoch = 7;
    m.u.roomState.selection.stage = 1;
    m.u.roomState.selection.phase[0] = 2;
    m.u.roomState.selection.phase[1] = 3;
    m.u.roomState.selection.optionsOpen = 1;
    for (int i = 0; i < ONLINE_MAX_MACHINES; i++)
        for (int h = 0; h < ONLINE_MAX_LOCAL_HUMANS; h++)
            m.u.roomState.machine[i].human[h].slot = -1;
    for (int i = 0; i < 3; i++)
    {
        WireMachine& w = m.u.roomState.machine[i];
        w.present = 1;
        w.platform = (uint8_t)(1 + i);
        w.flags = (uint8_t)(MACHINE_CONNECTED | (i == 1 ? MACHINE_READY | MACHINE_DESC_ACKED : 0));
        w.loadState = ONLINE_LOAD_PIPELINES;
        w.loadPercent = 42;
        w.lossPercent = 3;
        w.pingMs = (uint16_t)(20 * i);
        w.jitterMs = (uint16_t)i;
        std::snprintf(w.nickname, sizeof w.nickname, "M%d", i);
        w.humanCount = 1;
        w.human[i == 2 ? 1 : 0].slot = (int8_t)i;
        w.human[i == 2 ? 1 : 0].vote = ONLINE_VOTE_REMATCH;
        std::snprintf(w.human[i == 2 ? 1 : 0].name, sizeof w.human[0].name, "H%d", i);
    }
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_DESC;
    m.u.desc.revision = 5;
    {
        OnlineMatchDesc d;
        PortOnlineDescDefault(&d);
        d.seed = 1234;
        PortOnlineDescEncode(&d, m.u.desc.desc);
    }
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_DESC_ACK;
    m.u.descAck.revision = 5;
    m.u.descAck.hash = 0x0123456789ABCDEFull;
    m.u.descAck.accepted = 1;
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_LOCAL_PLAYER;
    m.u.localPlayer.op = 1;
    m.u.localPlayer.localIndex = 1;
    std::snprintf(m.u.localPlayer.name, sizeof m.u.localPlayer.name, "Couch");
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_LOCAL_PLAYER;
    m.u.localPlayer.op = 2;
    m.u.localPlayer.localIndex = 0;
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_READY;
    m.u.ready = 1;
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_START;
    m.u.start.matchSerial = 3;
    m.u.start.descRevision = 8;
    m.u.start.descHash = 0xFEEDFACECAFEBEEFull;
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_LOAD_PROGRESS;
    m.u.loadProgress.matchSerial = 3;
    m.u.loadProgress.loadState = ONLINE_LOAD_LOADING;
    m.u.loadProgress.percent = 70;
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_GO;
    m.u.go.matchSerial = 3;
    m.u.go.startInUs = 300000;
    m.u.go.delayTicks = 5;
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_BARRIER_DONE;
    m.u.barrierDone.matchSerial = 3;
    m.u.barrierDone.kind = 2;
    m.u.barrierDone.tick = 1500;
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_RESUME;
    m.u.resume.matchSerial = 3;
    m.u.resume.kind = 1;
    m.u.resume.delayTicks = 3;
    m.u.resume.actions = ONLINE_ACTION_RESUME | ONLINE_ACTION_RESEED;
    m.u.resume.tick = 3;
    m.u.resume.startInUs = 150000;
    m.u.resume.seed = 0x13579BDF;
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_DROP;
    m.u.drop.matchSerial = 3;
    m.u.drop.machine = 2;
    m.u.drop.cpuSlots = 0x0A;
    m.u.drop.tick = 4000;
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_DESYNC;
    m.u.desync.matchSerial = 3;
    m.u.desync.tick = 777;
    m.u.desync.reporter = 1;
    m.u.desync.hash[0] = 1;
    m.u.desync.hash[1] = 2;
    out.push_back(m);

    std::memset(&m, 0, sizeof m);
    m.kind = MSG_VOTE;
    m.u.vote.matchSerial = 3;
    m.u.vote.slot = 2;
    m.u.vote.vote = ONLINE_VOTE_CHANGE_TEAMS;
    out.push_back(m);

    const uint8_t serialKinds[3] = { MSG_VOTE_RESULT, MSG_ABORT, MSG_DELAY };
    const uint8_t values[3] = { ONLINE_VOTE_BACK_TO_ROOM, ABORT_DESYNC, 8 };
    for (int i = 0; i < 3; i++)
    {
        std::memset(&m, 0, sizeof m);
        m.kind = serialKinds[i];
        m.u.serialByte.matchSerial = 3;
        m.u.serialByte.value = values[i];
        out.push_back(m);
    }
    m = {};
    m.kind = MSG_SKIP;
    m.u.skip = { 3, 1502 };
    out.push_back(m);
    m = {};
    m.kind = MSG_SELECT;
    m.u.select = { 4, ONLINE_SELECT_TEAM, 1, 2, 1, 3 };
    out.push_back(m);
    return out;
}

void TestMessages()
{
    const std::vector<Message> all = SampleMessages();
    std::set<int> kinds;
    for (const Message& m : all)
    {
        uint8_t body[kMaxMessageBody];
        const size_t n = EncodeMessageBody(m, body);
        CHECK(n > 0);
        Message back;
        std::memset(&back, 0, sizeof back);
        CHECK(DecodeMessageBody(m.kind, body, n, &back));
        uint8_t again[kMaxMessageBody];
        CHECK(EncodeMessageBody(back, again) == n && std::memcmp(body, again, n) == 0);
        // One byte short or long is malformed.
        CHECK(!DecodeMessageBody(m.kind, body, n - 1, &back));
        kinds.insert(m.kind);
    }
    CHECK(kinds.size() == 21);

    // Field-level checks the decoders make.
    Message m = all[0];
    uint8_t body[kMaxMessageBody];
    size_t n = EncodeMessageBody(m, body);
    Message out;
    body[48] = 3;   // three humans
    CHECK(!DecodeMessageBody(MSG_JOIN, body, n, &out));
    n = EncodeMessageBody(m, body);
    body[32] = 0x07;   // a control character in the nickname
    CHECK(!DecodeMessageBody(MSG_JOIN, body, n, &out));

    Message g;
    std::memset(&g, 0, sizeof g);
    g.kind = MSG_GO;
    g.u.go.matchSerial = 1;
    g.u.go.startInUs = 100000;
    g.u.go.delayTicks = 9;
    n = EncodeMessageBody(g, body);
    CHECK(!DecodeMessageBody(MSG_GO, body, n, &out));
    // No delay is the rollback default.
    g.u.go.delayTicks = 0;
    n = EncodeMessageBody(g, body);
    CHECK(DecodeMessageBody(MSG_GO, body, n, &out) && out.u.go.delayTicks == 0);

    // A pause (barrier 4) resumes without a reseed; 5 names no barrier.
    Message r;
    std::memset(&r, 0, sizeof r);
    r.kind = MSG_RESUME;
    r.u.resume.matchSerial = 1;
    r.u.resume.kind = 4;
    r.u.resume.delayTicks = 2;
    r.u.resume.actions = ONLINE_ACTION_RESUME;
    r.u.resume.tick = 500;
    r.u.resume.startInUs = 100000;
    n = EncodeMessageBody(r, body);
    CHECK(DecodeMessageBody(MSG_RESUME, body, n, &out) && out.u.resume.kind == 4 &&
          out.u.resume.actions == ONLINE_ACTION_RESUME);
    r.u.resume.kind = 5;
    n = EncodeMessageBody(r, body);
    CHECK(!DecodeMessageBody(MSG_RESUME, body, n, &out));
    Message bd;
    std::memset(&bd, 0, sizeof bd);
    bd.kind = MSG_BARRIER_DONE;
    bd.u.barrierDone.matchSerial = 1;
    bd.u.barrierDone.kind = 4;
    bd.u.barrierDone.tick = 500;
    n = EncodeMessageBody(bd, body);
    CHECK(DecodeMessageBody(MSG_BARRIER_DONE, body, n, &out) && out.u.barrierDone.kind == 4);
    bd.u.barrierDone.kind = 0;
    n = EncodeMessageBody(bd, body);
    CHECK(!DecodeMessageBody(MSG_BARRIER_DONE, body, n, &out));

    Message extra = {};
    extra.kind = MSG_SKIP;
    extra.u.skip.matchSerial = 1;
    n = EncodeMessageBody(extra, body);
    CHECK(!DecodeMessageBody(MSG_SKIP, body, n, &out));
    extra.u.skip.presentation = 1;
    n = EncodeMessageBody(extra, body);
    CHECK(DecodeMessageBody(MSG_SKIP, body, n, &out));
    extra = {};
    extra.kind = MSG_SELECT;
    extra.u.select = { 1, ONLINE_SELECT_TEAM, 1, 8, 3, 3 };
    n = EncodeMessageBody(extra, body);
    CHECK(DecodeMessageBody(MSG_SELECT, body, n, &out));
    for (unsigned offset = 4; offset < 9; ++offset)
    {
        n = EncodeMessageBody(extra, body);
        body[offset] = 255;
        CHECK(!DecodeMessageBody(MSG_SELECT, body, n, &out));
    }

    extra.u.select.op = ONLINE_SELECT_OPTIONS;
    extra.u.select.phase = 1;
    n = EncodeMessageBody(extra, body);
    CHECK(DecodeMessageBody(MSG_SELECT, body, n, &out));
    n = EncodeMessageBody(all[3], body);
    CHECK(DecodeMessageBody(MSG_ROOM_STATE, body, n, &out) &&
          out.u.roomState.selection.optionsOpen == 1);
    body[275] = 2;
    CHECK(!DecodeMessageBody(MSG_ROOM_STATE, body, n, &out));

    // ROOM_STATE: two humans on one slot, and a missing host.
    Message rs = all[3];
    rs.u.roomState.machine[1].human[0].slot = 0;
    n = EncodeMessageBody(rs, body);
    CHECK(!DecodeMessageBody(MSG_ROOM_STATE, body, n, &out));
    rs = all[3];
    rs.u.roomState.host = 3;
    n = EncodeMessageBody(rs, body);
    CHECK(!DecodeMessageBody(MSG_ROOM_STATE, body, n, &out));
}

void TestPlain()
{
    uint8_t buf[kHelloSize];
    Discover d = { 0x12345678 };
    CHECK(EncodeDiscover(d, buf) == kDiscoverSize);
    Discover d2;
    CHECK(DecodeDiscover(buf, kDiscoverSize, &d2) && d2.queryId == d.queryId);
    CHECK(PlainClass(buf, kDiscoverSize, NULL) == DG_DISCOVER);
    CHECK(!DecodeDiscover(buf, kDiscoverSize - 1, &d2));
    buf[4] = ONLINE_PROTOCOL_VERSION + 1;   // another protocol
    CHECK(!DecodeDiscover(buf, kDiscoverSize, &d2));

    Announce a;
    std::memset(&a, 0, sizeof a);
    a.queryId = 9;
    a.gamePort = 41210;
    a.platform = ONLINE_PLATFORM_STEAM_DECK;
    a.machines = 2;
    a.humans = 3;
    a.flags = 1;
    a.build = 1;
    a.dataFamily = 2;
    std::snprintf(a.hostName, sizeof a.hostName, "Deck");
    CHECK(EncodeAnnounce(a, buf) == kAnnounceSize);
    Announce a2;
    CHECK(DecodeAnnounce(buf, kAnnounceSize, &a2));
    CHECK(a2.gamePort == 41210 && a2.humans == 3 && std::strcmp(a2.hostName, "Deck") == 0);

    Hello h;
    for (int i = 0; i < 32; i++)
        h.publicKey[i] = (uint8_t)i;
    for (int i = 0; i < 16; i++)
        h.nonce[i] = (uint8_t)(100 + i);
    CHECK(EncodeHello(h, buf) == kHelloSize);
    Hello h2;
    CHECK(DecodeHello(buf, kHelloSize, &h2) && std::memcmp(&h, &h2, sizeof h) == 0);
    // A HELLO of another protocol is still recognised, so the host can refuse it by version.
    buf[4] = 9;
    uint16_t protocol = 0;
    CHECK(PlainClass(buf, kHelloSize, &protocol) == DG_HELLO && protocol == 9);

    Welcome w;
    std::memcpy(w.publicKey, h.publicKey, 32);
    std::memcpy(w.nonce, h.nonce, 16);
    w.linkId = 0xABCDEF01;
    CHECK(EncodeWelcome(w, buf) == kWelcomeSize);
    Welcome w2;
    CHECK(DecodeWelcome(buf, kWelcomeSize, &w2) && w2.linkId == w.linkId);
    w.linkId = 0;
    EncodeWelcome(w, buf);
    CHECK(!DecodeWelcome(buf, kWelcomeSize, &w2));

    Refuse r;
    r.protocol = 7;
    std::memcpy(r.nonce, h.nonce, 16);
    r.reason = ONLINE_ERR_ROOM_FULL;
    CHECK(EncodeRefuse(r, buf) == kRefuseSize);
    Refuse r2;
    CHECK(DecodeRefuse(buf, kRefuseSize, &r2) && r2.protocol == 7 &&
          r2.reason == ONLINE_ERR_ROOM_FULL);
    buf[24] = ONLINE_ERR_NETWORK;
    CHECK(!DecodeRefuse(buf, kRefuseSize, &r2));
}

// A payload with one of every chunk and a run of messages, as a sender packs it.
std::vector<uint8_t> SamplePayload(int variant)
{
    uint8_t buf[kMaxPayload];
    ChunkWriter w(buf, sizeof buf);
    w.RelAck((uint16_t)(7 + variant), 0x80000001u);
    TimingChunk t = { 1000u + (uint32_t)variant, 900, 50, 123, 120 };
    w.Timing(t);
    w.InputAck(3, 1, 130);
    uint8_t recs[4][kRecordSize];
    std::memset(recs, 0, sizeof recs);
    for (int i = 0; i < 4; i++)
    {
        PadBytes p = { (uint16_t)(0x0100 | i), 40, -40, 2, -2, 10, 200 };
        RecordPut(recs[i], 0, p);
        RecordPut(recs[i], 1, p);
        recs[i][16] = 3;
    }
    w.Input(3, 0, 125, 4, &recs[0][0]);
    const uint64_t hashes[3] = { 1, 2, 3 };
    w.Hash(3, 0, 120, 3, hashes);
    const std::vector<Message> msgs = SampleMessages();
    for (size_t i = 0; i < msgs.size(); i++)
    {
        if ((int)(i % 3) != variant % 3)
            continue;
        uint8_t body[kMaxMessageBody];
        const size_t n = EncodeMessageBody(msgs[i], body);
        w.Reliable(msgs[i].kind, (uint16_t)(i + 1), body, n);
    }
    const uint64_t comps[4] = { 9, 8, 7, 6 };
    w.Components(3, 0, 100, 4, comps);
    if (variant == 1)
        w.Bye(0);
    return std::vector<uint8_t>(buf, buf + w.Size());
}

void TestPayload()
{
    for (int v = 0; v < 3; v++)
    {
        const std::vector<uint8_t> p = SamplePayload(v);
        static Payload out;
        CHECK(ParsePayload(p.data(), p.size(), &out));
        CHECK(out.hasTiming && out.timing.tick == 123 && out.hasRelAck &&
              out.relAck.bitmap == 0x80000001u);
        CHECK(out.inputCount == 1 && out.input[0].count == 4 && out.input[0].firstTick == 125);
        CHECK(out.hashCount == 1 && out.hash[0].count == 3 && out.hash[0].hash[2] == 3);
        CHECK(out.componentsCount == 1 && out.components[0].hash[3] == 6);
        CHECK(out.messageCount >= 6);
        CHECK(out.hasBye == (v == 1));
        CHECK(OnlineFuzzOne(p.data(), p.size()));
    }
    // An empty payload is well formed; two TIMING chunks are not.
    static Payload out;
    CHECK(ParsePayload(NULL, 0, &out));
    uint8_t twice[64];
    ChunkWriter w(twice, sizeof twice);
    TimingChunk t = {};
    w.Timing(t);
    w.Timing(t);
    CHECK(!ParsePayload(twice, w.Size(), &out));
    // An unknown chunk kind.
    uint8_t unknown[4] = { 0x08, 1, 0, 0 };
    CHECK(!ParsePayload(unknown, sizeof unknown, &out));
}

void TestGoldenBytes()
{
    Message join = {};
    join.kind = MSG_JOIN;
    join.u.join.protocol = 3;
    join.u.join.platform = ONLINE_PLATFORM_LINUX;
    join.u.join.binaryFamily = FAMILY_LINUX_X64;
    join.u.join.build = 0x12345678;
    join.u.join.dataFamily = 0x90ABCDEF;
    std::strcpy(join.u.join.version, "1.6.0");
    std::strcpy(join.u.join.nickname, "Host");
    join.u.join.humanCount = 1;
    std::strcpy(join.u.join.human[0], "Alex");
    const uint8_t expectedJoin[82] = { 0x03, 0x00, 0x03, 0x02, 0x78, 0x56, 0x34, 0x12, 0xEF, 0xCD,
                                       0xAB, 0x90, 0x00, 0x00, 0x00, 0x00, '1',  '.',  '6',  '.',
                                       '0',  0,    0,    0,    0,    0,    0,    0,    0,    0,
                                       0,    0,    'H',  'o',  's',  't',  0,    0,    0,    0,
                                       0,    0,    0,    0,    0,    0,    0,    0,    1,    0,
                                       'A',  'l',  'e',  'x',  0,    0,    0,    0,    0,    0,
                                       0,    0,    0,    0,    0,    0 };
    uint8_t bytes[kMaxMessageBody];
    CHECK(EncodeMessageBody(join, bytes) == sizeof expectedJoin);
    CHECK(std::memcmp(bytes, expectedJoin, sizeof expectedJoin) == 0);

    uint8_t record[kRecordSize] = {};
    const PadBytes pad = { 0x0100, 3, -3, 100, -100, 0x12, 0x34 };
    RecordPut(record, 0, pad);
    record[16] = 1;
    ChunkWriter writer(bytes, sizeof bytes);
    CHECK(writer.Input(0x12345678, 2, 0x10203040, 1, record));
    const uint8_t expectedInput[] = { 0x03, 0x1B, 0x00, 0x78, 0x56, 0x34, 0x12, 0x02, 0x01, 0x40,
                                      0x30, 0x20, 0x10, 0x00, 0x01, 0x02, 0xFC, 0x64, 0x9C, 0x12,
                                      0x34, 0,    0,    0,    0,    0,    0,    0,    0,    1 };
    CHECK(writer.Size() == sizeof expectedInput);
    CHECK(std::memcmp(bytes, expectedInput, sizeof expectedInput) == 0);
}

void TestRecords()
{
    CHECK(PortSimQuantizeStick(0) == 0 && PortSimQuantizeStick(2) == 0 &&
          PortSimQuantizeStick(-2) == 0);
    CHECK(PortSimQuantizeStick(3) == 2 && PortSimQuantizeStick(-3) == -4 &&
          PortSimQuantizeStick(127) == 126);
    CHECK(PortSimQuantizeStick(-128) == -128);
    for (int v = -128; v < 128; v++)
        CHECK((PortSimQuantizeStick((int8_t)v) & 1) == 0);

    uint8_t rec[kRecordSize] = {};
    PadBytes p = { 0x1F7F, 5, -5, 100, -100, 1, 255 };
    RecordPut(rec, 0, p);
    rec[16] = 1;
    const PadBytes back = RecordGet(rec, 0);
    CHECK(back.buttons == 0x1F7F);
    CHECK(back.stickX == 4 && back.stickY == -6 && back.substickX == 100 && back.triggerR == 255);
    CHECK(RecordValid(rec, 1));
    CHECK(!RecordValid(rec, 0));   // bytes for a human the machine does not have
    uint8_t bad[kRecordSize];
    std::memcpy(bad, rec, sizeof bad);
    bad[1] |= 0x80;   // a bit no PAD button uses
    CHECK(!RecordValid(bad, 1));
    std::memcpy(bad, rec, sizeof bad);
    bad[2] = 7;   // odd stick
    CHECK(!RecordValid(bad, 1));
    std::memcpy(bad, rec, sizeof bad);
    bad[16] = 0x41;   // reserved flag bit
    CHECK(!RecordValid(bad, 1));
    std::memcpy(bad, rec, sizeof bad);
    bad[16] = 0x3D;   // view lag bits are allowed
    CHECK(RecordValid(bad, 1));
}

void TestNames()
{
    uint8_t f[ONLINE_NAME_MAX];
    char out[ONLINE_NAME_MAX + 1];
    PutName(f, "  Alex  ");
    CHECK(GetName(f, out) && std::strcmp(out, "Alex") == 0);
    PutName(f, "0123456789abcdefXYZ");
    CHECK(GetName(f, out) && std::strlen(out) == 16);
    std::memset(f, 0, sizeof f);
    f[0] = 'A';
    f[2] = 'B';   // a byte after the terminator
    CHECK(!GetName(f, out));
    std::memset(f, 0, sizeof f);
    f[0] = 'A';
    f[1] = ' ';   // not trimmed
    CHECK(!GetName(f, out));
    std::memset(f, 0, sizeof f);
    f[0] = 0xC3;   // a cut UTF-8 sequence
    CHECK(!GetName(f, out));
    std::memset(f, 0, sizeof f);
    CHECK(GetName(f, out) && out[0] == '\0');
}

void TestSeal()
{
    KeyPair g;
    KeyPair h;
    for (int i = 0; i < 32; i++)
    {
        g.secret[i] = (uint8_t)r32();
        h.secret[i] = (uint8_t)r32();
    }
    KeyPairFromSecret(&g);
    KeyPairFromSecret(&h);
    uint8_t nonce[16];
    for (int i = 0; i < 16; i++)
        nonce[i] = (uint8_t)i;
    uint8_t gA[32], hA[32], gB[32], hB[32];
    CHECK(DeriveLinkKeys(g.secret, h.publicKey, g.publicKey, h.publicKey, nonce, 42, gA, hA));
    CHECK(DeriveLinkKeys(h.secret, g.publicKey, g.publicKey, h.publicKey, nonce, 42, gB, hB));
    CHECK(std::memcmp(gA, gB, 32) == 0 && std::memcmp(hA, hB, 32) == 0);
    CHECK(std::memcmp(gA, hA, 32) != 0);
    // Another link id or nonce gives other keys.
    uint8_t gC[32], hC[32];
    CHECK(DeriveLinkKeys(g.secret, h.publicKey, g.publicKey, h.publicKey, nonce, 43, gC, hC));
    CHECK(std::memcmp(gA, gC, 32) != 0);
    // A low-order point makes an all-zero secret, which is refused.
    uint8_t zero[32] = {};
    CHECK(!DeriveLinkKeys(g.secret, zero, g.publicKey, zero, nonce, 42, gC, hC));

    uint8_t plain[300];
    for (size_t i = 0; i < sizeof plain; i++)
        plain[i] = (uint8_t)(i * 7);
    uint8_t dg[kMaxDatagram];
    const size_t n = SealDatagram(gA, 42, 5, plain, sizeof plain, dg);
    CHECK(n == kSealedHeader + sizeof plain + kTagSize);
    SealedHeader sh;
    CHECK(DecodeSealedHeader(dg, n, &sh) && sh.linkId == 42 && sh.seq == 5);
    uint8_t back[kMaxDatagram];
    size_t bn = 0;
    CHECK(OpenDatagram(gB, dg, n, back, &bn) && bn == sizeof plain &&
          std::memcmp(back, plain, bn) == 0);
    CHECK(!OpenDatagram(hB, dg, n, back, &bn));   // the other direction's key
    // Any flipped bit, header included, fails the tag.
    int opened = 0;
    for (size_t i = 0; i < n; i++)
    {
        uint8_t copy[kMaxDatagram];
        std::memcpy(copy, dg, n);
        copy[i] ^= (uint8_t)(1u << (i % 8));
        opened += OpenDatagram(gB, copy, n, back, &bn) ? 1 : 0;
    }
    CHECK(opened == 0);
    CHECK(!OpenDatagram(gB, dg, n - 1, back, &bn));
}

void TestReplay()
{
    ReplayWindow w;
    std::set<uint64_t> seen;
    uint64_t top = 0;
    CHECK(!w.Fresh(0));
    for (int i = 0; i < 200000; i++)
    {
        uint64_t seq;
        const uint32_t pick = r32() % 10;
        if (pick < 6)
            seq = top + 1 + r32() % 3;
        else if (pick < 9)
            seq = top > 70 ? top - r32() % 70 : 1 + r32() % 70;
        else
            seq = top + 1 + r32() % 100;
        if (seq == 0)
            continue;
        const bool model = seen.count(seq) == 0 && (seq > top || top - seq <= 64);
        CHECK(w.Fresh(seq) == model);
        if (model)
        {
            w.Accept(seq);
            seen.insert(seq);
            if (seq > top)
                top = seq;
        }
    }
}

void TestAddresses()
{
    char host[ONLINE_ADDRESS_MAX];
    uint16_t port = 0;
    CHECK(AddrSplit("192.168.1.20", host, sizeof host, &port, 41210) && port == 41210 &&
          std::strcmp(host, "192.168.1.20") == 0);
    CHECK(AddrSplit(" 10.0.0.2:5000 ", host, sizeof host, &port, 41210) && port == 5000);
    CHECK(AddrSplit("[fe80::1]:41211", host, sizeof host, &port, 41210) && port == 41211 &&
          std::strcmp(host, "fe80::1") == 0);
    CHECK(AddrSplit("2001:db8::7", host, sizeof host, &port, 41210) && port == 41210 &&
          std::strcmp(host, "2001:db8::7") == 0);
    CHECK(AddrSplit("example.org:41210", host, sizeof host, &port, 1) &&
          std::strcmp(host, "example.org") == 0);
    CHECK(!AddrSplit("host:0", host, sizeof host, &port, 1));
    CHECK(!AddrSplit("host:65536", host, sizeof host, &port, 1));
    CHECK(!AddrSplit("host:12x", host, sizeof host, &port, 1));
    CHECK(!AddrSplit("a b", host, sizeof host, &port, 1));
    CHECK(!AddrSplit("", host, sizeof host, &port, 1));
    CHECK(!AddrSplit("[::1", host, sizeof host, &port, 1));
    CHECK(!AddrSplit("http://x", host, sizeof host, &port, 1));

    NetAddr a;
    char text[ONLINE_ADDRESS_MAX];
    CHECK(AddrParseNumeric("192.168.0.1", 7, &a) && a.family == 4 && a.ip[3] == 1);
    AddrFormat(a, text, sizeof text);
    CHECK(std::strcmp(text, "192.168.0.1:7") == 0);
    CHECK(AddrIsLanSource(a));
    CHECK(AddrParseNumeric("2001:db8:0:0:0:0:0:1", 41210, &a) && a.family == 6);
    AddrFormat(a, text, sizeof text);
    CHECK(std::strcmp(text, "[2001:db8::1]:41210") == 0);
    CHECK(!AddrIsLanSource(a));
    CHECK(AddrParseNumeric("::ffff:10.1.2.3", 1, &a) && a.family == 4 && a.ip[0] == 10 &&
          a.ip[3] == 3);
    CHECK(AddrParseNumeric("::1", 1, &a) && a.family == 6 && AddrIsLanSource(a));
    CHECK(!AddrParseNumeric("256.1.1.1", 1, &a));
    CHECK(!AddrParseNumeric("1.2.3", 1, &a));
    CHECK(!AddrParseNumeric("1::2::3", 1, &a));
    CHECK(!AddrParseNumeric("example.org", 1, &a));
    CHECK(AddrParseNumeric("8.8.8.8", 1, &a) && !AddrIsLanSource(a));
    CHECK(AddrParseNumeric("172.31.0.1", 1, &a) && AddrIsLanSource(a));
    CHECK(AddrParseNumeric("172.32.0.1", 1, &a) && !AddrIsLanSource(a));
    CHECK(AddrParseNumeric("100.64.0.1", 1, &a) && AddrIsLanSource(a));   // carrier-grade NAT
    CHECK(AddrParseNumeric("100.127.255.254", 1, &a) && AddrIsLanSource(a));
    CHECK(AddrParseNumeric("100.128.0.1", 1, &a) && !AddrIsLanSource(a));
    CHECK(AddrParseNumeric("100.63.255.255", 1, &a) && !AddrIsLanSource(a));
    CHECK(AddrParseNumeric("fd7a:115c:a1e0::1", 1, &a) && AddrIsLanSource(a));
}

// Every truncation and every single-byte change of valid datagrams and payloads, then random bytes.
void TestRobustness()
{
    std::vector<std::vector<uint8_t>> seeds;
    for (int v = 0; v < 3; v++)
        seeds.push_back(SamplePayload(v));
    {
        uint8_t b[kHelloSize];
        Discover d = { 5 };
        EncodeDiscover(d, b);
        seeds.push_back(std::vector<uint8_t>(b, b + kDiscoverSize));
        Hello h;
        std::memset(&h, 7, sizeof h);
        EncodeHello(h, b);
        seeds.push_back(std::vector<uint8_t>(b, b + kHelloSize));
        Announce a;
        std::memset(&a, 0, sizeof a);
        a.gamePort = 1;
        a.machines = 1;
        std::snprintf(a.hostName, sizeof a.hostName, "Host");
        EncodeAnnounce(a, b);
        seeds.push_back(std::vector<uint8_t>(b, b + kAnnounceSize));
    }
    for (const Message& m : SampleMessages())
    {
        uint8_t body[kMaxMessageBody + 1];
        body[0] = m.kind;
        const size_t n = EncodeMessageBody(m, body + 1);
        seeds.push_back(std::vector<uint8_t>(body, body + 1 + n));
    }

    long cases = 0;
    long bad = 0;
    auto check = [&](const uint8_t* p, size_t n) {
        if (OnlineFuzzOne(p, n))
            return 0;
        if (bad < 5)
        {
            std::printf("  round trip broken (%s), %zu bytes:", g_onlineFuzzWhy, n);
            for (size_t i = 0; i < n && i < 96; i++)
                std::printf(" %02x", p[i]);
            std::printf("\n");
        }
        return 1;
    };
    for (const std::vector<uint8_t>& s : seeds)
    {
        for (size_t len = 0; len <= s.size(); len++, cases++)
            bad += check(s.data(), len);
        std::vector<uint8_t> m = s;
        for (size_t i = 0; i < m.size(); i++)
        {
            const uint8_t keep = m[i];
            for (int v = 0; v < 256; v++, cases++)
            {
                m[i] = (uint8_t)v;
                bad += check(m.data(), m.size());
            }
            m[i] = keep;
        }
    }

    // ONLINE_FUZZ_ITERATIONS lengthens the random run.
    const char* env = std::getenv("ONLINE_FUZZ_ITERATIONS");
    const long iterations = env != NULL ? std::atol(env) : 2000000;
    std::vector<uint8_t> buf(kMaxPayload);
    for (long i = 0; i < iterations; i++, cases++)
    {
        size_t n;
        const uint32_t mode = r32() % 4;
        if (mode == 0)
        {
            // Pure noise.
            n = r32() % (kMaxPayload + 1);
            for (size_t k = 0; k < n; k++)
                buf[k] = (uint8_t)r32();
        }
        else
        {
            // Chunk headers that line up, with bodies of noise or of a sample.
            n = 0;
            while (n + 3 < kMaxPayload && r32() % 6 != 0)
            {
                const uint8_t kinds[] = { CH_TIMING,      CH_RELACK,     CH_INPUT, CH_INPUT_ACK,
                                          CH_HASH,        CH_COMPONENTS, CH_BYE,   MSG_JOIN,
                                          MSG_ROOM_STATE, MSG_DESC,      MSG_GO,   MSG_RESUME,
                                          MSG_SKIP,       MSG_SELECT };
                const uint8_t kind = kinds[r32() % sizeof kinds];
                size_t len = r32() % 64;
                if (kind == CH_INPUT)
                    len = 10 + kRecordSize * (r32() % 4);
                if (kind == MSG_ROOM_STATE)
                    len = 276;
                if (n + 3 + len > kMaxPayload)
                    break;
                buf[n] = kind;
                Put16(&buf[n + 1], (uint16_t)len);
                for (size_t k = 0; k < len; k++)
                    buf[n + 3 + k] =
                        mode == 1 ? (uint8_t)r32() : (uint8_t)(r32() % 3 == 0 ? r32() : 0);
                n += 3 + len;
            }
        }
        bad += check(buf.data(), n);
    }
    std::printf("robustness: %ld inputs, %ld broke an encode round trip\n", cases, bad);
    CHECK(bad == 0);
}
}   // namespace

// RFC 5769's IPv4 Binding response: 192.0.2.1:32853, after SOFTWARE and before MESSAGE-INTEGRITY.
const uint8_t kStunVector[] = {
    0x01, 0x01, 0x00, 0x3c, 0x21, 0x12, 0xa4, 0x42, 0xb7, 0xe7, 0xa7, 0x01, 0xbc, 0x34, 0xd6, 0x86,
    0xfa, 0x87, 0xdf, 0xae, 0x80, 0x22, 0x00, 0x0b, 0x74, 0x65, 0x73, 0x74, 0x20, 0x76, 0x65, 0x63,
    0x74, 0x6f, 0x72, 0x20, 0x00, 0x20, 0x00, 0x08, 0x00, 0x01, 0xa1, 0x47, 0xe1, 0x12, 0xa6, 0x43,
    0x00, 0x08, 0x00, 0x14, 0x2b, 0x91, 0xf5, 0x99, 0xfd, 0x9e, 0x90, 0xc3, 0x8c, 0x74, 0x89, 0xf9,
    0x2a, 0xf9, 0xba, 0x53, 0xf0, 0x6b, 0xe7, 0xd7, 0x80, 0x28, 0x00, 0x04, 0xc0, 0x7d, 0x4c, 0x96
};

void TestStun()
{
    const uint8_t* txid = kStunVector + 8;
    std::vector<uint8_t> v(kStunVector, kStunVector + sizeof kStunVector);
    NetAddr a;
    CHECK(DecodeStunResponse(v.data(), v.size(), txid, &a));
    CHECK(a.family == 4 && a.ip[0] == 192 && a.ip[1] == 0 && a.ip[2] == 2 && a.ip[3] == 1 &&
          a.port == 32853);

    uint8_t req[kStunRequestSize];
    CHECK(EncodeStunRequest(txid, req) == kStunRequestSize);
    CHECK(req[0] == 0x00 && req[1] == 0x01 && req[2] == 0 && req[3] == 0 &&
          std::memcmp(req + 4, kStunVector + 4, 16) == 0);

    uint8_t otherTxid[kStunTxidSize];
    std::memcpy(otherTxid, txid, sizeof otherTxid);
    otherTxid[11] ^= 1;
    CHECK(!DecodeStunResponse(v.data(), v.size(), otherTxid, &a));

    auto mutated = [&](size_t at, uint8_t value) {
        std::vector<uint8_t> m = v;
        m[at] = value;
        return DecodeStunResponse(m.data(), m.size(), txid, &a);
    };
    CHECK(!mutated(4, 0x22));    // cookie
    CHECK(!mutated(0, 0x41));    // the top two bits
    CHECK(!mutated(1, 0x11));    // an error response
    CHECK(!mutated(3, 0x40));    // message length past the datagram
    CHECK(!mutated(3, 0x3d));    // message length not a multiple of 4
    CHECK(!mutated(23, 0x40));   // SOFTWARE's length past the message
    CHECK(!mutated(39, 0x0c));   // XOR-MAPPED-ADDRESS overrunning into the next attribute
    CHECK(!mutated(41, 0x02));   // IPv6, which this does not read
    CHECK(!DecodeStunResponse(v.data(), 20, txid, &a));
    CHECK(!DecodeStunResponse(v.data(), 19, txid, &a));
    for (size_t n = 21; n < v.size(); n++)
        CHECK(!DecodeStunResponse(v.data(), n, txid, &a));

    // An attribute header cut short by the end of the message.
    std::vector<uint8_t> cut(v.begin(), v.begin() + 20);
    cut[3] = 4;
    cut.insert(cut.end(), { 0x00, 0x20, 0x00 });
    cut.push_back(0x08);
    CHECK(!DecodeStunResponse(cut.data(), cut.size(), txid, &a));

    // MAPPED-ADDRESS alone, as an RFC 3489 server answers.
    std::vector<uint8_t> plain(v.begin(), v.begin() + 20);
    plain[3] = 12;
    const uint8_t mapped[] = { 0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0xa1, 0x02, 203, 0, 113, 9 };
    plain.insert(plain.end(), mapped, mapped + sizeof mapped);
    CHECK(DecodeStunResponse(plain.data(), plain.size(), txid, &a));
    CHECK(a.ip[0] == 203 && a.ip[3] == 9 && a.port == 0xa102);
    // XOR-MAPPED-ADDRESS wins when both come.
    plain[3] = 24;
    plain.insert(plain.end(), kStunVector + 36, kStunVector + 48);
    CHECK(DecodeStunResponse(plain.data(), plain.size(), txid, &a));
    CHECK(a.ip[0] == 192 && a.port == 32853);

    // The session's plaintext classes and a STUN reply never pass for each other.
    CHECK(PlainClass(v.data(), v.size(), NULL) == 0 && v[0] != DG_SEALED);
    uint8_t hello[kHelloSize];
    Hello h;
    std::memset(&h, 0, sizeof h);
    EncodeHello(h, hello);
    CHECK(!DecodeStunResponse(hello, sizeof hello, hello + 8, &a));
}

int main()
{
    TestMessages();
    TestPlain();
    TestPayload();
    TestGoldenBytes();
    TestRecords();
    TestNames();
    TestSeal();
    TestReplay();
    TestAddresses();
    TestStun();
    TestRobustness();
    if (g_failures != 0)
    {
        std::printf("online_wire: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("online_wire: ok\n");
    return 0;
}
