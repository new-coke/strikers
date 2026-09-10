#include "Game/FE/feCaptainGridComponent.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/GameInfo.h"

struct CellItem
{
    int mIconType;
    const char* mIconName;
};

static const CellItem NormalCaptainCellItems[9] = {
    { 0, "choose_capt_mario" },
    { 4, "choose_capt_daisy" },
    { 1, "choose_capt_dk" },
    { 5, "choose_capt_luigi" },
    { 8, "choose_capt_super" },
    { 3, "choose_capt_peach" },
    { 2, "choose_capt_wario" },
    { 6, "choose_capt_waluigi" },
    { 7, "choose_capt_yoshi" },
};

static const CellItem LeipzigCaptainCellItems[4] = {
    { 0, "choose_capt_mario" },
    { 1, "choose_capt_dk" },
    { 2, "choose_capt_wario" },
    { 3, "choose_capt_peach" },
};

static CellItem CaptainCellItems[9];
static int NUM_CAPTAIN_CELL_ITEMS = 9;

extern bool g_e3_Build;

/**
 * Offset/Address/Size: 0x950 | 0x800C2044 | size: 0x228
 */
ICaptainGridComponent::ICaptainGridComponent(TLComponentInstance* parentcomponent, bool ismirrored)
    : IGridComponent<eTeamID>(parentcomponent, "highlight", ismirrored)
{
    int numItems = g_e3_Build ? 4 : 9;
    NUM_CAPTAIN_CELL_ITEMS = g_e3_Build ? 4 : 9;

    for (int i = 0; i < numItems; i++)
    {
        if (g_e3_Build)
        {
            CaptainCellItems[i].mIconName = LeipzigCaptainCellItems[i].mIconName;
            CaptainCellItems[i].mIconType = LeipzigCaptainCellItems[i].mIconType;
        }
        else
        {
            CaptainCellItems[i].mIconName = NormalCaptainCellItems[i].mIconName;
            CaptainCellItems[i].mIconType = NormalCaptainCellItems[i].mIconType;
        }
    }
}

/**
 * Offset/Address/Size: 0x8BC | 0x800C1FB0 | size: 0x94
 */
ICaptainGridComponent::~ICaptainGridComponent()
{
}

/**
 * Offset/Address/Size: 0x6CC | 0x800C1DC0 | size: 0x1F0
 */
void ICaptainGridComponent::BuildMapMenu()
{
    TLSlide* activeslide;
    int i;

    activeslide = mParentComponent->GetActiveSlide();
    mInstanceTable = (TLInstance**)nlMalloc(NUM_CAPTAIN_CELL_ITEMS * sizeof(void*), 8, false);

    for (i = 0; i < (int)NUM_CAPTAIN_CELL_ITEMS; i++)
    {
        mInstanceTable[CaptainCellItems[i].mIconType] = FEFinder<TLInstance, 2>::Find(activeslide,
            InlineHasher(nlStringLowerHash(CaptainCellItems[i].mIconName)),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0));
    }

    int numRows = g_e3_Build ? 2 : 3;
    int numCols = g_e3_Build ? 2 : 3;
    int base;
    int NUM_ELEMENTS = numRows * numCols;
    int row = 0;

    base = 0;
    for (; row < numRows; row++)
    {
        i = base;
        for (int j = 0; j < numCols; j++)
        {
            int col = i % numCols;

            int left = i - 1;
            if (col - 1 < 0)
                left = i + numCols - 1;

            int right = i + 1;
            if (col + 1 >= numCols)
                right = i - numCols + 1;

            int up = (i - numCols + NUM_ELEMENTS) % NUM_ELEMENTS;
            int down = (i + numCols) % NUM_ELEMENTS;

            int itemID = CaptainCellItems[(unsigned int)i].mIconType;
            mMapMenu->AddItem(
                itemID,
                mInstanceTable[itemID],
                CaptainCellItems[left].mIconType,
                CaptainCellItems[right].mIconType,
                CaptainCellItems[up].mIconType,
                CaptainCellItems[down].mIconType,
                true);
            i++;
        }
        base += numCols;
    }

    mMapMenu->SetSelectedItem(CaptainCellItems[0].mIconType);
}

/**
 * Offset/Address/Size: 0x57C | 0x800C1C70 | size: 0x150
 */
void ICaptainGridComponent::RebuildInstanceTable()
{
    TLSlide* slide;
    int i;

    slide = mParentComponent->GetActiveSlide();

    for (i = 0; i < (int)NUM_CAPTAIN_CELL_ITEMS; i++)
    {
        TLInstance* inst = FEFinder<TLInstance, 2>::Find(slide,
            InlineHasher(nlStringLowerHash(CaptainCellItems[i].mIconName)),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0));
        mInstanceTable[CaptainCellItems[i].mIconType] = inst;
        mMapMenu->ChangeItem(CaptainCellItems[i].mIconType, mInstanceTable[CaptainCellItems[i].mIconType]);
    }

    if (!g_e3_Build)
    {
        if (GameInfoManager::Instance()->IsSuperTeamUnlocked() && mMapMenu->IsItemActive(8))
        {
            mMapMenu->SetItemActive(8, true);
        }
        else
        {
            mMapMenu->SetItemActive(8, false);
        }
    }

    mMapMenu->UpdateAllItems();
}

/**
 * Offset/Address/Size: 0x318 | 0x800C1A0C | size: 0x264
 */
void ICaptainGridComponent::Update(eFEINPUT_PAD pad)
{
    if (!g_e3_Build)
    {
        if (GameInfoManager::Instance()->IsSuperTeamUnlocked() && mMapMenu->IsItemActive(8))
        {
            mMapMenu->SetItemActive(8, true);
        }
        else
        {
            mMapMenu->SetItemActive(8, false);
        }
    }

    int oldSelected = mMapMenu->GetSelectedItem();
    mHasChangedSinceLastUpdate = false;

    if (g_pFEInput->IsAutoPressed(pad, 0xB, true, NULL))
    {
        if (mIsMirrored)
        {
            mMapMenu->MoveRight(false);
        }
        else
        {
            mMapMenu->MoveLeft(false);
        }

        FEAudio::PlayAnimAudioEvent("sfx_character_select_left", false);
    }
    else if (g_pFEInput->IsAutoPressed(pad, 0xC, true, NULL))
    {
        if (mIsMirrored)
        {
            mMapMenu->MoveLeft(false);
        }
        else
        {
            mMapMenu->MoveRight(false);
        }

        FEAudio::PlayAnimAudioEvent("sfx_character_select_right", false);
    }
    else if (g_pFEInput->IsAutoPressed(pad, 0xD, true, NULL))
    {
        mMapMenu->MoveUp(false);
        FEAudio::PlayAnimAudioEvent("sfx_character_select_up", false);
    }
    else if (g_pFEInput->IsAutoPressed(pad, 0xE, true, NULL))
    {
        mMapMenu->MoveDown(false);
        FEAudio::PlayAnimAudioEvent("sfx_character_select_down", false);
    }

    if (oldSelected != mMapMenu->GetSelectedItem())
    {
        mHasChangedSinceLastUpdate = true;
    }

    if (mHighliteVisibilityAtAnimEnd)
    {
        TLSlide* activeSlide = mParentComponent->GetActiveSlide();
        bool shouldShow;

        if (activeSlide == NULL)
        {
            shouldShow = true;
        }
        else if (activeSlide->m_time >= activeSlide->m_start + activeSlide->m_duration)
        {
            shouldShow = true;
        }
        else
        {
            shouldShow = false;
        }

        if (shouldShow)
        {
            mHighliteVisibilityAtAnimEnd = false;
            mHighliteComponent->m_bVisible = true;
        }
    }

    mMapMenu->Update(0.0f);
}

/**
 * Offset/Address/Size: 0x29C | 0x800C1990 | size: 0x7C
 */
void ICaptainGridComponent::UpdateSuperTeamIconState()
{
    if (g_e3_Build)
    {
        return;
    }

    if (GameInfoManager::Instance()->IsSuperTeamUnlocked())
    {
        if (mMapMenu->IsItemActive(8))
        {
            mMapMenu->SetItemActive(8, true);
            return;
        }
    }
    mMapMenu->SetItemActive(8, false);
}

/**
 * Offset/Address/Size: 0x20C | 0x800C1900 | size: 0x90
 */
void ICaptainGridComponent::SetValid(eTeamID teamID, bool valid)
{
    int position;

    switch (teamID)
    {
    case TEAM_DAISY:
        position = 4;
        break;
    case TEAM_DONKEYKONG:
        position = 1;
        break;
    case TEAM_LUIGI:
        position = 5;
        break;
    case TEAM_MARIO:
        position = 0;
        break;
    case TEAM_PEACH:
        position = 3;
        break;
    case TEAM_WARIO:
        position = 2;
        break;
    case TEAM_WALUIGI:
        position = 6;
        break;
    case TEAM_YOSHI:
        position = 7;
        break;
    case TEAM_MYSTERY:
        position = 8;
        break;
    default:
        position = 0;
        break;
    }

    mMapMenu->SetItemActive(position, valid);
}

/**
 * Offset/Address/Size: 0x17C | 0x800C1870 | size: 0x90
 */
bool ICaptainGridComponent::IsValid(eTeamID teamID)
{
    int position;

    switch (teamID)
    {
    case TEAM_DAISY:
        position = 4;
        break;
    case TEAM_DONKEYKONG:
        position = 1;
        break;
    case TEAM_LUIGI:
        position = 5;
        break;
    case TEAM_MARIO:
        position = 0;
        break;
    case TEAM_PEACH:
        position = 3;
        break;
    case TEAM_WARIO:
        position = 2;
        break;
    case TEAM_WALUIGI:
        position = 6;
        break;
    case TEAM_YOSHI:
        position = 7;
        break;
    case TEAM_MYSTERY:
        position = 8;
        break;
    default:
        position = 0;
        break;
    }

    return mMapMenu->IsItemActive(position);
}

/**
 * Offset/Address/Size: 0xB4 | 0x800C17A8 | size: 0xC8
 */
eTeamID ICaptainGridComponent::GetSelectedItem() const
{
    int selectedItem = mMapMenu->GetSelectedItem();
    long i;
    eTeamID result = TEAM_INVALID;

    for (i = 0; i < (long)NUM_CAPTAIN_CELL_ITEMS; i++)
    {
        if (selectedItem != CaptainCellItems[i].mIconType)
        {
            continue;
        }
        switch (CaptainCellItems[i].mIconType)
        {
        case 4:
            result = TEAM_DAISY;
            break;
        case 1:
            result = TEAM_DONKEYKONG;
            break;
        case 5:
            result = TEAM_LUIGI;
            break;
        case 0:
            result = TEAM_MARIO;
            break;
        case 3:
            result = TEAM_PEACH;
            break;
        case 2:
            result = TEAM_WARIO;
            break;
        case 6:
            result = TEAM_WALUIGI;
            break;
        case 7:
            result = TEAM_YOSHI;
            break;
        case 8:
            result = TEAM_MYSTERY;
            break;
        default:
            result = TEAM_MARIO;
            break;
        }
    }
    return result;
}

/**
 * Offset/Address/Size: 0x24 | 0x800C1718 | size: 0x90
 */
void ICaptainGridComponent::MoveHighlightToTarget(eTeamID teamID)
{
    int position;

    switch (teamID)
    {
    case TEAM_DAISY:
        position = 4;
        break;
    case TEAM_DONKEYKONG:
        position = 1;
        break;
    case TEAM_LUIGI:
        position = 5;
        break;
    case TEAM_MARIO:
        position = 0;
        break;
    case TEAM_PEACH:
        position = 3;
        break;
    case TEAM_WARIO:
        position = 2;
        break;
    case TEAM_WALUIGI:
        position = 6;
        break;
    case TEAM_YOSHI:
        position = 7;
        break;
    case TEAM_MYSTERY:
        position = 8;
        break;
    default:
        position = 0;
        break;
    }

    mMapMenu->SetSelectedItem(position);
}

/**
 * Offset/Address/Size: 0x0 | 0x800C16F4 | size: 0x24
 */
void ICaptainGridComponent::SetAllItemsActive()
{
    mMapMenu->SetAllItemsActive();
}
