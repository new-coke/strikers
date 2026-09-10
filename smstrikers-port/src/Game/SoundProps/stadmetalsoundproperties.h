#ifndef _STADMETALSOUNDPROPERTIES_H_
#define _STADMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class STADMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    STADMETALSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern STADMETALSoundPropAccessor gSTADMETALSoundPropAccessor;
extern SoundPropAccessor* gpSTADMETALSoundPropAccessor;

#endif // _STADMETALSOUNDPROPERTIES_H_
