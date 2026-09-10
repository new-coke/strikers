#ifndef _TIMEREGIONS_H_
#define _TIMEREGIONS_H_

#include "NL/nlList.h"
#include "NL/nlMemory.h"
#include "Game/GameTweaks.h"

void DestroyTimeRegions();
void InitializeTimeRegions();
void WriteFrameRateStatsToFile();

class TimeRegion
{
public:
    static nlListContainer<TimeRegion*> sTimeRegionList;

    TimeRegion(const char* pName, bool (*pConditionFunc)())
        : m_pName(pName)
        , m_pConditionFunc(pConditionFunc)
        , m_fThreshold(0.0f)
        , m_unk10(0)
        , m_unk14(0)
    {
        ListEntry<TimeRegion*>* entry = (ListEntry<TimeRegion*>*)nlMalloc(sizeof(ListEntry<TimeRegion*>), 8, false);
        if (entry != nullptr)
        {
            entry->next = nullptr;
            entry->entry = this;
        }
        nlListAddEnd<ListEntry<TimeRegion*> >(&sTimeRegionList.m_Head, &sTimeRegionList.m_Tail, entry);
    }

    /**
     * Offset/Address/Size: 0x888 | 0x801626A8 | size: 0x100
     */
    virtual ~TimeRegion()
    {
        sTimeRegionList.RemoveEntry(this);
    }

    const char* m_pName;
    bool (*m_pConditionFunc)();
    float m_fThreshold;
    int m_unk10;
    int m_unk14;
};

#endif // _TIMEREGIONS_H_
