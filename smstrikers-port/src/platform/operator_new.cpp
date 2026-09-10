// The global allocation operators nlMemory.cpp leaves out; being weak, a partial override mixes.

#include <cstddef>
#include <new>

// Built as C++17: libc++ offers the over-aligned operators under C++11 and libstdc++ does not.
#if !defined(__cpp_aligned_new)
#error "operator_new.cpp must be compiled as C++17 (see strikers_operators)"
#endif

// size_t, not unsigned long: the two differ on Windows and so does the mangled name.
void* nlMalloc(std::size_t size, unsigned int alignment, bool atEnd);
void nlFree(void* ptr);

// C++14 sized deallocation; nlFree recovers the size from the block header.
void operator delete(void* ptr, std::size_t) noexcept { nlFree(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { nlFree(ptr); }

void operator delete(void* ptr, const std::nothrow_t&) noexcept { nlFree(ptr); }
void operator delete[](void* ptr, const std::nothrow_t&) noexcept { nlFree(ptr); }

// --- C++17 over-aligned forms ------------------------------------------------ Dawn and Abseil
// both use over-aligned types.
#if !defined(_WIN32)
#define PORT_HOST_ALIGNED_NEW 1
#include <cstdlib>
#endif

namespace {

// Returns null on failure, like the nothrow forms want; the throwing forms raise above.
void* port_aligned_alloc(std::size_t size, std::size_t align) noexcept
{
#if defined(PORT_HOST_ALIGNED_NEW)
    if (align < sizeof(void*))
        align = sizeof(void*);
    void* p = nullptr;
    if (posix_memalign(&p, align, size ? size : 1) != 0)
        return nullptr;
    return p;
#else
    return nlMalloc(size ? size : 1, (unsigned int)align, false);
#endif
}

}  // namespace

void* operator new(std::size_t size, std::align_val_t align)
{
    void* p = port_aligned_alloc(size, (std::size_t)align);
    if (p == nullptr)
        throw std::bad_alloc();
    return p;
}

void* operator new[](std::size_t size, std::align_val_t align)
{
    return ::operator new(size, align);
}

void* operator new(std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept
{
    return port_aligned_alloc(size, (std::size_t)align);
}

void* operator new[](std::size_t size, std::align_val_t align, const std::nothrow_t&) noexcept
{
    return port_aligned_alloc(size, (std::size_t)align);
}

void operator delete(void* ptr, std::align_val_t) noexcept { nlFree(ptr); }
void operator delete[](void* ptr, std::align_val_t) noexcept { nlFree(ptr); }
void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept { nlFree(ptr); }
void operator delete[](void* ptr, std::size_t, std::align_val_t) noexcept { nlFree(ptr); }
void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept { nlFree(ptr); }
void operator delete[](void* ptr, std::align_val_t, const std::nothrow_t&) noexcept { nlFree(ptr); }
