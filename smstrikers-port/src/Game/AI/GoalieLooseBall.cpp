#include "Game/AI/GoalieLooseBall.h"

#include "Game/AI/FilteredRandom.h"
#include "Game/CharacterTriggers.h"
#include "NL/nlMath.h"

LooseBallInfo LooseBallAnims::mLooseBallKickInfo[3];
LooseBallInfo LooseBallAnims::mTrapBallInfo;
LooseBallInfo LooseBallAnims::mAttackSTSInfo;
LooseBallInfo LooseBallAnims::mSwatSTSInfo[2];
LooseBallInfo LooseBallAnims::mDiveBallInfo;
LooseBallInfo* LooseBallAnims::mLooseBallDesperationInfo[4];

LooseBallInfo* LooseBallAnims::mpLooseBallInfo = NULL;
unsigned int LooseBallAnims::muNumLooseBallAnims = 0;

static LooseBallInfo gCatches[8];
static const int gLooseBallAnims[8] = {
    0x00000033,
    0x00000076,
    0x0000007F,
    0x0000007E,
    0x00000079,
    0x0000007B,
    0x00000078,
    0x0000007A
};
static const int gLooseBallKickAnims[3] = { 0x3, 0x4, 0x5 };
static const int gSwatSTSAnim[2] = { 0x6, 0x7 };

/**
 * Offset/Address/Size: 0x6FC | 0x80053404 | size: 0x1C
 */
static bool LooseBallCallback(float fTime, float fDuration, unsigned long uEventID, float, void* pUserData)
{
    if (uEventID == 0xCF84E7CDul)
    {
        LooseBallInfo* info = (LooseBallInfo*)pUserData;
        info->mfPickupTime = fTime;
        info->mfAnimDuration = fDuration;
    }

    return true;
}

void LooseBallInfo::InitInstance(cPlayer* pPlayer, int nAnimID, eLooseBallAnimType eAnimType)
{
    mfAnimDuration = 0.0f;
    mfPickupTime = 0.0f;
    mnAnimID = nAnimID;
    mAnimType = eAnimType;
    GetAnimTriggerInfo(pPlayer, nAnimID, LooseBallCallback, this);
    pPlayer->GetJointPositionFuture(&mv3PickupPos, nAnimID, pPlayer->m_nBallJointIndex, mfPickupTime, true, true, false);
    mfPickupDistance = nlSqrt(mv3PickupPos.e[0] * mv3PickupPos.e[0] + mv3PickupPos.e[1] * mv3PickupPos.e[1], true);
    maPickupAngle = (unsigned short)(s16)(10430.378f * nlATan2f(mv3PickupPos.y, mv3PickupPos.x));
}

/**
 * Offset/Address/Size: 0x1D8 | 0x80052EE0 | size: 0x524
 */
void LooseBallAnims::Init(cPlayer* pPlayer)
{
    if (mpLooseBallInfo != NULL)
        return;

    muNumLooseBallAnims = 8;
    mpLooseBallInfo = gCatches;
    int i;

    for (i = 0; (unsigned int)i < muNumLooseBallAnims; i++)
    {
        mpLooseBallInfo[i].InitInstance(pPlayer, gLooseBallAnims[i], LOOSEBALL_ANIM_PICKUP);
    }

    for (i = 0; i < 3; i++)
    {
        mLooseBallKickInfo[i].InitInstance(pPlayer, gLooseBallKickAnims[i], LOOSEBALL_ANIM_KICK);
    }

    mTrapBallInfo.InitInstance(pPlayer, 0x7C, LOOSEBALL_ANIM_TRAP);
    mAttackSTSInfo.InitInstance(pPlayer, 0x71, LOOSEBALL_ANIM_ATTACK);

    for (i = 0; i < 2; i++)
    {
        mSwatSTSInfo[i].InitInstance(pPlayer, gSwatSTSAnim[i], LOOSEBALL_ANIM_SWAT);
    }

    mDiveBallInfo.InitInstance(pPlayer, 0x77, LOOSEBALL_ANIM_PICKUP);

    mLooseBallDesperationInfo[0] = &mTrapBallInfo;
    mLooseBallDesperationInfo[1] = &mDiveBallInfo;
    mLooseBallDesperationInfo[2] = GetLooseBallAnim(6);
    mLooseBallDesperationInfo[3] = GetLooseBallAnim(7);
}

/**
 * Offset/Address/Size: 0x1C8 | 0x80052ED0 | size: 0x10
 */
void LooseBallAnims::Destroy()
{
    mpLooseBallInfo = NULL;
    muNumLooseBallAnims = 0;
}

/**
 * Offset/Address/Size: 0xB0 | 0x80052DB8 | size: 0x118
 */
const LooseBallInfo* LooseBallAnims::FindLooseBallAnim(const nlVector3& v3LocalBallPosition, bool bFrontOnly)
{
    float fClosestDistSq = 1000000.0f;
    float fDistFromOrigSq = 0.2f + ((v3LocalBallPosition.x * v3LocalBallPosition.x) + (v3LocalBallPosition.y * v3LocalBallPosition.y) + (v3LocalBallPosition.z * v3LocalBallPosition.z));
    const LooseBallInfo* pInfo = GetLooseBallAnim(0);

    for (unsigned int i = 0; i < muNumLooseBallAnims; i++)
    {
        if (!bFrontOnly || GetLooseBallAnim(i)->mv3PickupPos.x >= 0.0f)
        {
            float fDX = v3LocalBallPosition.x - GetLooseBallAnim(i)->mv3PickupPos.x;
            float fDY = v3LocalBallPosition.y - GetLooseBallAnim(i)->mv3PickupPos.y;
            float fDist2D = nlSqrt((fDX * fDX) + (fDY * fDY), true);

            if (fDist2D < fClosestDistSq)
            {
                float fPickupDistSq = GetLooseBallAnim(i)->mfPickupDistance * GetLooseBallAnim(i)->mfPickupDistance;
                if (fDistFromOrigSq > fPickupDistSq)
                {
                    fClosestDistSq = fDist2D;
                    pInfo = GetLooseBallAnim(i);
                }
            }
        }
    }

    return pInfo;
}

LooseBallInfo* LooseBallAnims::GetLooseBallAnim(unsigned int uIndex)
{
    return &mpLooseBallInfo[uIndex];
}

/**
 * Offset/Address/Size: 0x14 | 0x80052D1C | size: 0x9C
 */
const LooseBallInfo* LooseBallAnims::GetSwatSTSInfo(int nSide)
{
    static FilteredRandomRange randgenSwats;

    if (nSide < 0)
    {
        int index = randgenSwats.genrand(2);
        return &mSwatSTSInfo[index];
    }
    else if (nSide == 0)
    {
        return &mSwatSTSInfo[0];
    }
    else
    {
        return &mSwatSTSInfo[1];
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80052D08 | size: 0x14
 */
const LooseBallInfo* LooseBallAnims::GetDesperationInfo(unsigned int type)
{
    return mLooseBallDesperationInfo[type];
}
