#ifndef _HAMBROSGENSOUNDPROPERTIES_H_
#define _HAMBROSGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class HAMBROSSoundPropAccessor : public SoundPropAccessor
{
public:
    HAMBROSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern HAMBROSSoundPropAccessor gHAMBROSSoundPropAccessor;
extern SoundPropAccessor* gpHAMBROSSoundPropAccessor;

#endif // _HAMBROSGENSOUNDPROPERTIES_H_
