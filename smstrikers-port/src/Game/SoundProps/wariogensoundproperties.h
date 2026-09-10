#ifndef _WARIOGENSOUNDPROPERTIES_H_
#define _WARIOGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class WARIOSoundPropAccessor : public SoundPropAccessor
{
public:
    WARIOSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern WARIOSoundPropAccessor gWARIOSoundPropAccessor;
extern SoundPropAccessor* gpWARIOSoundPropAccessor;

#endif // _WARIOGENSOUNDPROPERTIES_H_
