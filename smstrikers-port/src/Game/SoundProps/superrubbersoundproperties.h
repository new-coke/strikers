#ifndef _SUPERRUBBERSOUNDPROPERTIES_H_
#define _SUPERRUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class SUPERRUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    SUPERRUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern SUPERRUBBERSoundPropAccessor gSUPERRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpSUPERRUBBERSoundPropAccessor;

#endif // _SUPERRUBBERSOUNDPROPERTIES_H_
