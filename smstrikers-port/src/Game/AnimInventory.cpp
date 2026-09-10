#include "Game/AnimInventory.h"

#include "NL/nlString.h"
#include "NL/nlMemory.h"
#include "NL/nlFileGC.h"
#include "NL/nlWare.h"

cInventory<cSAnim>* g_pDefaultSAnimInventory = nullptr;

/**
 * Offset/Address/Size: 0x438 | 0x800073B4 | size: 0xA0
 */
cAnimInventory::cAnimInventory(const AnimProperties* props, int count)
{
    m_nNumProperties = count;
    m_pSAnimInventory = 0;
    m_pSAnims = 0;
    m_pAnimProperties = props;

    m_pSAnimInventory = new (nlMalloc(sizeof(cInventory<cSAnim>), 8, false)) cInventory<cSAnim>();

    if (g_pDefaultSAnimInventory == 0)
        g_pDefaultSAnimInventory = m_pSAnimInventory;

    m_pSAnims = (cSAnim**)nlMalloc((unsigned long)(m_nNumProperties * sizeof(void*)), 8, 0);
}

/**
 * Offset/Address/Size: 0x29C | 0x80007218 | size: 0x19C
 */
cAnimInventory::~cAnimInventory()
{
    delete m_pSAnimInventory;
    delete[] m_pSAnims;
    g_pDefaultSAnimInventory = 0;
}

/**
 * Offset/Address/Size: 0x88 | 0x80007004 | size: 0x214
 */
void cAnimInventory::AddAnimBundle(const char* szFilename)
{
    int i;
    int len;
    void* pMem;

    pMem = nlLoadEntireFileToVirtualMemory(szFilename, &len, 0x10000, 0, AllocateStart);
    m_pSAnimInventory->AddFile((char*)pMem, len);

    for (i = 0; i < m_nNumProperties; i++)
    {
        m_pSAnims[i] = m_pSAnimInventory->Find((char*)m_pAnimProperties[i].animName);
        if (m_pSAnims[i] == 0)
        {
            nlPrintf("Warning! Could not find \"%s\" in bundle \"%s\"\n",
                m_pAnimProperties[i].animName,
                szFilename);
            m_pSAnims[i] = g_pDefaultSAnimInventory->Find((char*)m_pAnimProperties[i].animName);
            if (m_pSAnims[i] == 0)
            {
                m_pSAnims[i] = m_pSAnims[0];
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x78 | 0x80006FF4 | size: 0x10
 */
cSAnim* cAnimInventory::GetAnim(int i)
{
    return m_pSAnims[i];
}

/**
 * Offset/Address/Size: 0x64 | 0x80006FE0 | size: 0x14
 */
ePlayMode cAnimInventory::GetPlayMode(int i)
{
    return m_pAnimProperties[i].playMode;
}

/**
 * Offset/Address/Size: 0x50 | 0x80006FCC | size: 0x14
 */
float cAnimInventory::GetBlendTime(int i)
{
    return m_pAnimProperties[i].blendAmount;
}

/**
 * Offset/Address/Size: 0x3C | 0x80006FB8 | size: 0x14
 */
bool cAnimInventory::GetMirrored(int i)
{
    return m_pAnimProperties[i].mirror;
}

/**
 * Offset/Address/Size: 0x28 | 0x80006FA4 | size: 0x14
 */
int cAnimInventory::GetBallRotationMode(int i)
{
    return m_pAnimProperties[i].ballRotationMode;
}

/**
 * Offset/Address/Size: 0x14 | 0x80006F90 | size: 0x14
 */
int cAnimInventory::GetEndPhase(int i)
{
    return m_pAnimProperties[i].endPhase;
}

/**
 * Offset/Address/Size: 0x0 | 0x80006F7C | size: 0x14
 */
u8 cAnimInventory::GetMatchCharacterSpeed(int i)
{
    return m_pAnimProperties[i].matchCharacterSpeed;
}
