#ifndef _STADCRATERSOUNDPROPERTIES_H_
#define _STADCRATERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class STADCRATERSoundPropAccessor : public SoundPropAccessor
{
public:
    STADCRATERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern STADCRATERSoundPropAccessor gSTADCRATERSoundPropAccessor;
extern SoundPropAccessor* gpSTADCRATERSoundPropAccessor;

#endif // _STADCRATERSOUNDPROPERTIES_H_
