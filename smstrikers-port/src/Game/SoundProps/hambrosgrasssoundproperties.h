#ifndef _HAMBROSGRASSSOUNDPROPERTIES_H_
#define _HAMBROSGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class HAMBROSGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    HAMBROSGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern HAMBROSGRASSSoundPropAccessor gHAMBROSGRASSSoundPropAccessor;
extern SoundPropAccessor* gpHAMBROSGRASSSoundPropAccessor;

#endif // _HAMBROSGRASSSOUNDPROPERTIES_H_
