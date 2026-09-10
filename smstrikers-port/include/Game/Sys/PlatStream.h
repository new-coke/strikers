#ifndef _PLATSTREAM_H_
#define _PLATSTREAM_H_

namespace PlatAudio
{

void InitStreaming();
void ShutdownStreaming();
void ConfigureStreamBuffers(unsigned long count);
bool IsStreamingInited();
void StopAllStreams();

}; // namespace PlatAudio

// class GCAudioStreaming
// {
// public:
// };

// class nlStaticSortedSlot<GCAudioStreaming
// {
// public:
// };

#endif // _PLATSTREAM_H_
