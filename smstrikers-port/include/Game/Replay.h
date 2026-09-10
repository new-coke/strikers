#ifndef _REPLAY_H_
#define _REPLAY_H_

#include <string.h>
#include "NL/nlMath.h"
#include "NL/nlSlotPool.h"

class DrawableCharacter;
class DrawableBall;
class DrawableExplosionFragment;
class DrawablePowerup;
class CrowdManager;
class EmissionManager;
class DrawableNetMesh;
class RenderSnapshot;
class cPoseNode;
class LoadFrame;

template <typename T>
struct ReplayFrameTraits
{
    enum
    {
        IsLoadFrame = false
    };
};

template <>
struct ReplayFrameTraits<LoadFrame>
{
    enum
    {
        IsLoadFrame = true
    };
};

void nlBreak();

class WriteByteStream
{
public:
    /* 0x0 */ char mCount;
    /* 0x4 */ char* mStorage;
}; // total size: 0x8

class ReadByteStream
{
public:
    /* 0x0 */ char mCount;
    /* 0x4 */ const char* mStorage;
}; // total size: 0x8

enum ReplayNonBlendables
{
    REPLAY_NON_BLENDABLES = 0,
    DO_NOT_REPLAY_NON_BLENDABLES = 1,
};

struct ReplayablePod
{
}; // total size: 0x1

struct NotReplayablePod
{
}; // total size: 0x1

template <typename T>
struct ReplayableCategory
{
    typedef NotReplayablePod Type;
};

template <>
struct ReplayableCategory<bool>
{
    typedef ReplayablePod Type;
};

template <>
struct ReplayableCategory<char>
{
    typedef ReplayablePod Type;
};

template <>
struct ReplayableCategory<unsigned char>
{
    typedef ReplayablePod Type;
};

template <>
struct ReplayableCategory<unsigned short>
{
    typedef ReplayablePod Type;
};

template <>
struct ReplayableCategory<int>
{
    typedef ReplayablePod Type;
};

template <>
struct ReplayableCategory<unsigned int>
{
    typedef ReplayablePod Type;
};

template <>
struct ReplayableCategory<unsigned long>
{
    typedef ReplayablePod Type;
};

// PORT: uintptr_t is unsigned long on LP64 and unsigned long long on Windows.
template <>
struct ReplayableCategory<unsigned long long>
{
    typedef ReplayablePod Type;
};

template <>
struct ReplayableCategory<float>
{
    typedef ReplayablePod Type;
};

template <>
struct ReplayableCategory<nlVector3>
{
    typedef ReplayablePod Type;
};

template <>
struct ReplayableCategory<nlQuaternion>
{
    typedef ReplayablePod Type;
};

template <typename T>
inline typename ReplayableCategory<T>::Type ReplayableCategoryOf(const T& current)
{
    typename ReplayableCategory<T>::Type category;
    return category;
}

class SaveFrame
{
public:
    template <int N, typename T>
    void Replayable(T& current);

    template <int N, typename T>
    void Replayable(T& current, ReplayablePod);

    template <int N>
    void Replayable(bool& current, ReplayablePod);

    template <int N, typename T>
    void Replayable(T& current, NotReplayablePod);

    template <int N, typename T>
    void ReplayablePolymorphicPtr(T*& ptr);

    /* 0x0 */ int mInterval;
    /* 0x4 */ WriteByteStream mStream;
}; // total size: 0xC

template <int N, typename T>
inline void SaveFrame::Replayable(T& current)
{
    typename ReplayableCategory<T>::Type category = ReplayableCategoryOf(current);
    Replayable<N>(current, category);
}

template <int N, typename T>
inline void SaveFrame::Replayable(T& current, ReplayablePod)
{
    if (N == 0 || mInterval == N)
    {
        memcpy(mStream.mStorage, &current, sizeof(T));
        mStream.mStorage += sizeof(T);
    }
}

template <int N>
inline void SaveFrame::Replayable(bool& current, ReplayablePod)
{
    char value = current ? 1 : 0;
    memcpy(mStream.mStorage, &value, 1);
    mStream.mStorage += 1;
}

template <int N, typename T>
inline void SaveFrame::Replayable(T& current, NotReplayablePod)
{
    if (N == 0 || mInterval == N)
    {
        current.Replay(*this);
    }
}

template <int N, typename FrameType, typename T>
void Replayable(FrameType& frame, T& current);

#include "Game/LoadFrame.h"

template <int N, typename T>
inline void SaveFrame::ReplayablePolymorphicPtr(T*& ptr)
{
    T* current = ptr;
    if (N == 0 || mInterval == N)
    {
        unsigned char notNull = (current != 0);
        memcpy(mStream.mStorage, &notNull, 1);
        mStream.mStorage++;

        if (notNull)
        {
            char typeId = (char)current->GetType();
            if (typeId < 0 || typeId > 4)
                nlBreak();

            memcpy(mStream.mStorage, &typeId, 1);
            mStream.mStorage++;

            ::Replayable<N>(*this, typeId, current);
        }
    }
}

#include "Game/Compressor.h"

template <int N, typename FrameType, typename T>
void ReplayablePolymorphic(FrameType& frame, T*& ptr)
{
    frame.template ReplayablePolymorphicPtr<N>(ptr);
}

// Proxy generic for FloatCompressor-style types (Read/Transfer/Apply).
template <int N, typename FrameType, typename T>
void Replayable(FrameType& frame, const T& proxy)
{
    if (N == 0 || frame.mInterval == N)
    {
        if (N == 0)
        {
            proxy.Replay(frame);
        }
        else
        {
            proxy.template ReplayInterval<N>(frame);
        }
    }
}

template <int N, typename FrameType, typename T>
void Replayable(FrameType& frame, T& current)
{
    if (N == 0 || frame.mInterval == N)
    {
        frame.template Replayable<N>(current);
    }
}

enum ReplayType
{
    REPLAY_TYPE_GOAL = 6,
    REPLAY_TYPE_HIGHLIGHT = 7,
    REPLAY_TYPE_HYPER_STRIKE = 8,
    NUM_REPLAY_TYPES = 9,
};

class Replay
{
public:
    struct Frame
    {
        Frame(char* begin, int size, Frame* next);

        static Frame* Alloc() { return mSlotPool.Allocate(); }

        char* End() const { return mBegin + mSize; }

        /* 0x00 */ float mTime;
        /* 0x04 */ char* mBegin;
        /* 0x08 */ int mSize;
        /* 0x0C */ int mInterval;
        /* 0x10 */ unsigned int mEvents;
        /* 0x14 */ int mReelIdx;
        /* 0x18 */ Frame* mNext;
        static SlotPool<Frame> mSlotPool;
    }; // total size: 0x1C

    struct Reel
    {
        Reel()
        {
            mBegin = nullptr;
            mLast = nullptr;
            mAge = 0;
        };

        /* 0x0 */ Frame* mBegin;
        /* 0x4 */ Frame* mLast;
        /* 0x8 */ int mAge;
        // Do not "fix" this layout from dwarf.txt. The debug ELF's ReplayManager.cpp
        // compile unit describes a 0x10 Reel (float mAge, int mQuality, mBegin, mLast),
        // but the retail build indexes mReels with `mulli rX,rY,12`, so the shipped
        // stride is 0xC and this ordering is what reproduces LockReel exactly.
    }; // total size: 0xC

    Replay(char* memory, int memorySize, int maxFrameSize);
    ~Replay();

    Frame* Next(Frame* frame, int reelIdx) const;
    float TimeOfLastOccurence(unsigned int events) const;
    void NewFrame();
    bool IsReelValid(int reelIdx) const;
    bool DidOccurInLastNumSeconds(unsigned int events, float seconds) const;
    bool LockReel(float numSeconds, int idx, int quality);
    float BeginTime() const;
    float EndTime() const;
    void PlayReel(int reelIdx);

    template <typename T>
    void Play(float time, T& previous, T& current, float* blend) const;

    template <typename T>
    void Record(float time, T& snapshot, unsigned int events);

    /* 0x00 */ Frame* mFree;
    /* 0x04 */ Reel mReels[4];
    /* 0x34 */ int mReelIdx;
    /* 0x38 */ int mTick;
    /* 0x3C */ int mMemorySize;
    /* 0x40 */ int mMaxFrameSize;
    /* 0x44 */ int mActualMaxFrameSize;
    /* 0x48 */ Reel* mHighlights[3];

}; // total size: 0x54

/**
 * Offset/Address/Size: 0x3C | 0x8011294C | size: 0x168
 */
template <typename T>
void Replay::Record(float time, T& snapshot, unsigned int events)
{
    for (int interval = 1; interval <= 3; interval++)
    {
        if (mTick % interval == 0)
        {
            NewFrame();

            SaveFrame frame;
            char* storage = mFree->mBegin;
            frame.mInterval = interval;
            frame.mStream.mCount = 0;
            frame.mStream.mStorage = storage;
            Replayable<0>(frame, snapshot);

            int frameSize = frame.mStream.mStorage - mFree->mBegin;
            if (mActualMaxFrameSize < frameSize)
            {
                mActualMaxFrameSize = frameSize;
            }

            mReels[0].mLast = mFree;
            mFree->mReelIdx = 0;
            mFree->mTime = time;
            mFree->mInterval = interval;
            mFree->mEvents = events;

            mFree->mNext = new (Frame::Alloc()) Frame(mFree->mBegin + frameSize, mFree->mSize - frameSize, mFree->mNext);
            mFree->mSize = frameSize;
            mFree = mFree->mNext;
        }
    }

    mTick++;
}

/**
 * Offset/Address/Size: 0x1A4 | 0x80112AB4 | size: 0x1B8
 */
template <typename T>
void Replay::Play(float time, T& previous, T& current, float* blend) const
{
    if (time < BeginTime())
    {
        time = BeginTime();
    }
    if (time > EndTime())
    {
        time = EndTime();
    }

    int interval;
    Frame* rhs;
    Frame* lhs;
    Frame* tryLhs;
    LoadFrame previousLoadFrame;
    LoadFrame currentLoadFrame;

    for (interval = 1; interval <= 3; interval++)
    {
        rhs = mReels[mReelIdx].mBegin;
        while (rhs != NULL)
        {
            if (rhs->mInterval == interval && rhs->mTime > time)
            {
                lhs = NULL;
                tryLhs = mReels[mReelIdx].mBegin;
                while (tryLhs != NULL)
                {
                    if (tryLhs->mInterval == interval && tryLhs->mTime <= time)
                    {
                        lhs = tryLhs;
                    }
                    tryLhs = Next(tryLhs, mReelIdx);
                }

                if (lhs != NULL && rhs != NULL)
                {
                    blend[interval - 1] = (time - lhs->mTime) / (rhs->mTime - lhs->mTime);

                    float aheadOfFrame = time - lhs->mTime;
                    char* lhsBegin = lhs->mBegin;
                    previousLoadFrame.mInterval = interval;
                    previousLoadFrame.mStream.mCount = 0;
                    previousLoadFrame.mStream.mStorage = lhsBegin;
                    previousLoadFrame.mReplayNonBlendables = REPLAY_NON_BLENDABLES;
                    previousLoadFrame.mNonBlendableAheadOfFrame = aheadOfFrame;
                    Replayable<0>(previousLoadFrame, previous);

                    char* rhsBegin = rhs->mBegin;
                    currentLoadFrame.mInterval = interval;
                    currentLoadFrame.mStream.mCount = 0;
                    currentLoadFrame.mStream.mStorage = rhsBegin;
                    currentLoadFrame.mReplayNonBlendables = DO_NOT_REPLAY_NON_BLENDABLES;
                    currentLoadFrame.mNonBlendableAheadOfFrame = 0.0f;
                    Replayable<0>(currentLoadFrame, current);

                    break;
                }
            }
            rhs = Next(rhs, mReelIdx);
        }
    }
}

template <typename T>
void Blend(const float* blendFactors, const T& lhs, const T& rhs, T& result)
{
    result.Blend(blendFactors, lhs, rhs);
}

// Moved here from the top of the file so that this header's own types and templates are complete before EmissionController.h is parsed.
#include "Game/Effects/EmissionController.h"

#endif // _REPLAY_H_
