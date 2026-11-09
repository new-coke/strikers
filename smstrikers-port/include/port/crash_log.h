#ifndef PORT_CRASH_LOG_H
#define PORT_CRASH_LOG_H

#ifdef __cplusplus
extern "C"
{
#endif

// The file is strikers-crash-log-<date>_<time>.txt beside strikers.ini, whatever 'log' says.

// Writes one line; returns 0 once it is on storage, -1 on failure or past the session's cap.
int PortCrashLog(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // PORT_CRASH_LOG_H
