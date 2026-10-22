// Built by tools/test_pose_snapshot.py with complete selected production bodies.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>
#include <vector>

namespace probe {
std::size_t checks, allocationCalls, allocationBytes, frees, live, copyCalls, copyBytes;
bool countCopies;
const char* phase="initialization";
struct Allocation { void* pointer; std::size_t bytes; } allocations[4096];
[[noreturn]] void fail(const char* expression, int line) {
    std::fprintf(stderr,"pose snapshot check failed in %s at line %d: %s\n",phase,line,expression);
    std::abort();
}
void check(bool result, const char* expression, int line) {
    ++checks;
    if (!result) fail(expression,line);
}
void* allocate(std::size_t size) {
    void* p=std::malloc(size ? size : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
bool untrack(void* p) {
    for (auto& a:allocations) if(a.pointer==p && p) {
        a.pointer=nullptr; ++frees; --live; return true;
    }
    return false;
}
void* copy(void* destination,const void* source,std::size_t bytes) {
    if (countCopies) { ++copyCalls; copyBytes+=bytes; }
    return std::memcpy(destination,source,bytes);
}
}
#define CHECK(x) probe::check(bool(x),#x,__LINE__)

// nlMalloc-owned objects are destroyed by the real Drawable destructor. Pair all
// allocations with malloc/free here so ASan checks the actual ownership path.
void* operator new(std::size_t size) { return probe::allocate(size); }
void* operator new[](std::size_t size) { return probe::allocate(size); }
void operator delete(void* p) noexcept { probe::untrack(p); std::free(p); }
void operator delete[](void* p) noexcept { probe::untrack(p); std::free(p); }
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete[](p); }
void* nlMalloc(std::size_t size,unsigned int,bool) {
    void* p=probe::allocate(size);
    for(auto& a:probe::allocations) if(!a.pointer) {
        a={p,size}; ++probe::allocationCalls; probe::allocationBytes+=size; ++probe::live; return p;
    }
    probe::fail("allocation fixture capacity",__LINE__);
}
extern "C" void nlFree(void* p) {
    if (!p) return;
    CHECK(probe::untrack(p));
    std::free(p);
}

// Only Replay transport is replaced. The real cPoseNode/PoseAccumulator types
// and the entire DrawableCharacter::Replay function are compiled unchanged.
#define _REPLAY_H_
template<int,class Frame,class Value> void Replayable(Frame&,Value&&);
template<int,class Frame,class Value> void ReplayablePolymorphic(Frame&,Value&);

// Transparent probe around the production Vector memcpy; every copied byte is
// still written by libc memcpy. No pose/copy decision is replaced by the probe.
#define memcpy probe::copy
#include "Game/PoseAccumulator.h"
#undef memcpy
static_assert(sizeof(u32)==4 && sizeof(s32)==4,"Host fixture requires the port's fixed-width TARGET_PC types");
static_assert(sizeof(nlVector3)==12 && sizeof(nlQuaternion)==16 && sizeof(nlMatrix4)==64);
static_assert(sizeof(RotAccum)==32 && sizeof(ScaleAccum)==20 && sizeof(TransAccum)==20);

#ifndef nlAssert
#define nlAssert(x) CHECK(x)
#endif
#ifndef ASSERTMSGLINE
#define ASSERTMSGLINE(line,condition,message) CHECK(condition)
#endif

struct EffectsTexturing { int identity=17; } defaultEffects;
enum { eFXTex_Nothing=0, GOALIE=1, FIELDER=2, SkinAnimatedNPC_BOWSER=3, SkinAnimatedNPC_OTHER=4 };
EffectsTexturing* fxGetTexturing(int value) { CHECK(value==eFXTex_Nothing); return &defaultEffects; }
struct HeadTrackFixture { float m_fHeadSpin=231.0f,m_fHeadTilt=127.0f; };
class cPN_SAnimController;
struct cCharacter {
    nlVector3 m_v3Position{},m_v3Velocity{};
    unsigned short m_aActualFacingDirection=217;
    int m_nBip01JointIndex_0xA4=0,m_nHeadJointIndex=0,m_eClassType=FIELDER;
    float m_Dirt=0.25f;
    HeadTrackFixture* m_pHeadTrack=nullptr;
    cPoseNode* m_pPoseTree=nullptr;
    cPoseAccumulator* m_pPoseAccumulator=nullptr;
    EffectsTexturing* m_pEffectsTexturing=nullptr;
    nlMatrix4 m_m4WorldMatrix{};
    nlVector3 GetJointPosition(int i) const { return m_pPoseAccumulator->GetNodeMatrix(i).GetTranslation(); }
};
struct BallFixture { nlVector3 m_v3Position{}; } ball;
BallFixture* g_pBall=&ball;
class SkinAnimatedNPC {
public:
    int type=SkinAnimatedNPC_OTHER;
    int GetSkinAnimatedNPC_Type() const { return type; }
};
class SkinAnimatedMovableNPC:public SkinAnimatedNPC {
public:
    nlVector3 mv3Position{};
    unsigned short maFacingDirection=913;
    bool mbIsVisible=true;
    cPoseAccumulator* mpPoseAccumulator=nullptr;
    cPoseNode* mpPoseTree=nullptr;
    float GetHeadSpin() const { return 137.0f; }
    float GetHeadTilt() const { return 59.0f; }
};
class Bowser:public SkinAnimatedMovableNPC {
public:
    int mnHeadJointIndex=0;
    nlMatrix4 mLastHeadMatrix{};
};

#include "pose_snapshot_declarations.inc"

template<int Low,int High,int Bits> struct FloatCompressor {
    float& value;
    explicit FloatCompressor(float& v):value(v){}
};
struct SaveFrame {
    static constexpr bool loading=false;
    int mInterval=1;
    std::vector<unsigned char> bytes;
    template<class T> void transfer(T& value) {
        const auto* p=reinterpret_cast<const unsigned char*>(&value);
        bytes.insert(bytes.end(),p,p+sizeof(T));
    }
};
struct LoadFrame {
    static constexpr bool loading=true;
    int mInterval=1;
    const std::vector<unsigned char>& bytes;
    std::size_t cursor=0;
    explicit LoadFrame(const std::vector<unsigned char>& b):bytes(b){}
    template<class T> void transfer(T& value) {
        CHECK(cursor+sizeof(T)<=bytes.size());
        std::memcpy(&value,bytes.data()+cursor,sizeof(T)); cursor+=sizeof(T);
    }
};
template<class Frame> struct ReplayFrameTraits { static constexpr bool IsLoadFrame=Frame::loading; };
template<class T> T& unwrap(T& v) { return v; }
template<int L,int H,int B> float& unwrap(FloatCompressor<L,H,B>& v) { return v.value; }
template<int Interval,class Frame,class Value> void Replayable(Frame& frame,Value&& value) {
    if (frame.mInterval==Interval) frame.transfer(unwrap(value));
}

#include "pose_snapshot_foundation.inc"

std::size_t evaluations,treeDestructions;
struct TestPose:public cPoseNode {
    unsigned seed;
    explicit TestPose(unsigned s):seed(s){}
    ~TestPose() override { ++treeDestructions; }
    void Evaluate(int,float weight,cPoseAccumulator* out) const override { Evaluate(weight,out); }
    void Evaluate(float weight,cPoseAccumulator* out) const override {
        ++evaluations;
        for(int i=0;i<out->GetNumNodes();++i) {
            const unsigned key=seed+unsigned(i)*13;
            const unsigned short angle=static_cast<unsigned short>(31+(key%411));
            if(key%4==0) out->BlendRotIdentity(i,weight);
            else if(key%4==1) out->BlendRotAroundZ(i,angle,weight);
            else {
                float s,c; nlSinCos(&s,&c,angle/2);
                nlQuaternion q={0.0f,0.0f,s,c};
                out->BlendRot(i,&q,weight,(key%7)==0);
                if(key%4==3) out->BlendRotAroundZ(i,angle/3,weight*0.25f);
            }
            if(key%3==0) out->BlendScaleIdentity(i,weight);
            else {
                nlVector3 scale={0.9f+float(key%9)*0.02f,1.1f,0.95f};
                out->BlendScale(i,&scale,weight,false);
            }
            if(key%6==0) out->BlendTransIdentity(i,weight);
            else {
                nlVector3 trans={float(key%11)*0.03125f,float(i)*0.0625f,float(key%5)*0.125f};
                out->BlendTrans(i,&trans,weight,(key%5)==0);
            }
        }
        for(int i=0;i<8;++i) out->m_MorphWeights.mData[i]=float((seed+i)%13)*0.0625f;
    }
    cPoseNode* Update(float) override { return this; }
    int GetType() override { return 1001; }
    void BlendRootTrans(nlVector3*,float,float*) override {}
    void BlendRootRot(unsigned short*,float,float*) override {}
};
template<int Interval,class Frame,class Value> void ReplayablePolymorphic(Frame& frame,Value& value) {
    if(frame.mInterval!=Interval) return;
    unsigned seed=0;
    if constexpr(!Frame::loading) {
        auto* pose=dynamic_cast<TestPose*>(value); CHECK(pose); seed=pose->seed;
    }
    frame.transfer(seed);
    if constexpr(Frame::loading) value=new TestPose(seed);
}

#include "pose_snapshot_implementation.inc"
class ReferenceDrawableCharacter:public DrawableCharacter {
public:
    void Grab(cCharacter&);
    void Grab(SkinAnimatedMovableNPC&);
};
#include "pose_snapshot_reference.inc"

struct Hierarchy {
    cSHierarchy value{};
    std::array<u32,64> ids{};
    std::array<int,64> parents{},counts{},pushPop{},mirror{};
    int childStorage[64][64]{};
    std::array<int*,64> children{};
    std::array<nlVector3,64> translation{};
    std::array<u8,64> preserve{};
    int next=0;
    int tree(int count,int parent) {
        const int node=next++; parents[node]=parent;
        const int left=(count-1)/2, right=count-1-left;
        if(left) childStorage[node][counts[node]++]=tree(left,node);
        if(right) childStorage[node][counts[node]++]=tree(right,node);
        return node;
    }
    explicit Hierarchy(int count,bool branches=true) {
        CHECK(count>=0 && count<=64);
        value.m_nNumNodes=count; value.m_nPelvisNodeIndex=count/3; value.m_nSpineNodeIndex=count/2;
        value.m_pNodeID=ids.data(); value.m_pParent=parents.data(); value.m_pNumChildren=counts.data();
        value.m_pChildren=children.data(); value.m_pPushPop=pushPop.data(); value.m_pMirrorTable=mirror.data();
        value.m_pV3TranslationOffset=translation.data(); value.m_pPreserveBoneLength=preserve.data();
        for(int i=0;i<count;++i) {
            ids[i]=100u+i; children[i]=childStorage[i]; mirror[i]=count-1-i;
            preserve[i]=(i%5)==0; translation[i]={0.0625f*i,0.125f,0.5f};
        }
        if(count) {
            if(branches) tree(count,-1);
            else {
                parents[0]=-1;
                for(int i=1;i<count;++i) { parents[i]=0; childStorage[0][counts[0]++]=i; }
            }
            int depth=0; value.BuildPushPopFlags(0,0,depth);
        }
    }
};

template<class T> void equalVector(const Vector<T>& a,const Vector<T>& b) {
    CHECK(a.mSize==b.mSize);
    CHECK(!a.mSize || std::memcmp(a.mData,b.mData,sizeof(T)*a.mSize)==0);
}
void equalPose(const cPoseAccumulator& a,const cPoseAccumulator& b,bool previous) {
    CHECK(a.m_BaseSHierarchy==b.m_BaseSHierarchy);
    equalVector(a.m_NodeMatrices,b.m_NodeMatrices);
    if(previous) equalVector(a.m_PrevNodeMatrices,b.m_PrevNodeMatrices);
    equalVector(a.m_rot,b.m_rot); equalVector(a.m_scale,b.m_scale); equalVector(a.m_trans,b.m_trans);
    equalVector(a.m_cb,b.m_cb); equalVector(a.m_MorphWeights,b.m_MorphWeights);
    for(int i=0;i<a.m_NodeMatrices.mSize;++i)
        for(float x:a.m_NodeMatrices.mData[i].e) CHECK(std::isfinite(x));
}
void equalDrawable(const DrawableCharacter& a,const DrawableCharacter& b,bool previous) {
    equalPose(*a.mPoseAccumulator,*b.mPoseAccumulator,previous);
    CHECK(a.mVisible==b.mVisible && a.mDirt==b.mDirt && a.mFacingDirection==b.mFacingDirection);
    CHECK(a.mHeadSpin==b.mHeadSpin && a.mHeadTilt==b.mHeadTilt && a.mHeight==b.mHeight);
    CHECK(a.mEffectsTexturing==b.mEffectsTexturing && a.mCharacter==b.mCharacter && a.mBowser==b.mBowser);
    if(std::memcmp(&a.mPosition,&b.mPosition,sizeof(nlVector3))!=0)
        std::fprintf(stderr,"positions actual=(%a,%a,%a) reference=(%a,%a,%a)\n",a.mPosition.x,a.mPosition.y,a.mPosition.z,b.mPosition.x,b.mPosition.y,b.mPosition.z);
    CHECK(std::memcmp(&a.mPosition,&b.mPosition,sizeof(nlVector3))==0);
    CHECK(std::memcmp(&a.mVelocity,&b.mVelocity,sizeof(nlVector3))==0);
    CHECK(std::memcmp(&a.mHeadPosition,&b.mHeadPosition,sizeof(nlVector3))==0);
    CHECK(std::memcmp(&a.mBip01Position,&b.mBip01Position,sizeof(nlVector3))==0);
}
void poison(Vector<nlMatrix4>& matrices,unsigned seed) {
    for(int i=0;i<matrices.mSize;++i) for(int j=0;j<16;++j)
        matrices.mData[i].e[j]=float(seed+i*17+j)*0.03125f+8.0f;
}
nlMatrix4 world(unsigned seed) {
    nlMatrix4 result; nlMakeRotationMatrixZ(result,float(seed%71)*0.001f);
    result.m41=float(seed%3);result.m42=2.0f;result.m43=-0.25f;return result;
}
struct HistoryCallback {
    std::array<nlMatrix4,65> expected{};
    std::size_t calls=0;
    int size=0;
    void set(const cPoseAccumulator& source) {
        size=source.m_NodeMatrices.mSize;
        std::memcpy(expected.data(),source.m_NodeMatrices.mData,sizeof(nlMatrix4)*size);
    }
    static void invoke(uintptr_t context,unsigned int,cPoseAccumulator* pose,unsigned int node,int) {
        auto& state=*reinterpret_cast<HistoryCallback*>(context); ++state.calls;
        CHECK(pose->m_PrevNodeMatrices.mSize==state.size);
        CHECK(std::memcmp(pose->m_PrevNodeMatrices.mData,state.expected.data(),sizeof(nlMatrix4)*state.size)==0);
        pose->m_NodeMatrices.mData[node].m41+=0.125f;
    }
};

bool requireSkip;
std::size_t groups,skippedMatrixBytes,comparedFrames;
void direct_copy_and_build(int count,bool branches) {
    probe::phase="direct copy and BuildNodeMatrices";
    const auto entryLive=probe::live;
    {
        Hierarchy hierarchy(count,branches);
        cPoseAccumulator live(&hierarchy.value,true),snapshot(&hierarchy.value,true),reference(&hierarchy.value,true);
        HistoryCallback callback;
        for(unsigned iteration=0;iteration<12;++iteration) {
            TestPose pose(iteration*29+7); live.Pose(pose,world(iteration));
            poison(live.m_PrevNodeMatrices,100+iteration);
            poison(snapshot.m_PrevNodeMatrices,700+iteration);
            reference=snapshot;
            cPoseAccumulator oldSnapshot(snapshot),liveBefore(live);
            callback.set(live);
            live.SetBuildNodeMatrixCallback(count/2,HistoryCallback::invoke,(uintptr_t)&callback,0);
            liveBefore=live;
            const auto beforeAlloc=probe::allocationCalls;
            const auto beforeCopies=probe::copyCalls,beforeBytes=probe::copyBytes;
            probe::countCopies=true; CopySnapshotPose(snapshot,live); probe::countCopies=false;
            reference=live;
            CHECK(probe::allocationCalls==beforeAlloc);
            const std::size_t copies=probe::copyCalls-beforeCopies;
            CHECK(copies==(requireSkip ? 6u : 7u));
            std::size_t full=sizeof(nlMatrix4)*(count+1)*2+
                (sizeof(RotAccum)+sizeof(ScaleAccum)+sizeof(TransAccum)+sizeof(cBuildNodeMatrixCallbackInfo))*count+8*sizeof(float);
            CHECK(probe::copyBytes-beforeBytes==full-(requireSkip ? sizeof(nlMatrix4)*(count+1):0));
            equalPose(snapshot,reference,false);
            if(requireSkip) { equalVector(snapshot.m_PrevNodeMatrices,oldSnapshot.m_PrevNodeMatrices); skippedMatrixBytes+=sizeof(nlMatrix4)*(count+1); }
            CHECK(snapshot.m_NodeMatrices.mData!=live.m_NodeMatrices.mData);
            CHECK(snapshot.m_PrevNodeMatrices.mData!=live.m_PrevNodeMatrices.mData);
            equalPose(live,liveBefore,true);
            auto* oldCurrent=snapshot.m_NodeMatrices.mData;
            auto* oldPrevious=snapshot.m_PrevNodeMatrices.mData;
            snapshot.BuildNodeMatrices(world(iteration+31));
            reference.BuildNodeMatrices(world(iteration+31));
            CHECK(snapshot.m_PrevNodeMatrices.mData==oldCurrent && snapshot.m_NodeMatrices.mData==oldPrevious);
            equalPose(snapshot,reference,true); equalPose(live,liveBefore,true);
            CHECK(callback.calls==2*(iteration+1));
            live.SetBuildNodeMatrixCallback(count/2,nullptr,0,0);
            ++comparedFrames;
        }
        const auto before=probe::copyCalls;
        probe::countCopies=true; CopySnapshotPose(snapshot,snapshot); probe::countCopies=false;
        CHECK(probe::copyCalls==before);
    }
    CHECK(probe::live==entryLive);++groups;
}

void fallback_copies() {
    probe::phase="fallback copies";
    const auto entryLive=probe::live;
    {
        Hierarchy h(11),other(11),small(1),empty(0);
        cPoseAccumulator source(&h.value,true); TestPose pose(33);source.Pose(pose,world(5));poison(source.m_PrevNodeMatrices,213);
        auto check=[&](cPoseAccumulator& target,cPoseAccumulator& input) {
            cPoseAccumulator expected(target); expected=input;
            CopySnapshotPose(target,input); equalPose(target,expected,true);
        };
        cPoseAccumulator target(&other.value,true);check(target,source); // different hierarchy
        cPoseAccumulator shaped(&small.value,true);check(shaped,source); // different node count
        cPoseAccumulator noPrevious(&h.value,false);check(noPrevious,source);
        cPoseAccumulator noHistorySource(&h.value,false); noHistorySource.Pose(pose,world(7));check(target,noHistorySource);
        cPoseAccumulator zero(&empty.value,true),zeroTarget(&empty.value,true);poison(zero.m_PrevNodeMatrices,311);check(zeroTarget,zero);
        cPoseAccumulator wrongSize(source);--wrongSize.m_NodeMatrices.mSize;check(target,wrongSize);
        cPoseAccumulator wrongPrevious(source);--wrongPrevious.m_PrevNodeMatrices.mSize;check(target,wrongPrevious);
        cPoseAccumulator nullHierarchy(source);nullHierarchy.m_BaseSHierarchy=nullptr;check(target,nullHierarchy);
        // The implicit assignment remains full for live accumulators and other users.
        target=source;equalPose(target,source,true);
    }
    CHECK(probe::live==entryLive);++groups;
}

void character_paths(bool goalie) {
    probe::phase=goalie?"far goalie Grab":"character Grab";
    const auto entryLive=probe::live;
    {
        Hierarchy h(goalie?42:38);
        cPoseAccumulator live(&h.value,true);TestPose pose(93);HeadTrackFixture head;
        cCharacter character;character.m_pPoseAccumulator=&live;character.m_pPoseTree=&pose;character.m_pHeadTrack=&head;
        character.m_nHeadJointIndex=h.value.m_nNumNodes/2;character.m_nBip01JointIndex_0xA4=1;
        character.m_v3Position={-3.0f,2.0f,0.25f};character.m_v3Velocity={0.125f,0.5f,0.0f};
        character.m_eClassType=goalie?GOALIE:FIELDER;character.m_m4WorldMatrix=world(45);
        ball.m_v3Position={2.0f,0.0f,0.0f};
        live.Pose(pose,world(0));
        DrawableCharacter snapshot;ReferenceDrawableCharacter reference;
        const auto beforeAlloc=probe::allocationCalls;
        snapshot.Grab(character);CHECK(probe::allocationCalls-beforeAlloc==8);
        reference.Grab(character);equalDrawable(snapshot,reference,true);
        for(unsigned i=0;i<16;++i) {
            pose.seed=171+i; live.Pose(pose,world(i));poison(live.m_PrevNodeMatrices,900+i);
            poison(snapshot.mPoseAccumulator->m_PrevNodeMatrices,400+i);
            poison(reference.mPoseAccumulator->m_PrevNodeMatrices,400+i);
            cPoseAccumulator liveBefore(live);
            const auto allocations=probe::allocationCalls,copies=probe::copyCalls,evaluated=evaluations;
            probe::countCopies=true;snapshot.Grab(character);probe::countCopies=false;
            CHECK(probe::allocationCalls==allocations);
            CHECK(probe::copyCalls-copies==(requireSkip?6u:7u));
            reference.Grab(character);
            CHECK(evaluations-evaluated==(goalie?2u:0u));
            equalDrawable(snapshot,reference,goalie);equalPose(live,liveBefore,true);
            snapshot.BuildNodeMatrices();reference.BuildNodeMatrices();
            equalDrawable(snapshot,reference,true);equalPose(live,liveBefore,true);
            CHECK(snapshot.mPoseAccumulator->m_cb.mData[character.m_nHeadJointIndex].funcCallback==nullptr);
            ++comparedFrames;
        }
        DrawableCharacter assigned;assigned.Grab(character);assigned=snapshot;
        equalPose(*assigned.mPoseAccumulator,*snapshot.mPoseAccumulator,true);
        // Serialization keeps its real body. Fixture transport deliberately
        // avoids asserting unrelated compressor quantization.
        snapshot.Grab(character);reference.Grab(character);
        SaveFrame saved,newReference;snapshot.Replay(saved);reference.Replay(newReference);
        probe::phase="Replay save";
        CHECK(saved.bytes==newReference.bytes);
        const auto deaths=treeDestructions;
        LoadFrame load(saved.bytes),loadReference(newReference.bytes);
        probe::phase="Replay load";
        snapshot.Replay(load);reference.Replay(loadReference);
        CHECK(load.cursor==saved.bytes.size() && loadReference.cursor==newReference.bytes.size());
        CHECK(treeDestructions==deaths+2 && !snapshot.mPoseTree && !reference.mPoseTree);
        equalDrawable(snapshot,reference,true);
        snapshot.Grab(character);reference.Grab(character);
        cPoseAccumulator beforeReplay(*snapshot.mPoseAccumulator);
        LoadFrame skipped(saved.bytes);skipped.mInterval=0;snapshot.Replay(skipped);
        CHECK(skipped.cursor==0);equalPose(*snapshot.mPoseAccumulator,beforeReplay,true);
        // EvaluateFrom is the NIS path; it must still evaluate/build each pose.
        probe::phase="EvaluateFrom";
        snapshot.EvaluateFrom(pose,{0.25f,0.5f,0.75f},1337);
        reference.EvaluateFrom(pose,{0.25f,0.5f,0.75f},1337);
        equalDrawable(snapshot,reference,true);
    }
    CHECK(probe::live==entryLive);++groups;
}

void npc_and_blend() {
    probe::phase="Bowser NPC Grab";
    const auto entryLive=probe::live;
    {
        Hierarchy h(41);cPoseAccumulator live(&h.value,true);TestPose pose(277);live.Pose(pose,world(8));
        Bowser npc;npc.type=SkinAnimatedNPC_BOWSER;npc.mnHeadJointIndex=17;npc.mpPoseAccumulator=&live;npc.mpPoseTree=&pose;
        npc.mv3Position={1.0f,-3.0f,0.5f};
        DrawableCharacter snapshot;ReferenceDrawableCharacter reference;
        snapshot.Grab(npc);reference.Grab(npc);equalDrawable(snapshot,reference,true);
        for(unsigned i=0;i<16;++i) {
            pose.seed=401+i;live.Pose(pose,world(i));poison(live.m_PrevNodeMatrices,800+i);
            poison(snapshot.mPoseAccumulator->m_PrevNodeMatrices,600+i);
            poison(reference.mPoseAccumulator->m_PrevNodeMatrices,600+i);
            const auto allocations=probe::allocationCalls,copies=probe::copyCalls;
            probe::countCopies=true;snapshot.Grab(npc);probe::countCopies=false;
            CHECK(probe::allocationCalls==allocations && probe::copyCalls-copies==(requireSkip?6u:7u));
            reference.Grab(npc);equalDrawable(snapshot,reference,false);
            snapshot.BuildNodeMatrices();const nlMatrix4 head=npc.mLastHeadMatrix;
            reference.BuildNodeMatrices();CHECK(std::memcmp(&head,&npc.mLastHeadMatrix,sizeof(head))==0);
            equalDrawable(snapshot,reference,true);++comparedFrames;
        }
        DrawableCharacter lhs,rhs;ReferenceDrawableCharacter leftReference,rightReference;
        lhs.Grab(npc);leftReference.Grab(npc);pose.seed=922;live.Pose(pose,world(44));
        rhs.Grab(npc);rightReference.Grab(npc);
        DrawableCharacter blended,blendReference;
        // Blend leaves these fields to its caller; make that fixture state
        // explicit instead of comparing uninitialized constructor members.
        blended.mHeight=blendReference.mHeight=0.0f;
        blended.mVelocity=blendReference.mVelocity={0.0f,0.0f,0.0f};
        probe::phase="Blend";
        for(float t:{0.0f,0.25f,0.5f,0.75f,1.0f}) {
            blended.Blend(&t,lhs,rhs);blendReference.Blend(&t,leftReference,rightReference);
            equalDrawable(blended,blendReference,true);
            CHECK(blended.mPoseAccumulator->m_PrevNodeMatrices.mSize==0);
        }
        snapshot.Free();CHECK(!snapshot.mPoseAccumulator);
        snapshot.Grab(npc);equalPose(*snapshot.mPoseAccumulator,live,true);
    }
    CHECK(probe::live==entryLive);++groups;
}

int main(int argc,char** argv) {
    requireSkip=argc==2 && std::strcmp(argv[1],"--require-skip")==0;
    for(int nodes:{1,11,38,39,40,41,42}) {
        direct_copy_and_build(nodes,false);direct_copy_and_build(nodes,true);
    }
    fallback_copies();character_paths(false);character_paths(true);npc_and_blend();
    CHECK(probe::live==0 && probe::allocationCalls==probe::frees);
    std::printf("pose_snapshot: PASS groups=%zu checks=%zu compared_frames=%zu allocations=%zu frees=%zu skipped_previous_bytes=%zu mode=%s\n",
        groups,probe::checks,comparedFrames,probe::allocationCalls,probe::frees,skippedMatrixBytes,requireSkip?"skip-required":"full-copy-baseline");
}
