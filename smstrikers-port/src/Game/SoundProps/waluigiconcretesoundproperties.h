#ifndef _WALUIGICONCRETESOUNDPROPERTIES_H_
#define _WALUIGICONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class WALUIGICONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    WALUIGICONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern WALUIGICONCRETESoundPropAccessor gWALUIGICONCRETESoundPropAccessor;
extern SoundPropAccessor* gpWALUIGICONCRETESoundPropAccessor;

#endif // _WALUIGICONCRETESOUNDPROPERTIES_H_
