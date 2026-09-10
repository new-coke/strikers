#ifndef _SUPERGENSOUNDPROPERTIES_H_
#define _SUPERGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class SUPERSoundPropAccessor : public SoundPropAccessor
{
public:
    SUPERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern SUPERSoundPropAccessor gSUPERSoundPropAccessor;
extern SoundPropAccessor* gpSUPERSoundPropAccessor;

#endif // _SUPERGENSOUNDPROPERTIES_H_
