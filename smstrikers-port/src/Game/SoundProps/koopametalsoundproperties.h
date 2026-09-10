#ifndef _KOOPAMETALSOUNDPROPERTIES_H_
#define _KOOPAMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class KOOPAMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    KOOPAMETALSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern KOOPAMETALSoundPropAccessor gKOOPAMETALSoundPropAccessor;
extern SoundPropAccessor* gpKOOPAMETALSoundPropAccessor;

#endif // _KOOPAMETALSOUNDPROPERTIES_H_
