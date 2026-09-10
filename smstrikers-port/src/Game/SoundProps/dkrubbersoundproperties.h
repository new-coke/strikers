#ifndef _DKRUBBERSOUNDPROPERTIES_H_
#define _DKRUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class DKRUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    DKRUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern DKRUBBERSoundPropAccessor gDKRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpDKRUBBERSoundPropAccessor;

#endif // _DKRUBBERSOUNDPROPERTIES_H_
