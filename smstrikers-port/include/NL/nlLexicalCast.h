#ifndef _NLLEXICALCAST_H_
#define _NLLEXICALCAST_H_

#include "types.h"
#include "strtold.h"
#include "NL/nlBasicString.h"
#include "NL/nlDebug.h"
#include "NL/nlPrint.h"

namespace Detail
{
template <typename To, typename From>
struct LexicalCastImpl
{
    static To Do(const From& f);
};
} // namespace Detail

template <typename To, typename From>
To LexicalCast(const From& from)
{
    return Detail::LexicalCastImpl<To, From>::Do(const_cast<From&>(from));
}

namespace Detail
{

template <typename To>
struct LexicalCastImpl<To, const char*>
{
    static To Do(const char* s);
};

template <typename To>
struct LexicalCastImpl<To*, const char*>
{
    static To* Do(const char* value);
};

template <typename Allocator>
struct LexicalCastImpl<BasicString<char, Allocator>, const char*>
{
    static BasicString<char, Allocator> Do(const char* s);
};

// PORT: a string literal has array type.
template <typename Allocator, int N>
struct LexicalCastImpl<BasicString<char, Allocator>, char[N]>
{
    static BasicString<char, Allocator> Do(const char (&s)[N])
    {
        return BasicString<char, Allocator>(s);
    }
};

template <typename Allocator, int N>
struct LexicalCastImpl<BasicString<unsigned short, Allocator>, unsigned short[N]>
{
    static BasicString<unsigned short, Allocator> Do(const unsigned short (&s)[N])
    {
        return BasicString<unsigned short, Allocator>(s);
    }
};

template <>
struct LexicalCastImpl<bool, const char*>
{
    static bool Do(const char* s) { return strcmp("true", s) == 0; }
};

template <typename To>
struct LexicalCastImpl<To, int>
{
    static To Do(int t);
};

template <typename To>
struct LexicalCastImpl<To, unsigned long>
{
    static To Do(unsigned long t)
    {
        char s[0x40];
        nlSNPrintf(s, 0x40, "%u", t);
        return To(s);
    }
};

// PORT: `unsigned long` and `unsigned long long` are the same width on the hosts this was written for and are never the same type.
template <typename To>
struct LexicalCastImpl<To, unsigned long long>
{
    static To Do(unsigned long long t)
    {
        char s[0x40];
        nlSNPrintf(s, 0x40, "%llu", t);
        return To(s);
    }
};

template <typename To>
struct LexicalCastImpl<To, unsigned int>
{
    static To Do(unsigned int t)
    {
        char s[0x40];
        nlSNPrintf(s, 0x40, "%u", t);
        return To(s);
    }
};

template <typename To>
struct LexicalCastImpl<To, char>
{
    static To Do(char t);
};

template <typename To>
struct LexicalCastImpl<To, float>
{
    static To Do(float t);
};

template <typename To>
struct LexicalCastImpl<To, bool>
{
    static To Do(bool t);
};

template <typename Allocator>
struct LexicalCastImpl<BasicString<char, Allocator>, int>
{
    static BasicString<char, Allocator> Do(int t);
};

template <typename Allocator>
struct LexicalCastImpl<BasicString<char, Allocator>, float>
{
    static BasicString<char, Allocator> Do(float t);
};

template <typename Allocator>
struct LexicalCastImpl<BasicString<char, Allocator>, bool>
{
    static BasicString<char, Allocator> Do(bool t);
};

template <>
struct LexicalCastImpl<bool, int>
{
    static bool Do(int t)
    {
        bool result;
        if (t)
            result = true;
        else
            result = false;
        return result;
    }
};

template <>
struct LexicalCastImpl<bool, float>
{
    static bool Do(float t)
    {
        bool result;
        if (t)
            result = true;
        else
            result = false;
        return result;
    }
};
} // namespace Detail

typedef BasicString<unsigned short, Detail::TempStringAllocator> WideBasicString;
typedef BasicString<char, Detail::TempStringAllocator> NLString;

template <>
inline WideBasicString Detail::LexicalCastImpl<WideBasicString, WideBasicString>::Do(
    const WideBasicString& f)
{
    return f;
}

template <>
inline WideBasicString Detail::LexicalCastImpl<WideBasicString, const unsigned short*>::Do(
    const unsigned short* const& f)
{
    return WideBasicString(f);
}

namespace Detail
{
template <typename Allocator>
BasicString<char, Allocator> LexicalCastImpl<BasicString<char, Allocator>, const char*>::Do(
    const char* s)
{
    return BasicString<char, Allocator>(s);
}
} // namespace Detail

template <>
inline int Detail::LexicalCastImpl<int, const char*>::Do(const char* s)
{
    return (int)atof(s);
}

template <>
inline float Detail::LexicalCastImpl<float, const char*>::Do(const char* s)
{
    return (float)atof(s);
}

template <>
inline NLString Detail::LexicalCastImpl<NLString, char>::Do(char t)
{
    char s[0x40];
    nlSNPrintf(s, 0x40, "%c", t);
    return NLString(s);
}

namespace Detail
{
template <typename Allocator>
inline BasicString<char, Allocator> LexicalCastImpl<BasicString<char, Allocator>, int>::Do(int t)
{
    char s[0x40];
    nlSNPrintf(s, 0x40, "%i", t);
    return BasicString<char, Allocator>(s);
}

template <typename Allocator>
inline BasicString<char, Allocator> LexicalCastImpl<BasicString<char, Allocator>, float>::Do(float t)
{
    char s[0x40];
    nlSNPrintf(s, 0x40, "%f", t);
    return BasicString<char, Allocator>(s);
}

template <typename Allocator>
inline BasicString<char, Allocator> LexicalCastImpl<BasicString<char, Allocator>, bool>::Do(bool t)
{
    if (t)
    {
        return BasicString<char, Allocator>("true");
    }
    return BasicString<char, Allocator>("false");
}

template <typename To>
To LexicalCastImpl<To, const char*>::Do(const char* s)
{
    return (To)atof(s);
}

template <typename To>
inline To LexicalCastImpl<To, int>::Do(int t)
{
    return (To)t;
}

template <typename To>
inline To LexicalCastImpl<To, float>::Do(float t)
{
    return (To)t;
}

template <typename To>
inline To LexicalCastImpl<To, bool>::Do(bool t)
{
    return t;
}

template <>
inline const char* LexicalCastImpl<const char*, bool>::Do(bool value)
{
    return value ? "true" : "false";
}

template <>
inline const char* LexicalCastImpl<const char*, int>::Do(int)
{
    nlBreak();
    return 0;
}

template <>
inline const char* LexicalCastImpl<const char*, float>::Do(float)
{
    nlBreak();
    return 0;
}

template <typename To>
inline To* LexicalCastImpl<To*, const char*>::Do(const char* value)
{
    return (To*)value;
}

} // namespace Detail

template <>
inline WideBasicString LexicalCast<WideBasicString, WideBasicString>(
    const WideBasicString& from)
{
    return Detail::LexicalCastImpl<WideBasicString, WideBasicString>::Do(from);
}

template <>
inline WideBasicString LexicalCast<WideBasicString, const unsigned short*>(
    const unsigned short* const& from)
{
    return Detail::LexicalCastImpl<WideBasicString, const unsigned short*>::Do(from);
}

#endif // _NLLEXICALCAST_H_
