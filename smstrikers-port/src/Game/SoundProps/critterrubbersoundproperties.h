#ifndef _CRITTERRUBBERSOUNDPROPERTIES_H_
#define _CRITTERRUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class CRITTERRUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    CRITTERRUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern CRITTERRUBBERSoundPropAccessor gCRITTERRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpCRITTERRUBBERSoundPropAccessor;

#endif // _CRITTERRUBBERSOUNDPROPERTIES_H_
