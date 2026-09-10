#ifndef _MARIOGENSOUNDPROPERTIES_H_
#define _MARIOGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class MARIOSoundPropAccessor : public SoundPropAccessor
{
public:
    MARIOSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern MARIOSoundPropAccessor gMARIOSoundPropAccessor;
extern SoundPropAccessor* gpMARIOSoundPropAccessor;

#endif // _MARIOGENSOUNDPROPERTIES_H_
