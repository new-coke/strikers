#ifndef _STADKONGASOUNDPROPERTIES_H_
#define _STADKONGASOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class STADKONGASoundPropAccessor : public SoundPropAccessor
{
public:
    STADKONGASoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern STADKONGASoundPropAccessor gSTADKONGASoundPropAccessor;
extern SoundPropAccessor* gpSTADKONGASoundPropAccessor;

#endif // _STADKONGASOUNDPROPERTIES_H_
