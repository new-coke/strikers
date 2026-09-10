#ifndef _BOWSERCONCRETESOUNDPROPERTIES_H_
#define _BOWSERCONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class BOWSERCONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    BOWSERCONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern BOWSERCONCRETESoundPropAccessor gBOWSERCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpBOWSERCONCRETESoundPropAccessor;

#endif // _BOWSERCONCRETESOUNDPROPERTIES_H_
