#include "Game/DB/Simmer.h"
#include "Game/GameInfo.h"
#include "NL/nlLexicalCast.h"
#include "NL/nlString.h"
#include <stdio.h>

extern "C" char* fgets(char*, int, FILE*);

static const char* SIM_FILE = "SimmerStats.ini";

/**
 * Offset/Address/Size: 0xC18 | 0x80191494 | size: 0x4
 */
Simulator::Simulator()
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0x0 | 0x8019087C | size: 0xC18
 */
void Simulator::InitializeStats()
{
    eDifficultyID diff;
    GameplaySettings::eSkillLevel skillLevel = GameInfoManager::Instance()->GetGameplayOptions().SkillLevel;
    int length = GameInfoManager::Instance()->GetGameplayOptions().GameTime;
    unsigned char doMean;
    unsigned char isMeanFound;
    unsigned char isSDFound;
    isMeanFound = 0;
    isSDFound = 0;
    doMean = 1;
    if (skillLevel == GameplaySettings::ROOKIE)
    {
        diff = (eDifficultyID)1;
    }
    else if (skillLevel == GameplaySettings::PROFESSIONAL)
    {
        diff = (eDifficultyID)2;
    }
    else
    {
        diff = (eDifficultyID)3;
    }
    BasicString<char, Detail::TempStringAllocator> searchString = LexicalCast<BasicString<char, Detail::TempStringAllocator> >((int)diff);
    if (length <= 120)
    {
        searchString = searchString.Append(" 120");
    }
    else if (length <= 300)
    {
        searchString = searchString.Append(" 300");
    }
    else
    {
        searchString = searchString.Append(" 600");
    }
    BasicString<char, Detail::TempStringAllocator> meanString = searchString.Append(" Average");
    BasicString<char, Detail::TempStringAllocator> SDString = searchString.Append(" StdDev");
    BasicString<char, Detail::TempStringAllocator> statString;
    char line[0x100];
    if (FILE* pFile = fopen(SIM_FILE, "r"))
    {
        while (fgets(line, 0x100, pFile) != 0)
        {
            unsigned char isLineFound;
            if (nlStrNCmp<char>(meanString.c_str(), line, meanString.mData ? (unsigned long)(meanString.mData->mData.mSize - 1) : 0) == 0)
            {
                statString = BasicString<char, Detail::TempStringAllocator>(line);
                isLineFound = 1;
                isMeanFound = 1;
                doMean = 1;
            }
            else if (nlStrNCmp<char>(SDString.c_str(), line, SDString.mData ? (unsigned long)(SDString.mData->mData.mSize - 1) : 0) == 0)
            {
                statString = BasicString<char, Detail::TempStringAllocator>(line);
                isLineFound = 1;
                isSDFound = 1;
                doMean = 0;
            }
            else
            {
                isLineFound = 0;
            }
            if (isLineFound == 1)
            {
                Tokenizer<BasicString<char, Detail::TempStringAllocator> > tokenizer(statString, ",");
                int i = 0;
                Tokenizer<BasicString<char, Detail::TempStringAllocator> >::iterator iter = tokenizer.begin();
                ++iter;
                for (;
                    iter != tokenizer.end();)
                {
                    while (i == 2 || (unsigned)(i - 6) <= 3u || i == 16 || i == 18)
                    {
                        i++;
                    }
                    ePlayerStats stat = (ePlayerStats)i;
                    if (doMean == 1)
                    {
                        mStatistics[stat].mMean = (float)atof(iter.mToken.c_str());
                    }
                    else
                    {
                        mStatistics[stat].mStandardDeviation = (float)atof(iter.mToken.c_str());
                    }
                    ++iter;
                    i = (int)(stat + 1);
                }
                if (isMeanFound == 1 && isSDFound == 1)
                {
                    break;
                }
            }
        }
        fclose(pFile);
    }
}
