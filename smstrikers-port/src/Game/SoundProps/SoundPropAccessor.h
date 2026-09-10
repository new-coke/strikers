#ifndef SOUND_PROP_ACCESSOR_H
#define SOUND_PROP_ACCESSOR_H

#include "types.h"

struct SoundProperties
{
    const char* eventName; // 0x00
    const char* soundName; // 0x04
    float f0;              // 0x08
    float f1;              // 0x0C
    float f2;              // 0x10
    u32 i0;                // 0x14
    u32 i1;                // 0x18
}; // sizeof = 0x1C

class SoundPropAccessor
{
public:
    SoundPropAccessor() { m_unk_0x08 = 0; };
    virtual SoundProperties* GetSoundProperty(unsigned int index) const = 0;
    virtual SoundProperties* GetSoundPropTable() = 0;
    virtual u32 GetNumSFX() const = 0;
    virtual const char* GetSoundPropTableName() const = 0;
    virtual const char* GetHTMLFileName() const = 0;
    virtual bool IsUsingOrigTable() const = 0;
    virtual inline void SetSoundPropTable(SoundProperties* table) { m_pTable = table; };
    virtual void ResetSoundPropTable() = 0;

protected:
    /* 0x04 */ SoundProperties* m_pTable;
    /* 0x08 */ bool m_unk_0x08;
};

#endif // SOUND_PROP_ACCESSOR_H
