#include "Game/FE/feChooseSideComponent.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/FE/feTweener.h"
#include "Game/GameInfo.h"

#include "Game/FE/feTweenFuncs.h"

/**
 * Offset/Address/Size: 0x16B0 | 0x800C4AF4 | size: 0xCC
 */
IChooseSide::IChooseSide()
{
    mContext = CONTEXT_FE;

    for (int i = 0; i < 21; i++)
    {
        mInstanceTable[i] = NULL;
    }

    for (int i = 0; i < 4; i++)
    {
        mPlayingSides[i] = -1;
        mPlayerReady[i] = false;
    }
}

/**
 * Offset/Address/Size: 0x1658 | 0x800C4A9C | size: 0x58
 */
IChooseSide::~IChooseSide()
{
}

/**
 * Offset/Address/Size: 0x15D4 | 0x800C4A18 | size: 0x84
 */
UpdateResult IChooseSide::Update(float fDeltaT, eFEINPUT_PAD* padresult, int disabledSide)
{
    UpdateResult result;
    if (mContext == CONTEXT_FE)
    {
        result = UpdateForFE(fDeltaT, padresult);
    }
    else
    {
        result = UpdateForPause(fDeltaT, padresult);
    }
    CheckControllers(disabledSide);
    mTweenManager.Update(fDeltaT);
    return result;
}

/**
 * Offset/Address/Size: 0xFD4 | 0x800C4418 | size: 0x600
 */
UpdateResult IChooseSide::UpdateForFE(float fDeltaT, eFEINPUT_PAD* padresult)
{
    for (int i = 0, offset = 0; i < 4; i++, offset += 4)
    {
        int destPosIndex;
        TLInstance* inst = mInstanceTable[i];
        if (g_pFEInput->IsConnected((eFEINPUT_PAD)i))
        {
            inst->m_bVisible = true;
            PositionController(i, false, true);
        }
        else
        {
            inst->m_bVisible = false;
            mPlayingSides[i] = -1;

            if (mInstanceTable[i + 8] != NULL)
            {
                mInstanceTable[i + 8]->m_bVisible = true;
            }

            if (mInstanceTable[i + 12] != NULL)
            {
                mInstanceTable[i + 12]->m_bVisible = false;
            }

            PositionController(i, false, false);
            SetReady(i, false);
        }

        if (g_pFEInput->JustPressed((eFEINPUT_PAD)i, 0x200, false, NULL))
        {
            if (mPlayerReady[i])
            {
                mPlayerReady[i] = false;
                mInstanceTable[(offset / 4) + 4]->m_bVisible = false;
                TLInstance* ri2 = mInstanceTable[16];
                if (ri2 != NULL)
                {
                    if (AllPlayersReady())
                        ri2->m_bVisible = true;
                    else
                        ri2->m_bVisible = false;
                }
                FEAudio::PlayAnimAudioEvent("sfx_back", false);
                FEAudio::PlayAnimAudioEvent("sfx_back_no_screen_change", false);
                continue;
            }
            else
            {
                if (padresult != NULL)
                {
                    *padresult = (eFEINPUT_PAD)i;
                }
                return UPDATE_GO_BACK;
            }
        }

        if (g_pFEInput->JustPressed((eFEINPUT_PAD)i, 0x100, false, NULL))
        {
            if (mPlayerReady[i])
            {
                if (padresult != NULL)
                {
                    *padresult = (eFEINPUT_PAD)i;
                }
                FEAudio::PlayAnimAudioEvent("sfx_accept_no_screen_change", false);
                return UPDATE_GO_FORWARD;
            }
            int* playingSide = &mPlayingSides[offset / 4];
            if (*playingSide != -1)
            {
                mPlayerReady[i] = true;
                // PORT: mInstanceTable[i + 4], reached by pointer arithmetic over 4-byte pointers.
                mInstanceTable[(offset / 4) + 4]->m_bVisible = true;
                TLInstance* ri3 = mInstanceTable[16];
                if (ri3 != NULL)
                {
                    if (AllPlayersReady())
                        ri3->m_bVisible = true;
                    else
                        ri3->m_bVisible = false;
                }
                if (padresult != NULL)
                {
                    *padresult = (eFEINPUT_PAD)i;
                }
                FEAudio::PlayAnimAudioEvent("sfx_accept_no_screen_change", false);
                if (AllPluggedInAreReady())
                {
                    return UPDATE_GO_FORWARD;
                }
            }
            else
            {
                if (padresult != NULL)
                {
                    *padresult = (eFEINPUT_PAD)i;
                }
                if (AllControllersAreCentred())
                {
                    return UPDATE_GO_FORWARD;
                }
                FEAudio::PlayAnimAudioEvent("sfx_accept_no_screen_change", false);
                return UPDATE_OK;
            }
        }
    }
    return UPDATE_OK;
}

/**
 * Offset/Address/Size: 0xCDC | 0x800C4120 | size: 0x2F8
 */
UpdateResult IChooseSide::UpdateForPause(float fDeltaT, eFEINPUT_PAD* padresult)
{
    for (int i = 0; i < 4; i++)
    {
        TLInstance* instance = mInstanceTable[i];
        if (g_pFEInput->IsConnected((eFEINPUT_PAD)i))
        {
            instance->m_bVisible = true;
            PositionController(i, false, true);
        }
        else
        {
            instance->m_bVisible = false;
            mPlayingSides[i] = -1;

            if (mInstanceTable[i + 8] != NULL)
            {
                mInstanceTable[i + 8]->m_bVisible = true;
            }

            if (mInstanceTable[i + 12] != NULL)
            {
                mInstanceTable[i + 12]->m_bVisible = false;
            }

            PositionController(i, false, false);
            SetReady(i, false);
        }

        if (g_pFEInput->JustPressed((eFEINPUT_PAD)i, 0x200, false, NULL))
        {
            FEAudio::PlayAnimAudioEvent("sfx_back", false);
            if (padresult != NULL)
            {
                *padresult = (eFEINPUT_PAD)i;
            }
            SaveChanges();
            return UPDATE_GO_BACK;
        }
    }

    return UPDATE_OK;
}

/**
 * Offset/Address/Size: 0x820 | 0x800C3C64 | size: 0x4BC
 */
void IChooseSide::CheckControllers(int disabledSide)
{
    for (int i = 0; i < 4; i++)
    {
        eFEINPUT_PAD pad = (eFEINPUT_PAD)i;
        if (mPlayerReady[i])
        {
            continue;
        }

        if (g_pFEInput->JustPressed(pad, 11, true, NULL))
        {
            if (mPlayingSides[i] == -1 && disabledSide == 0)
            {
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
                continue;
            }

            if (MoveSideLeft(i) != 0)
            {
                FEAudio::PlayAnimAudioEvent("sfx_side_select_left", false);
            }
            else
            {
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
            }

            PositionController(i, true, true);
            UpdatePressAText();
        }
        else if (g_pFEInput->JustPressed(pad, 12, true, NULL))
        {
            if (mPlayingSides[i] == -1 && disabledSide == 1)
            {
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
                continue;
            }

            if (MoveSideRight(i) != 1)
            {
                FEAudio::PlayAnimAudioEvent("sfx_side_select_right", false);
            }
            else
            {
                FEAudio::PlayAnimAudioEvent("sfx_deny", false);
            }

            PositionController(i, true, true);
            UpdatePressAText();
        }
    }
}

/**
 * Offset/Address/Size: 0x4C0 | 0x800C3904 | size: 0x360
 */
void IChooseSide::ResetAndPositionControllers(bool reset)
{
    for (int i = 0; i < 4; i++)
    {
        SetReady(i, false);

        if (!g_pFEInput->IsConnected((eFEINPUT_PAD)i))
        {
            mPlayingSides[i] = -1;
            PositionController(i, false, true);

            if (mInstanceTable[i + 12] != NULL)
            {
                mInstanceTable[i + 12]->m_bVisible = false;
            }

            if (mInstanceTable[i] != NULL)
            {
                mInstanceTable[i]->m_bVisible = false;
            }
        }
        else if (reset)
        {
            mPlayingSides[i] = -1;
            PositionController(i, false, true);
        }
        else
        {
            mPlayingSides[i] = (short)GameInfoManager::Instance()->GetPlayingSide((unsigned short)i);
            PositionController(i, false, true);
        }
    }
}

/**
 * Offset/Address/Size: 0x3DC | 0x800C3820 | size: 0xE4
 */
void IChooseSide::SetReady(int padindex, bool isready)
{
    mPlayerReady[padindex] = isready;
    mInstanceTable[padindex + 4]->m_bVisible = isready;
    UpdatePressAText();
}

void IChooseSide::UpdatePressAText()
{
    if (mInstanceTable[16] != NULL)
    {
        if (AllPlayersReady())
            mInstanceTable[16]->m_bVisible = true;
        else
            mInstanceTable[16]->m_bVisible = false;
    }
}

/**
 * Offset/Address/Size: 0x2A4 | 0x800C36E8 | size: 0x138
 */
void IChooseSide::PositionController(int padindex, bool usetween, bool setvisibilities)
{
    int playingside = mPlayingSides[padindex];
    ControllerPos controllerpos = playingside == 0 ? CPOS_HOME : (playingside == 1 ? CPOS_AWAY : CPOS_NEUTRAL);

    TLInstance* instance = mInstanceTable[padindex];
    feVector3 pos = instance->GetPosition();

    mTweenManager.clearTweensOnObj(instance);

    if (usetween)
    {
        FETweener* tween = mTweenManager.createTween(
            pos.e,
            &mControllerDestPos[controllerpos],
            0.075f,
            0.0f,
            1,
            TweenFunctions::linear,
            instance,
            TweenSetPosCallback);
        mTweenManager.startTween(tween);
    }
    else
    {
        mInstanceTable[padindex]->SetAssetPosition(mControllerDestPos[controllerpos], pos.e[1], pos.e[2]);
    }

    if (setvisibilities)
    {
        instance = mInstanceTable[padindex + 12];
        instance->m_bVisible = (playingside == -1);
        instance = mInstanceTable[padindex + 8];
        instance->m_bVisible = (playingside != -1);
    }

    if (mInstanceTable[padindex + 17] != NULL)
    {
        if (!g_pFEInput->IsConnected((eFEINPUT_PAD)padindex))
            mInstanceTable[padindex + 17]->m_bVisible = false;
        else
            mInstanceTable[padindex + 17]->m_bVisible = (playingside == -1);
    }
}

/**
 * Offset/Address/Size: 0x1DC | 0x800C3620 | size: 0xC8
 */
bool IChooseSide::AllPlayersReady() const
{
    u8 playerReady = 0;

    for (int i = 0; i < 4; i++)
    {
        if (mPlayerReady[i])
        {
            playerReady = 1;
        }
        else if (mPlayingSides[i] != -1)
        {
            playerReady = 0;
            break;
        }
    }

    if (playerReady == 1)
    {
        return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0x16C | 0x800C35B0 | size: 0x70
 */
bool IChooseSide::AllPluggedInAreReady() const
{
    for (int i = 0; i < 4; i++)
    {
        if (g_pFEInput->IsConnected((eFEINPUT_PAD)i))
        {
            if (!mPlayerReady[i])
                return false;
        }
    }
    return true;
}

int IChooseSide::MoveSideLeft(int padindex)
{
    int prevSide = mPlayingSides[padindex];

    switch (mPlayingSides[padindex])
    {
    case 1:
        MoveSideNone(padindex);
        break;
    case -1:
        mPlayingSides[padindex] = 0;
        break;
    case 0:
        break;
    default:
        break;
    }

    return prevSide;
}

int IChooseSide::MoveSideRight(int padindex)
{
    int prevSide = mPlayingSides[padindex];

    switch (mPlayingSides[padindex])
    {
    case 0:
        MoveSideNone(padindex);
        break;
    case -1:
        mPlayingSides[padindex] = 1;
        break;
    case 1:
        break;
    default:
        break;
    }

    return prevSide;
}

void IChooseSide::MoveSideNone(int padindex)
{
    mPlayingSides[padindex] = -1;
}

/**
 * Offset/Address/Size: 0x114 | 0x800C3558 | size: 0x58
 */
bool IChooseSide::AtLeastOnePlayerReady() const
{
    for (int i = 0; i < 4; i++)
    {
        if (mPlayerReady[i])
            return true;
    }
    return false;
}

/**
 * Offset/Address/Size: 0xBC | 0x800C3500 | size: 0x58
 */
bool IChooseSide::AllControllersAreCentred() const
{
    for (int i = 0; i < 4; i++)
    {
        if (mPlayingSides[i] != -1)
            return false;
    }
    return true;
}

/**
 * Offset/Address/Size: 0x58 | 0x800C349C | size: 0x64
 */
void IChooseSide::TweenSetPosCallback(void* obj, const float* value)
{
    TLInstance* instance = (TLInstance*)obj;
    feVector3 pos = instance->GetPosition();
    instance->SetAssetPosition(*(const float*)value, pos.f.y, pos.f.z);
}

/**
 * Offset/Address/Size: 0x0 | 0x800C3444 | size: 0x58
 */
void IChooseSide::SaveChanges()
{
    for (int i = 0; i < 4; i++)
    {
        GameInfoManager::Instance()->SetPlayingSide(i, mPlayingSides[i]);
    }
}

void IChooseSide::SetArrowVisible(int instanceid, bool leftvisible, bool rightvisible)
{
    instanceid += 12;
    TLComponentInstance* componentinstance = static_cast<TLComponentInstance*>(mInstanceTable[instanceid]);
    if (componentinstance == NULL)
    {
        return;
    }

    TLImageInstance* imageinstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        componentinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("p1arrows1")),
        InlineHasher(nlStringLowerHash("arrow")));
    if (imageinstance != NULL)
    {
        imageinstance->m_bVisible = leftvisible;
    }

    imageinstance = FEFinder<TLImageInstance, 2>::Find<TLSlide>(
        componentinstance->GetActiveSlide(),
        InlineHasher(nlStringLowerHash("p1arrows1")),
        InlineHasher(nlStringLowerHash("arrow2")));
    if (imageinstance != NULL)
    {
        imageinstance->m_bVisible = rightvisible;
    }
}

void IChooseSide::MakeArrowsFollowController()
{
    for (int i = 0; i < 4; i++)
    {
        if (mInstanceTable[i] == NULL || mInstanceTable[i + 12] == NULL)
        {
            continue;
        }

        const feVector3& controllerpos = mInstanceTable[i]->GetAssetPosition();
        const feVector3& arrowpos = mInstanceTable[i + 12]->GetAssetPosition();
        mInstanceTable[i + 12]->SetAssetPosition(controllerpos.f.x, arrowpos.f.y, arrowpos.f.z);
        mInstanceTable[i + 12]->m_bVisible = mInstanceTable[i]->m_bVisible;

        if (controllerpos.f.x == mControllerDestPos[0])
        {
            SetArrowVisible(i, false, true);
        }
        else if (controllerpos.f.x == mControllerDestPos[1])
        {
            SetArrowVisible(i, true, false);
        }
        else if (controllerpos.f.x == mControllerDestPos[2])
        {
            SetArrowVisible(i, true, true);
        }
        else
        {
            SetArrowVisible(i, false, false);
        }
    }
}
