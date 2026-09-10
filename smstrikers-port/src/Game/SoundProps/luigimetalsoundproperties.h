#ifndef _LUIGIMETALSOUNDPROPERTIES_H_
#define _LUIGIMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class LUIGIMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    LUIGIMETALSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern LUIGIMETALSoundPropAccessor gLUIGIMETALSoundPropAccessor;
extern SoundPropAccessor* gpLUIGIMETALSoundPropAccessor;

#endif // _LUIGIMETALSOUNDPROPERTIES_H_
