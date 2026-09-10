#ifndef _CRITTERGENSOUNDPROPERTIES_H_
#define _CRITTERGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class CRITTERSoundPropAccessor : public SoundPropAccessor
{
public:
    CRITTERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern CRITTERSoundPropAccessor gCRITTERSoundPropAccessor;
extern SoundPropAccessor* gpCRITTERSoundPropAccessor;

#endif // _CRITTERGENSOUNDPROPERTIES_H_
