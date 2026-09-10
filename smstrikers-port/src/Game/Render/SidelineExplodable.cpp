#include "Game/Render/SidelineExplodable.h"
#include "Game/Render/AnimatedModelExplodable.h"
#include "Game/Render/StaticModelExplodable.h"
#include "Game/Character.h"
#include "Game/Effects/EffectsGroup.h"
#include "Game/Effects/EmissionManager.h"
#include "Game/Field.h"
#include "Game/Game.h"
#include "Game/Physics/CollisionSpace.h"
#include "Game/WorldManager.h"
#include "NL/glx/glxTexture.h"
#include "NL/nlFunction.h"
#include "NL/nlMath.h"
#include "NL/vmath.h"
#include "NL/nlString.h"
#include "types.h"

#include "NL/nlBind.h"

extern PhysicsWorld* g_PhysicsWorld;

float ExplosionFragment::sfFadeOutTime = 1.0f;

nlList<SidelineExplodableNode> SidelineExplodableManager::sSidelineExplodableList(NULL, NULL);
nlList<DrawableFragmentHandleNode> SidelineExplodableManager::sUnusedDrawableFragments(NULL, NULL);
bool SidelineExplodableManager::sbIsInitialized;
ExplosionFragment** SidelineExplodableManager::sFragmentLookupTable = NULL;
SlotPool<SidelineExplodableNode> SidelineExplodableNode::sSidelineExplodableNodeSlotPool(16, 16);
SlotPool<DrawableFragmentHandleNode> DrawableFragmentHandleNode::sDrawableFragmentHandleNodePool(16, 16);

/**
 * Offset/Address/Size: 0x2188 | 0x801694E8 | size: 0x200
 */
ExplosionFragment::~ExplosionFragment()
{
    Deactivate();
}

void ExplosionFragment::Deactivate()
{
    if (mbIsActive)
    {
        if (mpPhysicsObject != NULL)
        {
            delete mpPhysicsObject;
        }

        DrawableObject* drawable = WorldManager::s_World->FindDrawableObject(mFragmentModelHash);
        drawable->m_uObjectFlags &= ~1;
        mpPhysicsObject = NULL;
        SidelineExplodableManager::ReturnDrawableFragmentToPool(mDrawableFragmentID);
        mDrawableFragmentID = 0xFFFF;
        mbIsActive = false;
    }

    mFragmentModelHash = 0;

    if (mpSmokeEmissionController != NULL)
    {
        EmissionController& smokeControl = *mpSmokeEmissionController;
        smokeControl.mUpdateCallback = Function1<void, EmissionController&>();
    }

    if (mStationaryTransform != NULL)
    {
        operator delete(mStationaryTransform);
        mStationaryTransform = NULL;
    }
}

/**
 * Offset/Address/Size: 0x2134 | 0x80169494 | size: 0x54
 */
void UpdateEmissionControllerPosition(EmissionController& ec, ExplosionFragment* pFragment)
{
    const nlVector3* pPos;
    if (pFragment->mpPhysicsObject != NULL)
    {
        pPos = &pFragment->mpPhysicsObject->GetPosition();
    }
    else
    {
        pPos = (const nlVector3*)&(pFragment->mStationaryTransform->m41);
    }
    ec.SetPosition(*pPos);
}

/**
 * Offset/Address/Size: 0x2128 | 0x80169488 | size: 0xC
 */
void EmissionControllerFinished(EmissionController& ec, ExplosionFragment* p0)
{
    p0->mpSmokeEmissionController = NULL;
}

/**
 * Offset/Address/Size: 0x20EC | 0x8016944C | size: 0x3C
 */
nlVector3& ExplosionFragment::GetPosition() const
{
    if (mpPhysicsObject != NULL)
    {
        return mpPhysicsObject->GetPosition();
    }
    return *(nlVector3*)&mStationaryTransform->e2[3];
}

/**
 * Offset/Address/Size: 0x2048 | 0x801693A8 | size: 0xA4
 */
void ExplosionFragment::GetRotation(nlMatrix4* dest) const
{
    if (mpPhysicsObject != NULL)
    {
        mpPhysicsObject->GetRotation(dest);
        return;
    }

    nlMatrix4* src = mStationaryTransform;

    // Copy 3x3 rotation with 4th column as zeros
    ((f32*)dest)[0] = ((f32*)src)[0];
    ((f32*)dest)[1] = ((f32*)src)[1];
    ((f32*)dest)[2] = ((f32*)src)[2];
    ((f32*)dest)[3] = 0.0f;

    ((f32*)dest)[4] = ((f32*)src)[4];
    ((f32*)dest)[5] = ((f32*)src)[5];
    ((f32*)dest)[6] = ((f32*)src)[6];
    ((f32*)dest)[7] = 0.0f;

    ((f32*)dest)[8] = ((f32*)src)[8];
    ((f32*)dest)[9] = ((f32*)src)[9];
    ((f32*)dest)[10] = ((f32*)src)[10];
    ((f32*)dest)[11] = 0.0f;

    // 4th row is 0, 0, 0, 1
    ((f32*)dest)[12] = 0.0f;
    ((f32*)dest)[13] = 0.0f;
    ((f32*)dest)[14] = 0.0f;
    ((f32*)dest)[15] = 1.0f;
}

/**
 * Offset/Address/Size: 0x200C | 0x8016936C | size: 0x3C
 */
SidelineExplodable::SidelineExplodable()
{
    mExplosionFragments.mData = NULL;
    mExplosionFragments.mSize = 0;
    mExplosionFragments.mCapacity = 0;
    mNumActiveFragments = 0;
    mbAngleRangeInitialized = false;
    mbIsMainModelVisible = true;
    mfExplodeTime = 0.0f;
    mpAssociatedEffect = NULL;
}

/**
 * Offset/Address/Size: 0x1D74 | 0x801690D4 | size: 0x298
 */
SidelineExplodable::~SidelineExplodable()
{
    for (int i = 0; i < mExplosionFragments.mSize; i++)
    {
        mExplosionFragments[i].Deactivate();
    }

    SidelineExplodableManager::RemoveSidelineExplodable(this);
}

/**
 * Offset/Address/Size: 0x1CC4 | 0x80169024 | size: 0xB0
 */
void SidelineExplodable::Initialize(int numFragmentModels)
{
    mNumFragmentModels = numFragmentModels;
    Allocate();

    SidelineExplodableManager::AddSidelineExplodable(this);
}

/**
 * Offset/Address/Size: 0x1C9C | 0x80168FFC | size: 0x28
 */
void SidelineExplodable::Allocate()
{
    mExplosionFragments.resize(mNumFragmentModels);
}

void SidelineExplodable::DeAllocate()
{
}

/**
 * Offset/Address/Size: 0x19A0 | 0x80168D00 | size: 0x2FC
 */
void SidelineExplodable::Update(float fDeltaT)
{
    if (mNumActiveFragments != 0)
    {
        for (int i = 0; i < mNumFragmentModels; i++)
        {
            if (mExplosionFragments[i].mfRemainingLifespan <= 0.0f)
            {
                if (mExplosionFragments[i].mbIsActive)
                {
                    mExplosionFragments[i].Deactivate();

                    mNumActiveFragments--;
                    ExplodableCategoryData& categoryData = GetCategoryData();
                    if (mNumActiveFragments == categoryData.mNumStationaryFragments)
                    {
                        g_pEventManager->CreateValidEvent(0x67, 0x14);
                    }
                }
            }
            else if (!mExplosionFragments[i].mbInfiniteLifespan)
            {
                mExplosionFragments[i].mfRemainingLifespan -= fDeltaT;
            }
        }

        if (mbIsMainModelVisible)
        {
            DestroyAllActiveFragments(true);
        }
    }
    else
    {
        if (mfExplodeTime > 0.0f)
        {
            mfExplodeTime -= fDeltaT;
        }

        if (mfExplodeTime < 0.0f)
        {
            Explode();
            mfExplodeTime = 0.0f;
        }
    }
}

/**
 * Offset/Address/Size: 0x1710 | 0x80168A70 | size: 0x290
 */
void SidelineExplodable::DestroyAllActiveFragments(bool renewExplodables)
{
    if (mNumActiveFragments != 0)
    {
        for (int i = 0; i < mNumFragmentModels; i++)
        {
            if (mExplosionFragments[i].mbIsActive)
            {
                if (renewExplodables || !mExplosionFragments[i].mbInfiniteLifespan)
                {
                    mExplosionFragments[i].Deactivate();

                    mNumActiveFragments--;
                    ExplodableCategoryData& categoryData = GetCategoryData();
                    if (mNumActiveFragments == categoryData.mNumStationaryFragments)
                    {
                        g_pEventManager->CreateValidEvent(0x67, 0x14);
                    }
                }
            }
        }
    }

    if (renewExplodables)
    {
        mbIsMainModelVisible = true;
    }

    mfExplodeTime = 0.0f;
}

// PORT: the allocation above must not go back to a literal.
static_assert(sizeof(SidelineExplosionPhysicsObject) >= 0x30,
    "SidelineExplosionPhysicsObject is smaller than the console's 0x30");

void SidelineExplodable::InitializePhysicsObject(PhysicsObject* pPhysicsObject, const nlMatrix4& worldMatrix, bool bIsStationary)
{
    unsigned short min;
    unsigned short max;
    nlPolar velPolar;
    float fragmentSpeed;
    nlVector3 vel;
    nlVector3 angularVel;

    FindExplosionAngleRange(min, max);
    pPhysicsObject->SetWorldMatrix(worldMatrix);

    if (!bIsStationary)
    {
        int maxAngle = mMaxExplosionAngle;
        const unsigned short minAngleValue = mMinExplosionAngle;
        const int minAngle = minAngleValue;
        float randomAngle = nlRandomf(0.0f, 1.0f, &nlDefaultSeed);
        short angleDelta = maxAngle - minAngle;

        velPolar.a = minAngle + (short)(int)(randomAngle * (float)angleDelta);

        fragmentSpeed = nlRandomf(-0.0f, 0.0f, &nlDefaultSeed) + 40.0f;
        float fragmentHeight = nlRandomf(-0.0f, 0.0f, &nlDefaultSeed) + 13.0f;
        float speedAngle = (3.1415927f * fragmentSpeed) / 180.0f;
        maxAngle = (int)(10430.378f * speedAngle);

        velPolar.r = fragmentHeight * nlSin((u16)maxAngle + 0x4000);
        nlPolarToCartesian(vel, velPolar);
        vel.z = fragmentHeight * nlSin((u16)maxAngle);

        float angVelX = nlRandomf(-6.0f, 6.0f, &nlDefaultSeed);
        float angVelY = nlRandomf(-6.0f, 6.0f, &nlDefaultSeed);
        float angVelZ = nlRandomf(-6.0f, 6.0f, &nlDefaultSeed);
        angularVel.x = angVelZ;
        angularVel.y = angVelY;
        angularVel.z = angVelX;
        pPhysicsObject->SetLinearVelocity(vel);
        pPhysicsObject->SetAngularVelocity(angularVel);
    }
}

/**
 * Offset/Address/Size: 0xE84 | 0x801681E4 | size: 0x88C
 */
void SidelineExplodable::Explode()
{
    if (mNumActiveFragments != 0 || !mbIsMainModelVisible)
    {
        return;
    }

    nlMatrix4 worldMatrix;
    worldMatrix.SetIdentity();
    worldMatrix.SetTranslation(GetWorldMatrix().GetTranslation());
    if (!WorldManager::s_World->IsSphereInFrustum(worldMatrix, 1.0f))
        return;

    mbIsMainModelVisible = false;
    if (!g_pGame->mbCaptainShotToScoreOn)
    {
        EmissionController* pSmokeControl = EmissionManager::Create(fxGetGroup("explosion_smoke"), 0);
        pSmokeControl->SetPosition(*(nlVector3*)&GetWorldMatrix().m41);
    }

    CollisionBobombData* pEventData = new ((CollisionBobombData*)&g_pEventManager->CreateValidEvent(0x66, 0x34)->m_data) CollisionBobombData();
    pEventData->v3ExplosionLocation = *(nlVector3*)&GetWorldMatrix().m41;

    int iFragment = 0;
    for (; iFragment < mNumFragmentModels; iFragment++)
    {
        unsigned short handle = SidelineExplodableManager::GetDrawableFragmentFromPool();
        if ((u16)handle == 0xFFFF)
            continue;

        ExplosionFragment& fragment = mExplosionFragments.mData[iFragment];
        fragment.mDrawableFragmentID = handle;
        SidelineExplodableManager::RegisterFragment(&fragment, handle);

        ExplodableCategoryData& fragmentCategoryData = GetCategoryData();
        unsigned long hash = fragmentCategoryData.mFragmentModelList[iFragment];
        DrawableObject* drawable = WorldManager::s_World->FindDrawableObject(hash);
        fragment.mFragmentModelHash = drawable->m_uHashID;

        if (iFragment < GetCategoryData().mNumStationaryFragments)
            fragment.mbIsStationary = true;

        nlMatrix4 fragmentInitialTransform;
        AABBDimensions dim;
        drawable->GetAABBDimensions(dim, false);
        drawable->m_bRenderPlanarShadow = true;

        fragment.mfRemainingLifespan = nlRandomf(-0.0f, 0.0f, &nlDefaultSeed) + 2.0f;

        if (!fragment.mbIsStationary)
        {
            // PORT: 0x30 is the console's sizeof for this class, which is bigger here.
            PhysicsBox* box = (PhysicsBox*)::operator new(
                sizeof(SidelineExplosionPhysicsObject), 8, false);
            box = new (box) SidelineExplosionPhysicsObject(g_CollisionSpace, g_PhysicsWorld, dim.mDim.x, dim.mDim.y, dim.mDim.z, &fragment);
            box->SetDensity(5.0f);

            ExplodableCategoryData& transformCategoryData = GetCategoryData();
            fragmentInitialTransform = transformCategoryData.mInitialTransforms[iFragment];
            nlMultMatrices(fragmentInitialTransform, fragmentInitialTransform, GetWorldMatrix());

            InitializePhysicsObject(box, fragmentInitialTransform, fragment.mbIsStationary);
            fragment.mpPhysicsObject = box;
        }
        else
        {
            ExplodableCategoryData& transformCategoryData = GetCategoryData();
            fragmentInitialTransform = transformCategoryData.mInitialTransforms[iFragment];
            nlMultMatrices(fragmentInitialTransform, fragmentInitialTransform, GetWorldMatrix());

            fragment.SetStationaryTransform(fragmentInitialTransform);
            fragment.mbInfiniteLifespan = true;
        }

        fragment.mbIsActive = true;
        mNumActiveFragments++;

        EmissionController* pSmokeControl = EmissionManager::Create(fxGetGroup("explosion_fragment_smoke"), 0);
        pSmokeControl->SetUpdateCallback(Function1<void, EmissionController&>(
            Bind<void>(UpdateEmissionControllerPosition, placeholder0, &fragment)));
        pSmokeControl->SetFinishedCallback(Function1<void, EmissionController&>(
            Bind<void>(EmissionControllerFinished, placeholder0, &fragment)));

        pSmokeControl->SetPosition(*(nlVector3*)&GetWorldMatrix().m41);
        fragment.mpSmokeEmissionController = pSmokeControl;
    }
}

/**
 * Offset/Address/Size: 0xC08 | 0x80167F68 | size: 0x27C
 */
void SidelineExplodable::FindExplosionAngleRange(unsigned short& min, unsigned short& max) const
{
    if (!mbAngleRangeInitialized)
    {
        float explosionRadius = cField::GetSidelineY(1U);
        const nlMatrix4& worldMatrix = GetWorldMatrix();
        u16 angleToCentreOfField = (u16)(s32)(10430.378f * nlATan2f(worldMatrix.m42, worldMatrix.m41));
        angleToCentreOfField = (u16)(angleToCentreOfField + 0x8000);

        nlPolar polar = { 0, explosionRadius };
        polar.a = angleToCentreOfField;

        nlVector3 particleDestination = GetWorldMatrix().GetTranslation();
        nlAddPolarToCartesian(particleDestination, polar);

        polar.a = angleToCentreOfField;
        bool foundMax = false;
        while (!foundMax)
        {
            particleDestination = GetWorldMatrix().GetTranslation();
            nlAddPolarToCartesian(particleDestination, polar);
            if (!cField::IsOnField(particleDestination))
            {
                foundMax = true;
                mMaxExplosionAngle = polar.a - 0x38E;
            }
            else
            {
                polar.a += 0x38E;
            }
        }

        polar.a = angleToCentreOfField;
        bool foundMin = false;
        while (!foundMin)
        {
            particleDestination = GetWorldMatrix().GetTranslation();
            nlAddPolarToCartesian(particleDestination, polar);
            if (!cField::IsOnField(particleDestination))
            {
                foundMin = true;
                mMinExplosionAngle = polar.a + 0x38E;
            }
            else
            {
                polar.a -= 0x38E;
            }
        }

        particleDestination = GetWorldMatrix().GetTranslation();
        polar.a = mMinExplosionAngle;
        nlAddPolarToCartesian(particleDestination, polar);

        particleDestination = GetWorldMatrix().GetTranslation();
        polar.a = mMaxExplosionAngle;
        nlAddPolarToCartesian(particleDestination, polar);

        mbAngleRangeInitialized = true;
    }

    min = mMinExplosionAngle;
    max = mMaxExplosionAngle;
}

/**
 * Offset/Address/Size: 0xC04 | 0x80167F64 | size: 0x4
 */
// PORT: was `void ...(unsigned long) {}`, installed through a cast.
unsigned long SidelineExplodableTextureLoadCallback(unsigned long textureId)
{
    return textureId;
}

/**
 * Offset/Address/Size: 0x9EC | 0x80167D4C | size: 0x218
 */
bool ExplodableCategoryData::LoadGeometry()
{
    glxTextureLoadCallback_t cb = glx_SetLoadCallback(SidelineExplodableTextureLoadCallback);

    int numFragmentModelsLoaded = 0;
    if (!WorldManager::s_World->LoadGeometry(mBaseModelName, true, true, mFragmentModelList, &numFragmentModelsLoaded))
    {
        return false;
    }

    mNumStationaryFragments = numFragmentModelsLoaded;

    int numFragmentModelsLoaded2 = 0;
    if (!WorldManager::s_World->LoadGeometry(mFragmentModelName, true, true, &mFragmentModelList[mNumStationaryFragments], &numFragmentModelsLoaded2))
    {
        return false;
    }

    mNumFragmentModels = numFragmentModelsLoaded2 + mNumStationaryFragments;

    if (mUnexplodedModelName != NULL)
    {
        int numModelsLoaded = 0;
        if (!WorldManager::s_World->LoadGeometry(mUnexplodedModelName, true, false, &mUnexplodedModel, &numModelsLoaded))
        {
            return false;
        }

        DrawableObject* drawable = WorldManager::s_World->FindDrawableObject(mUnexplodedModel);
        drawable->m_uObjectFlags &= ~1;
        drawable->m_bRenderPlanarShadow = true;
    }

    glx_SetLoadCallback(cb);

    int i;
    for (i = 0; i < mNumFragmentModels; i++)
    {
        DrawableObject* drawable = WorldManager::s_World->FindDrawableObject(mFragmentModelList[i]);
        drawable->m_uObjectFlags &= ~1;
        drawable->m_uObjectCreationFlags |= 0xF000;
        mInitialTransforms[i] = drawable->GetWorldMatrix();
    }

    return true;
}

void SidelineExplodableManager::Initialize()
{
    sbIsInitialized = true;
    sFragmentLookupTable =
        (ExplosionFragment**)nlMalloc(20 * sizeof(ExplosionFragment*), 8, false);
    for (unsigned short i = 0; i < 20; i++)
    {
        ReturnDrawableFragmentToPool(i);
    }
}

void SidelineExplodableManager::ReturnDrawableFragmentToPool(unsigned short handle)
{
    DrawableFragmentHandleNode* node = DrawableFragmentHandleNode::sDrawableFragmentHandleNodePool.Allocate();

    if (node != NULL)
    {
        node->mID = 0;
        node->next = NULL;
    }

    node->mID = handle;
    nlListAddEnd<DrawableFragmentHandleNode>(&sUnusedDrawableFragments.m_pStart, &sUnusedDrawableFragments.m_pEnd, node);
    sFragmentLookupTable[handle] = NULL;
}

unsigned short SidelineExplodableManager::GetDrawableFragmentFromPool()
{
    DrawableFragmentHandleNode* node = sUnusedDrawableFragments.m_pStart;
    unsigned short handle;
    if (node == NULL)
    {
        handle = 0xFFFF;
    }
    else
    {
        nlListRemoveStart(&sUnusedDrawableFragments.m_pStart, &sUnusedDrawableFragments.m_pEnd);
        handle = node->mID;
        *(u32*)node = *(u32*)&DrawableFragmentHandleNode::sDrawableFragmentHandleNodePool.m_FreeList;
        DrawableFragmentHandleNode::sDrawableFragmentHandleNodePool.m_FreeList = (SlotPoolEntry*)node;
    }
    return handle;
}

/**
 * Offset/Address/Size: 0x938 | 0x80167C98 | size: 0xB4
 */
void SidelineExplodableManager::CleanUp()
{
    AnimatedModelExplodable::CleanUp();
    StaticModelExplodable::CleanUp();

    if (sbIsInitialized)
    {
        DrawableFragmentHandleNode** pTail = &sUnusedDrawableFragments.m_pEnd;
        SlotPoolBase* pPool = &DrawableFragmentHandleNode::sDrawableFragmentHandleNodePool;
        DrawableFragmentHandleNode* node;

        while ((node = sUnusedDrawableFragments.m_pStart) != NULL)
        {
            nlListRemoveStart<DrawableFragmentHandleNode>(&sUnusedDrawableFragments.m_pStart, pTail);
            ((SlotPoolEntry*)node)->next = pPool->m_FreeList;
            pPool->m_FreeList = (SlotPoolEntry*)node;
        }

        operator delete[](sFragmentLookupTable);
        sFragmentLookupTable = NULL;
        sbIsInitialized = false;
    }

    SidelineExplodableNode::sSidelineExplodableNodeSlotPool.FreeBlocks();
    DrawableFragmentHandleNode::sDrawableFragmentHandleNodePool.FreeBlocks();
}

/**
 * Offset/Address/Size: 0x810 | 0x80167B70 | size: 0x128
 */
void SidelineExplodableManager::Update(float fDeltaT)
{
    if (!sbIsInitialized)
    {
        Initialize();
    }

    SidelineExplodableNode* node = sSidelineExplodableList.m_pStart;
    while (node != NULL)
    {
        node->mpExplodable->SidelineExplodable::Update(fDeltaT);
        node = node->next;
    }
}

/**
 * Offset/Address/Size: 0x7EC | 0x80167B4C | size: 0x24
 */
int SidelineExplodableManager::GetNumExplodables()
{
    return nlListCountElements<SidelineExplodableNode>(sSidelineExplodableList.m_pStart);
}

/**
 * Offset/Address/Size: 0x7C4 | 0x80167B24 | size: 0x28
 */
void SidelineExplodableManager::GetVisibilityOfExplodableModels(bool* visibility, int numExplodables)
{
    SidelineExplodableNode* node;

    bool* visibilityPtr = visibility;
    node = sSidelineExplodableList.m_pStart;
    while (node != NULL)
    {
        *visibilityPtr = node->mpExplodable->mbIsMainModelVisible;
        visibilityPtr += 1;
        node = node->next;
    }
}

/**
 * Offset/Address/Size: 0x744 | 0x80167AA4 | size: 0x80
 */
void SidelineExplodableManager::SetVisibilityOfUnexplodedModels(bool* visibility, int numExplodables)
{
    bool* visibilityPtr = visibility;
    SidelineExplodableNode* node = sSidelineExplodableList.m_pStart;

    while (node != NULL)
    {
        node->mpExplodable->SetUnexplodedModelVisibility(*visibilityPtr);
        if (node->mpExplodable->mpAssociatedEffect != NULL)
        {
            node->mpExplodable->mpAssociatedEffect->m_bDisabled = !*visibilityPtr;
        }
        node = node->next;
        visibilityPtr++;
    }
}

SidelineExplodable* SidelineExplodableManager::GetClosestExplodable(const nlVector3& pos)
{
    SidelineExplodable* closest = NULL;
    float distance = 0.0f;
    SidelineExplodableNode* node = sSidelineExplodableList.m_pStart;

    while (node != NULL)
    {
        const nlMatrix4& matrix = node->mpExplodable->GetWorldMatrix();
        nlVector3 delta;
        nlVec3Sub(delta, matrix.GetTranslation(), pos);
        float nodeDistance = delta.GetLengthSq3D();
        if (closest == NULL || nodeDistance < distance)
        {
            closest = node->mpExplodable;
            distance = nodeDistance;
        }
        node = node->next;
    }

    return closest;
}

/**
 * Offset/Address/Size: 0x650 | 0x801679B0 | size: 0xF4
 */
void SidelineExplodableManager::TriggerExplosions(const nlVector3& pos, float explosionRadius)
{
    SidelineExplodableNode* node = sSidelineExplodableList.m_pStart;
    float divisor = 25.0f;
    float posX;
    float posY;
    float posZ;
    float triggerDist = 1.2f * explosionRadius;
    posZ = pos.z;
    posY = pos.y;
    posX = pos.x;

    while (node != NULL)
    {
        nlVector3 delta;
        const nlVector3& position = node->mpExplodable->GetWorldMatrix().GetTranslation();
        nlVec3Sub(delta, position, pos);
        float dist = nlSqrt(delta.GetLengthSq3D(), true);
        if (dist < triggerDist)
        {
            node->mpExplodable->mfExplodeTime = dist / divisor;
        }
        node = node->next;
    }
}

/**
 * Offset/Address/Size: 0x600 | 0x80167960 | size: 0x50
 */
void SidelineExplodableManager::DestroyAllActiveFragments(bool renewExplodables)
{
    SidelineExplodableNode* node = sSidelineExplodableList.m_pStart;
    while (node != NULL)
    {
        node->mpExplodable->DestroyAllActiveFragments(renewExplodables);
        node = node->next;
    }
}

void SidelineExplodableManager::AddSidelineExplodable(SidelineExplodable* pSidelineExplodable)
{
    SidelineExplodableNode* node = NULL;

    if (SidelineExplodableNode::sSidelineExplodableNodeSlotPool.m_FreeList == NULL)
    {
        SlotPoolBase::BaseAddNewBlock(&SidelineExplodableNode::sSidelineExplodableNodeSlotPool, sizeof(SidelineExplodableNode));
    }

    SlotPoolEntry* entry = SidelineExplodableNode::sSidelineExplodableNodeSlotPool.m_FreeList;
    if (entry != NULL)
    {
        node = (SidelineExplodableNode*)entry;
        SidelineExplodableNode::sSidelineExplodableNodeSlotPool.m_FreeList = entry->next;
    }

    if (node != NULL)
    {
        node->mpExplodable = NULL;
        node->next = NULL;
    }

    node->mpExplodable = pSidelineExplodable;
    node->next = NULL;
    nlListAddEnd<SidelineExplodableNode>(&sSidelineExplodableList.m_pStart,
        &sSidelineExplodableList.m_pEnd,
        node);
}

/**
 * Offset/Address/Size: 0x588 | 0x801678E8 | size: 0x78
 */
void SidelineExplodableManager::RemoveSidelineExplodable(SidelineExplodable* pSidelineExplodable)
{
    SidelineExplodableNode* node = sSidelineExplodableList.m_pStart;
    while (node != NULL)
    {
        SidelineExplodableNode* nextnode = node->next;
        if (node->mpExplodable == pSidelineExplodable)
        {
            nlListRemoveElement<SidelineExplodableNode>(&sSidelineExplodableList.m_pStart, node, &sSidelineExplodableList.m_pEnd);
            ((SlotPoolEntry*)node)->next = SidelineExplodableNode::sSidelineExplodableNodeSlotPool.m_FreeList;
            SidelineExplodableNode::sSidelineExplodableNodeSlotPool.m_FreeList = (SlotPoolEntry*)node;
        }
        node = nextnode;
    }
}

/**
 * Offset/Address/Size: 0x568 | 0x801678C8 | size: 0x20
 */
ExplosionFragment* SidelineExplodableManager::GetFragmentFromHandle(unsigned short handle)
{
    if (sFragmentLookupTable == NULL)
    {
        return 0;
    }
    return sFragmentLookupTable[handle];
}

void SidelineExplodableManager::RegisterFragment(ExplosionFragment* fragment, unsigned short handle)
{
    sFragmentLookupTable[handle] = fragment;
}

SidelineExplosionPhysicsObject::SidelineExplosionPhysicsObject(CollisionSpace* space, PhysicsWorld* world, float side1, float side2, float side3, ExplosionFragment* pExplosionFragment)
    : PhysicsBox(space, world, side1, side2, side3)
    , mpExplosionFragment(pExplosionFragment)
{
    SetCategory(0x400);
    SetCollide(0xFF);
}

struct SwizzledVelocityProxy
{
    float y_field;
    float x_field;
    float z_field;
};

struct CharacterVelocityProxy
{
    u8 _pad0[0x30];
    SwizzledVelocityProxy m_v3Velocity;
};

struct PhysicsCharacterProxy
{
    u8 _pad0[0x8C];
    CharacterVelocityProxy* m_pAICharacter;
};

/**
 * Offset/Address/Size: 0x358 | 0x801676B8 | size: 0x1B0
 */
ContactType SidelineExplosionPhysicsObject::Contact(PhysicsObject* other, dContact* contact, int what, PhysicsObject* otherObject)
{
    if (mpExplosionFragment->mfRemainingLifespan < (0.5f * ExplosionFragment::sfFadeOutTime))
    {
        return NO_CONTACT;
    }

    if (other->GetObjectType() == 0x1C)
    {
        return NO_CONTACT;
    }

    if (other->GetObjectType() == 0x19)
    {
        return NO_CONTACT;
    }

    if ((other->GetObjectType() == 0x0E) || (other->GetObjectType() == 0x0D))
    {
        CollisionExplosionFragmentPlayerData* eventData;
        // PORT: was two stand-ins at console 0x8C and 0x30, both of which are named members.
        cCharacter* player = ((PhysicsCharacter*)other->m_parentObject)->m_pAICharacter;

        if (player != NULL)
        {
            nlVector3* linearVelocity = &GetLinearVelocity();
            float deltaX;
            float deltaY;
            float deltaZ;

            // PORT: x_field was the second float and y_field the first, so this was componentwise all along.
            deltaY = linearVelocity->y - player->m_v3Velocity.y;
            deltaX = linearVelocity->x - player->m_v3Velocity.x;
            deltaZ = linearVelocity->z - player->m_v3Velocity.z;

            float deltaYSq = deltaY * deltaY;
            float deltaSq = deltaYSq + deltaX * deltaX + deltaZ * deltaZ;

            if (deltaSq > 36.0f)
            {
                eventData = new (&g_pEventManager->CreateValidEvent(0x31, 0x34)->m_data) CollisionExplosionFragmentPlayerData();

                eventData->pPlayer = (cFielder*)player;

                float y;
                float z;
                z = contact->geom.pos[2];
                y = contact->geom.pos[1];
                float x = contact->geom.pos[0];

                eventData->v3CollisionLocation.x = x;
                eventData->v3CollisionLocation.y = y;
                eventData->v3CollisionLocation.z = z;
                eventData->v3CollisionVelocity = GetLinearVelocity();
            }
        }
    }

    return TWO_WAY_CONTACT;
}

/**
 * Offset/Address/Size: 0x338 | 0x80167698 | size: 0x20
 */
bool SidelineExplosionPhysicsObject::SetContactInfo(dContact* contact, PhysicsObject* other, bool first)
{
    contact->surface.mu = 75.0f;
    contact->surface.mode = 0x14;
    contact->surface.soft_cfm = 0.0001f;
    return true;
}

/**
 * Offset/Address/Size: 0x1D4 | 0x80167534 | size: 0x164
 */
void SidelineExplosionPhysicsObject::PostUpdate()
{
    nlVector3 angularVelocity;
    GetAngularVelocity(&angularVelocity);

    float lenSq = angularVelocity.x * angularVelocity.x + angularVelocity.y * angularVelocity.y + angularVelocity.z * angularVelocity.z;
    if (lenSq > 100.0f)
    {
        float recip = nlRecipSqrt(lenSq, true);

        float xNorm = recip * angularVelocity.x;
        float yNorm;
        float zNorm;
        zNorm = recip * angularVelocity.z;
        yNorm = recip * angularVelocity.y;
        angularVelocity.x = xNorm;
        angularVelocity.y = yNorm;
        angularVelocity.z = zNorm;

        nlVec3Scale(angularVelocity, angularVelocity, 10.0f);
        SetAngularVelocity(angularVelocity);
    }

    nlVector3 position = GetPosition();
    bool changed = false;

    if (position.x > 100.0f)
    {
        position.x = 100.0f;
        changed = true;
    }

    if (position.x < -100.0f)
    {
        position.x = -100.0f;
        changed = true;
    }

    if (position.y > 100.0f)
    {
        position.y = 100.0f;
        changed = true;
    }

    if (position.y < -100.0f)
    {
        position.y = -100.0f;
        changed = true;
    }

    if (position.z > 100.0f)
    {
        position.z = 100.0f;
        changed = true;
    }

    if (changed)
    {
        SetPosition(position, WORLD_COORDINATES);
    }
}

/**
 * Offset/Address/Size: 0xA0 | 0x80167400 | size: 0x134
 */
void SidelineExplodableManager::AssociateEffectWithNearbyFloatingCamera(EmissionController* pEmissionController)
{
    const nlVector3& position = pEmissionController->GetPosition();
    const char* floatingCamName = "environment/Sideline_Objects/standupcamera_base";
    SidelineExplodable* closest = GetClosestExplodable(position);

    const nlMatrix4& closestMat = closest->GetWorldMatrix();
    float cdy = closestMat.e2[3][1] - position.y;
    float cdx = closestMat.e2[3][0] - position.x;
    float closestDist = cdx * cdx + cdy * cdy;
    if (closestDist < 1.0f)
    {
        ExplodableCategoryData& catData = closest->GetCategoryData();
        if (nlStrNCmp<char>(catData.mBaseModelName, floatingCamName, 4) == 0)
        {
            closest->mpAssociatedEffect = pEmissionController;
        }
    }
}

/**
 * Offset/Address/Size: 0x70 | 0x801673D0 | size: 0x30
 */
void SidelineExplodableManager::UnAssociateEffectWithNearbyFloatingCamera(EmissionController* pEmissionController)
{
    SidelineExplodableNode* node = sSidelineExplodableList.m_pStart;
    while (node != NULL)
    {
        if (node->mpExplodable->GetAssociatedEffect() == pEmissionController)
        {
            node->mpExplodable->mpAssociatedEffect = 0;
        }
        node = node->next;
    }
}
