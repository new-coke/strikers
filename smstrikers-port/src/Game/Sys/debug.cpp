
#include "Game/Sys/debug.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "Game/Sys/simpleparser.h"

#include "extras.h"
#include "NL/nlFileGC.h"
// #include "NL/nlPrint.h"

// PORT: the definition lives here.
#include "NL/nlWare.h"

namespace tDebugPrintManager
{

const char* szChannelNames[DC_NUM_CHANNELS] = {
    "STARTUP",
    "GL",
    "GLPLAT",
    "RENDER",
    "AI",
    "CAMERA",
    "EVENT",
    "FE",
    "PHYSICS",
    "MEMORY",
    "SOUND",
    "TASKMAN",
    "LOADER",
    "MISC",
    "REPLAY",
    "NIS",
    "NET",
    "MULTISTREAM",
    "WORLD",
    "CONFIGSYS",
};

bool abChannels[DC_NUM_CHANNELS] = { true, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };

/**
 * Offset/Address/Size: 0x0 | 0x801D6770 | size: 0x54
 */
int Print(eDEBUG_CHANNEL channel, const char* format, ...)
{
    // PORT: the sound channel goes to stderr under STRIKERS_LOG_AUDIO.
    static int s_soundLog = -1;
    if (s_soundLog < 0)
    {
        const char* e = getenv("STRIKERS_LOG_AUDIO");
        s_soundLog = (e != NULL && *e != '\0') ? 1 : 0;
    }
    if (channel == DC_SOUND && s_soundLog)
    {
        va_list args;
        va_start(args, format);
        fputs("[game] sound: ", stderr);
        vfprintf(stderr, format, args);
        va_end(args);
    }
    return 0;
}

/**
 * Offset/Address/Size: 0x54 | 0x801D67C4 | size: 0xE8
 */
void Initialize()
{
    // Zero out all channels
    for (int i = 0; i < DC_NUM_CHANNELS; ++i)
        abChannels[i] = 0;

    // Parse file; if it fails, enable all
    if (ParseDebugChannelFile("PrintCfg.txt") == 0)
    {
        for (int i = 0; i < DC_NUM_CHANNELS; ++i)
        {
            abChannels[i] = 1;
        }
    }
}

/**
 * Offset/Address/Size: 0x13C | 0x801D68AC | size: 0xF8
 */
bool ParseDebugChannelFile(const char* path)
{
    SimpleParser parser;
    unsigned long uFileSize;
    char* token;

    char* pData = (char*)nlLoadEntireFile(path, &uFileSize, 0x20u, AllocateStart);
    if (!pData)
    {
        nlPrintf("Failed to load file %s.  All print channels will be enabled.\n", path);
        return false;
    }

    if (parser.StartParsing(pData, uFileSize, true))
    {
        for (;;)
        {
            token = parser.NextToken(true);
            if (!token)
                break;

            // skip comment lines starting with '#'
            if (token[0] == '#')
                continue;

            // scan list of known channel names and mark enabled
            for (int chan = 0; chan < DC_NUM_CHANNELS; ++chan)
            {
                if (strcmpi(szChannelNames[chan], token) == 0)
                {
                    abChannels[chan] = 1;
                }
            }
        }
    }

    delete[] pData;
    return true;
}

} // namespace tDebugPrintManager
