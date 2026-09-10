#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stddef.h>
// #include <wchar.h>

#define RAND_MAX 32767

#ifdef __cplusplus
extern "C"
{
#endif

    void srand(unsigned int seed);
    int rand(void);
    void abort(void);
    void exit(int status);
    // size_t wcstombs(char* dest, const wchar_t* src, size_t max);
    int atoi(const char* str);

    typedef int (*_compare_function)(const void*, const void*);
    void qsort(void* table_base, size_t num_members, size_t member_size, _compare_function compare_members);

#ifdef __cplusplus
}
#endif

#endif
