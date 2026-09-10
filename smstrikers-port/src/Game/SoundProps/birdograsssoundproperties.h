#ifndef _BIRDOGRASSSOUNDPROPERTIES_H_
#define _BIRDOGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class BIRDOGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    BIRDOGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern BIRDOGRASSSoundPropAccessor gBIRDOGRASSSoundPropAccessor;
extern SoundPropAccessor* gpBIRDOGRASSSoundPropAccessor;

#endif // _BIRDOGRASSSOUNDPROPERTIES_H_
