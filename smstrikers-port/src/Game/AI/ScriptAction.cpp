#include "Game/AI/ScriptAction.h"

#include "Game/AI/Fuzzy.h"
#include "Game/AI/FuzzyVariant.h"
#include "Game/AI/FielderDesires.h"

extern FuzzyVariant fvNotSet;

SlotPool<ScriptAction> ScriptAction::m_ScriptActionSlotPool(0xA, 0xA);

/**
 * Offset/Address/Size: 0xB4 | 0x80061E68 | size: 0x1EC
 */
ScriptAction::ScriptAction(eScriptActionType action_type, float fConfidence)
{
    m_sPlayParams.ePlayType = 0;
    m_sPlayParams.fDuration = 0.0f;

    m_eType = action_type;
    m_fConfidence = fConfidence;
    m_fSelectionChance = 1.0f;
}

/**
 * Offset/Address/Size: 0x64 | 0x80061E18 | size: 0x50
 */
float ScriptAction::CalcSelectionChance()
{
    float fChance;
    if (m_eType == SAT_SET_DESIRE)
    {
        fChance = GetCommonDesireData(m_sDesireParams.eDesireType).CalcFuzzyChance(m_fSelectionChance);
        return fChance;
    }
    fChance = RandomChance(m_fSelectionChance);
    return fChance;
}

/**
 * Offset/Address/Size: 0x0 | 0x80061DB4 | size: 0x64
 */
bool ScriptAction::RollChanceDice()
{
    if (m_eType == SAT_SET_DESIRE)
    {
        return GetCommonDesireData(m_sDesireParams.eDesireType).CalcBoolChance(m_fSelectionChance);
    }
    else
    {
        return RandomChance(m_fSelectionChance) >= 0.5f;
    }
}
