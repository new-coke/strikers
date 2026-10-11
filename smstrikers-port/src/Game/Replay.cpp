#include "Game/Replay.h"
#include "NL/nlConfig.h"
#include "NL/nlLexicalCast.h"
#include <cstdio> // PORT: fprintf

SlotPool<Replay::Frame> Replay::Frame::mSlotPool(0x400, 0x80);

namespace
{
bool renderMemoryLayout = false;
}

/**
 * Offset/Address/Size: 0x7C8 | 0x80214074 | size: 0x188
 */
Replay::Replay(char* memory, int memorySize, int maxFrameSize)
    : mFree(new (Frame::Alloc()) Frame(memory, memorySize, nullptr))
    , mReelIdx(0)
    , mTick(0)
    , mMemorySize(memorySize)
    , mMaxFrameSize(maxFrameSize)
    , mActualMaxFrameSize(0)
    , mMemory(memory) // PORT: for RecoverRing
{
    mFree->mNext = mFree;
    mReels[0].mBegin = mReels[0].mLast = mFree;

    renderMemoryLayout = GetConfigBool(Config::Global(), "draw_replay_bar", false);
}

/**
 * Offset/Address/Size: 0x740 | 0x80213FEC | size: 0x88
 */
Replay::~Replay()
{
    Frame* frame = mFree;

    do
    {
        Frame* next = frame->mNext;
        Frame::mSlotPool.Free(frame);
        frame = next;
    } while (frame != mFree);

    SlotPoolBase::BaseFreeBlocks(&Frame::mSlotPool, sizeof(Frame));
}

/**
 * Offset/Address/Size: 0x714 | 0x80213FC0 | size: 0x2C
 */
Replay::Frame::Frame(char* begin, int size, Frame* next)
{
    mTime = 0.0f;
    mBegin = begin;
    mSize = size;
    mInterval = 0;
    mEvents = 0;
    mReelIdx = -1;
    mNext = next;
}

/**
 * Offset/Address/Size: 0x6AC | 0x80213F58 | size: 0x68
 */
Replay::Frame* Replay::Next(Replay::Frame* frame, int reelIdx) const
{
    Frame* current = frame->mNext;

    while (current != mReels[reelIdx].mBegin)
    {
        if (current->mReelIdx == reelIdx)
        {
            return current;
        }

        if (reelIdx == 0 && current->mReelIdx > 0)
        {
            if (frame->mTime <= current->mTime)
            {
                return current;
            }
        }

        current = current->mNext;
    }

    return nullptr;
}

inline Replay::Frame* GetFrame(const Replay::Reel* reels, int reelIdx)
{
    return reels[reelIdx].mBegin;
}

/**
 * Offset/Address/Size: 0x618 | 0x80213EC4 | size: 0x94
 */
float Replay::TimeOfLastOccurence(unsigned int events) const
{
    Frame* frame = GetFrame(mReels, mReelIdx);
    // PORT: a reel can be empty.
    if (frame == nullptr)
    {
        return 0.0f;
    }
    float time = frame->mTime;

    while (frame != nullptr)
    {
        if ((frame->mEvents & events) != 0)
        {
            time = frame->mTime;
        }
        frame = Next(frame, mReelIdx);
    }

    return time;
}

static inline void ReleaseReel(Replay* replay, int idx);

namespace
{
// PORT: NewFrame's walk limits, many laps of a ring that holds a few thousand frames.
const int kNoRoomSteps = 200000;
const int kBrokenWalk = 1000000;
}

// PORT: releases the lowest-quality highlight reel, the older of two equals; false when none is held.
bool Replay::DropWeakestHighlight()
{
    int weakest = 0;
    for (int i = 1; i < 4; i++)
    {
        if (mReels[i].mBegin == nullptr || mReels[i].mLast == nullptr)
            continue;
        if (weakest == 0 || mReels[i].mAge < mReels[weakest].mAge ||
            (mReels[i].mAge == mReels[weakest].mAge &&
             mReels[i].mLast->mTime < mReels[weakest].mLast->mTime))
        {
            weakest = i;
        }
    }
    if (weakest == 0)
    {
        return false;
    }
    std::fprintf(stderr,
                 "[replay] no room for a frame beside the goal highlights; dropping highlight %d "
                 "(quality %d)\n",
                 weakest, mReels[weakest].mAge);
    ReleaseReel(this, weakest);
    mReels[weakest].mAge = 0;
    if (mReelIdx == weakest)
    {
        mReelIdx = 0;
    }
    return true;
}

// PORT: resets the ring to the constructor's single free frame; frames off the cycle stay in the pool.
void Replay::RecoverRing()
{
    int frames = 0, locked = 0, live = 0;
    Frame* f = mFree;
    do
    {
        frames++;
        if (f->mReelIdx > 0)
            locked++;
        else if (f->mReelIdx == 0)
            live++;
        f = f->mNext;
    } while (f != mFree && frames < kBrokenWalk);
    const bool cameBack = f == mFree;
    std::fprintf(stderr,
                 "[replay] the replay ring is broken (%d frames, %d locked, %d live, %s); starting it "
                 "again\n",
                 frames, locked, live, cameBack ? "a closed cycle" : "no closed cycle");
    Frame* keep = mFree;
    if (cameBack)
    {
        for (f = keep->mNext; f != keep;)
        {
            Frame* n = f->mNext;
            Frame::mSlotPool.Free(f);
            f = n;
        }
    }
    keep->mBegin = mMemory;
    keep->mSize = mMemorySize;
    keep->mReelIdx = -1;
    keep->mNext = keep;
    for (int i = 1; i < 4; i++)
    {
        mReels[i].mBegin = nullptr;
        mReels[i].mLast = nullptr;
        mReels[i].mAge = 0;
    }
    mReels[0].mBegin = nullptr; // PORT: Record restarts the live reel at its next frame
    mReels[0].mLast = keep;
    mReelIdx = 0;
}

/**
 * Offset/Address/Size: 0x49C | 0x80213D48 | size: 0x17C
 */
void Replay::NewFrame()
{
    // PORT: bounded, since locked highlight frames can leave every free stretch shorter than mMaxFrameSize.
    int steps = 0;
    while (mFree->mSize < mMaxFrameSize)
    {
        if (steps >= kNoRoomSteps)
        {
            steps = 0;
            if (!DropWeakestHighlight())
            {
                RecoverRing();
                continue;
            }
        }

        Frame* next = mFree->mNext;

        if (next->mReelIdx > 0)
        {
            int walked = 0;
            do
            {
                if (++walked > kBrokenWalk)
                {
                    RecoverRing();
                    break;
                }
                if (mReels[0].mBegin == mFree->mNext)
                {
                    mReels[0].mBegin = Next(mReels[0].mBegin, 0);
                }
                mFree = mFree->mNext;
            } while (mFree->mReelIdx > 0);
            steps += walked;
            mFree->mReelIdx = -1;
        }
        else
        {
            steps++;

            if (mReels[0].mBegin == next)
            {
                mReels[0].mBegin = Next(mReels[0].mBegin, 0);
            }

            if (mFree->End() == mFree->mNext->mBegin)
            {
                Frame* nextFrame = mFree->mNext;
                mFree->mSize += nextFrame->mSize;
                mFree->mNext = nextFrame->mNext;
                mFree->mReelIdx = -1;
                Frame::mSlotPool.Free(nextFrame);
            }
            else
            {
                mFree = mFree->mNext;
                mFree->mReelIdx = -1;
            }
        }
    }
}

/**
 * Offset/Address/Size: 0x480 | 0x80213D2C | size: 0x1C
 */
bool Replay::IsReelValid(int reelIdx) const
{
    return mReels[reelIdx].mBegin != nullptr;
}

/**
 * Offset/Address/Size: 0x3D4 | 0x80213C80 | size: 0xAC
 */
bool Replay::DidOccurInLastNumSeconds(unsigned int events, float seconds) const
{
    float timeThreshold = mReels[mReelIdx].mLast->mTime - seconds;
    Frame* frame = GetFrame(mReels, mReelIdx);

    while (frame != nullptr)
    {
        if (frame->mTime >= timeThreshold)
        {
            if ((frame->mEvents & events) != 0)
            {
                return true;
            }
        }

        frame = Next(frame, mReelIdx);
    }

    return false;
}

static inline void ReleaseReel(Replay* replay, int idx)
{
    Replay::Frame* frame = replay->mReels[0].mBegin;

    while (frame != nullptr)
    {
        if (frame->mReelIdx == idx)
        {
            frame->mReelIdx = 0;
        }

        frame = replay->Next(frame, 0);
    }

    frame = replay->mFree->mNext;

    while (frame != replay->mFree)
    {
        if (frame->mReelIdx == idx)
        {
            frame->mReelIdx = -1;
        }

        frame = frame->mNext;
    }

    replay->mReels[idx].mBegin = nullptr;
    replay->mReels[idx].mLast = nullptr;
}

/**
 * Offset/Address/Size: 0x38 | 0x802138E4 | size: 0x39C
 */
bool Replay::LockReel(float numSeconds, int idx, int quality)
{
    if (mReels[idx].mAge > quality)
    {
        return false;
    }

    Frame* frame;
    Frame* iter;
    Frame* head;
    float beginTime = mReels[mReelIdx].mLast->mTime - numSeconds;
    bool valid;

    head = mFree;
    frame = head->mNext;
    iter = frame;
    for (;;)
    {
        if (iter->mTime >= beginTime)
        {
            if (iter->mReelIdx > 0)
            {
                if (mReels[iter->mReelIdx].mAge > quality)
                {
                    valid = false;
                    break;
                }
            }
        }

        iter = iter->mNext;
        if (iter == head)
        {
            do
            {
                if (frame->mTime >= beginTime)
                {
                    if (frame->mReelIdx > 0)
                    {
                        ReleaseReel(this, frame->mReelIdx);
                    }
                }

                frame = frame->mNext;
            } while (frame != mFree);

            valid = true;
            break;
        }
    }

    if (!valid)
    {
        return false;
    }

    ReleaseReel(this, idx);

    int size = 0;
    beginTime = mReels[mReelIdx].mLast->mTime - numSeconds;
    {
        Frame* frame = mReels[0].mBegin;

        while (frame != nullptr)
        {
            if (frame->mTime >= beginTime)
            {
                size += frame->mSize;
            }

            frame = Next(frame, 0);
        }
    }

    if (size <= (int)(1.2f * (float)(mMemorySize / 4)))
    {
        mReels[idx].mAge = quality;
        {
            Frame* frame = mReels[0].mBegin;

            while (frame != nullptr)
            {
                if (frame->mTime >= beginTime)
                {
                    frame->mReelIdx = idx;
                    if (mReels[idx].mBegin == nullptr)
                    {
                        mReels[idx].mBegin = frame;
                    }
                    mReels[idx].mLast = frame;
                }

                frame = Next(frame, 0);
            }
        }

        return true;
    }

    return false;
}

/**
 * Unreferenced helper, dead-stripped at link. Deferred emission runs
 * bottom-to-top, so defining it below LockReel emits it ahead of LockReel:
 * its 0.0f and its s32-to-f32 conversion magic claim .sdata2 slots 0x0 and
 * 0x8 before LockReel's 1.2f, matching the target pool order
 * [0.0f, magic, 1.2f]. dwarf.txt attributes an erased Replay::RenderLayout()
 * to this TU with float locals x/width/w walking the frame ring;
 * renderMemoryLayout above is the "draw_replay_bar" flag that gated it and is
 * otherwise unread here.
 */

/**
 * Offset/Address/Size: 0x20 | 0x802138CC | size: 0x18
 */
float Replay::BeginTime() const
{
    // PORT: a reel can be empty.
    if (mReels[mReelIdx].mBegin == nullptr)
    {
        return 0.0f;
    }
    return mReels[mReelIdx].mBegin->mTime;
}

/**
 * Offset/Address/Size: 0x8 | 0x802138B4 | size: 0x18
 */
float Replay::EndTime() const
{
    // PORT: a reel can be empty.
    if (mReels[mReelIdx].mLast == nullptr)
    {
        return 0.0f;
    }
    return mReels[mReelIdx].mLast->mTime;
}

/**
 * Offset/Address/Size: 0x0 | 0x802138AC | size: 0x8
 */
void Replay::PlayReel(int reelIdx)
{
    mReelIdx = reelIdx;
}
