#ifndef _BIRDOMETALSOUNDPROPERTIES_H_
#define _BIRDOMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class BIRDOMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    BIRDOMETALSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern BIRDOMETALSoundPropAccessor gBIRDOMETALSoundPropAccessor;
extern SoundPropAccessor* gpBIRDOMETALSoundPropAccessor;

#endif // _BIRDOMETALSOUNDPROPERTIES_H_
