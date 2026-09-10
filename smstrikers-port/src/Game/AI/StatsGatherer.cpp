#include "Game/AI/StatsGatherer.h"

StatsGatherer::StatsGatherer()
    : InterpreterCore(10)
{
}

StatsGatherer::~StatsGatherer()
{
}

/**
 * Raw object offset/size: 0x1F4 | size: 0x134
 */
void StatsGatherer::SetTestName(const char* name)
{
    m_testName = name;
}
