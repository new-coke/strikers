#ifndef _FEAUDIO_H_
#define _FEAUDIO_H_

class Event;
void FEAudioEventHandler(Event* pEvent, void* userData);

struct AnimAudioEventLookup
{
    /* 0x00 */ unsigned long eventNameHash;
    /* 0x04 */ char szSFXType[50];

    operator unsigned long() const { return eventNameHash; }
}; // total size: 0x38

extern AnimAudioEventLookup* gp_AnimAudioEventTable;
extern unsigned long gNumAnimAudioEvents;

class FEAudio
{
public:
    static void EnableSounds(bool enable);
    static void ResetRandomVoiceToggleSFX();
    static void PlayRandomVoiceToggleSFX();
    static long PlayAnimAudioEvent(unsigned long incomingHash, bool value);
    static void StopAnimAudioEvent(const char* incomingstring);
    static long PlayAnimAudioEvent(const char* incomingstring, bool value);
    static void BuildAnimAudioEventLookup();
};

#endif // _FEAUDIO_H_
