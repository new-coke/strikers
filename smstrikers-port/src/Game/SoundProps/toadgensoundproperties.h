#ifndef _TOADGENSOUNDPROPERTIES_H_
#define _TOADGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class TOADSoundPropAccessor : public SoundPropAccessor
{
public:
    TOADSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern TOADSoundPropAccessor gTOADSoundPropAccessor;
extern SoundPropAccessor* gpTOADSoundPropAccessor;

#endif // _TOADGENSOUNDPROPERTIES_H_
