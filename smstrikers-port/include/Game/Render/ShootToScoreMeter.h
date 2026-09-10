#ifndef _SHOOTTOSCOREMETER_H_
#define _SHOOTTOSCOREMETER_H_

#include "NL/nlMath.h"
#include "NL/nlColour.h"

enum eHyperStatus
{
    STS_NO_HYPER = 0,
    STS_POSSIBLE_HYPER = 1,
    STS_GOT_HYPER = 2,
};

class ShootToScoreMeter
{
public:
    enum STSMeterType
    {
        REGULAR_SHOOT_TO_SCORE_PHASE1 = 0,
        REGULAR_SHOOT_TO_SCORE_PHASE2 = 1,
    };

    ShootToScoreMeter()
        : mfRumbleAmount(0.0f)
        , m_bMeterVisible(false)
        , m_MeterType(REGULAR_SHOOT_TO_SCORE_PHASE1)
        , m_fWhiteBarAngle(0.0f)
        , m_fWhiteBarPreviousAngle(0.0f)
        , m_fSavedWhiteBarAngle(0.0f)
        , mbShowSavedWhiteBar(false)
        , meHyper(STS_NO_HYPER)
        , m_fGreenBarAngle(0.0f)
        , m_fSavedGreenBarAngle(0.0f)
        , m_fGreenRegionWidth(0.0f)
        , m_fSavedGreenRegionWidth(0.0f)
        , m_fYellowRegionWidth(0.0f)
        , m_fSavedYellowRegionWidth(0.0f)
        , m_fGreenAndYellonRegionIntensity(0.7f)
    {
    }

    void TurnOnMeter(ShootToScoreMeter::STSMeterType type, float yellowWidth);
    void RumbleMeter(float rumbleIntensity, float xOffset, float zOffset);
    void DrawMeter();
    void DrawColouredRegion(float startAngle, float endAngle, const nlColour& startColour, const nlColour& endColour, nlMatrix4 meterMatrix, float scale);
    void DrawIndicatorBar(float angle, const nlColour& colour, const nlMatrix4& meterMatrix, float scale);
    void DrawTrailIndicatorBar(int index, float angleDifference, const nlColour& colour, const nlMatrix4& meterMatrix, float scale);
    void UpdateAndRender(float fDeltaT);
    void SetWhiteBarPosition(float position);
    void SetSavedWhiteBarPosition(float position);
    void SetGreenBarPosition(float position);
    void SetGreenRegionWidth(float width);
    void DrawCaptainMeter();

    /* 0x00 */ nlVector3 m_v3MeterPosition;            // size 0xC
    /* 0x0C */ nlVector3 m_v3OriginalMeterPosition;    // size 0xC
    /* 0x18 */ float mfRumbleAmount;                   // size 0x4
    /* 0x1C */ nlVector3 m_v3CrosshairPosition;        // size 0xC
    /* 0x28 */ bool m_bMeterVisible;                   // size 0x1
    /* 0x2C */ STSMeterType m_MeterType;               // size 0x4
    /* 0x30 */ float m_fWhiteBarAngle;                 // size 0x4
    /* 0x34 */ float m_fWhiteBarPreviousAngle;         // size 0x4
    /* 0x38 */ float m_fSavedWhiteBarAngle;            // size 0x4
    /* 0x3C */ bool mbShowSavedWhiteBar;               // size 0x1
    /* 0x40 */ eHyperStatus meHyper;                   // size 0x4
    /* 0x44 */ float m_fGreenBarAngle;                 // size 0x4
    /* 0x48 */ float m_fSavedGreenBarAngle;            // size 0x4
    /* 0x4C */ float m_fGreenRegionWidth;              // size 0x4
    /* 0x50 */ float m_fSavedGreenRegionWidth;         // size 0x4
    /* 0x54 */ float m_fYellowRegionWidth;             // size 0x4
    /* 0x58 */ float m_fSavedYellowRegionWidth;        // size 0x4
    /* 0x5C */ float m_fGreenAndYellonRegionIntensity; // size 0x4

    static ShootToScoreMeter instance;
    static float MeterWidth;
}; // total size: 0x60

#endif // _SHOOTTOSCOREMETER_H_
