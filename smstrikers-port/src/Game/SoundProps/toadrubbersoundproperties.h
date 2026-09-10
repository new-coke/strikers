#ifndef _TOADRUBBERSOUNDPROPERTIES_H_
#define _TOADRUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class TOADRUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    TOADRUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern TOADRUBBERSoundPropAccessor gTOADRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpTOADRUBBERSoundPropAccessor;

#endif // _TOADRUBBERSOUNDPROPERTIES_H_
