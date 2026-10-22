#ifndef PORT_AUDIO_WATCHDOG_H
#define PORT_AUDIO_WATCHDOG_H

/* Positions within the existing AudioMix watchdog scope, not accumulated times.
 * The low byte is a phase; bits 8-15 carry the synth subframe; bit 16 means
 * the next operation is a lock acquisition. No voice/sample loop is traced. */
enum PortAudioWatchdogDetail {
    PORT_AUDIO_WD_NONE = 0,
    PORT_AUDIO_WD_AI_CALLBACK = 1,
    PORT_AUDIO_WD_STREAM_CORRECT = 2,
    PORT_AUDIO_WD_SOFTWARE_DSP = 3,
    PORT_AUDIO_WD_AUX = 4,
    PORT_AUDIO_WD_STATE = 5,
    PORT_AUDIO_WD_SEQUENCE = 6,
    PORT_AUDIO_WD_SYNTH = 7,
    PORT_AUDIO_WD_SPATIAL = 8,
    PORT_AUDIO_WD_STREAM = 9,
    PORT_AUDIO_WD_VIRTUAL_SAMPLE = 10,
    PORT_AUDIO_WD_WAIT = 0x10000
};
#define PORT_AUDIO_WD_SUBFRAME(phase, index) ((unsigned int)(phase) | ((unsigned int)(index) << 8))

#ifdef __cplusplus
extern "C" {
#endif

#if defined(STRIKERS_VITA_NATIVE_RUNTIME) || (defined(MUSYX_THREADED_AUDIO) && defined(__GNUC__))
unsigned int PortAudioWatchdogSetDetail(unsigned int detail);
static inline unsigned int PortAudioWatchdogMark(unsigned int detail) {
    return PortAudioWatchdogSetDetail(detail);
}
#else
static inline unsigned int PortAudioWatchdogMark(unsigned int detail) {
    (void)detail;
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif
#endif
