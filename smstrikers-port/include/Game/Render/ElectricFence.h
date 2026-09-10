#ifndef _ELECTRICFENCE_H_
#define _ELECTRICFENCE_H_

#include "Game/Effects/EmissionController.h"
#include "NL/nlList.h"

class ElectricFenceGeometry
{
public:
    /* 0x000 */ nlVector3 position[32];
    /* 0x180 */ nlVector2 texcoord[32];
    /* 0x280 */ int vertCount;

    static SlotPool<ElectricFenceGeometry> sElectricFenceGeometryPool;
}; // total size: 0x284

class ElectricFenceData
{
public:
    ElectricFenceData(EmissionController* pEmissionController);

    /* 0x00 */ EmissionController* mpEmissionController;
    /* 0x04 */ float mfIntensity;
    /* 0x08 */ nlVector3 mPosition;
    /* 0x14 */ nlVector3 mNormal;
    /* 0x20 */ u8 mbIsFlat;
    /* 0x21 */ u8 pad_21[3];
    /* 0x24 */ ElectricFenceGeometry* mpGeometry;
    /* 0x28 */ ElectricFenceData* next;

    static nlList<ElectricFenceData> sActiveElectricFences;
    static int numAllocated;
    static SlotPool<ElectricFenceData> sElectricFenceDataPool;
}; // total size: 0x2C

void UpdateElectricFence(float fDeltaT);
void StopDisplayingElectricFence();
void DisplayElectricFence();
void FreeElectricFence();
void InitializeElectricFence();
void EmitElectricFenceCharacterEffect(const nlVector3& pos, const nlVector3& dir, unsigned long emitterID);
void EmitElectricFenceBallEffect(const nlVector3& pos, const nlVector3& dir, unsigned long emitterID, bool bNoSpark);

#endif // _ELECTRICFENCE_H_
