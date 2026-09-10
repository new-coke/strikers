#ifndef _STADPALACESOUNDPROPERTIES_H_
#define _STADPALACESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class STADPALACESoundPropAccessor : public SoundPropAccessor
{
public:
    STADPALACESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern STADPALACESoundPropAccessor gSTADPALACESoundPropAccessor;
extern SoundPropAccessor* gpSTADPALACESoundPropAccessor;

#endif // _STADPALACESOUNDPROPERTIES_H_
