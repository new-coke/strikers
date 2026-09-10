#ifndef _STADBOWSERSOUNDPROPERTIES_H_
#define _STADBOWSERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class STADBOWSERSoundPropAccessor : public SoundPropAccessor
{
public:
    STADBOWSERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern STADBOWSERSoundPropAccessor gSTADBOWSERSoundPropAccessor;
extern SoundPropAccessor* gpSTADBOWSERSoundPropAccessor;

#endif // _STADBOWSERSOUNDPROPERTIES_H_
