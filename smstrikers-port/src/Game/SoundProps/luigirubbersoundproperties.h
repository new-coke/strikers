#ifndef _LUIGIRUBBERSOUNDPROPERTIES_H_
#define _LUIGIRUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class LUIGIRUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    LUIGIRUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern LUIGIRUBBERSoundPropAccessor gLUIGIRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpLUIGIRUBBERSoundPropAccessor;

#endif // _LUIGIRUBBERSOUNDPROPERTIES_H_
