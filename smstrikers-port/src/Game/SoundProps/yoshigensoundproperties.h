#ifndef _YOSHIGENSOUNDPROPERTIES_H_
#define _YOSHIGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class YOSHISoundPropAccessor : public SoundPropAccessor
{
public:
    YOSHISoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

#endif // _YOSHIGENSOUNDPROPERTIES_H_
