#include "Game/FE/feSidekickGridComponent.h"
#include "Game/FE/feMapMenu.h"
#include "Game/FE/feFinder.h"
#include "Game/FE/tlComponentInstance.h"

struct CellItem
{
    int mIconType;
    const char* mIconName;
};

static const CellItem SidekickCellItems[4] = {
    { 0, "choose_sidek_toad" },
    { 1, "choose_sidek_hammer" },
    { 2, "choose_sidek_birdo" },
    { 3, "choose_sidek_koopa" },
};

/**
 * Offset/Address/Size: 0x648 | 0x800C2E58 | size: 0x48
 */
ISidekickGridComponent::ISidekickGridComponent(TLComponentInstance* parentcomponent, bool ismirrored)
    : IGridComponent<eSidekickID>(parentcomponent, "highlight", ismirrored)
{
}

/**
 * Offset/Address/Size: 0x5B4 | 0x800C2DC4 | size: 0x94
 */
ISidekickGridComponent::~ISidekickGridComponent()
{
}

/**
 * Offset/Address/Size: 0x408 | 0x800C2C18 | size: 0x1AC
 */
void ISidekickGridComponent::BuildMapMenu()
{
    TLSlide* activeslide = mParentComponent->GetActiveSlide();

    mInstanceTable = (TLInstance**)nlMalloc(4 * sizeof(TLInstance*), 8, false);

    for (int i = 0; i < 4; i++)
    {
        mInstanceTable[SidekickCellItems[i].mIconType] = FEFinder<TLInstance, 2>::Find(activeslide,
            InlineHasher(nlStringLowerHash(SidekickCellItems[i].mIconName)),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0),
            InlineHasher(0));
    }

    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            int leftIndex = ((((i * 2 + j) % 2) - 1) < 0) ? (i * 2 + j) + 1 : (i * 2 + j) - 1;
            int rightIndex = ((((i * 2 + j) % 2) + 1) >= 2) ? (i * 2 + j) - 1 : (i * 2 + j) + 1;
            int upIndex = ((i * 2 + j) - 2 + 4) % 4;
            int downIndex = ((i * 2 + j) + 2) % 4;
            const CellItem& currentItem = SidekickCellItems[(unsigned int)(i * 2 + j)];

            mMapMenu->AddItem(
                currentItem.mIconType,
                mInstanceTable[currentItem.mIconType],
                SidekickCellItems[leftIndex].mIconType,
                SidekickCellItems[rightIndex].mIconType,
                SidekickCellItems[upIndex].mIconType,
                SidekickCellItems[downIndex].mIconType,
                true);
        }
    }

    mMapMenu->SetSelectedItem(SidekickCellItems[0].mIconType);
}

/**
 * Offset/Address/Size: 0x324 | 0x800C2B34 | size: 0xE4
 */
void ISidekickGridComponent::RebuildInstanceTable()
{
    TLSlide* activeslide = mParentComponent->GetActiveSlide();

    for (int i = 0; i < 4; i++)
    {
        TLInstance* inst = FEFinder<TLInstance, 2>::Find<TLSlide>(
            activeslide,
            InlineHasher(nlStringLowerHash(SidekickCellItems[i].mIconName)),
            InlineHasher(0));

        int iconType = SidekickCellItems[i].mIconType;
        mInstanceTable[iconType] = inst;
        mMapMenu->ChangeItem(iconType, mInstanceTable[iconType]);
    }
}

/**
 * Offset/Address/Size: 0x114 | 0x800C2924 | size: 0x210
 */
void ISidekickGridComponent::Update(eFEINPUT_PAD pad)
{
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
 * Offset/Address/Size: 0xA4 | 0x800C28B4 | size: 0x70
 */
eSidekickID ISidekickGridComponent::GetSelectedItem() const
{
    int position = mMapMenu->GetSelectedItem();

    switch (position)
    {
    case 0:
        return SK_TOAD;
    case 1:
        return SK_HAMMERBROS;
    case 2:
        return SK_BIRDO;
    case 3:
        return SK_KOOPA;
    default:
        return SK_TOAD;
    }
}

/**
 * Offset/Address/Size: 0x34 | 0x800C2844 | size: 0x70
 */
void ISidekickGridComponent::MoveHighlightToTarget(eSidekickID id)
{
    int item;
    switch (id)
    {
    case SK_TOAD:
        item = 0;
        break;
    case SK_HAMMERBROS:
        item = 1;
        break;
    case SK_BIRDO:
        item = 2;
        break;
    case SK_KOOPA:
        item = 3;
        break;
    default:
        item = 0;
        break;
    }
    mMapMenu->SetSelectedItem(item);
}

/**
 * Offset/Address/Size: 0x0 | 0x800C2810 | size: 0x34
 */
void ISidekickGridComponent::SetVisibleInstanceTable(bool visible)
{
    mInstanceTable[0]->m_bVisible = visible;
    mInstanceTable[1]->m_bVisible = visible;
    mInstanceTable[2]->m_bVisible = visible;
    mInstanceTable[3]->m_bVisible = visible;
}
