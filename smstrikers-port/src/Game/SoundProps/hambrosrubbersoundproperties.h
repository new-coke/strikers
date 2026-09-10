#ifndef _HAMBROSRUBBERSOUNDPROPERTIES_H_
#define _HAMBROSRUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class HAMBROSRUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    HAMBROSRUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern HAMBROSRUBBERSoundPropAccessor gHAMBROSRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpHAMBROSRUBBERSoundPropAccessor;

#endif // _HAMBROSRUBBERSOUNDPROPERTIES_H_
