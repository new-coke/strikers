#include "Game/Camera/ReplayCamera.h"
#include "types.h"
#include "NL/nlConfig.h"
#include "NL/nlMath.h"
#include "NL/nlTask.h"
#include "Game/Field.h"
#include "Game/ReplayManager.h"
#include "Game/CharacterTemplate.h"
#include "Game/Render/depthoffield.h"
#include "NL/nlFormat.h"

/*
 * TODO: match-only specialization, approved as a documented exception.
 *
 * Retail emits this body at the end of this TU's main .text, immediately before
 * the ReplayCamera.h getter group; a body defined in a header instead lands in
 * that header's linkonce group, which the linker then places after the getters.
 * Defining the specialization here is currently the only known way to reproduce
 * retail's byte order, but the body is a verbatim copy of the generic template
 * in NL/nlBasicString.h and eight other TUs instantiate that same overload, so
 * this duplicates shared code and contradicts retail DWARF, which attributes
 * the body to nlBasicString.h.
 *
 * Replace this with a form that keeps the definition in nlBasicString.h as soon
 * as one is found. See smstrikers-notes docs/0047.
 */
template <>
template <>
BasicString<char, Detail::TempStringAllocator>& BasicString<char, Detail::TempStringAllocator>::AppendInPlace<Detail::TempStringAllocator>(const BasicString<char, Detail::TempStringAllocator>& rhs)
{
    (*this)[0];

    char* at;
    BasicString<char, Detail::TempStringAllocator>::Data* currentData = mData;
    if (currentData != 0)
    {
        at = currentData->mData.mData + currentData->mData.mSize - 1;
    }
    else
    {
        at = 0;
    }

    BasicString<char, Detail::TempStringAllocator>::Data* rhsData = rhs.mData;
    const char* begin;
    if (rhsData != 0)
    {
        begin = rhsData->mData.mData;
    }
    else
    {
        begin = 0;
    }

    insert(at, begin, rhsData != 0 ? rhsData->mData.mData + rhsData->mData.mSize - 1 : 0);
    return *this;
}

static inline float GetSideDirection(int side)
{
    return side == 0 ? -1.0f : 1.0f;
}

/**
 * Offset/Address/Size: 0x1C78 | 0x801AC97C | size: 0x4
 */
void ReplayCamera::UpdateTweakMode()
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0x1BC8 | 0x801AC8CC | size: 0xB0
 */
ReplayCamera::ReplayCamera()
{
    mDeltaFov = 0.0f;
    mFov = 50.0f;
    mSideOfInterest = 0;
    mNoDampenForOneUpdate = false;
    mFrozen = false;
    mFocus = 1;
    mCamPos = REPLAY_CAMERA_POSITION_SIDELINE;
    mViewMatrix.SetIdentity();
    nlVec3Set(mPosition, 0.0f, 0.0f, 2.0f);
    nlVec3Set(mLookAt, 0.0f, 0.0f, 1.0f);
}

/**
 * Offset/Address/Size: 0x1B88 | 0x801AC88C | size: 0x40
 */
const nlMatrix4& ReplayCamera::GetViewMatrix() const
{
    glMatrixLookAt(*(nlMatrix4*)&mViewMatrix, mPosition, mLookAt, mUpVector);
    return mViewMatrix;
};

/**
 * Offset/Address/Size: 0x1B84 | 0x801AC888 | size: 0x4
 */
void ReplayCamera::Update(float fDeltaT)
{
    // EMPTY
}

/**
 * Offset/Address/Size: 0x16E8 | 0x801AC3EC | size: 0x49C
 */
void ReplayCamera::ManualUpdate(float deltaT)
{
    if (!mFrozen)
    {
        nlVector3 lookAt = { 0.0f, 0.0f, 0.0f };
        int numFocusPoints = 0;

        if (ReplayManager::Instance()->mRender == NULL)
        {
            return;
        }

        RenderSnapshot* render = ReplayManager::Instance()->mRender;

        if (mFocus & 0x1)
        {
            numFocusPoints++;
            nlVec3Add(lookAt, lookAt, render->mBall.mPosition);
        }

        if (mFocus & 0x8)
        {
            DrawableCharacter* goalie = &render->mCharacters[GetGoalieIndex(mSideOfInterest)];
            nlVector3 bip01Pos = goalie->mPosition;
            bip01Pos.z += goalie->mHeight;
            nlVec3Add(lookAt, lookAt, bip01Pos);
        }

        if (mFocus & 0x2)
        {
            DrawableCharacter* player = render->mBall.IndexToPlayer(render->mBall.mOwnerIndex);
            if (player != NULL)
            {
                numFocusPoints++;
                nlVector3 bip01Pos = player->mPosition;
                bip01Pos.z += player->mHeight;
                nlVec3Add(lookAt, lookAt, bip01Pos);
            }
            else
            {
                player = render->mBall.IndexToPlayer(render->mBall.mPrevOwnerIndex);
                if (player != NULL)
                {
                    numFocusPoints++;
                    nlVector3 bip01Pos = player->mPosition;
                    bip01Pos.z += player->mHeight;
                    nlVec3Add(lookAt, lookAt, bip01Pos);
                }
            }
        }

        if (mFocus & 0x4)
        {
            nlVector3 netPos = { 0.0f, 0.0f, 0.0f };
            netPos.x = cField::GetGoalLineX(GetSideDirection(mSideOfInterest));
            numFocusPoints++;
            nlVec3Add(lookAt, lookAt, netPos);
        }

        if (numFocusPoints != 0)
        {
            float invCount = 1.0f / (float)numFocusPoints;
            nlVec3Scale(lookAt, invCount);
        }

        nlVector3 position = GetPosition(mCamPos, GetSideDirection(mSideOfInterest));

        if (mNoDampenForOneUpdate)
        {
            mLookAt = lookAt;
            mPosition = position;
            mNoDampenForOneUpdate = false;
        }
        else
        {
            Dampen(mLookAt, lookAt, 0.15f);
            Dampen(mPosition, position, 0.1f);
        }

        mFov -= deltaT * mDeltaFov;
        if (mFov < 10.0f)
        {
            mFov = 10.0f;
        }
        if (mFov > 120.0f)
        {
            mFov = 120.0f;
        }
    }

    if (nlTaskManager::m_pInstance->m_CurrState == 0x10)
    {
        nlVector3 dir;
        nlVec3Sub(dir, mPosition, mLookAt);
        DepthOfFieldManager::instance.m_fDistanceFromCamera = 4.0f + nlSqrt(dir.GetLengthSq3D(), true);
    }
}

/**
 * Offset/Address/Size: 0x16E0 | 0x801AC3E4 | size: 0x8
 */
void ReplayCamera::SetSideOfInterest(int sideOfInterest)
{
    mSideOfInterest = sideOfInterest;
}

/**
 * Offset/Address/Size: 0x1668 | 0x801AC36C | size: 0x78
 */
void ReplayCamera::CutTo(ReplayCameraPosition camPos)
{
    mFrozen = false;
    mNoDampenForOneUpdate = true;
    mCamPos = camPos;
    mPosition = GetPosition(mCamPos, -1.0f);
    mFov = GetFov(mCamPos);
}

void ReplayCamera::Dampen(nlVector3& from, const nlVector3& to, float dampFactor)
{
    from.x = (1.0f - dampFactor) * from.x + dampFactor * to.x;
    from.y = (1.0f - dampFactor) * from.y + dampFactor * to.y;
    from.z = (1.0f - dampFactor) * from.z + dampFactor * to.z;
}

/**
 * Offset/Address/Size: 0x11AC | 0x801ABEB0 | size: 0x4BC
 */
#pragma optimization_level 2
float ReplayCamera::GetFov(ReplayCameraPosition position) const
{
    switch (position)
    {
    case REPLAY_CAMERA_POSITION_INSIDE_NET:
        return GetConfigFloat(Config::Global(), "replay/camera_inside_net_fov", 50.0f);
    case REPLAY_CAMERA_POSITION_HIGH_UP:
        return GetConfigFloat(Config::Global(), "replay/camera_high_up_fov", 50.0f);
    default:
        if (position >= REPLAY_CAMERA_POSITION_GENERIC_0 && position <= REPLAY_CAMERA_POSITION_GENERIC_LAST)
        {
            BasicString<char, Detail::TempStringAllocator> prefix("replay/camera_");
            {
                BasicString<char, Detail::TempStringAllocator> formatStr("generic_{0}_fov");
                int idx = position - REPLAY_CAMERA_POSITION_GENERIC_0;
                prefix.AppendInPlace(Format(formatStr, idx));
            }
            float fov = GetConfigFloat(Config::Global(), prefix.c_str(), 50.0f);
            return fov;
        }
        return 27.0f;
    }
}
#pragma optimization_level 4

/**
 * Offset/Address/Size: 0x0 | 0x801AAD04 | size: 0x11AC
 */
nlVector3 ReplayCamera::GetPosition(ReplayCameraPosition position, float direction) const
{
    nlVector3 result = { 0.0f, 0.0f, 0.0f };
    float goalLineX = cField::GetGoalLineX(direction);
    float sidelineY = cField::GetSidelineY(1);

    switch (position)
    {
    case REPLAY_CAMERA_POSITION_INSIDE_NET:
    {
        float x = GetConfigFloat(Config::Global(), "replay/camera_inside_net_x", 7.0f);
        float y = GetConfigFloat(Config::Global(), "replay/camera_inside_net_y", 8.0f);
        float z = GetConfigFloat(Config::Global(), "replay/camera_inside_net_z", 2.0f);
        result.x = cField::GetGoalLineX(direction) + direction * x;
        result.y = y;
        result.z = z;
        break;
    }
    case REPLAY_CAMERA_POSITION_SIDELINE:
    {
        RenderSnapshot* render = ReplayManager::Instance()->mRender;
        result = render->mBall.mPosition;
        result.x *= 0.8f;
        result.y = cField::GetSidelineY(0) + (-5.0f);
        result.z = 2.0f;
        break;
    }
    case REPLAY_CAMERA_POSITION_BALL_TO_GOAL:
    {
        RenderSnapshot* render = ReplayManager::Instance()->mRender;
        nlVector3 ballPos = render->mBall.mPosition;
        nlVector3 goalPos = { 0.0f, 0.0f, 0.0f };
        goalPos.x = 30.0f * direction + goalLineX;

        nlVector3 ballToGoal;
        nlVec3Sub(ballToGoal, goalPos, ballPos);
        nlVec3Scale(ballToGoal, nlRecipSqrt(ballToGoal.GetLengthSq3D(), false));

        float behindDist = GetConfigFloat(Config::Global(), "replay/camera_ball_to_goal_behind_dist", 16.0f);
        float scale = -behindDist;
        float offsetX = scale * ballToGoal.x;
        float offsetY = scale * ballToGoal.y;
        float offsetZ = scale * ballToGoal.z;
        result.x = ballPos.x + offsetX;
        result.y = ballPos.y + offsetY;
        result.z = ballPos.z + offsetZ;

        float minHeight = GetConfigFloat(Config::Global(), "replay/camera_ball_to_goal_min_height", 3.0f);
        if (result.z < minHeight)
        {
            result.z = minHeight;
        }

        float minDistToGoal = GetConfigFloat(Config::Global(), "replay/camera_ball_to_goal_min_dist_to_goal", 8.0f);
        if ((float)fabs(goalPos.x - result.x) < minDistToGoal)
        {
            result.x = goalPos.x - direction * minDistToGoal;
        }
        break;
    }
    case REPLAY_CAMERA_POSITION_HIGH_UP:
    {
        float highX = GetConfigFloat(Config::Global(), "replay/camera_high_up_x", -6.0f);
        float highY = GetConfigFloat(Config::Global(), "replay/camera_high_up_y", 0.0f);
        float highZ = GetConfigFloat(Config::Global(), "replay/camera_high_up_z", 8.0f);
        float minDistBehind = GetConfigFloat(Config::Global(), "replay/camera_high_up_min_dist_behind", 8.0f);

        result.x = highX * GetSideDirection(mSideOfInterest);
        result.y = highY;
        result.z = highZ;

        if ((float)fabs(result.x - mLookAt.x) < minDistBehind)
        {
            result.x = mLookAt.x - minDistBehind * GetSideDirection(mSideOfInterest);
        }
        break;
    }
    default:
    {
        if (position >= REPLAY_CAMERA_POSITION_GENERIC_0 && position <= REPLAY_CAMERA_POSITION_GENERIC_LAST)
        {
            BasicString<char, Detail::TempStringAllocator> prefix("replay/camera_");
            prefix.AppendInPlace(Format(BasicString<char, Detail::TempStringAllocator>("generic_{0}_"), position - REPLAY_CAMERA_POSITION_GENERIC_0));

            float xVal = GetConfigFloat(Config::Global(), prefix.Append("x").c_str(), 0.0f) * GetSideDirection(mSideOfInterest);
            float yVal = GetConfigFloat(Config::Global(), prefix.Append("y").c_str(), 0.0f);
            float zVal = GetConfigFloat(Config::Global(), prefix.Append("z").c_str(), 0.0f);
            result.x = xVal;
            result.y = yVal;
            result.z = zVal;
        }
        break;
    }
    }

    nlVector3 limits = { 0.0f, 0.0f, 0.0f };
    limits.x = GetConfigFloat(Config::Global(), "replay/camera_max_behind_goal_line", 2.0f);
    limits.y = GetConfigFloat(Config::Global(), "replay/camera_max_beyond_side_line", 2.0f);
    limits.z = GetConfigFloat(Config::Global(), "replay/camera_max_height", 20.0f);

    float minZ = GetConfigFloat(Config::Global(), "replay/camera_min_height", 0.5f);

    if (result.z > limits.z)
        result.z = limits.z;
    if (result.z < minZ)
        result.z = minZ;

    if (result.x < -((float)fabs(goalLineX)) - limits.x)
        result.x = -((float)fabs(goalLineX)) - limits.x;
    if (result.x > limits.x + (float)fabs(goalLineX))
        result.x = limits.x + (float)fabs(goalLineX);

    if (result.y < -sidelineY - limits.y)
        result.y = -sidelineY - limits.y;
    if (result.y > sidelineY + limits.y)
        result.y = sidelineY + limits.y;

    return result;
}
