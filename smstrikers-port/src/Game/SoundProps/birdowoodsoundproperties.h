#ifndef _BIRDOWOODSOUNDPROPERTIES_H_
#define _BIRDOWOODSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class BIRDOWOODSoundPropAccessor : public SoundPropAccessor
{
public:
    BIRDOWOODSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern BIRDOWOODSoundPropAccessor gBIRDOWOODSoundPropAccessor;
extern SoundPropAccessor* gpBIRDOWOODSoundPropAccessor;

#endif // _BIRDOWOODSOUNDPROPERTIES_H_
