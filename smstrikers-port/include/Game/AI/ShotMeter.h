#ifndef _SHOTMETER_H_
#define _SHOTMETER_H_

#include "Game/AI/Fielder.h"

enum eShotMeterState
{
    SHOT_METER_INACTIVE = 0,
    SHOT_METER_ACTIVE = 1,
    SHOT_METER_RELEASED = 2,
    SHOT_METER_STS_ACTIVE = 3,
    SHOT_METER_STS_TRANSISTION = 4,
    SHOT_METER_STS_RELEASED = 5,
};

class ShotMeter
{
public:
    void Update(float fDeltaT, bool bHoldTime);
    void Abort(cFielder* pFielder);
    void CalcOneTimerValue(cFielder* pFielder, bool bWasPerfectPass);
    float GetTotalDuration() const;
    float GetShotAimValue() const
    {
        return mfSShotAimValue;
    }
    static bool IsActive(eShotMeterState state)
    {
        bool bShotMeterActive = false;
        if (state == SHOT_METER_ACTIVE || state == SHOT_METER_STS_ACTIVE || state == SHOT_METER_STS_TRANSISTION)
        {
            bShotMeterActive = true;
        }
        return bShotMeterActive;
    }
    void Reset();
    void ShotReleased(cFielder* pFielder);

    /* 0x00 */ eShotMeterState m_eShotMeterState;
    /* 0x04 */ float m_fTime;
    /* 0x08 */ float m_fScoreValue;
    /* 0x0C */ float m_fSpeedValue;
    /* 0x10 */ float m_fSTSValue;
    /* 0x14 */ float mfSShotAimValue;
}; // total size: 0x18

#endif // _SHOTMETER_H_
