#ifndef _DAISYCONCRETESOUNDPROPERTIES_H_
#define _DAISYCONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class DAISYCONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    DAISYCONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern DAISYCONCRETESoundPropAccessor gDAISYCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpDAISYCONCRETESoundPropAccessor;

#endif // _DAISYCONCRETESOUNDPROPERTIES_H_
