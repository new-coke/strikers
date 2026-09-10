#include "Game/FE/feMusic.h"
#include "Game/Audio/AudioLoader.h"
#include "Game/Audio/AudioStream.h"
#include "NL/nlString.h"

static const char* streamnames[] = {
    "FE_Main_Options",
    "FE_Main_Options",
    "FE_Cup_Battle",
    "FE_Cup_Super",
    "FE_Custom",
    "FE_101",
    "FE_Spoils",
    "FE_Main_Options",
};

extern AudioStreamTrack::TrackManagerBase* g_pTrackManager;

u32 FEMusic::mCurrentFEStreamHash;

/**
 * Offset/Address/Size: 0x0 | 0x80111C98 | size: 0xC
 */
void FEMusic::ResetCurrentFEStreamHash()
{
    mCurrentFEStreamHash = 0;
}

/**
 * Offset/Address/Size: 0xC | 0x80111CA4 | size: 0x60
 */
void FEMusic::StopStream()
{
    AudioStreamTrack::TrackManagerBase* mgr = g_pTrackManager;
    // PORT: null when audio is disabled, there is no manager to ask.
    AudioStreamTrack::StreamTrack* track =
        mgr != NULL ? mgr->GetTrack(nlStringLowerHash("FE")) : NULL;
    if (track)
    {
        track->Stop(0);
    }
    mCurrentFEStreamHash = 0;
}

/**
 * Offset/Address/Size: 0x6C | 0x80111D04 | size: 0x6C
 */
void FEMusic::StartStreamIfDifferent(int idx)
{
    u32 hash = nlStringLowerHash(streamnames[idx]);
    if (mCurrentFEStreamHash != hash)
    {
        AudioLoader::StartFEStream(streamnames[idx], true, "FE");
        mCurrentFEStreamHash = hash;
    }
}
