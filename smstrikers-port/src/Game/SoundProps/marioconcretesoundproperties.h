#ifndef _MARIOCONCRETESOUNDPROPERTIES_H_
#define _MARIOCONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class MARIOCONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    MARIOCONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern MARIOCONCRETESoundPropAccessor gMARIOCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpMARIOCONCRETESoundPropAccessor;

#endif // _MARIOCONCRETESOUNDPROPERTIES_H_
