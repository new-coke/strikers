#ifndef _WARIOCONCRETESOUNDPROPERTIES_H_
#define _WARIOCONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class WARIOCONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    WARIOCONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern WARIOCONCRETESoundPropAccessor gWARIOCONCRETEoundPropAccessor;
extern SoundPropAccessor* gpWARIOCONCRETESoundPropAccessor;

#endif // _WARIOCONCRETESOUNDPROPERTIES_H_
