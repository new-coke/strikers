#ifndef _SUPERMETALSOUNDPROPERTIES_H_
#define _SUPERMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class SUPERMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    SUPERMETALSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern SUPERMETALSoundPropAccessor gSUPERMETALSoundPropAccessor;
extern SoundPropAccessor* gpSUPERMETALSoundPropAccessor;

#endif // _SUPERMETALSOUNDPROPERTIES_H_
