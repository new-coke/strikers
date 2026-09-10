#ifndef _YOSHIWOODSOUNDPROPERTIES_H_
#define _YOSHIWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class WARIOWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    WARIOWOODSoundPropAccessor() { ResetSoundPropTable(); };

    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern WARIOWOODSoundPropAccessor gWARIOWOODSoundPropAccessor;
extern SoundPropAccessor* gpWARIOWOODSoundPropAccessor;

#endif // _YOSHIWOODSOUNDPROPERTIES_H_
