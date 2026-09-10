
#ifndef PORT_AUDIO_H
#define PORT_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

// Open the device. Called from salStartAi(), i.e. from inside sndInit(), so that a build with audio
// switched off never touches SDL's audio subsystem.
int PortAudioStart(void);

// Close it. Called from salExitAi(), before MusyX frees the ring this reads.
void PortAudioStop(void);

// Render and queue whatever the device has consumed since the last call.
void PortAudioUpdate(void);

// For the overlay and the exit report: buffers rendered, and whether anything non-zero has ever
// come out of the mixer.
void PortAudioStats(unsigned long* outBuffers, unsigned long* outUnderruns,
                    int* outEverNonSilent);
int PortAudioDeviceOpen(void);

// What PortAudioUpdate has cost the frame: calls, mean and worst milliseconds, and how many
// exceeded two.
void PortAudioUpdateCost(unsigned long* calls, double* meanMs, double* maxMs,
                         unsigned long* over2ms);

// The mixer's state, as STRIKERS_LOG_AUDIO prints it every two seconds: the six numbers that
// between them name every separate cause of silence.
typedef struct PortAudioMixInfo
{
    int studios;              // studios in state 1
    int voices;               // voices on them
    int withSample;           // voices whose sample resolves to memory
    unsigned int peakEnv;     // envelope, 16.16 >> 16
    unsigned int peakVol;     // pan volume
    int busPeak;              // |sample| on the left output, last tick
    unsigned int mixFrq;
    int dumping;
    unsigned long dumpFrames;

    // The master stage, which is the port's and not the console's.
    int rawPeak;
    float limitGain;
    float masterGain;
} PortAudioMixInfo;

// Returns 0 when the mixer has never run (audio off).
int PortAudioMixStats(PortAudioMixInfo* out);

// Start and stop the WAV dump STRIKERS_AUDIO_DUMP would have started at boot.
int  PortAudioDumpStart(const char* path);
void PortAudioDumpStop(void);
void PortAudioDumpWrite(const void* pcm, unsigned int frames);

#ifdef __cplusplus
}
#endif

#endif // PORT_AUDIO_H
