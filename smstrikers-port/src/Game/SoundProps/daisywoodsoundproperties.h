#ifndef _DAISYWOODSOUNDPROPERTIES_H_
#define _DAISYWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class DAISYWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    DAISYWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern DAISYWOODSoundPropAccessor gDAISYWOODSoundPropAccessor;
extern SoundPropAccessor* gpDAISYWOODSoundPropAccessor;

#endif // _DAISYWOODSOUNDPROPERTIES_H_
