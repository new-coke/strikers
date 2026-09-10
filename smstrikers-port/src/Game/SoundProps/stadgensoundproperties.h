#ifndef _STADGENSOUNDPROPERTIES_H_
#define _STADGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class STADGENSoundPropAccessor : public SoundPropAccessor
{
public:
    STADGENSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern STADGENSoundPropAccessor gSTADGENSoundPropAccessor;
extern SoundPropAccessor* gpSTADGENSoundPropAccessor;

#endif // _STADGENSOUNDPROPERTIES_H_
