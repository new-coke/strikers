#ifndef _NLSTRING_TMPL_H_
#define _NLSTRING_TMPL_H_

template <typename CharT>
CharT nlToUpper(CharT);

template <typename CharT>
unsigned long nlStrLen(const CharT* str)
{
    unsigned long n = 0;
    if (str)
    {
        while (str[n])
            n++;
    }
    return n;
}

/**
 * Offset/Address/Size: 0x0 | 0x8000DEFC | size: 0x40
 * CharT* nlStrNCpy<CharT>(CharT*, const CharT*, unsigned long)
 */
template <typename CharT>
CharT* nlStrNCpy(CharT* str1, const CharT* str2, unsigned long len)
{
    CharT* p;
    unsigned long c;
    unsigned long n;
    // PORT: on console this read from MEM1 address 0 and yielded "".
    if (str2 == NULL)
    {
        if (len)
            str1[0] = (CharT)0;
        return str1;
    }
    n = len;
    p = str1;
    goto test;
loop:
    if (sizeof(CharT) == 1)
    {
        p++;
        str2++;
    }
    else
    {
        str2++;
        p++;
    }
test:
    if (n-- == 0)
        goto done;
    if (sizeof(CharT) == 1)
        c = *(const unsigned char*)str2;
    else
        c = *str2;
    *p = c;
    if ((CharT)c)
        goto loop;
done:
    str1[len - 1] = '\0';
    return str1;
}

/**
 * Offset/Address/Size: 0x0 | 0x8000DEFC | size: 0x90
 * CharT* nlStrNCat<char>(char*, const char*, const char*, unsigned long)
 */
template <typename CharT>
CharT* nlStrNCat(CharT* dest, const CharT* a, const CharT* b, unsigned long maxsize)
{
    CharT* p;
    unsigned long n = 0;

    goto entry1;
body1:
    *p++ = *a++;
    n++;
    if (n >= maxsize)
    {
        dest[maxsize - 1] = (CharT)0;
        return dest;
    }
    goto test1;
entry1:
    p = dest;
test1:
    if (*a)
        goto body1;

    goto entry2;
body2:
    *p++ = *b++;
    n++;
    if (n >= maxsize)
    {
        dest[maxsize - 1] = (CharT)0;
        return dest;
    }
    goto test2;
entry2:
    p = &dest[n];
test2:
    if (*b)
        goto body2;

    dest[n] = (CharT)0;
    return dest;
}

template <typename CharT>
int nlStrCmp(const CharT* a, const CharT* b)
{
    int c1;
    int c2;

    do
    {
        c2 = (unsigned char)*b++;
        c1 = (unsigned char)*a++;
        if ((CharT)c1 == 0)
            break;
        if ((CharT)c2 == 0)
            break;
    } while ((CharT)c1 == (CharT)c2);

    return (CharT)c1 - (CharT)c2;
}

template <typename CharT>
int nlStrICmp(const CharT* str1, const CharT* str2)
{
    CharT c1;
    CharT c2;

    do
    {
        c1 = nlToUpper<CharT>(const_cast<CharT&>(*str1++));
        c2 = nlToUpper<CharT>(const_cast<CharT&>(*str2++));
    } while (c1 != 0 && c2 != 0 && c1 == c2);

    return c1 - c2;
}
/**
 * Offset/Address/Size: 0x0 | 0x8014C2B8 | size: 0x98
 */
template <typename CharT>
int nlStrNICmp(const CharT* a, const CharT* b, unsigned long maxsize)
{
    CharT c1;
    CharT c2;

    do
    {
        c1 = nlToUpper<CharT>(const_cast<CharT&>(*a++));
        c2 = nlToUpper<CharT>(const_cast<CharT&>(*b++));
    } while (--maxsize != 0 && c1 != 0 && c2 != 0 && c1 == c2);

    return c1 - c2;
}

template <typename CharT>
int nlStrNCmp(const CharT* a, const CharT* b, unsigned long maxsize)
{
    int c1;
    int c2;
    CharT av;

    do
    {
        --maxsize;
        av = *a;
        c2 = (unsigned char)*b;
        c1 = (unsigned char)av;
        b++;
        a++;
        if (maxsize == 0)
            break;
        if ((CharT)c1 == 0)
            break;
        if ((CharT)c2 == 0)
            break;
    } while ((CharT)c1 == (CharT)c2);
    return (CharT)c1 - (CharT)c2;
}

template <typename CharT>
CharT nlToUpper(CharT c)
{
    if ((c >= 0x61) && (c <= 0x7A))
    {
        return c & 0x5F;
    }
    return c;
}

/**
 * Offset/Address/Size: 0x0 | 0x801D2854 | size: 0x20
 * void nlToLower<unsigned char>(unsigned char)
 */

// Single character version
template <typename CharT>
CharT nlToLower(CharT c)
{
    if ((c >= 0x41) && (c <= 0x5A))
    {
        c = (CharT)(c | 0x20);
    }
    return c;
}
template <typename CharT>
CharT* nlToLower(CharT* str)
{
    CharT* cp = str;
    while (*cp)
    {
        *cp = nlToLower<CharT>(*cp);
        cp++;
    }
    return str;
}

template <typename CharT>
CharT* nlStrChr(const CharT* str, CharT ch)
{
    while (*str != static_cast<CharT>(0))
    {
        if (*str == ch)
            return const_cast<CharT*>(str);
        ++str;
    }
    return nullptr;
}

#endif // _NLSTRING_TMPL_H_
