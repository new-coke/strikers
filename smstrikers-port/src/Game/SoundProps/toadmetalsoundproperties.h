#ifndef _TOADMETALSOUNDPROPERTIES_H_
#define _TOADMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class TOADMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    TOADMETALSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern TOADMETALSoundPropAccessor gTOADMETALSoundPropAccessor;
extern SoundPropAccessor* gpTOADMETALSoundPropAccessor;

#endif // _TOADMETALSOUNDPROPERTIES_H_
