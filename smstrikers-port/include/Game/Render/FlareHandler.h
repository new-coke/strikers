#ifndef _FLAREHANDLER_H_
#define _FLAREHANDLER_H_

#include "NL/nlMath.h"
#include "Game/GL/gluMeshWriter.h"
#include "Game/Effects/efList.h"

enum eFlareType
{
    Flare_Halo = 0,
    Flare_Glow = 1,
    Flare_Num = 2,
};

class FlareStruct : public efNode
{
public:
    FlareStruct()
    {
    }

    /* 0x08 */ eFlareType type;
    /* 0x0C */ float size;
    /* 0x10 */ nlColour colour;
    /* 0x14 */ nlVector3 worldPosition;
}; // total size: 0x20

class FlareHandler
{
public:
    static FlareHandler instance;

    FlareHandler()
    {
    }

    void Initialize();
    void Cleanup();
    void AddHalo(const nlMatrix4& mat);
    void AddGlow(const nlMatrix4& mat);
    void AddFace(const FlareStruct* pFlare, GLMeshWriter* pMeshWriter);
    void Render();

    /* 0x0, */ nlMatrix4 viewMatrix;
    /* 0x40 */ nlVector3 viewRight;
    /* 0x4C */ nlVector3 viewUp;
    /* 0x58 */ efList halos;
    /* 0x64 */ efList glows;
}; // total size: 0x70

#endif // _FLAREHANDLER_H_
