#ifndef _MAIN_H_
#define _MAIN_H_

#include "NL/nlLocalization.h"

class LoadingManager;

extern bool g_bProfiling;
extern bool g_bTweaking;
extern bool g_e3_Build;
extern bool g_Europe;
extern bool g_bFranticPausing;
extern nlLocalization::nlLanguage g_Language;
extern LoadingManager* g_pTheLoadingManagerTask;

const int* GetRegion();

#if defined(PORT_VITA)
// Mark gameplay measurement boundaries around scene transitions.
void PortVitaEnterGameplay();
void PortVitaLeaveGameplay();
#endif

#endif // _MAIN_H_
