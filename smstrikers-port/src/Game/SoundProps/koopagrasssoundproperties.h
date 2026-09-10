#ifndef _KOOPAGRASSSOUNDPROPERTIES_H_
#define _KOOPAGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class KOOPAGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    KOOPAGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern KOOPAGRASSSoundPropAccessor gKOOPAGRASSSoundPropAccessor;
extern SoundPropAccessor* gpKOOPAGRASSSoundPropAccessor;

#endif // _KOOPAGRASSSOUNDPROPERTIES_H_
