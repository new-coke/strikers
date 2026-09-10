#ifndef _STADUNDERSOUNDPROPERTIES_H_
#define _STADUNDERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class STADUNDERSoundPropAccessor : public SoundPropAccessor
{
public:
    STADUNDERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern STADUNDERSoundPropAccessor gSTADUNDERSoundPropAccessor;
extern SoundPropAccessor* gpSTADUNDERSoundPropAccessor;

#endif // _STADUNDERSOUNDPROPERTIES_H_
