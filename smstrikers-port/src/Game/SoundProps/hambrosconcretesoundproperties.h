#ifndef _HAMBROSCONCRETESOUNDPROPERTIES_H_
#define _HAMBROSCONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class HAMBROSCONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    HAMBROSCONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern HAMBROSCONCRETESoundPropAccessor gHAMBROSCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpHAMBROSCONCRETESoundPropAccessor;

#endif // _HAMBROSCONCRETESOUNDPROPERTIES_H_
