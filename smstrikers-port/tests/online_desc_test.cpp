// The match descriptor's wire form and range checks, and the name rules.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port/online.h"

static int cases;

static void check(bool result, const char* name)
{
    ++cases;
    if (!result)
    {
        fprintf(stderr, "FAIL: %s\n", name);
        exit(1);
    }
}

static const char* check_field(void (*edit)(OnlineMatchDesc*))
{
    OnlineMatchDesc d;
    PortOnlineDescDefault(&d);
    edit(&d);
    return PortOnlineDescCheck(&d);
}

static bool rejects(void (*edit)(OnlineMatchDesc*), const char* field)
{
    const char* bad = check_field(edit);
    return bad != NULL && strcmp(bad, field) == 0;
}

static uint32_t s_rng = 0x2545F491u;

static uint32_t next_random(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

int main()
{
    OnlineMatchDesc d;
    PortOnlineDescDefault(&d);
    check(PortOnlineDescCheck(&d) == NULL, "default is valid");

    check(
        rejects([](OnlineMatchDesc* x) { x->protocol = ONLINE_PROTOCOL_VERSION + 1; }, "protocol"),
        "protocol");
    check(rejects([](OnlineMatchDesc* x) { x->gameTime = 121; }, "gameTime"), "gameTime 121");
    check(rejects([](OnlineMatchDesc* x) { x->gameTime = 0; }, "gameTime"), "gameTime 0");
    check(check_field([](OnlineMatchDesc* x) { x->gameTime = 900; }) == NULL, "gameTime 900");
    check(check_field([](OnlineMatchDesc* x) { x->gameTime = 3600; }) == NULL, "gameTime 3600");
    check(rejects([](OnlineMatchDesc* x) { x->gameTime = 1000; }, "gameTime"), "gameTime 1000");
    check(PortOnlineGameTimeStep(900, 1) == 1200, "step past retail");
    check(PortOnlineGameTimeStep(3600, 1) == 120, "step wraps up");
    check(PortOnlineGameTimeStep(120, -1) == 3600, "step wraps down");
    check(PortOnlineGameTimeStep(121, 1) == 120, "step from unknown");
    check(rejects([](OnlineMatchDesc* x) { x->team[1] = 9; }, "team"), "team 9");
    check(rejects([](OnlineMatchDesc* x) { x->team[0] = -1; }, "team"), "team -1");
    check(check_field([](OnlineMatchDesc* x) { x->team[0] = 8; }) == NULL, "team 8");
    check(rejects([](OnlineMatchDesc* x) { x->sidekick[0] = 4; }, "sidekick"), "sidekick 4");
    check(rejects([](OnlineMatchDesc* x) { x->sidekick[1] = -2; }, "sidekick"), "sidekick -2");
    check(rejects([](OnlineMatchDesc* x) { x->padSide[2] = 2; }, "padSide"), "padSide 2");
    check(rejects([](OnlineMatchDesc* x) { x->padSide[0] = 0; }, "slotMachine"),
          "side without machine");
    check(rejects([](OnlineMatchDesc* x) { x->slotMachine[3] = 4; }, "slotMachine"), "machine 4");
    check(rejects(
              [](OnlineMatchDesc* x) {
                  x->slotMachine[0] = 1;
                  x->slotMachine[1] = 1;
                  x->slotMachine[2] = 1;
              },
              "slotMachine"),
          "three slots on one machine");
    check(check_field([](OnlineMatchDesc* x) {
              x->slotMachine[0] = 0;
              x->slotMachine[1] = 1;
              x->slotMachine[2] = 0;
              x->slotMachine[3] = 1;
              x->padSide[0] = 0;
              x->padSide[1] = 1;
              x->padSide[2] = 0;
              x->padSide[3] = 1;
          }) == NULL,
          "couch 2v2");
    check(rejects([](OnlineMatchDesc* x) { x->stadium = 7; }, "stadium"), "stadium 7");
    check(rejects([](OnlineMatchDesc* x) { x->stadium = -1; }, "stadium"), "stadium -1");
    check(rejects([](OnlineMatchDesc* x) { x->skill = 5; }, "skill"), "skill");
    check(rejects([](OnlineMatchDesc* x) { x->powerUps = 2; }, "powerUps"), "powerUps");
    check(rejects([](OnlineMatchDesc* x) { x->customPowerups = 6; }, "customPowerups"),
          "customPowerups");
    check(rejects([](OnlineMatchDesc* x) { x->cheats = 0x10; }, "cheats"), "cheats");
    check(rejects([](OnlineMatchDesc* x) { x->shoot2Score = 2; }, "shoot2Score"), "shoot2Score");
    check(rejects([](OnlineMatchDesc* x) { x->bowser = 3; }, "bowser"), "bowser");
    check(rejects([](OnlineMatchDesc* x) { x->bowserRepeat = 2; }, "bowserRepeat"), "bowserRepeat");
    check(rejects([](OnlineMatchDesc* x) { x->pureGame = 2; }, "pureGame"), "pureGame");
    check(rejects([](OnlineMatchDesc* x) { x->pauseAllowed = 2; }, "pauseAllowed"), "pauseAllowed");
    check(rejects([](OnlineMatchDesc* x) { x->reserved[18] = 1; }, "reserved"), "reserved");

    // Round trip and byte offsets.
    d.build = 0x11223344u;
    d.seed = 0xA1B2C3D4u;
    d.slotMachine[1] = 1;
    d.padSide[1] = 1;
    d.pauseAllowed = 1;
    uint8_t wire[ONLINE_DESC_BYTES];
    PortOnlineDescEncode(&d, wire);
    check(wire[0] == ONLINE_PROTOCOL_VERSION && wire[1] == 0, "protocol little-endian");
    check(wire[2] == 180 && wire[3] == 0, "gameTime at 2");
    check(wire[4] == 0x44 && wire[7] == 0x11, "build at 4");
    check(wire[16] == 0xD4 && wire[19] == 0xA1, "seed at 16");
    check(wire[20] == 3 && wire[21] == 2, "teams at 20");
    check(wire[25] == 1 && wire[24] == 0xFF, "padSide at 24");
    check(wire[29] == 1, "slotMachine at 28");
    check(wire[32] == 1, "stadium at 32");
    check(wire[42] == 1, "pauseAllowed at 42");
    OnlineMatchDesc back;
    check(PortOnlineDescDecode(wire, &back) == NULL, "decode valid");
    check(memcmp(&back, &d, sizeof d) == 0, "round trip");
    check(PortOnlineDescHash(&back) == PortOnlineDescHash(&d), "hash stable");
    d.seed ^= 1;
    check(PortOnlineDescHash(&back) != PortOnlineDescHash(&d), "hash covers the seed");

    // A single-byte change decodes to bytes that re-encode identically, or leaves `out` alone.
    for (int at = 0; at < ONLINE_DESC_BYTES; at++)
    {
        for (int v = 0; v < 256; v++)
        {
            uint8_t bytes[ONLINE_DESC_BYTES];
            memcpy(bytes, wire, sizeof bytes);
            bytes[at] = (uint8_t)v;
            OnlineMatchDesc out;
            memset(&out, 0x5A, sizeof out);
            if (PortOnlineDescDecode(bytes, &out) == NULL)
            {
                uint8_t again[ONLINE_DESC_BYTES];
                PortOnlineDescEncode(&out, again);
                if (memcmp(again, bytes, sizeof bytes) != 0)
                    check(false, "accepted bytes re-encode identically");
                if (PortOnlineDescCheck(&out) != NULL)
                    check(false, "accepted descriptor checks clean");
            }
            else
            {
                const uint8_t* raw = (const uint8_t*)&out;
                for (size_t i = 0; i < sizeof out; i++)
                    if (raw[i] != 0x5A)
                        check(false, "rejected decode leaves output untouched");
            }
        }
    }
    ++cases;

    // Random bytes: whatever is accepted is in range.
    int accepted = 0;
    for (int n = 0; n < 200000; n++)
    {
        uint8_t bytes[ONLINE_DESC_BYTES];
        for (int i = 0; i < ONLINE_DESC_BYTES; i++)
            bytes[i] = (uint8_t)next_random();
        // Push some inputs past the early fields so the later checks run.
        if (n & 1)
        {
            memcpy(bytes, wire, 24);
            memset(bytes + 45, 0, 19);
        }
        OnlineMatchDesc out;
        if (PortOnlineDescDecode(bytes, &out) == NULL)
        {
            accepted++;
            if (PortOnlineDescCheck(&out) != NULL)
                check(false, "random accepted descriptor checks clean");
        }
    }
    ++cases;
    (void)accepted;

    char name[ONLINE_NAME_MAX + 1];
    check(PortOnlineCleanName(name, "  Toad  ") == 4 && strcmp(name, "Toad") == 0, "trim");
    check(PortOnlineCleanName(name, NULL) == 0 && name[0] == '\0', "null name");
    check(PortOnlineCleanName(name, "a\tb\x7F"
                                    "c") == 3 &&
              strcmp(name, "abc") == 0,
          "control characters dropped");
    check(PortOnlineCleanName(name, "abcdefghijklmnopqrstuvwxyz") == 16 &&
              strcmp(name, "abcdefghijklmnop") == 0,
          "cut at 16 bytes");
    // 15 ASCII bytes then a two-byte character: the character does not fit and is dropped whole.
    check(PortOnlineCleanName(name, "abcdefghijklmno\xC3\xA9") == 15, "no split character");
    check(PortOnlineCleanName(name, "\xC3\xA9t\xC3\xA9") == 5, "UTF-8 kept");
    check(PortOnlineCleanName(name, "a\xC0\xAF"
                                    "b") == 2 &&
              strcmp(name, "ab") == 0,
          "overlong dropped");
    check(PortOnlineCleanName(name, "a\xED\xA0\x80"
                                    "b") == 2,
          "surrogate dropped");
    check(PortOnlineCleanName(name, "a\xC2\x85"
                                    "b") == 2,
          "C1 control dropped");
    check(PortOnlineCleanName(name, "a\xE2\x82") == 1, "truncated sequence dropped");
    check(PortOnlineCleanName(name, "\xF0\x9F\x98\x80") == 4, "four-byte character kept");

    check(strcmp(PortOnlinePlatformName(ONLINE_PLATFORM_SWITCH), "Switch") == 0, "platform");
    check(strcmp(PortOnlinePlatformName(99), "Unknown") == 0, "platform out of range");

    printf("online_desc: %d cases\n", cases);
    return 0;
}
