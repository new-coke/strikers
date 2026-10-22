#pragma once
// Force-included only when compiling native_jobs.cpp for this host driver.
// The implementation still uses real pthread mutexes, condition waits and joins.
#include <pthread.h>
extern "C" int jobs_test_cond_wait(pthread_cond_t*, pthread_mutex_t*);
#define pthread_cond_wait jobs_test_cond_wait
