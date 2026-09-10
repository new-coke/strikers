#ifndef _TOADCONCRETESOUNDPROPERTIES_H_
#define _TOADCONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class TOADCONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    TOADCONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern TOADCONCRETESoundPropAccessor gTOADCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpTOADCONCRETESoundPropAccessor;

#endif // _TOADCONCRETESOUNDPROPERTIES_H_
