#if defined(STRIKERS_VITA_NATIVE_RUNTIME)
#include "port/overlay.h"
#include "port/benchmark.h"
#include "port/config.h"
#include "NL/gl/glFont.h"
#include <cstdio>
#include <cstring>
namespace {char scene[64]="-";int quit=0,forcedStadium=-1;PortDebugMatch match{};PortDebugSession session{};PortDebugCommand commands[32]{};unsigned head=0,count=0;}
extern "C" {
void PortOverlayInit(){}
int PortOverlayEnabled(){return PortFpsOverlayEnabled();}
void PortOverlayDraw(){if(!PortFpsOverlayEnabled())return;PortBenchLive live;PortBenchGetLive(&live);nlColour white={{255,255,255,255}};glFontBegin(false);glFontPrintf(GLV_Debug,1,1,white,"%.1f FPS  %.2f ms",live.fps,live.frameMs);glFontEnd();}
void PortOverlaySetScene(const char* name){std::snprintf(scene,sizeof scene,"%s",name?name:"-");}
void PortOverlaySetStadium(const char*){}
void PortOverlaySetMatch(float,int,int){}
const char* PortOverlaySceneName(){return scene;}
void PortOverlayToggleMenu(){}
int PortOverlayMenuOpen(){return 0;}
void PortOverlayHandleKey(int,int){}
void PortOverlayVitaInput(unsigned){}
int PortVitaDebugMenuEnabled(){return 0;}
int PortQuitRequested(){return quit;}
void PortRequestQuit(){quit=1;}
int PortDebugForcedStadium(){return forcedStadium;}
void PortDebugSetForcedStadium(int n){forcedStadium=n;}
void PortDebugSetPad(int,int,unsigned,int,int,int,int,int,int){}
void PortDebugSetMatch(const PortDebugMatch* m){if(m)match=*m;}
const PortDebugMatch* PortDebugGetMatch(){return &match;}
void PortDebugSetSession(const PortDebugSession* s){if(s)session=*s;}
const PortDebugSession* PortDebugGetSession(){return &session;}
int PortDebugPushCommand(const PortDebugCommand* c){if(!c||count==32)return 0;commands[(head+count)&31]=*c;++count;return 1;}
int PortDebugPopCommand(PortDebugCommand* c){if(!c||!count)return 0;*c=commands[head];head=(head+1)&31;--count;return 1;}
}
void PortVitaEnterGameplay(){PortBenchRendererGameplayStart();}
void PortVitaLeaveGameplay(){PortBenchRendererGameplayEnd();}
#endif
