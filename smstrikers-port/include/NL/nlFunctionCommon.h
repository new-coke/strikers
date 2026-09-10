#ifndef _NLFUNCTION_COMMON_DEFINED_
#define _NLFUNCTION_COMMON_DEFINED_

#include "NL/nlMemory.h"
#include "types.h"

enum Tag
{
    EMPTY = 0,
    FREE_FUNCTION = 1,
    FUNCTOR = 2,
};

template <typename T>
struct IsVoid
{
    enum
    {
        value = false,
    };
};

template <>
struct IsVoid<void>
{
    enum
    {
        value = true,
    };
};

template <bool Value>
struct BoolToType
{
};

namespace Detail
{
template <typename R, typename MemPtr>
struct MemFunImpl
{
private:
    MemPtr mMemFun;

public:
    MemFunImpl(MemPtr function)
        : mMemFun(function)
    {
    }

    template <typename T>
    R operator()(T* object) const
    {
        return (object->*mMemFun)();
    }

    template <typename T, typename P>
    R operator()(T* object, P argument) const
    {
        return (object->*mMemFun)(argument);
    }
};
} // namespace Detail

template <typename T, typename R, typename P>
Detail::MemFunImpl<R, R (T::*)(P)> MemFun(R (T::*function)(P))
{
    return Detail::MemFunImpl<R, R (T::*)(P)>(function);
}

template <typename T, typename R>
Detail::MemFunImpl<R, R (T::*)()> MemFun(R (T::*function)())
{
    return Detail::MemFunImpl<R, R (T::*)()>(function);
}

template <typename Signature>
class Function;

typedef void FnVoidVoid();

template <typename ReturnType, typename P1>
class Function1;

#endif // _NLFUNCTION_COMMON_DEFINED_
