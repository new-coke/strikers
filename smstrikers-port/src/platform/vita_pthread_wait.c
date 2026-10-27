// VitaSDK pthreads poll their cancel flag every 500 us while waiting, delaying
// signalled waiters. The port never cancels threads, so wait in the kernel.
// Returns pte_osResult: 0 OK, 2 failure, 3 timeout (milliseconds).
#if defined(PORT_VITA)
#include <psp2/kernel/threadmgr.h>

int __wrap_pte_osSemaphoreCancellablePend(SceUID semaphore, unsigned int* timeoutMs)
{
    int result;
    if (timeoutMs)
    {
        SceUInt us = *timeoutMs > 4000000u ? 4000000000u : *timeoutMs * 1000u;
        result = sceKernelWaitSema(semaphore, 1, &us);
    }
    else
        result = sceKernelWaitSema(semaphore, 1, NULL);
    if (result >= 0)
        return 0;
    return result == (int)0x80028005 ? 3 : 2; // SCE_KERNEL_ERROR_WAIT_TIMEOUT
}
#endif
