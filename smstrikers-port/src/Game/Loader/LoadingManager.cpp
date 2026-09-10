#include "Game/Loader/LoadingManager.h"
#include "Game/Sys/debug.h"
#include "NL/nlMemory.h"
#include "NL/nlTicker.h"

/**
 * Offset/Address/Size: 0x338 | 0x801E855C | size: 0x70
 */
LoadingManager::LoadingManager(unsigned int maxEntries)
{
    m_MaxEntries = maxEntries;
    m_CurrEntry = 0;
    m_NumEntries = 0;
    m_LoaderQueue = (Loader**)nlMalloc(maxEntries * sizeof(Loader*), 8, false);
    m_LoadFinished = false;
}

/**
 * Offset/Address/Size: 0x32C | 0x801E8550 | size: 0xC
 */
const char* LoadingManager::GetName()
{
    return "Loading Manager";
}

/**
 * Offset/Address/Size: 0xE0 | 0x801E8304 | size: 0x24C
 */
void LoadingManager::Run(float dt)
{
    u32 startTicker;
    u32 endTicker;
    bool var_r29 = 0;

    if (m_NumEntries == 0)
        return;

    if (m_LoadFinished == 0)
    {
        startTicker = nlGetTicker();
        var_r29 = m_LoaderQueue[m_CurrEntry]->Update();
        endTicker = nlGetTicker();
        float timeDiff = nlGetTickerDifference(startTicker, endTicker);
        if (timeDiff > 34.0f)
        {
            tDebugPrintManager::Print(DC_LOADER, "LoadingManager: Loader %s blocked in Update() for %f ms\n", m_LoaderQueue[m_CurrEntry]->GetName(), timeDiff);
        }
    }

    if ((m_LoadFinished != 0) || (var_r29 != 0))
    {
        m_LoadFinished = 0;
        m_NumEntries -= 1;
        m_CurrEntry = (m_CurrEntry + 1) % m_MaxEntries;
        if (m_NumEntries == 0)
        {
            m_LoadFinished = 1;
            return;
        }
        tDebugPrintManager::Print(DC_LOADER, "LoadingManager: Starting loader: %s\n", m_LoaderQueue[m_CurrEntry]->GetName());
        startTicker = nlGetTicker();
        m_LoaderQueue[m_CurrEntry]->StartLoad(this);
        endTicker = nlGetTicker();
        float timeDiff = nlGetTickerDifference(startTicker, endTicker);
        if (timeDiff > 34.0f)
        {
            tDebugPrintManager::Print(DC_LOADER, "LoadingManager: Loader %s blocked in StartLoad() for %f ms\n", m_LoaderQueue[m_CurrEntry]->GetName(), timeDiff);
            return;
        }
        tDebugPrintManager::Print(DC_LOADER, "LoadingManager: Loader %s took %f in StartLoad()\n", m_LoaderQueue[m_CurrEntry]->GetName(), timeDiff);
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x801E8224 | size: 0xE0
 */
void LoadingManager::QueueLoader(Loader* loader)
{
    bool shouldQueue = true;

    if (m_NumEntries == 0)
    {
        tDebugPrintManager::Print(DC_LOADER, "LoadingManager: Starting loader: %s\n", loader->GetName());
        if (loader->StartLoad(this))
        {
            shouldQueue = false;
        }
    }

    if (shouldQueue)
    {
        u32 index = (m_CurrEntry + m_NumEntries) % m_MaxEntries;
        m_LoaderQueue[index] = loader;
        m_NumEntries++;
        m_LoadFinished = false;
    }
}

/**
 * This function is not called and exists solely to ensure string literals
 * are placed in the correct order in the data segment to match the original binary.
 */
static void _EnsureDataSegmentOrder(char* name, float time)
{
    tDebugPrintManager::Print(DC_LOADER, "LoadingManager: Starting loader: %s\n", name);
    tDebugPrintManager::Print(DC_LOADER, "LoadingManager: Loader %s blocked in StartLoad() for %f ms\n", name, time);
    tDebugPrintManager::Print(DC_LOADER, "LoadingManager: Loader %s took %f in StartLoad()\n", name, time);
    tDebugPrintManager::Print(DC_LOADER, "LoadingManager: Starting loader: %s\n", name);
    tDebugPrintManager::Print(DC_LOADER, "LoadingManager: Loader %s blocked in Update() for %f ms\n", name, time);
}
