#ifndef _TOADGRASSSOUNDPROPERTIES_H_
#define _TOADGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class TOADGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    TOADGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern TOADGRASSSoundPropAccessor gTOADGRASSSoundPropAccessor;
extern SoundPropAccessor* gpTOADGRASSSoundPropAccessor;

#endif // _TOADGRASSSOUNDPROPERTIES_H_
