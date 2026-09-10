#ifndef _PEACHRUBBERSOUNDPROPERTIES_H_
#define _PEACHRUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class PEACHRUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    PEACHRUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern PEACHRUBBERSoundPropAccessor gPEACHRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpPEACHRUBBERSoundPropAccessor;

#endif // _PEACHRUBBERSOUNDPROPERTIES_H_
