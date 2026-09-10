#ifndef _YOSHIRUBBERSOUNDPROPERTIES_H_
#define _YOSHIRUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class YOSHIRUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    YOSHIRUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

#endif // _YOSHIRUBBERSOUNDPROPERTIES_H_
