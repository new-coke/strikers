#ifndef _CRITTERCONCRETESOUNDPROPERTIES_H_
#define _CRITTERCONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class CRITTERCONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    CRITTERCONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern CRITTERCONCRETESoundPropAccessor gCRITTERCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpCRITTERCONCRETESoundPropAccessor;

#endif // _CRITTERCONCRETESOUNDPROPERTIES_H_
