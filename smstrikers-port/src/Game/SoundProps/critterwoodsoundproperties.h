#ifndef _CRITTERWOODSOUNDPROPERTIES_H_
#define _CRITTERWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class CRITTERWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    CRITTERWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern CRITTERWOODSoundPropAccessor gCRITTERWOODSoundPropAccessor;
extern SoundPropAccessor* gpCRITTERWOODSoundPropAccessor;

#endif // _CRITTERWOODSOUNDPROPERTIES_H_
