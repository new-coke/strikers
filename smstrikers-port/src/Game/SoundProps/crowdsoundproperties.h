#ifndef _CROWDSOUNDPROPERTIES_H_
#define _CROWDSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class CROWDSoundPropAccessor : public SoundPropAccessor
{
public:
    CROWDSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern CROWDSoundPropAccessor gCROWDSoundPropAccessor;
extern SoundPropAccessor* gpCROWDSoundPropAccessor;

#endif // _CROWDSOUNDPROPERTIES_H_
