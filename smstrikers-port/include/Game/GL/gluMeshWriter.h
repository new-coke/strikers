#ifndef _GLUMESHWRITER_H_
#define _GLUMESHWRITER_H_

#include "Game/GL/GLMeshWriter.h"

#include "NL/nlMath.h"

class GLMeshWriter : public GLMeshWriterCore
{
public:
    using GLMeshWriterCore::Texcoord;

    ~GLMeshWriter() { }
    virtual bool End();
    virtual void Normal(const nlVector3& n);
    virtual void Texcoord(const nlVector2& uv);
    void Texcoord(short u, short v);

}; // total size: 0x70

inline void GLMeshWriterCore::Colour(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    nlColour colour;
    nlColourSet(colour, r, g, b, a);
    Colour(colour);
}

inline void GLMeshWriterCore::Texcoord(float u, float v)
{
    nlVector2 uv;
    uv.x = u;
    uv.y = v;
    Texcoord(uv);
}

inline void GLMeshWriterCore::Position(const nlVector3& v)
{
    Vertex(v);
}

inline void GLMeshWriterCore::Position(float x, float y, float z)
{
    nlVector3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    Vertex(v);
}

#endif // _GLUMESHWRITER_H_
