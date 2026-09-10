#ifndef _TWEAKSBASE_H_
#define _TWEAKSBASE_H_

#include "NL/nlString.h"

class TweaksBase
{
public:
    TweaksBase(const char* param_1);
    virtual ~TweaksBase();
    virtual void Init() = 0;

    /* 0x04 */ char mszFileName[32];
}; // total size: 0x24

#endif // _TWEAKSBASE_H_
