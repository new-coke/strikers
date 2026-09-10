#include "Game/DB/CustomTournament.h"
#include "Game/Sys/debug.h"

/**
 * Offset/Address/Size: 0xA3C | 0x8018DEEC | size: 0x60
 */
CustomTournament::CustomTournament()
{
    m_fenceBoundary = 0x1234ABCD;
    m_cup = NULL;
    m_tournMode = TM_INVALID;
    m_numTeams = 0;
    m_numGamesPerTeam = 0;
    m_cupConstructed = false;
    m_largestCupSize = sizeof(m_dataSpace);
}

/**
 * Offset/Address/Size: 0x9E8 | 0x8018DE98 | size: 0x54
 */
CustomTournament::~CustomTournament()
{
    if (m_cup != NULL)
    {
        m_cup = NULL;
    }
}

/**
 * Offset/Address/Size: 0x15C | 0x8018D60C | size: 0x88C
 */
BaseCup* CustomTournament::ConstructCup()
{
    if (m_cup != NULL)
    {
        m_cup = NULL;
    }

    switch (m_tournMode)
    {
    case TM_LEAGUE:
        switch (m_numGamesPerTeam)
        {
        case 1:
            switch (m_numTeams)
            {
            case 3:
                m_cup = new (&m_dataSpace.cup31) Cup<3, 3>;
                break;
            case 4:
                m_cup = new (&m_dataSpace.cup41) Cup<4, 3>;
                break;
            case 5:
                m_cup = new (&m_dataSpace.cup51) Cup<5, 5>;
                break;
            case 6:
                m_cup = new (&m_dataSpace.cup61) Cup<6, 5>;
                break;
            case 7:
                m_cup = new (&m_dataSpace.cup71) Cup<7, 7>;
                break;
            case 8:
                m_cup = new (&m_dataSpace.cup81) Cup<8, 7>;
                break;
            }
            break;
        case 2:
            switch (m_numTeams)
            {
            case 3:
                m_cup = new (&m_dataSpace.cup32) Cup<3, 6>;
                break;
            case 4:
                m_cup = new (&m_dataSpace.cup42) Cup<4, 6>;
                break;
            case 5:
                m_cup = new (&m_dataSpace.cup52) Cup<5, 10>;
                break;
            case 6:
                m_cup = new (&m_dataSpace.cup62) Cup<6, 10>;
                break;
            case 7:
                m_cup = new (&m_dataSpace.cup72) Cup<7, 14>;
                break;
            case 8:
                m_cup = new (&m_dataSpace.cup82) Cup<8, 14>;
                break;
            }
            break;
        }
        break;
    case TM_KNOCKOUT:
        switch (m_numTeams)
        {
        case 4:
            m_cup = new (&m_dataSpace.knockout4) Knockout<4>;
            break;
        case 8:
            m_cup = new (&m_dataSpace.knockout8) Knockout<8>;
            break;
        }
        break;
    }

    tDebugPrintManager::Print(DC_FE, "Tournament Cup Constructed\n");
    m_cupConstructed = true;
    return m_cup;
}

/**
 * Offset/Address/Size: 0x150 | 0x8018D600 | size: 0xC
 */
int CustomTournament::GetSaveDataSize() const
{
    return m_largestCupSize + 7;
}

/**
 * Offset/Address/Size: 0xAC | 0x8018D55C | size: 0xA4
 */
void CustomTournament::SerializeData(void* buffer) const
{
    void* ptr = buffer;

    memcpy(ptr, &m_tournMode, 4);
    ptr = (char*)ptr + 4;

    memcpy(ptr, &m_numTeams, 1);
    ptr = (char*)ptr + 1;

    memcpy(ptr, &m_numGamesPerTeam, 1);
    ptr = (char*)ptr + 1;

    memcpy(ptr, &m_cupConstructed, 1);
    ptr = (char*)ptr + 1;

    if (m_cupConstructed)
    {
        m_cup->SerializeData(ptr);
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8018D4B0 | size: 0xAC
 */
void CustomTournament::DeserializeData(void* buffer)
{
    void* ptr = buffer;

    memcpy(&m_tournMode, ptr, 4);
    ptr = (char*)ptr + 4;

    memcpy(&m_numTeams, ptr, 1);
    ptr = (char*)ptr + 1;

    memcpy(&m_numGamesPerTeam, ptr, 1);
    ptr = (char*)ptr + 1;

    memcpy(&m_cupConstructed, ptr, 1);
    ptr = (char*)ptr + 1;

    if (m_cupConstructed)
    {
        ConstructCup();
        m_cup->DeserializeData(ptr);
    }
}
