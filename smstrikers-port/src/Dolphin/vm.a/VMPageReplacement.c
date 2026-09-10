#include <dolphin/types.h>

static u32 g_vmFreePagesExist = 1;
static s32 g_vmPageReplacementPolicy = 1;
static u32 g_vmNextPageToSwap;

u32 __VMGetNumPagesInMRAM(void);
u32 VMBASEGetVirtualAddrFromPageInMRAM(u32 mramPage);
BOOL VMBASEIsPageValid(u32 virtualAddr);
BOOL VMBASEIsPageReferenced(u32 virtualAddr);
BOOL VMBASEIsPageDirty(u32 virtualAddr);
void VMBASESetPageReferenced(u32 virtualAddr, BOOL referenced);
BOOL VMBASEIsPageLocked(u32 mramPage);
u32 OSGetTick(void);

u32 __VMPageReplacementLRU(void);
u32 __VMPageReplacementRandom(void);
u32 __VMPageReplacementFIFO(void);

/**
 * Offset/Address/Size: 0x0 | 0x8025F528 | size: 0x44
 */
u32 __VMGetPageToReplace(void)
{
    if (g_vmPageReplacementPolicy == 0)
    {
        return __VMPageReplacementLRU();
    }
    if (g_vmPageReplacementPolicy == 1)
    {
        return __VMPageReplacementRandom();
    }
    return __VMPageReplacementFIFO();
}

/**
 * Offset/Address/Size: 0x44 | 0x8025F56C | size: 0x1D0
 */
u32 __VMPageReplacementLRU(void)
{
    u32 startPage;
    u32 selected;
    s32 class0x1;
    s32 class1x0;
    s32 class1x1;
    u32 virtualAddr;
    BOOL referenced;
    BOOL dirty;

    selected = 0;
    startPage = g_vmNextPageToSwap;
    class0x1 = -1;
    class1x0 = -1;
    class1x1 = -1;

    if ((s32)g_vmFreePagesExist != 0)
    {
        goto free_page_exists;
    }

loop:
    virtualAddr = VMBASEGetVirtualAddrFromPageInMRAM(g_vmNextPageToSwap);
    if (virtualAddr == 0)
    {
        goto select_current_page;
    }

    if (VMBASEIsPageValid(virtualAddr) == FALSE)
    {
        goto select_current_page;
    }

    referenced = VMBASEIsPageReferenced(virtualAddr);
    dirty = VMBASEIsPageDirty(virtualAddr);

    if (referenced == FALSE && dirty == FALSE)
    {
        if (VMBASEIsPageLocked(g_vmNextPageToSwap) == FALSE)
        {
            selected = g_vmNextPageToSwap;
            goto done_select;
        }
    }

    if (referenced == FALSE && dirty != FALSE)
    {
        if (class0x1 < 0)
        {
            if (VMBASEIsPageLocked(g_vmNextPageToSwap) == FALSE)
            {
                class0x1 = (s32)g_vmNextPageToSwap;
            }
        }
    }
    else if (referenced != FALSE && dirty == FALSE)
    {
        if (class1x0 < 0)
        {
            if (VMBASEIsPageLocked(g_vmNextPageToSwap) == FALSE)
            {
                class1x0 = (s32)g_vmNextPageToSwap;
            }
        }
    }
    else if (class1x1 < 0)
    {
        if (VMBASEIsPageLocked(g_vmNextPageToSwap) == FALSE)
        {
            class1x1 = (s32)g_vmNextPageToSwap;
        }
    }

    if (referenced != FALSE)
    {
        VMBASESetPageReferenced(virtualAddr, FALSE);
    }

    if (startPage == g_vmNextPageToSwap)
    {
        if (class0x1 >= 0)
        {
            selected = (u32)class0x1;
            goto done_select;
        }

        if (class1x0 >= 0)
        {
            selected = (u32)class1x0;
            goto done_select;
        }

        if (class1x1 >= 0)
        {
            selected = (u32)class1x1;
        }
        goto done_select;
    }

    g_vmNextPageToSwap = g_vmNextPageToSwap + 1;
    if (g_vmNextPageToSwap >= __VMGetNumPagesInMRAM())
    {
        g_vmNextPageToSwap = 0;
    }
    goto loop;

select_current_page:
    selected = g_vmNextPageToSwap;
    goto done_select;

free_page_exists:
    selected = startPage;

done_select:
    g_vmNextPageToSwap = g_vmNextPageToSwap + 1;
    if (g_vmNextPageToSwap >= __VMGetNumPagesInMRAM())
    {
        g_vmFreePagesExist = 0;
        g_vmNextPageToSwap = 0;
    }

    return selected;
}

/**
 * Offset/Address/Size: 0x214 | 0x8025F73C | size: 0x94
 */
u32 __VMPageReplacementRandom(void)
{
    u32 page;

    while (TRUE)
    {
        if ((s32)g_vmFreePagesExist != 0)
        {
            u32 nextPage = g_vmNextPageToSwap;

            g_vmNextPageToSwap = nextPage + 1;
            page = nextPage;
            if (g_vmNextPageToSwap >= __VMGetNumPagesInMRAM())
            {
                g_vmFreePagesExist = 0;
                g_vmNextPageToSwap = 0;
            }
        }
        else
        {
            u32 numPages = __VMGetNumPagesInMRAM();

            page = OSGetTick() % numPages;
        }

        if (!VMBASEIsPageLocked(page))
        {
            break;
        }
    }

    return page;
}

/**
 * Offset/Address/Size: 0x2A8 | 0x8025F7D0 | size: 0x6C
 */
u32 __VMPageReplacementFIFO(void)
{
    while (TRUE)
    {
        u32 page = g_vmNextPageToSwap++;

        if (g_vmNextPageToSwap >= __VMGetNumPagesInMRAM())
        {
            g_vmFreePagesExist = 0;
            g_vmNextPageToSwap = 0;
        }

        if (!VMBASEIsPageLocked(page))
        {
            return page;
        }
    }
}
