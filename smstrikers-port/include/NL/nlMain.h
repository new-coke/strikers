#ifndef _NLMAIN_H_
#define _NLMAIN_H_

#include "types.h"

u32 nlChecksum32(const void* data, unsigned long size);
void nlInit();

class RunningChecksum
{
public:
    void ChecksumData(const void* pData, unsigned long nDataLen);
    void ChecksumInt(unsigned long value);
    RunningChecksum();

    /* 0x00 */ u32 m_nChecksum;
};

#endif // _NLMAIN_H_
