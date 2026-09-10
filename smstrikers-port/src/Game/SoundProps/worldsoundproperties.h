#ifndef _WORLDSOUNDPROPERTIES_H_
#define _WORLDSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class WORLDSoundPropAccessor : public SoundPropAccessor
{
public:
    WORLDSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern WORLDSoundPropAccessor gWORLDSoundPropAccessor;
extern SoundPropAccessor* gpWORLDSoundPropAccessor;

#endif // _WORLDSOUNDPROPERTIES_H_
