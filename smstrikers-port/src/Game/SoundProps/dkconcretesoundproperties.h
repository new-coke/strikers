#ifndef _DKCONCRETESOUNDPROPERTIES_H_
#define _DKCONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class DKCONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    DKCONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern DKCONCRETESoundPropAccessor gDKCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpDKCONCRETESoundPropAccessor;

#endif // _DKCONCRETESOUNDPROPERTIES_H_
