#ifndef _WALUIGIMETALSOUNDPROPERTIES_H_
#define _WALUIGIMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class WALUIGIMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    WALUIGIMETALSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern WALUIGIMETALSoundPropAccessor gWALUIGIMETALSoundPropAccessor;
extern SoundPropAccessor* gpWALUIGIMETALSoundPropAccessor;

#endif // _WALUIGIMETALSOUNDPROPERTIES_H_
