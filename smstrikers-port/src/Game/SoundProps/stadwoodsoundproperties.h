#ifndef _STADWOODSOUNDPROPERTIES_H_
#define _STADWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class STADWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    STADWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern STADWOODSoundPropAccessor gSTADWOODSoundPropAccessor;
extern SoundPropAccessor* gpSTADWOODSoundPropAccessor;

#endif // _STADWOODSOUNDPROPERTIES_H_
