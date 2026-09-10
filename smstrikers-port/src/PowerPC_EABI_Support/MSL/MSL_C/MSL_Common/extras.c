#include "extras.h"
#include "ctype.h"
#include "string.h"

char* strlwr(char* str)
{
    char* p = str;

    while (*p != 0)
    {
        *p = _tolower(*p);
        p++;
    }

    return str;
}

char* _strlwr(char* str)
{
    return strlwr(str);
}

int stricmp(const char* str1, const char* str2)
{
    char a_var;
    char b_var;

    do
    {
        b_var = _tolower(*str1++);
        a_var = _tolower(*str2++);

        if (b_var < a_var)
        {
            return -1;
        }
        if (b_var > a_var)
        {
            return 1;
        }
    } while (b_var != 0);

    return 0;
}

int _stricmp(const char* str1, const char* str2)
{
    return stricmp(str1, str2);
}

char* strupr(char* str)
{
    char* p = str;

    while (*p != 0)
    {
        *p = toupper(*p);
        p++;
    }

    return str;
}

char* _strupr(char* str)
{
    return strupr(str);
}

char* strset(char* str, int ch)
{
    char* p = str;

    while (*str != 0)
    {
        *str++ = ch;
    }

    return p;
}

char* _strset(char* str, int ch)
{
    return strset(str, ch);
}

char* strnset(char* str, int ch, size_t n)
{
    char* p = str;

    while (n-- != 0 && *str != 0)
    {
        *str++ = ch;
    }

    return p;
}

char* _strnset(char* str, int ch, size_t n)
{
    return strnset(str, ch, n);
}

/**
 * Offset/Address/Size: 0x0 | 0x8023A000 | size: 0x8C
 */
int strcmpi(const char* str1, const char* str2)
{
    return stricmp(str1, str2);
}

int _strcmpi(const char* str1, const char* str2)
{
    return strcmpi(str1, str2);
}

int strcasecmp(const char* str1, const char* str2)
{
    return stricmp(str1, str2);
}

int _strcasecmp(const char* str1, const char* str2)
{
    return strcasecmp(str1, str2);
}

int _stricoll(const char* str1, const char* str2)
{
    return stricmp(str1, str2);
}

int _strncoll(const char* str1, const char* str2, size_t n)
{
    return strncmp(str1, str2, n);
}

int stricoll(const char* str1, const char* str2)
{
    return _stricoll(str1, str2);
}

int strncoll(const char* str1, const char* str2, size_t n)
{
    return _strncoll(str1, str2, n);
}
