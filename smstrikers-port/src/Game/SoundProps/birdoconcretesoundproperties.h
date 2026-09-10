#ifndef _BIRDOCONCRETESOUNDPROPERTIES_H_
#define _BIRDOCONCRETESOUNDPROPERTIES_H_

#include "SoundPropAccessor.h"

class BIRDOCONCRETESoundPropAccessor : public SoundPropAccessor
{
public:
    BIRDOCONCRETESoundPropAccessor() { ResetSoundPropTable(); };
    virtual inline SoundProperties* GetSoundProperty(unsigned int index) const;
    virtual inline SoundProperties* GetSoundPropTable();
    virtual inline u32 GetNumSFX() const;
    virtual inline const char* GetSoundPropTableName() const;
    virtual inline const char* GetHTMLFileName() const;
    virtual inline bool IsUsingOrigTable() const;
    virtual inline void ResetSoundPropTable();
};

extern BIRDOCONCRETESoundPropAccessor gBIRDOCONCRETESoundPropAccessor;
extern SoundPropAccessor* gpBIRDOCONCRETESoundPropAccessor;

#endif // _BIRDOCONCRETESOUNDPROPERTIES_H_
