#ifndef _WALUIGIGENSOUNDPROPERTIES_H_
#define _WALUIGIGENSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class WALUIGISoundPropAccessor : public SoundPropAccessor
{
public:
    WALUIGISoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern WALUIGISoundPropAccessor gWALUIGISoundPropAccessor;
extern SoundPropAccessor* gpWALUIGISoundPropAccessor;

#endif // _WALUIGIGENSOUNDPROPERTIES_H_
