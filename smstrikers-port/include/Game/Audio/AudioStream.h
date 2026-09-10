#ifndef _AUDIOSTREAM_H_
#define _AUDIOSTREAM_H_

#include "Game/Sys/GCStream.h"
#include "Game/Audio/AudioStreamAPI.h"
#include "Game/Audio/AudioStreamVirtuals.h"
#include "Game/Audio/PriorityStream.h"

namespace Audio
{

/**
 * Offset/Address/Size: 0x0 | 0x80141518 | size: 0x1AC
 */
template <int N>
void CreateTrackMgr()
{
    g_pTrackManager = new (8, false) AudioStreamTrack::TrackManager<N>(TrackMgrFileNameParamLookup);
}

}; // namespace Audio

// class Function0<void>
// {
// public:
// };

// class PriorityStream
// {
// public:
// };

// class Bind<void, Detail
// {
// public:
// };

// class MemFun<PriorityStream, void>(void (PriorityStream
// {
// public:
// };

#endif // _AUDIOSTREAM_H_
