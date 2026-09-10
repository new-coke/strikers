#ifndef _CRITTERMETALSOUNDPROPERTIES_H_
#define _CRITTERMETALSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class CRITTERMETALSoundPropAccessor : public SoundPropAccessor
{
public:
    CRITTERMETALSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern CRITTERMETALSoundPropAccessor gCRITTERMETALSoundPropAccessor;
extern SoundPropAccessor* gpCRITTERMETALSoundPropAccessor;

#endif // _CRITTERMETALSOUNDPROPERTIES_H_
