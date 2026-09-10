#ifndef _FORMATIONDEFINES_H_
#define _FORMATIONDEFINES_H_

#include "NL/nlMath.h"
#include "Game/Team.h"

void AILocToFieldLoc(nlVector3& result, const nlVector3& input, eTeamSide side);
void FieldLocToAILoc(nlVector3& dest, const nlVector3& field_location, eTeamSide nTeamSide);

class FormationSpec;

class FormationPos
{
public:
    FormationPos()
    {
        m_Location.x = 0.0f;
        m_Location.y = 0.0f;
        m_CaptainPreference = 0.0f;
    }
    void GetLocationForTeam(nlVector2& dest, int teamId) const;

    nlVector2 m_Location;      // offset 0x0, size 0x8
    float m_CaptainPreference; // offset 0x8, size 0x4
}; // total size: 0xC

class FormationSpec
{
public:
    FormationSpec()
    {
        m_ID = -1;
    }
    void Init(int id, int iKeyIndex, const char* name);
    void SetName(const char* name);
    nlVector2& GetKeyLocation() const;
    void CalculateExtents(nlVector2& minOut, nlVector2& maxOut, const nlVector2& input) const;

    /* 0x00 */ s32 m_ID;
    /* 0x04 */ s32 m_iKeyIndex;
    /* 0x08 */ f32 m_InRadius;
    /* 0x0C */ f32 m_OutRadius;
    /* 0x10 */ FormationPos m_Positions[4];
    /* 0x40 */ nlVector2 m_v2Min;
    /* 0x48 */ nlVector2 m_v2Max;
    /* 0x50 */ nlVector2 m_v2Center;
    /* 0x58 */ char m_Name[32];
}; // total size: 0x78

class FormationSet
{
public:
    FormationSet()
    {
        m_ID = -1;
        m_NumFormationDefs = 0;
        m_FormationDefArray = NULL;
        m_AutoDelete = false;
    }

    ~FormationSet()
    {
        if (m_AutoDelete)
        {
            delete[] m_FormationDefArray;
        }
    }

    void Init(int id, FormationSpec* formationArray, int numFormations, bool bCreateCopy);

    FormationSpec* GetFormationSpec(int index) const;
    FormationSpec* GetFormationSpecFromID(int formationID) const;
    static FormationSet* LoadFormationSets(const char* filename, int& out_numsets);

    /* 0x0 */ bool m_AutoDelete;
    /* 0x4 */ int m_ID;
    /* 0x8 */ int m_NumFormationDefs;
    /* 0xC */ FormationSpec* m_FormationDefArray;
}; // total size: 0x10

// class Config
// {
// public:
//     void TagValuePair::Get<BasicString<char, Detail::TempStringAllocator>>() const;
//     void Get<BasicString<char, Detail::TempStringAllocator>>(const char*, BasicString<char, Detail::TempStringAllocator>);
// };

// class LexicalCast<BasicString<char, Detail
// {
// public:
//     void TempStringAllocator>, float>(const float&);
//     void TempStringAllocator>, int>(const int&);
//     void TempStringAllocator>, bool>(const bool&);
// };

// class Detail
// {
// public:
//     void LexicalCastImpl<BasicString<char, Detail::TempStringAllocator>, float>::Do(float);
//     void LexicalCastImpl<BasicString<char, Detail::TempStringAllocator>, int>::Do(int);
//     void LexicalCastImpl<BasicString<char, Detail::TempStringAllocator>, bool>::Do(bool);
// };

#endif // _FORMATIONDEFINES_H_
