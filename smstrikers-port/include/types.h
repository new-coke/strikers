#ifndef _TYPES_H
#define _TYPES_H

// #include "global.h"
#include "dolphin/types.h"

typedef unsigned int uint;
typedef long double f128;
typedef volatile f128 vf128;

typedef u32 unknown;
typedef u8 unknown8;
typedef u16 unknown16;
typedef u32 unknown32;
typedef u64 unknown64;


// Workaround for strongly typed enums (our version of C++ doesn't support them)
#define DEFINE_ENUM_TYPE(name, ...) \
    struct name                     \
    {                               \
        enum Values                 \
        {                           \
            __VA_ARGS__             \
        };                          \
        typedef Values Type;        \
    }

// Workaround for Visual Studio and VS Code not recognising the above macro as a valid type
#define BEGIN_ENUM_TYPE(name) \
    struct name               \
    {                         \
        typedef

#define END_ENUM_TYPE \
    Type;             \
    }

// Workarounds for our version of C++ and other stupidities
#undef TRUE
#define TRUE (1)
#undef FALSE
#define FALSE (0)

#if !defined(__cplusplus)
#undef NULL
#define NULL ((void*)0)
#undef nullptr
#define nullptr 0
#elif __cplusplus >= 201103L
#define NULL 0
#else
#undef NULL
#define NULL 0
#undef nullptr
#define nullptr 0
#endif

// Random and useful macros
#define PATH_MAX (256) // Max path length

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b)) // Returns the maximum of a and b
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b)) // Returns the minimum of a and b
#endif

#ifndef ALIGN_DECL
#define ALIGN_DECL(ALIGNMENT) __attribute__((aligned(ALIGNMENT)))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(o) (sizeof((o)) / sizeof(*(o)))
#endif

// Flag manipulation macros
#define SET_FLAG(x, val)                (x |= (val))                     // Sets specific flag to 1
#define RESET_FLAG(x, val)              (x &= ~(val))                    // Resets specific flag from (val) back to 0
#define IS_FLAG(x, val)                 (x & val)                        // Return 1 if flag is set, 0 if flag is not set
#define ARRAY_SIZE(o)                   (sizeof((o)) / sizeof(*(o)))     // Array size define
#define ALIGN_PREV(X, N)                ((X) & ~((N) - 1))               // Align X to the previous N bytes (N must be power of two)
#define ALIGN_NEXT(X, N)                ALIGN_PREV(((X) + (N) - 1), N)   // Align X to the next N bytes (N must be power of two)
#define IS_ALIGNED(X, N)                ((X & ((N) - 1)) == 0)           // True if X is aligned to N bytes, else false
#define IS_NOT_ALIGNED(X, N)            (((X) & ((N) - 1)) != 0)         // True if X is not aligned to N bytes, else false
#define ATTRIBUTE_ALIGN(num)            __attribute__((aligned(num)))    // Align object to num bytes (num should be power of two)
#define IS_FLAG_SET(flags, bitsFromLSB) (((flags) >> (bitsFromLSB) & 1)) // Checks if a flag is set in a bitfield
#define ASSERT_HANG(cond) \
    if (!(cond))          \
    {                     \
        while (true)      \
        {                 \
        }                 \
    }

// clang-format off
#define FORCE_DONT_INLINE \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; \
	(void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0; (void*)0;
// clang-format on

inline void padStack(void)
{
    int pad = 0;
}
#define PAD_STACK() padStack()

#ifdef __MWERKS__
#define WEAKFUNC        __declspec(weak)
#define DECL_SECT(name) __declspec(section name)
#define ASM             asm
#else
#define WEAKFUNC
#define DECL_SECT(name)
#define ASM
#endif

#define INIT DECL_SECT(".init")

// Disable clangd warnings
#ifdef __clang__
// Allow string literals to be converted to char*
#pragma clang diagnostic ignored "-Wc++11-compat-deprecated-writable-strings"
#endif

#endif // _TYPES_H
