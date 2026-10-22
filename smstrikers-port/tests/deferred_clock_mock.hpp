// Host-only symbol substitution. The production translation unit calls these
// adapters so tests can inject operating-system clock/wait failures and count
// OFF-path clock queries without adding hooks to the Vita binary.
#pragma once
#include <pthread.h>
#include <time.h>
extern "C" {
int strikers_test_clock_gettime(clockid_t, timespec*);
int strikers_test_cond_timedwait(pthread_cond_t*, pthread_mutex_t*, const timespec*);
int strikers_test_cond_wait(pthread_cond_t*, pthread_mutex_t*);
#if defined(__APPLE__)
int strikers_test_cond_timedwait_relative(pthread_cond_t*, pthread_mutex_t*, const timespec*);
#endif
}
#define clock_gettime strikers_test_clock_gettime
#define pthread_cond_timedwait strikers_test_cond_timedwait
#define pthread_cond_wait strikers_test_cond_wait
#if defined(__APPLE__)
#define pthread_cond_timedwait_relative_np strikers_test_cond_timedwait_relative
#endif
