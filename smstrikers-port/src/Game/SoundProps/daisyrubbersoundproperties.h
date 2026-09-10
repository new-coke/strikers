#ifndef _DAISYRUBBERSOUNDPROPERTIES_H_
#define _DAISYRUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class DAISYRUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    DAISYRUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern DAISYRUBBERSoundPropAccessor gDAISYRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpDAISYRUBBERSoundPropAccessor;

#endif // _DAISYRUBBERSOUNDPROPERTIES_H_
