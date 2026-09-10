#ifndef _BOWSERGENSOUNDPROPERTIES_H_
#define _BOWSERGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class BOWSERSoundPropAccessor : public SoundPropAccessor
{
public:
    BOWSERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern BOWSERSoundPropAccessor gBOWSERSoundPropAccessor;
extern SoundPropAccessor* gpBOWSERSoundPropAccessor;

#endif // _BOWSERGENSOUNDPROPERTIES_H_
