#ifndef _BOWSERGRASSSOUNDPROPERTIES_H_
#define _BOWSERGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class BOWSERGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    BOWSERGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern BOWSERGRASSSoundPropAccessor gBOWSERGRASSSoundPropAccessor;
extern SoundPropAccessor* gpBOWSERGRASSSoundPropAccessor;

#endif // _BOWSERGRASSSOUNDPROPERTIES_H_
