#ifndef _DKMETALSOUNDPROPERTIES_H_
#define _DKMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class DKMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    DKMETALSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern DKMETALSoundPropAccessor gDKMETALSoundPropAccessor;
extern SoundPropAccessor* gpDKMETALSoundPropAccessor;

#endif // _DKMETALSOUNDPROPERTIES_H_
