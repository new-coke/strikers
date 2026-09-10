#ifndef _WALUIGIGRASSSOUNDPROPERTIES_H_
#define _WALUIGIGRASSSOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class WALUIGIGRASSSoundPropAccessor : public SoundPropAccessor
{
public:
    WALUIGIGRASSSoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern WALUIGIGRASSSoundPropAccessor gWALUIGIGRASSSoundPropAccessor;
extern SoundPropAccessor* gpWALUIGIGRASSSoundPropAccessor;

#endif // _WALUIGIGRASSSOUNDPROPERTIES_H_
