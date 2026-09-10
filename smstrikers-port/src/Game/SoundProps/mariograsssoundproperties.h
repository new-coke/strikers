#ifndef _MARIOGRASSSOUNDPROPERTIES_H_
#define _MARIOGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class MARIOGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    MARIOGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern MARIOGRASSSoundPropAccessor gMARIOGRASSSoundPropAccessor;
extern SoundPropAccessor* gpMARIOGRASSSoundPropAccessor;

#endif // _MARIOGRASSSOUNDPROPERTIES_H_
