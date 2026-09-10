#ifndef _DKGRASSSOUNDPROPERTIES_H_
#define _DKGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class DKGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    DKGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern DKGRASSSoundPropAccessor gDKGRASSSoundPropAccessor;
extern SoundPropAccessor* gpDKGRASSSoundPropAccessor;

#endif // _DKGRASSSOUNDPROPERTIES_H_
