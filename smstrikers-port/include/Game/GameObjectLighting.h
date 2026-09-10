#ifndef _GAMEOBJECTLIGHTING_H_
#define _GAMEOBJECTLIGHTING_H_

#include "types.h"

u32 GetGameObjectLightRamp();
void UpdateGameObjectLighting();
void InitializeGameObjectLighting();
bool AlwaysUseCameraRelativeCharacterLighting();
void* GetInGameLightData();
void* GetCameraRelativeLightData();

#endif // _GAMEOBJECTLIGHTING_H_
