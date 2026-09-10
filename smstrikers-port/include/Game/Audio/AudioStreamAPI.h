#ifndef _AUDIOSTREAMAPI_H_
#define _AUDIOSTREAMAPI_H_

class PriorityStream;

namespace Audio
{

void InitStreaming();
void StopStreaming();
bool TrackMgrFileNameParamLookup(const char* param, char* out, unsigned long size);
void DestroyTrackMgr();
void CreatePriorityStreams();
void DestroyPriorityStreams();
PriorityStream* GetPriorityStream();
void ConfigureStreamBuffers(unsigned long count);

} // namespace Audio

#endif // _AUDIOSTREAMAPI_H_
