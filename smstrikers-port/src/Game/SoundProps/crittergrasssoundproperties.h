#ifndef _CRITTERGRASSSOUNDPROPERTIES_H_
#define _CRITTERGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class CRITTERGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    CRITTERGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern CRITTERGRASSSoundPropAccessor gCRITTERGRASSSoundPropAccessor;
extern SoundPropAccessor* gpCRITTERGRASSSoundPropAccessor;

#endif // _CRITTERGRASSSOUNDPROPERTIES_H_
