#ifndef _WARIORUBBERSOUNDPROPERTIES_H_
#define _WARIORUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class WARIORUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    WARIORUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern WARIORUBBERSoundPropAccessor gWARIORUBBERSoundPropAccessor;
extern SoundPropAccessor* gpWARIORUBBERSoundPropAccessor;

#endif // _WARIORUBBERSOUNDPROPERTIES_H_
