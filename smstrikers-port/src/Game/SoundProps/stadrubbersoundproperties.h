#ifndef _STADRUBBERSOUNDPROPERTIES_H_
#define _STADRUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class STADRUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    STADRUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern STADRUBBERSoundPropAccessor gSTADRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpSTADRUBBERSoundPropAccessor;

#endif // _STADRUBBERSOUNDPROPERTIES_H_
