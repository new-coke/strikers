#ifndef _YOSHIGRASSSOUNDPROPERTIES_H_
#define _YOSHIGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class YOSHIGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    YOSHIGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

#endif // _YOSHIGRASSSOUNDPROPERTIES_H_
