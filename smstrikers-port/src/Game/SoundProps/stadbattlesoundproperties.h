#ifndef _STADBATTLESOUNDPROPERTIES_H_
#define _STADBATTLESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class STADBATTLESoundPropAccessor : public SoundPropAccessor
{
public:
    STADBATTLESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern STADBATTLESoundPropAccessor gSTADBATTLESoundPropAccessor;
extern SoundPropAccessor* gpSTADBATTLESoundPropAccessor;

#endif // _STADBATTLESOUNDPROPERTIES_H_
