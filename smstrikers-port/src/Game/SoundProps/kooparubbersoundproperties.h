#ifndef _KOOPARUBBERSOUNDPROPERTIES_H_
#define _KOOPARUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class KOOPARUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    KOOPARUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern KOOPARUBBERSoundPropAccessor gKOOPARUBBERSoundPropAccessor;
extern SoundPropAccessor* gpKOOPARUBBERSoundPropAccessor;

#endif // _KOOPARUBBERSOUNDPROPERTIES_H_
