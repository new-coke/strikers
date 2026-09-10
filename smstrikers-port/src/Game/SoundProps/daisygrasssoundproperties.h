#ifndef _DAISYGRASSSOUNDPROPERTIES_H_
#define _DAISYGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class DAISYGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    DAISYGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern DAISYGRASSSoundPropAccessor gDAISYGRASSSoundPropAccessor;
extern SoundPropAccessor* gpDAISYGRASSSoundPropAccessor;

#endif // _DAISYGRASSSOUNDPROPERTIES_H_
