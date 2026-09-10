#ifndef _MARIOWOODSOUNDPROPERTIES_H_
#define _MARIOWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class MARIOWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    MARIOWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern MARIOWOODSoundPropAccessor gMARIOWOODSoundPropAccessor;
extern SoundPropAccessor* gpMARIOWOODSoundPropAccessor;

#endif // _MARIOWOODSOUNDPROPERTIES_H_
