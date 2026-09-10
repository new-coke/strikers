#include "Game/FormationDefines.h"
#include "NL/nlConfig.h"
#include "NL/nlLexicalCast.h"
#include "NL/nlPrint.h"

static const nlVector3 v3Zero = { 0.0f, 0.0f, 0.0f };

static inline float Remap(float value, float fromMin, float fromMax, float toMin, float toMax)
{
    float percent = (value - fromMin) / (fromMax - fromMin);
    if (percent > 1.0f)
        percent = 1.0f;
    if (percent < 0.0f)
        percent = 0.0f;
    return toMin + percent * (toMax - toMin);
}

static inline void FieldLocToAILoc(nlVector2& dest, const nlVector2& field_location, eTeamSide nTeamSide)
{
    float fMinFromX = -20.60211f;
    float fMaxFromX = 20.60211f;
    float fMinFromY = -12.0825f;
    float fMaxFromY = 12.0825f;

    if (nTeamSide == AWAY)
    {
        fMinFromX = -fMinFromX;
        fMaxFromX = -fMaxFromX;
        fMinFromY = -fMinFromY;
        fMaxFromY = -fMaxFromY;
    }

    dest.x = Remap(field_location.x, fMinFromX, fMaxFromX, 0.0f, 4.0f);
    dest.y = Remap(field_location.y, fMinFromY, fMaxFromY, -1.0f, 1.0f);
}

static inline void AILocToFieldLoc(nlVector2& dest, const nlVector2& ai_location, eTeamSide nTeamSide)
{
    float fMinFromX = -20.60211f;
    float fMaxFromX = 20.60211f;
    float fMinFromY = -12.0825f;
    float fMaxFromY = 12.0825f;

    if (nTeamSide == AWAY)
    {
        fMinFromX = -fMinFromX;
        fMaxFromX = -fMaxFromX;
        fMinFromY = -fMinFromY;
        fMaxFromY = -fMaxFromY;
    }

    dest.x = Remap(ai_location.x, 0.0f, 4.0f, fMinFromX, fMaxFromX);
    dest.y = Remap(ai_location.y, -1.0f, 1.0f, fMinFromY, fMaxFromY);
}

/**
 * Offset/Address/Size: 0xE10 | 0x8003BC20 | size: 0xB4
 */
void FieldLocToAILoc(nlVector3& dest, const nlVector3& field_location, eTeamSide nTeamSide)
{
    float fMaxFromY, fMinFromX, fMaxFromX, fMinFromY;
    fMinFromX = -20.60211f;
    fMaxFromX = 20.60211f;
    fMinFromY = -12.0825f;
    fMaxFromY = 12.0825f;

    if (nTeamSide == AWAY)
    {
        fMinFromX = -fMinFromX;
        fMaxFromX = -fMaxFromX;
        fMinFromY = -fMinFromY;
        fMaxFromY = -fMaxFromY;
    }

    dest.x = Remap(field_location.x, fMinFromX, fMaxFromX, 0.0f, 4.0f);
    dest.y = Remap(field_location.y, fMinFromY, fMaxFromY, -1.0f, 1.0f);
    dest.z = 0.0f;
}

/**
 * Offset/Address/Size: 0xD5C | 0x8003BB6C | size: 0xB4
 */
void AILocToFieldLoc(nlVector3& result, const nlVector3& input, eTeamSide side)
{
    f32 maxZ;
    f32 minX = 0.0f;
    f32 normX;
    f32 maxX = 4.0f;
    f32 minZ = -1.0f;
    f32 xScale = 41.20422f;
    f32 yScale = 24.165f;
    maxZ = 1.0f;

    if (side == AWAY)
    {
        minX = maxX;
        maxX = 0.0f;
        minZ = maxZ;
        maxZ = -1.0f;
    }

    normX = (input.x - minX) / (maxX - minX);
    if (normX > 1.0f)
        normX = 1.0f;
    if (normX < 0.0f)
        normX = 0.0f;

    result.x = normX * xScale + (-20.60211f);
    f32 normZ = (input.y - minZ) / (maxZ - minZ);
    if (normZ > 1.0f)
        normZ = 1.0f;
    if (normZ < 0.0f)
        normZ = 0.0f;
    result.y = normZ * yScale + (-12.0825f);
    result.z = 0.0f;
}

/**
 * Offset/Address/Size: 0xD24 | 0x8003BB34 | size: 0x38
 */
void FormationPos::GetLocationForTeam(nlVector2& dest, int teamId) const
{
    if (teamId == 0)
    {
        dest = m_Location;
        return;
    }
    nlVec2Set(dest, -m_Location.x, -m_Location.y);
}

/**
 * Offset/Address/Size: 0xCF0 | 0x8003BB00 | size: 0x34
 */
nlVector2& FormationSpec::GetKeyLocation() const
{
    if (m_iKeyIndex >= 0 && m_iKeyIndex < 4)
    {
        return const_cast<nlVector2&>(m_Positions[m_iKeyIndex].m_Location);
    }
    return *(nlVector2*)&const_cast<nlVector3&>(v3Zero);
}

/**
 * Offset/Address/Size: 0xC94 | 0x8003BAA4 | size: 0x5C
 */
void FormationSpec::CalculateExtents(nlVector2& minOut, nlVector2& maxOut, const nlVector2& input) const
{
    const float fieldHalfWidth = 18.541899f;
    const float fieldHalfHeight = 10.87425f;

    minOut.x = -fieldHalfWidth + (input.x - m_v2Min.x);
    maxOut.x = fieldHalfWidth + (input.x - m_v2Max.x);
    minOut.y = -fieldHalfHeight + (input.y - m_v2Min.y);
    maxOut.y = fieldHalfHeight + (input.y - m_v2Max.y);
}

static inline float FormationMin(float current, float value)
{
    if (current <= value)
        return current;
    else
        return value;
}

static inline float FormationMax(float current, float value)
{
    if (current >= value)
        return current;
    else
        return value;
}

void FormationSpec::SetName(const char* name)
{
    nlStrNCpy(m_Name, name, 32);
    m_Name[31] = 0;
}

void FormationSpec::Init(int id, int iKeyIndex, const char* name)
{
    m_ID = id;
    m_iKeyIndex = iKeyIndex;
    if (name != m_Name)
    {
        SetName(name);
    }
    m_v2Min.x = 999999.9f;
    m_v2Min.y = 999999.9f;
    m_v2Max.x = -999999.9f;
    m_v2Max.y = -999999.9f;
    m_v2Center.x = 0.0f;
    m_v2Center.y = 0.0f;

    for (int i_fielder = 0; i_fielder < 4; i_fielder++)
    {
        m_v2Min.x = FormationMin(m_v2Min.x, m_Positions[i_fielder].m_Location.x);
        m_v2Min.y = FormationMin(m_v2Min.y, m_Positions[i_fielder].m_Location.y);
        m_v2Max.x = FormationMax(m_v2Max.x, m_Positions[i_fielder].m_Location.x);
        m_v2Max.y = FormationMax(m_v2Max.y, m_Positions[i_fielder].m_Location.y);
        {
            float cx = m_v2Center.x + m_Positions[i_fielder].m_Location.x;
            float cy = m_v2Center.y + m_Positions[i_fielder].m_Location.y;
            m_v2Center.x = cx;
            m_v2Center.y = cy;
        }
    }

    {
        float cx = m_v2Center.x * 0.25f;
        float cy = m_v2Center.y * 0.25f;
        m_v2Center.x = cx;
        m_v2Center.y = cy;
    }
}

/**
 * Offset/Address/Size: 0xC84 | 0x8003BA94 | size: 0x10
 */
FormationSpec* FormationSet::GetFormationSpec(int index) const
{
    return &m_FormationDefArray[index];
}

/**
 * Offset/Address/Size: 0xC08 | 0x8003BA18 | size: 0x7C
 */
FormationSpec* FormationSet::GetFormationSpecFromID(int formationID) const
{
    // Check for negative ID
    if (formationID < 0 || formationID >= m_NumFormationDefs)
    {
        // Fall through to linear search
    }
    else
    {
        // Try direct index access first
        FormationSpec* spec = &m_FormationDefArray[formationID];
        if (formationID == spec->m_ID)
        {
            return spec;
        }
    }

    // Linear search through all formation defs
    FormationSpec* array = m_FormationDefArray;
    int i = 0;
    int count = m_NumFormationDefs;
    for (; count > 0; count--)
    {
        if (formationID == array->m_ID)
        {
            return &m_FormationDefArray[i];
        }
        array++;
        i++;
    }

    return NULL;
}

void FormationSet::Init(int id, FormationSpec* formationArray, int numFormations, bool bCreateCopy)
{
    m_ID = id;
    m_NumFormationDefs = numFormations;

    if (bCreateCopy)
    {
        m_AutoDelete = true;
        m_FormationDefArray = new (8, false) FormationSpec[numFormations];
        for (int i = 0; i < numFormations; i++)
        {
            m_FormationDefArray[i] = formationArray[i];
        }
    }
    else
    {
        m_AutoDelete = false;
        m_FormationDefArray = formationArray;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x8003AE10 | size: 0xC08
 */
FormationSet* FormationSet::LoadFormationSets(const char* filename, int& out_numsets)
{
    Config config(Config::ALLOCATE_HIGH);
    config.LoadFromFile(filename);

    out_numsets = GetConfigInt(config, "Number Of Formation Sets", 0);
    if (out_numsets == 0)
    {
        return NULL;
    }

    FormationSet* setList = new (8, false) FormationSet[out_numsets];
    char section_name[128];
    char var_name[128];
    FormationSpec formationList[42];
    int formation_id = 0;
    int i_set;
    int i_formation;
    int i_pos;

    for (i_set = 0; i_set < out_numsets; i_set++)
    {
        nlSNPrintf(section_name, 127, "FORMATION_SET%d", i_set);

        i_formation = 0;
        while (true)
        {
            nlSNPrintf(var_name, 127, "%s/F%d_NAME", section_name, i_formation);
            if (!config.Exists(var_name))
            {
                break;
            }

            FormationSpec& formation = formationList[i_formation];

            formation.SetName(
                config.Get<BasicString<char, Detail::TempStringAllocator> >(
                          var_name, BasicString<char, Detail::TempStringAllocator>("Unnamed"))
                    .c_str());

            nlSNPrintf(var_name, 127, "%s/F%d_KEY_POS", section_name, i_formation);
            int keyIndex = GetConfigInt(config, var_name, -1);

            nlSNPrintf(var_name, 127, "%s/F%d_INRADIUS", section_name, i_formation);
            float inRadius = GetConfigFloat(config, var_name, 7.0f);

            nlSNPrintf(var_name, 127, "%s/F%d_OUTRADIUS", section_name, i_formation);
            float outRadius = GetConfigFloat(config, var_name, 11.0f);

            formation.m_InRadius = inRadius;
            formation.m_OutRadius = outRadius;

            for (i_pos = 0; i_pos < 4; i_pos++)
            {
                FormationPos& position = formation.m_Positions[i_pos];

                nlSNPrintf(var_name, 127, "%s/F%d_P%d_X", section_name, i_formation, i_pos);
                float xVal = GetConfigFloat(config, var_name, -9999.9f);

                nlSNPrintf(var_name, 127, "%s/F%d_P%d_Y", section_name, i_formation, i_pos);
                float yVal = GetConfigFloat(config, var_name, -9999.9f);

                nlSNPrintf(var_name, 127, "%s/F%d_P%d_CAPTAINPREF", section_name, i_formation, i_pos);
                float captainPref = GetConfigFloat(config, var_name, 0.0f);

                nlVector2 ailocation = { 0.0f, 0.0f };
                nlVector2 fieldLocation;
                ailocation.x = xVal;
                ailocation.y = yVal;
                AILocToFieldLoc(fieldLocation, ailocation, HOME);

                position.m_Location = fieldLocation;
                position.m_CaptainPreference = captainPref;
            }

            formation.Init(formation_id, keyIndex, formation.m_Name);

            i_formation++;
            formation_id++;
        }

        setList[i_set].Init(i_set, formationList, i_formation, true);
    }

    return setList;
}
