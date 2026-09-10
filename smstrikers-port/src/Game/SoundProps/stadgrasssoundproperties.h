#ifndef _STADGRASSSOUNDPROPERTIES_H_
#define _STADGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class STADGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    STADGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern STADGRASSSoundPropAccessor gSTADGRASSSoundPropAccessor;
extern SoundPropAccessor* gpSTADGRASSSoundPropAccessor;

#endif // _STADGRASSSOUNDPROPERTIES_H_
