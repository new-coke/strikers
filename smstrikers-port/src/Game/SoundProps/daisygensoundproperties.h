#ifndef _DAISYGENSOUNDPROPERTIES_H_
#define _DAISYGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class DAISYSoundPropAccessor : public SoundPropAccessor
{
public:
    DAISYSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern DAISYSoundPropAccessor gDAISYSoundPropAccessor;
extern SoundPropAccessor* gpDAISYSoundPropAccessor;

#endif // _DAISYGENSOUNDPROPERTIES_H_
