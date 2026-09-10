#ifndef _KOOPAGENSOUNDPROPERTIES_H_
#define _KOOPAGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class KOOPASoundPropAccessor : public SoundPropAccessor
{
public:
    KOOPASoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern KOOPASoundPropAccessor gKOOPASoundPropAccessor;
extern SoundPropAccessor* gpKOOPASoundPropAccessor;

#endif // _KOOPAGENSOUNDPROPERTIES_H_
