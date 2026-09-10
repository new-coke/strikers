#ifndef _DKGENSOUNDPROPERTIES_H_
#define _DKGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class DKSoundPropAccessor : public SoundPropAccessor
{
public:
    DKSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern DKSoundPropAccessor gDKSoundPropAccessor;
extern SoundPropAccessor* gpDKSoundPropAccessor;

#endif // _DKGENSOUNDPROPERTIES_H_
