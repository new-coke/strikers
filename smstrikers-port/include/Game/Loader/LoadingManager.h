#ifndef _LOADINGMANAGER_H_
#define _LOADINGMANAGER_H_

#include "Game/Loader.h"
#include "NL/nlTask.h"

class LoadingManager : public nlTask
{
public:
    LoadingManager(unsigned int maxEntries);
    virtual void Run(float dt);
    virtual const char* GetName();
    void QueueLoader(Loader* loader);

    /* 0x18 */ u32 m_MaxEntries;
    /* 0x1C */ u32 m_CurrEntry;
    /* 0x20 */ u32 m_NumEntries;
    /* 0x24 */ Loader** m_LoaderQueue;
    /* 0x28 */ bool m_LoadFinished;
}; // total size: 0x2C

#endif // _LOADINGMANAGER_H_
