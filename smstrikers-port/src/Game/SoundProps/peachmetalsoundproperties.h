#ifndef _PEACHMETALSOUNDPROPERTIES_H_
#define _PEACHMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class PEACHMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    PEACHMETALSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern PEACHMETALSoundPropAccessor gPEACHMETALSoundPropAccessor;
extern SoundPropAccessor* gpPEACHMETALSoundPropAccessor;

#endif // _PEACHMETALSOUNDPROPERTIES_H_
