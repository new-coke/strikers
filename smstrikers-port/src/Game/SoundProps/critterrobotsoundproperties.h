#ifndef _CRITTERROBOTSOUNDPROPERTIES_H_
#define _CRITTERROBOTSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class CRITTERROBOTSoundPropAccessor : public SoundPropAccessor
{
public:
    CRITTERROBOTSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern CRITTERROBOTSoundPropAccessor gCRITTERROBOTSoundPropAccessor;
extern SoundPropAccessor* gpCRITTERROBOTSoundPropAccessor;

#endif // _CRITTERROBOTSOUNDPROPERTIES_H_
