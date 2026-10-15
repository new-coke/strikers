#ifndef _DOLPHIN_OSEXEC_H_
#define _DOLPHIN_OSEXEC_H_

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    BOOL valid;
    u32 restartCode;
    u32 bootDol;
    void* regionStart;
    void* regionEnd;
    int argsUseDefault;
    void* argsAddr;
} OSExecParams;

typedef int (*appGetNextCallback)(void*, u32*, u32*);
typedef void (*appInitCallback)(void (*)(char*));
typedef void* (*appGetEntryCallback)();
typedef void (*AppLoaderCallback)(appInitCallback*, appGetNextCallback*, appGetEntryCallback*);

#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
extern OSExecParams* __OSExecParams;
extern s32 __OSAppLoaderOffset;
#else
OSExecParams* __OSExecParams AT_ADDRESS(0x800030F0);
s32 __OSAppLoaderOffset AT_ADDRESS(0x800030F4);
#endif

void OSExecv(const char* dolfile, const char** argv);
void OSExecl(const char* dolfile, const char* arg0, ...);

#ifdef __cplusplus
}
#endif

#endif
