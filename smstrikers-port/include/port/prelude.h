#ifndef PORT_PRELUDE_H
#define PORT_PRELUDE_H


// LP64/LLP64: the tree's 32-bit pointer assumptions are widened to 8 bytes, and on an ILP32 host
// every one of those is wrong the other way.
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ != 8
#error "port assumes 64-bit pointers"
#endif

// include/port/endian.h byte-swaps unconditionally, so on a big-endian host those helpers would
// corrupt data that was already correct.
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) &&             \
    __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "port_be* in port/endian.h assume a little-endian host"
#endif

// The decomp is built with MWCC's -char signed and relies on it, for sign-extended packed normals
// and comparisons against negative char constants; CMake pins -fsigned-char. Apple arm64 defaults
// to signed and arm64 Linux does not.
#if defined(__CHAR_UNSIGNED__)
#error "port requires signed char (-fsigned-char); see PORT_FLAGS in CMakeLists.txt"
#endif

// wchar_t is 16-bit on MWCC/Gekko, and six files cast L"..." literals to `const unsigned short*`;
// at 32 bits they compile and produce UTF-32 read as UTF-16. CMake pins -fshort-wchar.
#if defined(__SIZEOF_WCHAR_T__) && __SIZEOF_WCHAR_T__ != 2
#error "port requires 16-bit wchar_t (-fshort-wchar); see PORT_FLAGS in CMakeLists.txt"
#endif

#include <stdarg.h>

// Reconciles the decomp's GX struct-tag spellings with Aurora's anonymous typedefs.
#if defined(PORT_USE_AURORA)
#include "port/gx_tags.h"
#include "port/aurora_compat.h"
#endif
#ifndef __cplusplus
// dolphin/types.h would pull this in from its TARGET_PC branch, which the port disables to keep
// BOOL a 4-byte int.
#include <stdbool.h>
#endif
#include <math.h>
#include <stdint.h>
#include <stdio.h>


// MSL declares va_list as `typedef __va_list_struct __va_list[1]`, and headers in the tree spell
// the underlying name directly.
#ifndef __va_list
#define __va_list va_list
#endif

// NL's allocators pair a custom placement `new[]` with an explicit nlFree, since the port no longer
// replaces global operator delete[]. The array cookie is not sizeof(size_t): clang keeps array data
// at max_align_t, 16 here.


// So `new T[n]` is not used for these: the storage is allocated, a header of known size written,
// and the elements placement-constructed.
#if defined(__cplusplus) && __cplusplus >= 201103L
#include <cstddef>
#include <new>
#include <type_traits>

extern "C" void nlFree(void* ptr);
// size_t, not unsigned long: a different type on Windows, and the mangled name differs with it.
void* nlMalloc(size_t size, unsigned int alignment, bool atEnd);

namespace NlPortDetail
{
// 16 rather than sizeof(size_t), so the elements stay 16-aligned.
const std::size_t kArrayHeader = 16;

template <typename T>
inline T* ArrayNew(int count, unsigned int alignment, bool atEnd, const char* name)
{
    (void)name;
    if (count <= 0)
        return 0;

        // No (unsigned long) cast: the expression is already std::size_t, which such a cast would
        // narrow to 32 bits on Windows on its way into nlMalloc.
    char* base = (char*)nlMalloc(
        kArrayHeader + sizeof(T) * (std::size_t)count, alignment, atEnd);
    if (base == 0)
        return 0;

    *(std::size_t*)base = (std::size_t)count;
    T* first = (T*)(base + kArrayHeader);
    for (int i = 0; i < count; i++)
        ::new ((void*)&first[i]) T();
    return first;
}

template <typename T>
inline void ArrayDelete(T* ptr)
{
    // A default-constructed Vector never allocated and still calls Delete.
    if (ptr == 0)
        return;

    char* base = (char*)ptr - kArrayHeader;
    const std::size_t n = *(const std::size_t*)base;
    for (std::size_t i = n; i-- > 0;)
        ptr[i].~T();
    nlFree(base);
}
} // namespace NlPortDetail

#define NL_PORT_ARRAY_NEW(T, count, align, atEnd, name)                        \
    ::NlPortDetail::ArrayNew<T>((count), (align), (atEnd), (name))
#define NL_PORT_ARRAY_DELETE(T, ptr) ::NlPortDetail::ArrayDelete<T>(ptr)
#else
#define NL_PORT_ARRAY_NEW(T, count, align, atEnd, name)                        \
    (new ((align), (atEnd), (name)) T[(count)])
#define NL_PORT_ARRAY_DELETE(T, ptr) nlFree(ptr)
#endif

// MWCC storage/section attributes have no host equivalent.
#ifndef WEAKFUNC
#define WEAKFUNC
#endif

// MSL names its stdio object `_FILE`, which the tree spells directly.
typedef FILE _FILE;

// MSL exposes float limits as int32 arrays punted through a float pointer, e.g.
// `*(f32*)__float_max`; src/platform/msl_consts.c defines them.
#ifdef __cplusplus
extern "C" {
#endif
extern int32_t __float_max[];
extern int32_t __float_nan[];
extern int32_t __float_huge[];
// float, as pnSAnimController.cpp redeclares it at block scope, which inherits this C linkage under MSVC.
extern float __float_min[];

// MSL character-class table, indexed by unsigned char (see msl_consts.c).
extern unsigned char __ctype_map[];
#ifdef __cplusplus
}
#endif

// PowerPC intrinsics MWCC exposes: seven are reachable from Game/NL/ode code, each with an exact or
// better-than-exact portable equivalent.

#ifdef __cplusplus
extern "C++" {
#endif

// Macros, not inline functions, because glibc declares both as real extern functions and a static
// definition of a name already declared non-static is an error; they must come after <math.h>, or
// glibc's own declaration would expand as an invocation.
#define __fabs(x)  __builtin_fabs(x)
#define __fabsf(x) __builtin_fabsf(x)

static inline int    __abs(int x)     { return x < 0 ? -x : x; }

// PPC frsqrte is a 5-bit estimate every caller here refines with Newton-Raphson, so the exact value
// converges identically.
static inline double __frsqrte(double x) { return 1.0 / __builtin_sqrt(x); }

// __builtin_clz is undefined at zero, where cntlzw yields 32.
static inline unsigned int __cntlzw(unsigned int x)
{
    return x ? (unsigned int)__builtin_clz(x) : 32u;
}

#ifdef __cplusplus
}
#endif

// Ordering hints for a single-core in-order CPU with software-managed caches; the host is
// cache-coherent.

// MSL spells case-insensitive compare `strcmpi`; strcasecmp is in <strings.h>, POSIX and absent on
// Windows, where the pair is in <string.h> underscore-prefixed.
#if defined(_WIN32)
#include <string.h>
#define strcmpi  _stricmp
#define strnicmp _strnicmp
#else
#include <strings.h>
#define strcmpi  strcasecmp
#define strnicmp strncasecmp
#endif

#define __memcpy(d, s, n) __builtin_memcpy((d), (s), (n))

#define __sync()     __sync_synchronize()
#define __dcbz(a, o) ((void)0)
#define __dcbf(a, o) ((void)0)
#define __dcbi(a, o) ((void)0)

// Aurora's GXSetArray takes a buffer size and a little-endian flag as well as the console's three
// arguments, behind a GXSETARRAY macro in its own GXGeometry.h that a non-Aurora build never sees,
// so it is defined here rather than in include/dolphin/.

#if !defined(PORT_USE_AURORA) && !defined(GXSETARRAY)
#define GXSETARRAY(attr, data, size, stride, le)                                \
    ((void)(size), (void)(le), GXSetArray((attr), (data), (stride)))
#endif

#endif // PORT_PRELUDE_H
