#include "Game/Physics/PhysicsFakeBall.h"
#include "Game/Physics/PhysicsPlane.h"
#include "Game/Physics/PhysicsGroundPlane.h"
#include "Game/Physics/Physics.h"
#include "Game/Physics/PhysicsAIBall.h"
#include "Game/Player.h"
#include "Game/FixedUpdateTask.h"
#include "Game/AI/AiUtil.h"
#include "NL/nlDLRing.h"
#include "NL/nlDLListContainer.h"

static const nlVector3 v3Zero = { 0.f, 0.f, 0.f };

static f32 CANT_COLLIDE = *(f32*)__float_max;

SlotPool<BallCacheInfo> BallCacheInfo::mBallCacheInfoSlotPool(16, 16);
FakeBallWorld* FakeBallWorld::mpPredictWorld;
nlDLListSlotPool<BallCacheInfo*> FakeBallWorld::mBallCacheList;
float FakeBallWorld::mfLastCacheTime = -1.0f;
nlDLListIterator<BallCacheInfo*>* FakeBallWorld::mpCacheIterator;

class SimpleCollisionSpace : public CollisionSpace
{
public:
    SimpleCollisionSpace(PhysicsWorld*);
    virtual ~SimpleCollisionSpace() { };
};

/**
 * Offset/Address/Size: 0x20AC | 0x80139498 | size: 0x1D8
 */
void FakeBallWorld::Init(cBall* pBall)
{
    if (mpPredictWorld == NULL)
    {
        mpPredictWorld = new (nlMalloc(sizeof(FakeBallWorld), 8, false)) FakeBallWorld(pBall);
    }

    ClearBallCache();
}

/**
 * Offset/Address/Size: 0x1F14 | 0x80139300 | size: 0x198
 */
void FakeBallWorld::Destroy()
{
    if (mpPredictWorld != NULL)
    {
        delete mpPredictWorld;
        mpPredictWorld = NULL;
    }

    ClearBallCache();
    mBallCacheList.m_Allocator.FreeBlocks();
    BallCacheInfo::mBallCacheInfoSlotPool.FreeBlocks();
}

/**
 * Offset/Address/Size: 0x1E28 | 0x80139214 | size: 0xEC
 */
void FakeBallWorld::InvalidateBallCache()
{
    ClearBallCache();
}

BallCacheInfo* FakeBallWorld::AddCacheEntry(float fTime, PhysicsBall* pPhysicsBall)
{
    DLListEntry<BallCacheInfo*>* pNewEntry;
    BallCacheInfo* pNewInfo;
    BallCacheInfo::mBallCacheInfoSlotPool.AllocateForReturn(pNewInfo);
    pNewInfo->mfTime = FakeBallWorld::mfLastCacheTime;
    pNewInfo->mv3Position = ((PhysicsObject*)pPhysicsBall)->GetPosition();
    pNewInfo->mv3LinearVelocity = ((PhysicsObject*)pPhysicsBall)->GetLinearVelocity();
    pNewEntry = FakeBallWorld::mBallCacheList.m_Allocator.Allocate();
    if (pNewEntry != NULL)
    {
        pNewEntry->m_next = NULL;
        pNewEntry->m_prev = NULL;
        pNewEntry->entry = pNewInfo;
    }
    nlDLRingAddEnd(&FakeBallWorld::mBallCacheList.m_Head, pNewEntry);
    return pNewInfo;
}

/**
 * Offset/Address/Size: 0x1768 | 0x80138B54 | size: 0x6C0
 */
bool FakeBallWorld::GetPredictedBallPosition(float fDeltaTime, nlVector3& v3Position, nlVector3& v3Velocity)
{
    cBall* pBall = mpPredictWorld->GetBall();
    if (pBall->m_pOwner != NULL)
    {
        v3Position = pBall->m_v3Position;
        v3Velocity = mpPredictWorld->mpBall->m_pOwner->m_v3Velocity;
        return false;
    }
    float fPhysicsTick = FixedUpdateTask::GetPhysicsUpdateTick();
    float fSimTime = FixedUpdateTask::mSimulationTime;
    if (mfLastCacheTime < fSimTime)
    {
        ClearBallCache();
    }
    else if (mBallCacheList.m_Head != NULL)
    {
        BallCacheInfo* pLast = NULL;
        nlDLListIterator<BallCacheInfo*> iter;
        DLListEntry<BallCacheInfo*>* pEntry = nlDLRingGetStart(mBallCacheList.m_Head);
        iter.m_Head = mBallCacheList.m_Head;
        iter.m_Curr = pEntry;
        while (iter.m_Curr != NULL)
        {
            BallCacheInfo* pCur = iter.m_Curr->entry;
            if (fSimTime >= pCur->mfTime)
            {
                if (pLast != NULL)
                {
                    mBallCacheList.RemoveStart(&pLast);
                    BallCacheInfo::mBallCacheInfoSlotPool.Free(pLast);
                }
                pLast = pCur;
                iter.next();
            }
            else
            {
                if (pLast != NULL)
                {
                    if (fSimTime - pLast->mfTime < pCur->mfTime - fSimTime)
                    {
                        pCur = pLast;
                    }
                }
                float distSq = nlGetLengthSquared3D(pCur->mv3Position.x - mpPredictWorld->mpBall->m_v3Position.x, pCur->mv3Position.y - mpPredictWorld->mpBall->m_v3Position.y, pCur->mv3Position.z - mpPredictWorld->mpBall->m_v3Position.z);
                if (!(distSq > 0.0025f))
                {
                    break;
                }
                ClearBallCache();
                break;
            }
        }
    }

    float fTargetTime = fSimTime + fDeltaTime;

    while (mfLastCacheTime < fTargetTime)
    {
        if (mfLastCacheTime < fSimTime)
        {
            mpPredictWorld->mbHitSuccess = false;
            mpPredictWorld->mpPhysicsBall->CloneBall(*mpPredictWorld->mpBall->m_pPhysicsBall);
            mfLastCacheTime = fSimTime;
        }
        else
        {
            PhysicsUpdate(mpPredictWorld->mpPhysicsWorld, fPhysicsTick);
            mfLastCacheTime += fPhysicsTick;
        }
        AddCacheEntry(mfLastCacheTime, (PhysicsBall*)mpPredictWorld->mpPhysicsBall);
    }

    DLListEntry<BallCacheInfo*>** pHeadRef = &mBallCacheList.m_Head;
    float overshoot = mfLastCacheTime - fTargetTime;
    BallCacheInfo* pPrev;
    BallCacheInfo* pNext;
    if (fDeltaTime < overshoot)
    {
        DLListEntry<BallCacheInfo*>* pEntry = nlDLRingGetStart(*pHeadRef);
        pNext = pEntry->entry;
        nlDLListIterator<BallCacheInfo*> iter;
        iter.m_Head = *pHeadRef;
        iter.m_Curr = pEntry;
        pPrev = pNext;
        while (!nlDLRingIsEnd(iter.m_Head, iter.m_Curr) && pNext->mfTime < fTargetTime)
        {
            pPrev = pNext;
            iter.next();
            pNext = iter.m_Curr->entry;
        }
    }
    else
    {
        DLListEntry<BallCacheInfo*>* pEntry = nlDLRingGetEnd(*pHeadRef);
        pNext = pEntry->entry;
        nlDLListIterator<BallCacheInfo*> iter;
        iter.m_Head = *pHeadRef;
        iter.m_Curr = pEntry;
        pPrev = pNext;
        while (!nlDLRingIsStart(iter.m_Head, iter.m_Curr) && pPrev->mfTime >= fTargetTime)
        {
            pNext = pPrev;
            if (nlDLRingIsStart(iter.m_Head, iter.m_Curr))
            {
                iter.m_Curr = NULL;
            }
            else
            {
                iter.m_Curr = iter.m_Curr->m_prev;
            }
            pPrev = iter.m_Curr->entry;
        }
    }

    if (pNext != pPrev && fTargetTime > pPrev->mfTime)
    {
        float fPercent = (fTargetTime - pPrev->mfTime) / (pNext->mfTime - pPrev->mfTime);

        if (fPercent < 1.0f)
        {
            nlVecLerp(v3Position, pPrev->mv3Position, pNext->mv3Position, fPercent);
            nlVecLerp(v3Velocity, pPrev->mv3LinearVelocity, pNext->mv3LinearVelocity, fPercent);
        }
        else
        {
            v3Position = pNext->mv3Position;
            v3Velocity = pNext->mv3LinearVelocity;
        }
    }
    else
    {
        v3Position = pPrev->mv3Position;
        v3Velocity = pPrev->mv3LinearVelocity;
    }
    return true;
}

/**
 * Offset/Address/Size: 0x11AC | 0x80138598 | size: 0x5BC
 */
float FakeBallWorld::GetPredictedPlaneIntersectTime(const nlVector4& v4Plane, nlVector3& v3ContactPoint, nlVector3& v3ContactVelocity)
{
    cBall* pBall = mpPredictWorld->mpBall;

    float fDist = pBall->m_v3Position.x * v4Plane.x
                + pBall->m_v3Position.y * v4Plane.y
                + pBall->m_v3Position.z * v4Plane.z
                - v4Plane.w;

    if (fDist < 0.0f)
    {
        return -1.0f;
    }

    float fVelDot = pBall->m_v3Velocity.x * v4Plane.x
                  + pBall->m_v3Velocity.y * v4Plane.y
                  + pBall->m_v3Velocity.z * v4Plane.z;

    if (fVelDot >= 0.0f)
    {
        return -2.0f;
    }

    if (!GetPredictedBallPosition(0.0f, v3ContactPoint, v3ContactVelocity))
    {
        return -2.5f;
    }

    float fSimulationTime;
    float fDistanceNext;
    float fMaxTime;
    float fPhysicsTick = FixedUpdateTask::GetPhysicsUpdateTick();
    fSimulationTime = FixedUpdateTask::mSimulationTime;
    DLListEntry<BallCacheInfo*>** ppHead = &mBallCacheList.m_Head;
    DLListEntry<BallCacheInfo*>* pHead = *ppHead;

    if (pHead != NULL)
    {
        DLListEntry<BallCacheInfo*>* pEntry = nlDLRingGetStart(pHead);
        DLListEntry<BallCacheInfo*>* pHeadRef = *ppHead;
        DLListEntry<BallCacheInfo*>* pListEntry = pEntry;
        BallCacheInfo* pPrev;
        BallCacheInfo* pNext = pEntry->entry;

        fDistanceNext = pNext->mv3Position.x * v4Plane.x
                      + pNext->mv3Position.y * v4Plane.y
                      + pNext->mv3Position.z * v4Plane.z
                      - v4Plane.w;

        while (!nlDLRingIsEnd(pHeadRef, pListEntry))
        {
            if (nlDLRingIsEnd(pHeadRef, pListEntry) || pListEntry == NULL)
            {
                pListEntry = NULL;
            }
            else
            {
                pListEntry = pListEntry->m_next;
            }

            pPrev = pNext;
            pNext = pListEntry->entry;
            float fDistancePrev = fDistanceNext;

            float fDistanceNew = pNext->mv3Position.x * v4Plane.x
                               + pNext->mv3Position.y * v4Plane.y
                               + pNext->mv3Position.z * v4Plane.z
                               - v4Plane.w;
            fDistanceNext = fDistanceNew;

            if (fDistanceNew < 0.0f)
            {
                float fPercent = fDistancePrev / (fDistancePrev - fDistanceNew);
                float fTime = Interpolate(pPrev->mfTime, pNext->mfTime, fPercent) - fSimulationTime;

                float fInvPercent = 1.0f - fPercent;
                v3ContactPoint.x = fInvPercent * pPrev->mv3Position.x + fPercent * pNext->mv3Position.x;
                v3ContactPoint.y = fInvPercent * pPrev->mv3Position.y + fPercent * pNext->mv3Position.y;
                v3ContactPoint.z = fInvPercent * pPrev->mv3Position.z + fPercent * pNext->mv3Position.z;
                v3ContactVelocity.x = fInvPercent * pPrev->mv3LinearVelocity.x + fPercent * pNext->mv3LinearVelocity.x;
                v3ContactVelocity.y = fInvPercent * pPrev->mv3LinearVelocity.y + fPercent * pNext->mv3LinearVelocity.y;
                v3ContactVelocity.z = fInvPercent * pPrev->mv3LinearVelocity.z + fPercent * pNext->mv3LinearVelocity.z;

                return fTime;
            }

            if (fDistanceNew >= fDistancePrev)
            {
                v3ContactPoint = pPrev->mv3Position;
                v3ContactVelocity = pPrev->mv3LinearVelocity;
                return -3.0f;
            }
        }
    }

    DLListEntry<BallCacheInfo*>* pLastEntry = nlDLRingGetEnd(*ppHead);
    BallCacheInfo* pCurCache = pLastEntry->entry;

    float fDistanceCur = pCurCache->mv3Position.x * v4Plane.x
                       + pCurCache->mv3Position.y * v4Plane.y
                       + pCurCache->mv3Position.z * v4Plane.z
                       - v4Plane.w;

    fMaxTime = 6.0f + fSimulationTime;

    while (mfLastCacheTime < fMaxTime)
    {
        BallCacheInfo* pLastCache = pCurCache;
        float fDistanceLast = fDistanceCur;

        PhysicsUpdate(mpPredictWorld->mpPhysicsWorld, fPhysicsTick);

        mfLastCacheTime += fPhysicsTick;
        BallCacheInfo* pNewInfo = AddCacheEntry(mfLastCacheTime, (PhysicsBall*)mpPredictWorld->mpPhysicsBall);

        pCurCache = pNewInfo;

        float fDistanceNewCache = pNewInfo->mv3Position.x * v4Plane.x
                                + pNewInfo->mv3Position.y * v4Plane.y
                                + pNewInfo->mv3Position.z * v4Plane.z
                                - v4Plane.w;
        fDistanceCur = fDistanceNewCache;

        if (fDistanceNewCache < 0.0f)
        {
            float fPercent = fDistanceLast / (fDistanceLast - fDistanceNewCache);
            float fTime = Interpolate(pLastCache->mfTime, pNewInfo->mfTime, fPercent) - fSimulationTime;

            float fInvPercent = 1.0f - fPercent;
            v3ContactPoint.x = fInvPercent * pLastCache->mv3Position.x + fPercent * pNewInfo->mv3Position.x;
            v3ContactPoint.y = fInvPercent * pLastCache->mv3Position.y + fPercent * pNewInfo->mv3Position.y;
            v3ContactPoint.z = fInvPercent * pLastCache->mv3Position.z + fPercent * pNewInfo->mv3Position.z;
            v3ContactVelocity.x = fInvPercent * pLastCache->mv3LinearVelocity.x + fPercent * pNewInfo->mv3LinearVelocity.x;
            v3ContactVelocity.y = fInvPercent * pLastCache->mv3LinearVelocity.y + fPercent * pNewInfo->mv3LinearVelocity.y;
            v3ContactVelocity.z = fInvPercent * pLastCache->mv3LinearVelocity.z + fPercent * pNewInfo->mv3LinearVelocity.z;

            return fTime;
        }

        if (fDistanceNewCache >= fDistanceLast)
        {
            v3ContactPoint = pLastCache->mv3Position;
            v3ContactVelocity = pLastCache->mv3LinearVelocity;
            return -4.0f;
        }
    }

    v3ContactPoint = pCurCache->mv3Position;
    v3ContactVelocity = pCurCache->mv3LinearVelocity;
    return -5.0f;
}

/**
 * Offset/Address/Size: 0xDB0 | 0x8013819C | size: 0x3FC
 */
float FakeBallWorld::GetPredictedHeightLimitTime(float fHeight, float fMinTime, nlVector3& v3ContactPoint, nlVector3& v3ContactVelocity, bool bDownOnly)
{
    cBall* pBall = mpPredictWorld->mpBall;

    float speedSq = pBall->m_v3Velocity.x * pBall->m_v3Velocity.x
                  + pBall->m_v3Velocity.y * pBall->m_v3Velocity.y
                  + pBall->m_v3Velocity.z * pBall->m_v3Velocity.z;

    if (speedSq < 0.0001f)
    {
        v3ContactPoint = pBall->m_v3Position;
        v3ContactVelocity = v3Zero;
        return fMinTime;
    }

    bool freeball = GetPredictedBallPosition(fMinTime, v3ContactPoint, v3ContactVelocity);
    if (v3ContactPoint.z <= fHeight)
    {
        if (!bDownOnly || v3ContactVelocity.z <= 0.0f)
        {
            return fMinTime;
        }
    }

    if (!freeball)
    {
        return -2.0f;
    }

    float fPhysicsTick = FixedUpdateTask::GetPhysicsUpdateTick();
    const float fSimulationTime = FixedUpdateTask::mSimulationTime;
    float fTestTime = fSimulationTime + fMinTime;
    float fLastZVel = 0.0f;

    DLListEntry<BallCacheInfo*>** ppHead = &mBallCacheList.m_Head;
    DLListEntry<BallCacheInfo*>* pEntry = nlDLRingGetStart(*ppHead);
    DLListEntry<BallCacheInfo*>* pHeadRef = *ppHead;
    DLListEntry<BallCacheInfo*>* pListEntry = pEntry;

    while (pListEntry)
    {
        BallCacheInfo* pCur = pListEntry->entry;

        if (pCur->mfTime >= fTestTime)
        {
            float zPos = pCur->mv3Position.z;
            float zVel = pCur->mv3LinearVelocity.z;

            if ((zPos <= fHeight && (!bDownOnly || zVel <= 0.0f)) || (fLastZVel < 0.0f && zVel > 0.0f))
            {
                v3ContactPoint = pCur->mv3Position;
                v3ContactVelocity = pCur->mv3LinearVelocity;
                return pCur->mfTime - fSimulationTime;
            }
        }

        fLastZVel = pCur->mv3LinearVelocity.z;

        if (nlDLRingIsEnd(pHeadRef, pListEntry) || pListEntry == NULL)
        {
            pListEntry = NULL;
        }
        else
        {
            pListEntry = pListEntry->m_next;
        }
    }

    fTestTime = fSimulationTime + 6.0f;

    while (mfLastCacheTime < fTestTime)
    {
        PhysicsUpdate(mpPredictWorld->mpPhysicsWorld, fPhysicsTick);
        mfLastCacheTime += fPhysicsTick;

        BallCacheInfo* pNewInfo = AddCacheEntry(mfLastCacheTime, (PhysicsBall*)mpPredictWorld->mpPhysicsBall);

        float zPos = pNewInfo->mv3Position.z;
        float zVel = pNewInfo->mv3LinearVelocity.z;

        if ((zPos <= fHeight && (!bDownOnly || zVel <= 0.0f)) || (fLastZVel < 0.0f && zVel > 0.0f))
        {
            v3ContactPoint = pNewInfo->mv3Position;
            v3ContactVelocity = pNewInfo->mv3LinearVelocity;
            return pNewInfo->mfTime - fSimulationTime;
        }

        fLastZVel = zVel;
    }

    return -1.0f;
}

/**
 * Offset/Address/Size: 0x7B0 | 0x80137B9C | size: 0x600
 */
float FakeBallWorld::GetPredictedPosAtDistance(float fDistance, nlVector3& v3Position, nlVector3& v3Velocity)
{
    cBall* pBall = mpPredictWorld->GetBall();

    float speedSq = pBall->m_v3Velocity.x * pBall->m_v3Velocity.x
                  + pBall->m_v3Velocity.y * pBall->m_v3Velocity.y
                  + pBall->m_v3Velocity.z * pBall->m_v3Velocity.z;

    if (speedSq < 0.0001f)
    {
        v3Position = pBall->m_v3Position;
        v3Velocity = v3Zero;
        return -1.0f;
    }

    if (!GetPredictedBallPosition(0.0f, v3Position, v3Velocity))
    {
        return -1.5f;
    }

    float fSimulationTime;
    float fDistanceTargetSq;
    float fMaxTime;
    float fPhysicsTick;
    fPhysicsTick = FixedUpdateTask::GetPhysicsUpdateTick();
    fDistanceTargetSq = fDistance * fDistance;
    fSimulationTime = FixedUpdateTask::mSimulationTime;

    DLListEntry<BallCacheInfo*>** ppHead = &mBallCacheList.m_Head;
    DLListEntry<BallCacheInfo*>* pHead = *ppHead;

    if (pHead != NULL)
    {
        DLListEntry<BallCacheInfo*>* pHeadRef;
        DLListEntry<BallCacheInfo*>* pListEntry = nlDLRingGetStart(pHead);
        pHeadRef = *ppHead;
        BallCacheInfo* pPrev;
        BallCacheInfo* pNext = pListEntry->entry;

        float fDistanceNextSq = nlGetLengthSquared3D(pNext->mv3Position.x - pBall->m_v3Position.x, pNext->mv3Position.y - pBall->m_v3Position.y, pNext->mv3Position.z - pBall->m_v3Position.z);

        while (!nlDLRingIsEnd(pHeadRef, pListEntry))
        {
            if (nlDLRingIsEnd(pHeadRef, pListEntry) || pListEntry == NULL)
                pListEntry = NULL;
            else
                pListEntry = pListEntry->m_next;

            pPrev = pNext;
            pNext = pListEntry->entry;
            float fDistancePrevSq = fDistanceNextSq;

            fDistanceNextSq = nlGetLengthSquared3D(pNext->mv3Position.x - pBall->m_v3Position.x, pNext->mv3Position.y - pBall->m_v3Position.y, pNext->mv3Position.z - pBall->m_v3Position.z);

            if (fDistanceNextSq > fDistanceTargetSq)
            {
                float sqrtPrev = nlSqrt(fDistancePrevSq, true);
                float sqrtNext = nlSqrt(fDistanceNextSq, true);
                float fPercent = (fDistance - sqrtPrev) / (sqrtNext - sqrtPrev);
                float fTime = Interpolate(pPrev->mfTime, pNext->mfTime, fPercent) - fSimulationTime;

                float fInvPercent = 1.0f - fPercent;
                v3Position.x = fInvPercent * pPrev->mv3Position.x + fPercent * pNext->mv3Position.x;
                v3Position.y = fInvPercent * pPrev->mv3Position.y + fPercent * pNext->mv3Position.y;
                v3Position.z = fInvPercent * pPrev->mv3Position.z + fPercent * pNext->mv3Position.z;
                v3Velocity.x = fInvPercent * pPrev->mv3LinearVelocity.x + fPercent * pNext->mv3LinearVelocity.x;
                v3Velocity.y = fInvPercent * pPrev->mv3LinearVelocity.y + fPercent * pNext->mv3LinearVelocity.y;
                v3Velocity.z = fInvPercent * pPrev->mv3LinearVelocity.z + fPercent * pNext->mv3LinearVelocity.z;

                return fTime;
            }

            if (fDistanceNextSq <= fDistancePrevSq)
            {
                v3Position = pPrev->mv3Position;
                v3Velocity = pPrev->mv3LinearVelocity;
                return -2.0f;
            }
        }
    }

    DLListEntry<BallCacheInfo*>* pLastEntry = nlDLRingGetEnd(*ppHead);
    BallCacheInfo* pCurCache = pLastEntry->entry;

    float fDistanceCurSq = nlGetLengthSquared3D(pCurCache->mv3Position.x - pBall->m_v3Position.x, pCurCache->mv3Position.y - pBall->m_v3Position.y, pCurCache->mv3Position.z - pBall->m_v3Position.z);

    fMaxTime = 6.0f + fSimulationTime;

    while (mfLastCacheTime < fMaxTime)
    {
        BallCacheInfo* pLastCache = pCurCache;
        float fDistanceLastSq = fDistanceCurSq;

        PhysicsUpdate(mpPredictWorld->mpPhysicsWorld, fPhysicsTick);

        mfLastCacheTime += fPhysicsTick;
        BallCacheInfo* pNewInfo = AddCacheEntry(mfLastCacheTime, (PhysicsBall*)mpPredictWorld->mpPhysicsBall);

        pCurCache = pNewInfo;

        fDistanceCurSq = nlGetLengthSquared3D(pNewInfo->mv3Position.x - pBall->m_v3Position.x, pNewInfo->mv3Position.y - pBall->m_v3Position.y, pNewInfo->mv3Position.z - pBall->m_v3Position.z);

        if (fDistanceCurSq > fDistanceTargetSq)
        {
            float sqrtLast = nlSqrt(fDistanceLastSq, true);
            float sqrtCur = nlSqrt(fDistanceCurSq, true);
            float fPercent = (fDistance - sqrtLast) / (sqrtCur - sqrtLast);
            float fTime = Interpolate(pLastCache->mfTime, pNewInfo->mfTime, fPercent) - fSimulationTime;

            float fInvPercent = 1.0f - fPercent;
            v3Position.x = fInvPercent * pLastCache->mv3Position.x + fPercent * pNewInfo->mv3Position.x;
            v3Position.y = fInvPercent * pLastCache->mv3Position.y + fPercent * pNewInfo->mv3Position.y;
            v3Position.z = fInvPercent * pLastCache->mv3Position.z + fPercent * pNewInfo->mv3Position.z;
            v3Velocity.x = fInvPercent * pLastCache->mv3LinearVelocity.x + fPercent * pNewInfo->mv3LinearVelocity.x;
            v3Velocity.y = fInvPercent * pLastCache->mv3LinearVelocity.y + fPercent * pNewInfo->mv3LinearVelocity.y;
            v3Velocity.z = fInvPercent * pLastCache->mv3LinearVelocity.z + fPercent * pNewInfo->mv3LinearVelocity.z;

            return fTime;
        }

        if (fDistanceCurSq <= fDistanceLastSq)
        {
            v3Position = pLastCache->mv3Position;
            v3Velocity = pLastCache->mv3LinearVelocity;
            return -3.0f;
        }
    }

    v3Position = pCurCache->mv3Position;
    v3Velocity = pCurCache->mv3LinearVelocity;
    return -4.0f;
}

FakeBallWorld::FakeBallWorld(cBall* pBall)
    : mpBall(pBall)
{
    mpPhysicsWorld = new (nlMalloc(sizeof(PhysicsWorld), 8, false)) PhysicsWorld();
    mpCollisionSpace = new (nlMalloc(sizeof(SimpleCollisionSpace), 8, false)) SimpleCollisionSpace(mpPhysicsWorld);
    mpPhysicsWorld->SetCFM(0.00001f);
    mpGroundPlane = new (nlMalloc(sizeof(PhysicsGroundPlane), 8, false)) PhysicsGroundPlane(mpCollisionSpace);
    mpPhysicsBall = new (nlMalloc(sizeof(FakePhysicsBall), 8, false)) FakePhysicsBall(0.18f, *this);
}

FakeBallWorld::~FakeBallWorld()
{
    delete mpPhysicsBall;
    delete mpGroundPlane;
    delete mpCollisionSpace;
    delete mpPhysicsWorld;
}

static inline void ResetBallIteratorState()
{
    static nlDLListIterator<BallCacheInfo*> iter = FakeBallWorld::mBallCacheList.Begin();

    iter.m_Curr = nlDLRingGetStart(FakeBallWorld::mBallCacheList.m_Head);
    iter.m_Head = FakeBallWorld::mBallCacheList.m_Head;
    FakeBallWorld::mpCacheIterator = &iter;

    if (FakeBallWorld::mpCacheIterator->m_Curr != NULL)
    {
        if (nlDLRingIsEnd(FakeBallWorld::mpCacheIterator->m_Head, FakeBallWorld::mpCacheIterator->m_Curr) || iter.m_Curr == NULL)
        {
            iter.m_Curr = NULL;
        }
        else
        {
            iter.m_Curr = iter.m_Curr->m_next;
        }
    }
}

/**
 * Offset/Address/Size: 0x668 | 0x80137A54 | size: 0xC8
 */
void FakeBallWorld::ResetBallIterator()
{
    nlVector3 v3Position;
    nlVector3 v3Velocity;

    GetPredictedBallPosition(0.0f, v3Position, v3Velocity);
    ResetBallIteratorState();
}

/**
 * Offset/Address/Size: 0x484 | 0x80137870 | size: 0x1E4
 */
void FakeBallWorld::GetNextBallPosition(nlVector3& v3BallPos)
{
    if (mpCacheIterator->m_Curr != NULL)
    {
        DLListEntry<BallCacheInfo*>* entry = mpCacheIterator->m_Curr;
        BallCacheInfo* info = entry->entry;
        v3BallPos = info->mv3Position;

        nlDLListIterator<BallCacheInfo*>* iter = mpCacheIterator;
        if (nlDLRingIsEnd(iter->m_Head, iter->m_Curr) || iter->m_Curr == NULL)
        {
            iter->m_Curr = NULL;
        }
        else
        {
            iter->m_Curr = iter->m_Curr->m_next;
        }
        return;
    }

    float fPhysicsTick = FixedUpdateTask::GetPhysicsUpdateTick();
    FakeBallWorld* predictWorld = mpPredictWorld;
    PhysicsUpdate(predictWorld->mpPhysicsWorld, fPhysicsTick);

    predictWorld = mpPredictWorld;
    mfLastCacheTime += fPhysicsTick;

    BallCacheInfo* newInfo = AddCacheEntry(mfLastCacheTime, (PhysicsBall*)predictWorld->mpPhysicsBall);

    v3BallPos = newInfo->mv3Position;
}

void FakeBallWorld::GetNextBallPosVel(nlVector3& v3BallPos, nlVector3& v3BallVel)
{
    nlDLListIterator<BallCacheInfo*>* cacheIter = FakeBallWorld::mpCacheIterator;

    if (cacheIter->m_Curr != NULL)
    {
        BallCacheInfo* info = cacheIter->m_Curr->entry;
        v3BallPos = info->mv3Position;
        v3BallVel = info->mv3LinearVelocity;

        if (nlDLRingIsEnd(cacheIter->m_Head, cacheIter->m_Curr) || cacheIter->m_Curr == NULL)
        {
            cacheIter->m_Curr = NULL;
        }
        else
        {
            cacheIter->m_Curr = cacheIter->m_Curr->m_next;
        }
        return;
    }

    float tick = FixedUpdateTask::GetPhysicsUpdateTick();
    PhysicsUpdate(FakeBallWorld::mpPredictWorld->mpPhysicsWorld, tick);
    FakeBallWorld::mfLastCacheTime += tick;

    BallCacheInfo* newInfo = FakeBallWorld::AddCacheEntry(mfLastCacheTime, (PhysicsBall*)FakeBallWorld::mpPredictWorld->mpPhysicsBall);

    v3BallPos = newInfo->mv3Position;
    v3BallVel = newInfo->mv3LinearVelocity;
}

/**
 * Offset/Address/Size: 0x98 | 0x80137484 | size: 0x3EC
 */
bool FakeBallWorld::FindBallIntercept(const nlVector3& v3PlayerPos, float fPlayerReach, float fPlayerSpeed, nlVector3& v3InterceptPos, nlVector3& v3InterceptVel, float& fInterceptTime, float& fClosestDist, float fMaxTime)
{
    fInterceptTime = 0.0f;
    fClosestDist = 10000.0f;
    unsigned char bDone = 0;

    float fPlayerDistPerTick = fPlayerSpeed * FixedUpdateTask::GetPhysicsUpdateTick();

    nlVector3 v3NewBallPos;
    nlVector3 v3NewBallVel;
    nlVector3 v3CurrentVelocity;
    nlVector3 v3CurrentPosition;
    GetPredictedBallPosition(0.0f, v3CurrentPosition, v3CurrentVelocity);
    ResetBallIteratorState();

    float fPlayerDistanceFromStartingPoint = fPlayerReach;

    while (!bDone)
    {
        GetNextBallPosVel(v3NewBallPos, v3NewBallVel);

        fPlayerDistanceFromStartingPoint += fPlayerDistPerTick;

        float dx = v3NewBallPos.x - v3PlayerPos.x;
        float dy = v3NewBallPos.y - v3PlayerPos.y;
        float dist = nlSqrt(dx * dx + dy * dy, true);
        float adjustedDist = fabsf(dist - fPlayerDistanceFromStartingPoint);

        if (adjustedDist >= fClosestDist)
        {
            bDone = 1;
        }
        else
        {
            v3InterceptPos = v3NewBallPos;
            v3InterceptVel = v3NewBallVel;
            fClosestDist = adjustedDist;
        }

        fInterceptTime += FixedUpdateTask::GetPhysicsUpdateTick();

        if (fInterceptTime >= fMaxTime)
        {
            bDone = 1;
        }
    }

    return fInterceptTime < fMaxTime;
}

void FakeBallWorld::SetHitInfo(dContactGeom* info)
{
    mbHitSuccess = true;
    mContactInfo = *info;
}

void FakeBallWorld::ClearBallCache()
{
    if (mBallCacheList.m_Head != NULL)
    {
        DLListEntry<BallCacheInfo*>* current = nlDLRingGetStart(mBallCacheList.m_Head);
        nlDLListIterator<BallCacheInfo*> iter;
        iter.m_Head = mBallCacheList.m_Head;
        iter.m_Curr = current;
        while (iter.hasNext())
        {
            BallCacheInfo::mBallCacheInfoSlotPool.Free(iter.m_Curr->entry);
            iter.next();
        }
        mBallCacheList.Clear();
    }
    mfLastCacheTime = -1.0f;
}

FakePhysicsBall::FakePhysicsBall(float radius, FakeBallWorld& fakeBallWorld)
    : PhysicsBall(fakeBallWorld.mpCollisionSpace, fakeBallWorld.mpPhysicsWorld, radius)
    , mWorld(fakeBallWorld)
{
}

/**
 * Offset/Address/Size: 0x60 | 0x8013744C | size: 0x38
 */
ContactType FakePhysicsBall::Contact(PhysicsObject* object, dContact* contact, int numContacts)
{
    if (mWorld.mbHitSuccess)
    {
        return NO_CONTACT;
    }

    return PhysicsBall::Contact(object, contact, numContacts);
}

PhysicsGoaliePlane::PhysicsGoaliePlane(const nlVector4& plane, FakeBallWorld& fakeBallWorld)
    : PhysicsPlane(fakeBallWorld.mpCollisionSpace, plane.x, plane.y, plane.z, plane.w)
    , mWorld(fakeBallWorld)
{
}

ContactType PhysicsGoaliePlane::Contact(PhysicsObject* object, dContact* info, int numContacts)
{
    mWorld.SetHitInfo(&info->geom);
    return NO_CONTACT;
}
