#ifndef _WARIOGRASSSOUNDPROPERTIES_H_
#define _WARIOGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class WARIOGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    WARIOGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern WARIOGRASSSoundPropAccessor gWARIOGRASSSoundPropAccessor;
extern SoundPropAccessor* gpWARIOGRASSSoundPropAccessor;

#endif // _WARIOGRASSSOUNDPROPERTIES_H_
