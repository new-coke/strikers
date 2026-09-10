#ifndef _KOOPACONCRETESOUNDPROPERTIES_H_
#define _KOOPACONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class KOOPACONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    KOOPACONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern KOOPACONCRETESoundPropAccessor gKOOPACONCRETESoundPropAccessor;
extern SoundPropAccessor* gpKOOPACONCRETESoundPropAccessor;

#endif // _KOOPACONCRETESOUNDPROPERTIES_H_
