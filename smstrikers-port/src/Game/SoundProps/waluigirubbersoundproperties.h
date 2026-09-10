#ifndef _WALUIGIRUBBERSOUNDPROPERTIES_H_
#define _WALUIGIRUBBERSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class WALUIGIRUBBERSoundPropAccessor : public SoundPropAccessor
{
public:
    WALUIGIRUBBERSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern WALUIGIRUBBERSoundPropAccessor gWALUIGIRUBBERSoundPropAccessor;
extern SoundPropAccessor* gpWALUIGIRUBBERSoundPropAccessor;

#endif // _WALUIGIRUBBERSOUNDPROPERTIES_H_
