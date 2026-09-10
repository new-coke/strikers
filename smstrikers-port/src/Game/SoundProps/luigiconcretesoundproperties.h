#ifndef _LUIGICONCRETESOUNDPROPERTIES_H_
#define _LUIGICONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class LUIGICONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    LUIGICONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern LUIGICONCRETESoundPropAccessor gLUIGICONCRETESoundPropAccessor;
extern SoundPropAccessor* gpLUIGICONCRETESoundPropAccessor;

#endif // _LUIGICONCRETESOUNDPROPERTIES_H_
