#ifndef _STADCONCRETESOUNDPROPERTIES_H_
#define _STADCONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class STADCONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    STADCONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern STADCONCRETESoundPropAccessor gSTADCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpSTADCONCRETESoundPropAccessor;

#endif // _STADCONCRETESOUNDPROPERTIES_H_
