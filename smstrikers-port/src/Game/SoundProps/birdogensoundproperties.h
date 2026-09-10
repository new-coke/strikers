#ifndef _BIRDOGENSOUNDPROPERTIES_H_
#define _BIRDOGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class BIRDOSoundPropAccessor : public SoundPropAccessor
{
public:
    BIRDOSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern BIRDOSoundPropAccessor gBIRDOSoundPropAccessor;
extern SoundPropAccessor* gpBIRDOSoundPropAccessor;

#endif // _BIRDOGENSOUNDPROPERTIES_H_
