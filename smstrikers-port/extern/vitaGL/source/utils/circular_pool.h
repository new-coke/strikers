/* Strikers compatibility fix: temporaries must align ARM float stores and GPU streams. */
#ifndef VGL_CIRCULAR_POOL_H
#define VGL_CIRCULAR_POOL_H
#include <stdint.h>
#include <stddef.h>

static inline uint8_t *vgl_pool_allocate(uint8_t **cursor, const uint8_t *limit, uint32_t size) {
    const uintptr_t alignment = 16;
    const uintptr_t current = (uintptr_t)*cursor;
    const uintptr_t end = (uintptr_t)limit;
    if (current > UINTPTR_MAX - (alignment - 1)) return NULL;
    const uintptr_t begin = (current + alignment - 1) & ~(alignment - 1);
    if (begin > end || (uintptr_t)size > end - begin) return NULL;
    const uintptr_t finish = begin + size;
    // Keep the cursor bounded even when the last allocation ends in a short tail.
    const uintptr_t padding = (alignment - (finish & (alignment - 1))) & (alignment - 1);
    *cursor = (uint8_t *)(padding <= end - finish ? finish + padding : end);
    return (uint8_t *)begin;
}
#endif
