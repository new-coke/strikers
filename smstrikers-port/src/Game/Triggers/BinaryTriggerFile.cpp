#include "Game/Triggers/BinaryTriggerFile.h"
#include "port/endian.h"

#include "NL/nlFile.h"

/**
 * Offset/Address/Size: 0x0 | 0x802142DC | size: 0x78
 */
BinaryTriggerFile::BinaryTriggerFile(const char* FileName)
{
    m_pFileData = NULL;
    m_pCurrentAnim = 0;
    m_CurrentTrigger = 0;
    m_pFileData = (FILE_HEADER*)nlLoadEntireFile(FileName, &m_FileSize, 0x20, AllocateEnd);
    if (m_pFileData == NULL)
    {
        // PORT: upstream carried on and dereferenced null.
        m_pFirstAnim = NULL;
        m_pFirstTrigger = NULL;
        return;
    }

    // PORT: the file is big-endian.
    port_be16_array(&m_pFileData->Version, 4);

    m_pFirstAnim = (ANIM_RECORD*)((u8*)m_pFileData + sizeof(FILE_HEADER));
    for (u32 i = 0; i < m_pFileData->AnimCount; i++)
    {
        port_be32_array(&m_pFirstAnim[i].hash, 1);
        port_be16_array(&m_pFirstAnim[i].TriggerCount, 2);
    }

    m_pFirstTrigger = (TRIGGER_RECORD*)((u8*)m_pFirstAnim + (m_pFileData->AnimCount * sizeof(ANIM_RECORD)));
    {
        // The triggers fill the gap between the anim table and the bytecode, and every field in one is 32 bits.
        const u8* triggerEnd = (const u8*)m_pFileData + m_pFileData->BytecodeOffset;
        const u8* triggerStart = (const u8*)m_pFirstTrigger;
        if (triggerEnd > triggerStart)
        {
            port_be32_array(m_pFirstTrigger, (unsigned long)(triggerEnd - triggerStart) / 4);
        }
    }
}
