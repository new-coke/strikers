#ifndef _PEACHCONCRETESOUNDPROPERTIES_H_
#define _PEACHCONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class PEACHCONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    PEACHCONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern PEACHCONCRETESoundPropAccessor gPEACHCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpPEACHCONCRETESoundPropAccessor;

#endif // _PEACHCONCRETESOUNDPROPERTIES_H_
