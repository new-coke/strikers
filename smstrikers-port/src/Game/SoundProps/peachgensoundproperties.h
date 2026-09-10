#ifndef _PEACHGENSOUNDPROPERTIES_H_
#define _PEACHGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class PEACHSoundPropAccessor : public SoundPropAccessor
{
public:
    PEACHSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern PEACHSoundPropAccessor gPEACHSoundPropAccessor;
extern SoundPropAccessor* gpPEACHSoundPropAccessor;

#endif // _PEACHGENSOUNDPROPERTIES_H_
