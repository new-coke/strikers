#ifndef _SUPERGRASSSOUNDPROPERTIES_H_
#define _SUPERGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class SUPERGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    SUPERGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern SUPERGRASSSoundPropAccessor gSUPERGRASSSoundPropAccessor;
extern SoundPropAccessor* gpSUPERGRASSSoundPropAccessor;

#endif // _SUPERGRASSSOUNDPROPERTIES_H_
