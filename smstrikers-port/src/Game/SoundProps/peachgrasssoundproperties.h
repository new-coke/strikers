#ifndef _PEACHGRASSSOUNDPROPERTIES_H_
#define _PEACHGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class PEACHGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    PEACHGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern PEACHGRASSSoundPropAccessor gPEACHGRASSSoundPropAccessor;
extern SoundPropAccessor* gpPEACHGRASSSoundPropAccessor;

#endif // _PEACHGRASSSOUNDPROPERTIES_H_
