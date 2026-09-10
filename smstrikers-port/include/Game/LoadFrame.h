#ifndef _LOADFRAME_H_
#define _LOADFRAME_H_

class LoadFrame
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
    /* 0x4 */ ReadByteStream mStream;
    /* 0xC */ ReplayNonBlendables mReplayNonBlendables;
    /* 0x10 */ float mNonBlendableAheadOfFrame;
}; // total size: 0x14

template <int N, typename T>
inline void LoadFrame::Replayable(T& current)
{
    typename ReplayableCategory<T>::Type category = ReplayableCategoryOf(current);
    Replayable<N>(current, category);
}

template <int N, typename T>
inline void LoadFrame::Replayable(T& current, ReplayablePod)
{
    if (N == 0 || mInterval == N)
    {
        memcpy(&current, mStream.mStorage, sizeof(T));
        mStream.mStorage += sizeof(T);
    }
}

template <int N>
inline void LoadFrame::Replayable(bool& current, ReplayablePod)
{
    char value = 0;
    memcpy(&value, mStream.mStorage, 1);
    mStream.mStorage += 1;
    current = (value != 0);
}

template <int N, typename T>
inline void LoadFrame::Replayable(T& current, NotReplayablePod)
{
    if (N == 0 || mInterval == N)
    {
        current.Replay(*this);
    }
}

template <int N, typename T>
void LoadFrame::ReplayablePolymorphicPtr(T*& current)
{
    if (N == 0 || mInterval == N)
    {
        unsigned char notNull = 1;
        memcpy(&notNull, mStream.mStorage, 1);
        mStream.mStorage++;

        if (notNull)
        {
            char typeId = 0;
            memcpy(&typeId, mStream.mStorage, 1);
            mStream.mStorage++;

            if (typeId < 0 || typeId > 4)
                nlBreak();

            ::Replayable<N>(*this, typeId, current);
        }
        else
        {
            current = 0;
        }
    }
}

#endif // _LOADFRAME_H_
