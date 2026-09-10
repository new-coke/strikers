#ifndef _TOADWOODSOUNDPROPERTIES_H_
#define _TOADWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class TOADWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    TOADWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern TOADWOODSoundPropAccessor gTOADWOODSoundPropAccessor;
extern SoundPropAccessor* gpTOADWOODSoundPropAccessor;

#endif // _TOADWOODSOUNDPROPERTIES_H_
