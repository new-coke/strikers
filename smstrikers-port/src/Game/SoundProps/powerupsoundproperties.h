#ifndef _POWERUPSOUNDPROPERTIES_H_
#define _POWERUPSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class PWRUPSoundPropAccessor : public SoundPropAccessor
{
public:
    PWRUPSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern PWRUPSoundPropAccessor gPWRUPSoundPropAccessor;
extern SoundPropAccessor* gpPWRUPSoundPropAccessor;

#endif // _POWERUPSOUNDPROPERTIES_H_
