#ifndef _SUPERCONCRETESOUNDPROPERTIES_H_
#define _SUPERCONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class SUPERCONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    SUPERCONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern SUPERCONCRETESoundPropAccessor gSUPERCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpSUPERCONCRETESoundPropAccessor;

#endif // _SUPERCONCRETESOUNDPROPERTIES_H_
