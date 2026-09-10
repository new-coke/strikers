#ifndef _HAMBROSMETALSOUNDPROPERTIES_H_
#define _HAMBROSMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class HAMBROSMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    HAMBROSMETALSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern HAMBROSMETALSoundPropAccessor gHAMBROSMETALSoundPropAccessor;
extern SoundPropAccessor* gpHAMBROSMETALSoundPropAccessor;

#endif // _HAMBROSMETALSOUNDPROPERTIES_H_
