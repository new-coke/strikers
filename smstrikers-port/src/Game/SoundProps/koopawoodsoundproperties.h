#ifndef _KOOPAWOODSOUNDPROPERTIES_H_
#define _KOOPAWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class KOOPAWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    KOOPAWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern KOOPAWOODSoundPropAccessor gKOOPAWOODSoundPropAccessor;
extern SoundPropAccessor* gpKOOPAWOODSoundPropAccessor;

#endif // _KOOPAWOODSOUNDPROPERTIES_H_
