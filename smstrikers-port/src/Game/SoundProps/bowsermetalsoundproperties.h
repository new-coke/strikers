#ifndef _BOWSERMETALSOUNDPROPERTIES_H_
#define _BOWSERMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class BOWSERMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    BOWSERMETALSoundPropAccessor() { ResetSoundPropTable(); }
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern BOWSERMETALSoundPropAccessor gBOWSERMETALSoundPropAccessor;
extern SoundPropAccessor* gpBOWSERMETALSoundPropAccessor;

#endif // _BOWSERMETALSOUNDPROPERTIES_H_
