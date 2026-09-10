#ifndef _STADPIPESOUNDPROPERTIES_H_
#define _STADPIPESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class STADPIPESoundPropAccessor : public SoundPropAccessor
{
public:
    STADPIPESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern STADPIPESoundPropAccessor gSTADPIPESoundPropAccessor;
extern SoundPropAccessor* gpSTADPIPESoundPropAccessor;

#endif // _STADPIPESOUNDPROPERTIES_H_
