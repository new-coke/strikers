#ifndef _MARIORUBBERSOUNDPROPERTIES_H_
#define _MARIORUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class MARIORUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    MARIORUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern MARIORUBBERSoundPropAccessor gMARIORUBBERSoundPropAccessor;
extern SoundPropAccessor* gpMARIORUBBERSoundPropAccessor;

#endif // _MARIORUBBERSOUNDPROPERTIES_H_
