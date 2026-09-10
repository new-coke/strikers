/*  Metrowerks Standard Library  */

/*  $Date: 1999/07/30 01:02:53 $
 *  $Revision: 1.8.4.1 $
 *  $NoKeywords: $
 *
 *		Copyright 1995-1999 Metrowerks, Inc.
 *		All rights reserved.
 */

/*
 *	wprintf.c
 *
 *	Routines
 *	--------
 *		wscanf
 *		fwscanf
 *		swscanf
 *
 *		vwscanf
 *		__vswscanf
 *		__vfwscanf
 */

#pragma ANSI_strict off /*  990729 vss  empty compilation unit illegal in ANSI C */

#ifndef __NO_WIDE_CHAR /* mm 980204 */

#pragma ANSI_strict reset

#include "types.h"
#include "limits.h"
#include "wchar_t.h"
#include "stdarg.h"
#include "scanf.h"
#include "stdio.h"

// #include "limits.h"
// #include "stdarg.h"
// #include "scanf.h"
// #include <ansi_parms.h>
// // #include "cwchar"

// #include <cstdio>
// #include <cstddef>
// #include <cerrno>

// #include <climits>
// #include <cstdarg>
// #include <cstring>
// #include <string_io.h>
// #include <cstdlib>

// #include <wchar.h>
// #include <wctype.h>

#define conversion_buff_size 512
#define conversion_max       509
#define bad_conversion       0xFFFF

enum argument_options
{
    normal_argument,
    short_argument,
    long_argument,
    long_long_argument, /* mm 961219 */
    double_argument,
    long_double_argument,
    wchar_argument /* mm 990407 */
};

typedef wchar_t char_map[32];

typedef struct
{
    unsigned char suppress_assignment;
    unsigned char field_width_specified;
    unsigned char argument_options;
    wchar_t conversion_char;
    int field_width;
    char_map char_set;
} scan_format;

#define set_char_map(map, ch) map[ch >> 3] |= (1 << (ch & 7))
#define tst_char_map(map, ch) (map[ch >> 3] & (1 << (ch & 7)))

static const wchar_t* parse_format(const wchar_t* format_string, scan_format* format)
{
    scan_format f = { 0, 0, normal_argument, 0, INT_MAX, { 0 } };
    const wchar_t* s = format_string;
    wchar_t c;
    int flag_found, invert;

    if ((c = *++s) == L'%')
    {
        f.conversion_char = c;
        *format = f;
        return ((const wchar_t*)s + 1);
    }

    if (c == L'*')
    {
        f.suppress_assignment = 1;

        c = *++s;
    }

    if (iswdigit(c))
    {
        f.field_width = 0;

        do
        {
            f.field_width = (f.field_width * 10) + (c - L'0');
            c = *++s;
        } while (iswdigit(c));

        if (f.field_width == 0)
        {
            f.conversion_char = bad_conversion;
            return ((const wchar_t*)s + 1);
        }

        f.field_width_specified = 1;
    }

    flag_found = 1;

    switch (c)
    {
    case L'h':

        f.argument_options = short_argument;

        break;

    case L'l':

        f.argument_options = long_argument;

        break;

    case L'L':

        f.argument_options = long_long_argument; /*mm 961219*/

        break;

    default:

        flag_found = 0;
    }

    if (flag_found)
        c = *++s;

    f.conversion_char = c;

    switch (c)
    {
    case L'd':
    case L'i':
    case L'u':
    case L'o':
    case L'x':
    case L'X':

        break;

    case L'f':
    case L'e':
    case L'E':
    case L'g':
    case L'G':

        if (f.argument_options == short_argument)
        {
            f.conversion_char = bad_conversion;
            break;
        }

        if (f.argument_options == long_argument)
            f.argument_options = double_argument;

        if (f.argument_options == long_long_argument)  /*mm 961219*/
            f.argument_options = long_double_argument; /*mm 961219*/

        break;

    case L'p':

        f.argument_options = long_argument;
        f.conversion_char = 'x';

        break;

    case L'c':

        if (f.argument_options == long_argument) /* mm 990407 */
            f.argument_options = wchar_argument; /* mm 990407 */
        else                                     /* mm 990407 */
            if (f.argument_options != normal_argument)
                f.conversion_char = bad_conversion;

        break;

    case L's':

        if (f.argument_options == long_argument) /* mm 990407 */
            f.argument_options = wchar_argument; /* mm 990407 */
        else                                     /* mm 990407 */
            if (f.argument_options != normal_argument)
                f.conversion_char = bad_conversion;

        {
            int i;
            wchar_t* p;

            /*for (i = sizeof(f.char_set), p = f.char_set; i; --i)*/
            for (i = (sizeof(f.char_set) / sizeof(wchar_t)), p = f.char_set; i; --i)
                *p++ = 0xFFFF;

            f.char_set[1] = 0xC1;
            f.char_set[4] = 0xFE;
        }

        break;

    case L'n':

        break;

    case L'[':

        c = *++s;

        invert = 0;

        if (c == L'^')
        {
            invert = 1;
            c = *++s;
        }

        if (c == L']')
        {
            set_char_map(f.char_set, L']');
            c = *++s;
        }

        while (c && c != L']')
        {
            int d;

            set_char_map(f.char_set, c);

            if (*(s + 1) == L'-' && (d = *(s + 2)) != 0 && d != L']')
            {
                while (++c <= d)
                    set_char_map(f.char_set, c);

                c = *(s += 3);
            }
            else
                c = *++s;
        }

        if (!c)
        {
            f.conversion_char = bad_conversion;
            break;
        }

        if (invert)
        {
            int i;
            char* p; /* mm 990408 */

            for (i = sizeof(f.char_set), p = (char*)f.char_set; i; --i, ++p) /* mm 990408 */
                *p = ~*p;
        }

        break;

    default:

        f.conversion_char = bad_conversion;

        break;
    }

    *format = f;

    return ((const wchar_t*)s + 1);
}

static int __wsformatter(wint_t (*wReadProc)(void*, wint_t, int), void* wReadProcArg,
    const wchar_t* format_str, va_list arg) /* mm 990325 */
{
    int num_chars, chars_read, items_assigned, conversions, i;
    int base, negative, overflow;
    const wchar_t* format_ptr;
    wchar_t format_char; /* mm-961002*/
    wchar_t c;           /* mm-961002*/
    scan_format format;
    long long_num;
    unsigned long u_long_num;
#ifdef __MSL_LONGLONG_SUPPORT__         /*mm 961219*/
    long long long_long_num;            /*mm 961219*/
    unsigned long long u_long_long_num; /*mm 961219*/
#endif                                  /*mm 961219*/
#ifndef _No_Floating_Point
    long double long_double_num;
#endif
    wchar_t* arg_ptr;

    format_ptr = format_str;
    chars_read = 0;
    items_assigned = 0;
    conversions = 0;

    while (!(*wReadProc)(wReadProcArg, 0, __TestForwcsError) && (format_char = *format_ptr) != 0) /* mm 990325 */
    {
        if (iswspace(format_char))
        {
            do
                format_char = *++format_ptr;
            while (iswspace(format_char));

            while ((iswspace)(c = (*wReadProc)(wReadProcArg, 0, __GetAwChar))) /* mm 990325 */ /* mm 990409 */
                ++chars_read;

            (*wReadProc)(wReadProcArg, c, __UngetAwChar); /* mm 990325 */

            continue;
        }

        if (format_char != L'%')
        {
            if ((c = (*wReadProc)(wReadProcArg, 0, __GetAwChar)) != format_char) /* mm 990325 */
            {
                (*wReadProc)(wReadProcArg, c, __UngetAwChar); /* mm 990325 */
                goto exit;
            }

            ++chars_read;
            ++format_ptr;

            continue;
        }

        format_ptr = parse_format(format_ptr, &format);

        if (!format.suppress_assignment && format.conversion_char != L'%')
            arg_ptr = va_arg(arg, unsigned short*);
        else
            arg_ptr = 0;

        switch (format.conversion_char)
        {
        case L'd':

            base = 10;

            goto signed_int;

        case L'i':

            base = 0;

        signed_int:

#ifdef __MSL_LONGLONG_SUPPORT__                                                                                      /*mm 961219*/
            if (format.argument_options == long_long_argument)                                                       /*mm 961219*/
                u_long_long_num =                                                                                    /*mm 961219*/
                    __wcstoull(base, format.field_width, wReadProc, wReadProcArg, &num_chars, &negative, &overflow); /* mm 990325 */
            else                                                                                                     /*mm 961219*/
#endif                                                                                                               /*mm 961219*/
                u_long_num
                    = __wcstoul(base, format.field_width, wReadProc, wReadProcArg, &num_chars, &negative, &overflow); /* mm 990325 */

            if (!num_chars)
                goto exit;

            chars_read += num_chars;

#ifdef __MSL_LONGLONG_SUPPORT__                                                  /*mm 961219*/
            if (format.argument_options == long_long_argument)                   /*mm 961219*/
                long_long_num = (negative ? -u_long_long_num : u_long_long_num); /*mm 961219*/
            else                                                                 /*mm 961219*/
#endif                                                                           /*mm 961219*/
                long_num = (negative ? -u_long_num : u_long_num);

            if (arg_ptr)
            {
                switch (format.argument_options)
                {
                case normal_argument:
                    *(int*)arg_ptr = long_num;
                    break;
                case short_argument:
                    *(short*)arg_ptr = long_num;
                    break;
                case long_argument:
                    *(long*)arg_ptr = long_num;
                    break;
#ifdef __MSL_LONGLONG_SUPPORT__ /*mm 961219*/
                case long_long_argument:
                    *(long long*)arg_ptr = long_long_num;
                    break;
#endif /*mm 961219*/
                }

                ++items_assigned;
            }

            ++conversions;

            break;

        case L'o':

            base = 8;

            goto unsigned_int;

        case L'u':

            base = 10;

            goto unsigned_int;

        case L'x':
        case L'X':

            base = 16;

        unsigned_int:

#ifdef __MSL_LONGLONG_SUPPORT__                                                                                      /*mm 961219*/
            if (format.argument_options == long_long_argument)                                                       /*mm 961219*/
                u_long_long_num =                                                                                    /*mm 961219*/
                    __wcstoull(base, format.field_width, wReadProc, wReadProcArg, &num_chars, &negative, &overflow); /* mm 990325 */
            else                                                                                                     /*mm 961219*/
#endif                                                                                                               /*mm 961219*/
                u_long_num
                    = __wcstoul(base, format.field_width, wReadProc, wReadProcArg, &num_chars, &negative, &overflow); /* mm 990325 */

            if (!num_chars)
                goto exit;

            chars_read += num_chars;

            if (negative)
#ifdef __MSL_LONGLONG_SUPPORT__                                    /*mm 961219*/
                if (format.argument_options == long_long_argument) /*mm 961219*/
                    u_long_long_num = -u_long_long_num;            /*mm 961219*/
                else                                               /*mm 961219*/
#endif                                                             /*mm 961219*/
                    u_long_num = -u_long_num;

            if (arg_ptr)
            {
                switch (format.argument_options)
                {
                case normal_argument:
                    *(unsigned int*)arg_ptr = u_long_num;
                    break;
                case short_argument:
                    *(unsigned short*)arg_ptr = u_long_num;
                    break;
                case long_argument:
                    *(unsigned long*)arg_ptr = u_long_num;
                    break;
#ifdef __MSL_LONGLONG_SUPPORT__ /*mm 961219*/
                case long_long_argument:
                    *(unsigned long long*)arg_ptr = u_long_long_num;
                    break;
#endif /*mm 961219*/
                }

                ++items_assigned;
            }

            ++conversions;

            break;

#ifndef _No_Floating_Point

        case L'f':
        case L'e':
        case L'E':
        case L'g':
        case L'G':

            long_double_num = __wcstold(format.field_width, wReadProc, wReadProcArg, &num_chars, &overflow); /* mm 990325 */

            if (!num_chars)
                goto exit;

            chars_read += num_chars;

            if (arg_ptr)
            {
                switch (format.argument_options)
                {
                case normal_argument:
                    *(float*)arg_ptr = long_double_num;
                    break;
                case double_argument:
                    *(double*)arg_ptr = long_double_num;
                    break;
                case long_double_argument:
                    *(long double*)arg_ptr = long_double_num;
                    break;
                }

                ++items_assigned;
            }

            ++conversions;

            break;

#endif

        case L'c':

            if (!format.field_width_specified)
                format.field_width = 1;

            if (arg_ptr)
            {
                num_chars = 0;

                while (format.field_width-- && (c = (*wReadProc)(wReadProcArg, 0, __GetAwChar)) != WEOF) /* mm 980202 */ /* mm 990325 */
                {
                    if (format.argument_options == wchar_argument) /* mm 990407 */
                        *arg_ptr++ = c;                            /* mm 990407 */
                    else                                           /* mm 990407 */
                    {
                        i = wctomb((char*)arg_ptr, c);            /* mm 990407 */
                        if (i < 0)                                /* mm 990407 */
                            goto exit;                            /* mm 990407 */
                        arg_ptr = (wchar_t*)((char*)arg_ptr + i); /* mm 990407 */
                    }
                    ++num_chars;
                }

                if (!num_chars)
                    goto exit;

                chars_read += num_chars;

                ++items_assigned;
            }
            else
            {
                num_chars = 0;

                while (format.field_width-- && (c = (*wReadProc)(wReadProcArg, 0, __GetAwChar)) != WEOF) /* mm 980202 */ /* mm 990325 */
                    ++num_chars;

                if (!num_chars)
                    goto exit;
            }

            ++conversions;

            break;

        case L'%':

            while ((iswspace)(c = (*wReadProc)(wReadProcArg, 0, __GetAwChar))) /* mm 990409 */
                ++chars_read;

            if (c != L'%')
            {
                (*wReadProc)(wReadProcArg, c, __UngetAwChar);
                goto exit;
            }

            ++chars_read;

            break;

        case L's':

            c = (*wReadProc)(wReadProcArg, 0, __GetAwChar); /*mm 970218*/ /* mm 990325 */    /* mm 990413 */
            while (iswspace(c))                                                              /*mm 970218*/
            {                                                                                /*mm 970218*/
                ++chars_read;                                                                /*mm 970218*/
                c = (*wReadProc)(wReadProcArg, 0, __GetAwChar); /*mm 970218*/ /*mm 980130 */ /* mm 990325 */
            } /*mm 970218*/

            (*wReadProc)(wReadProcArg, c, __UngetAwChar); /* mm 990325 */

        case L'[':

            if (arg_ptr)
            {
                num_chars = 0;
                while (format.field_width-- && (c = (*wReadProc)(wReadProcArg, 0, __GetAwChar)) != WEOF && tst_char_map(format.char_set, c)) /* mm 980202 */ /* mm 990325 */
                {
                    if (format.argument_options == wchar_argument) /* mm 990407 */
                        *arg_ptr++ = c;                            /* mm 990407 */
                    else
                    {
                        i = wctomb((char*)arg_ptr, c);            /* mm 990407 */
                        if (i < 0)                                /* mm 990407 */
                            goto exit;                            /* mm 990407 */
                        arg_ptr = (wchar_t*)((char*)arg_ptr + i); /* mm 990407 */
                    }

                    ++num_chars;
                }

                if (!num_chars)
                {
                    (*wReadProc)(wReadProcArg, c, __UngetAwChar); /* mm 961114 */ /* mm 990325 */
                    goto exit;
                }

                chars_read += num_chars;

                *arg_ptr = 0;

                ++items_assigned;
            }
            else
            {
                num_chars = 0;

                while (format.field_width-- && (c = (*wReadProc)(wReadProcArg, 0, __GetAwChar)) != WEOF && tst_char_map(format.char_set, c)) /* mm 980202 */ /* mm 990325 */
                    ++num_chars;

                if (!num_chars)
                {
                    (*wReadProc)(wReadProcArg, c, __UngetAwChar); /* mm 970513 */ /* mm 990325 */
                    goto exit;
                }
                chars_read += num_chars; /* mm 970501 */
            }

            if (format.field_width >= 0)
                (*wReadProc)(wReadProcArg, c, __UngetAwChar); /* mm 990325 */

            ++conversions;

            break;

        case L'n':

            if (arg_ptr)
                switch (format.argument_options)
                {
                case normal_argument:
                    *(int*)arg_ptr = chars_read;
                    break;
                case short_argument:
                    *(short*)arg_ptr = chars_read;
                    break;
                case long_argument:
                    *(long*)arg_ptr = chars_read;
                    break;
#ifdef __MSL_LONGLONG_SUPPORT__ /*mm 961219*/
                case long_long_argument:
                    *(long long*)arg_ptr = chars_read;
                    break;
#endif /*mm 961219*/
                }

            continue;

        case bad_conversion:
        default:

            goto exit;
        }
    }

exit:

    if ((*wReadProc)(wReadProcArg, 0, __TestForwcsError) && conversions == 0) /* mm 990325 */
        return (WEOF);                                                        /* mm 980202 */

    return (items_assigned);
}

/* The remainder of this file has been heavily modified mm 990325 */
wint_t __wFileRead(void* File, wint_t ch, int Action)
{
    switch (Action)
    {
    case __GetAwChar:
        return (fgetwc((FILE*)File));
    case __UngetAwChar:
        return (ungetwc(ch, (FILE*)File));
    case __TestForwcsError:
        return (ferror((FILE*)File) || feof((FILE*)File));
    }
    return 0; /* to satisfy compiler */
}

/**
 * Offset/Address/Size: 0x0 | 0x80236E54 | size: 0x98
 */
wint_t __wStringRead(void* isc, wint_t ch, int Action)
{
    wchar_t RetVal;
    __wInStrCtrl* Iscp = (__wInStrCtrl*)isc;
    switch (Action)
    {
    case __GetAwChar:
        RetVal = *(Iscp->wNextChar);
        if (RetVal == L'\0')
        {
            Iscp->wNullCharDetected = 1;
            return (WEOF);
        }
        else
        {
            Iscp->wNextChar++;
            return (RetVal);
        }
    case __UngetAwChar:
        if (!Iscp->wNullCharDetected)    /* mm 990413 */
            Iscp->wNextChar--;           /* mm 990413 */
        else                             /* mm 990413 */
            Iscp->wNullCharDetected = 0; /* mm 990413 */
        return (ch);
    case __TestForwcsError:
        return (Iscp->wNullCharDetected);
    }
    return 0; /* to satisfy compiler */
}

/*
    fwscanf

    Description

    The fwscanf function reads input fromthe stream
    pointed to by file, under control of the wide string
    pointed to by format that specifies that admissible input
    sequences and how they are to be converted for assignment,
    using subsequent arguments as pointers to the objects to
    recieve the converted input. If there are insufficient
    arguments for the format, the behaviour is undefined. If the
    format is exhausted while arguments remain, the excess
    arguments are evaluated (as always) but are otherwise
    ignored.

    Returns

    The fwscanf function returns the value of the macro
    EOF if an input failure occurs before and conversion.
    Otherwise, the fwscanf function returns the number of input
    items assigned, whcih can be fewer than provided for, or
    even zero, in the event of an early matching failure.
*/

int fwscanf(FILE* file, const wchar_t* format, ...)
{
#if __PPC_EABI__ || __MIPS__ /*__dest_os == __mips_bare  */
    va_list args;
    if (fwide(file, 1) <= 0)
        return (EOF);
    va_start(args, format);
    return (__vfwscanf(file, format, args));
#else
    if (fwide(file, 1) <= 0)
        return (EOF);
    return (__wsformatter(&__wFileRead, (void*)file, format, __va_start(format)));
#endif
}

/*
    wscanf

    Description

    The wscanf function is equivalent to fwscanf with the
    argument stdin interposed before the arguments to wscanf

    Returns

    The wscanf function returns the value of the macro EOF
    if an input failure occurs before any conversion. Otherwise,
    the wscanf function returns the number of iniput items
    assigned, whcih can be fewer than provided for, or even
    zero, in the event of an early matching failure.
*/

int wscanf(const wchar_t* format, ...)
{
#if __PPC_EABI__ || __MIPS__ /*__dest_os == __mips_bare  */
    va_list args;
    if (fwide(stdin, 1) <= 0)
        return (EOF);
    va_start(args, format);
    return (__vfwscanf(stdin, format, args));
#else
    if (fwide(stdin, 1) <= 0)
        return (EOF);
    return (__wsformatter(&__wFileRead, (void*)stdin, format, __va_start(format)));
#endif
}

/*
    vwscanf

    Description

    The vwscanf function is equivalent to wscanf with the
    variable argument list replaced by arg, which shall have
    been initialized by the va_start macro (and possibly
    subsequent va_arg calls). The vwscanf function does not
    invoke the va_end macro

    Returns

    The vwscanf function returns the value of the macro EOF
    if an input failure occurs before any conversion. Otherwise,
    the wscanf function returns the number of iniput items
    assigned, whcih can be fewer than provided for, or even
    zero, in the event of an early matching failure.
*/

int vwscanf(const wchar_t* format, va_list arg)
{
    return (__wsformatter(&__wFileRead, (void*)stdin, format, arg));
}

/*
    vswscanf

    Description

    The vswscanf function is equivalent to swscanf with the
    variable argument list replaced by arg, which shall have
    been initialized by the va_start macro (and possibly
    subsequent va_arg calls). The vwscanf function does not
    invoke the va_end macro

    Returns

    The vswscanf function returns the value of the macro EOF
    if an input failure occurs before any conversion. Otherwise,
    the wscanf function returns the number of iniput items
    assigned, whcih can be fewer than provided for, or even
    zero, in the event of an early matching failure.
*/

int __vswscanf(const wchar_t* s, const wchar_t* format, va_list arg)
{
    __wInStrCtrl wisc;
    wisc.wNextChar = (wchar_t*)s;
    wisc.wNullCharDetected = 0;

    return (__wsformatter(&__wStringRead, (void*)&wisc, format, arg));
}

/*
    swscanf

    Description

    The swscanf function is equivalent to fwscanf, except
    that the argument s specifies a wide string from which
    the input is to be obtained, rather that from a stream. Reaching
    the end of the wide string is equivalent to encountering
    end-of-file for the fwscanf function.


    Returns

    The swscanf function returns the value of the macro EOF
    if an input failure occurs before any conversion. Otherwise,
    the wscanf function returns the number of iniput items
    assigned, whcih can be fewer than provided for, or even
    zero, in the event of an early matching failure.
*/

int swscanf(const wchar_t* s, const wchar_t* format, ...)
{
#if __PPC_EABI__ || __MIPS__ /*   __dest_os == __mips_bare  */
    va_list args;
    va_start(args, format);
    return (__vswscanf(s, format, args));
#else
    return (__vswscanf(s, format, __va_start(format)));
#endif
}

#endif /* #ifndef __NO_WIDE_CHAR*/ /* mm 981020 */

/*  Change Record
 *	980121 	JCM  First code release.
 *  mm 980206    Added call to fwide()
 * mm 981020  Added #ifndef __NO_WIDE_CHAR wrappers
 *  mm 990325	Changes to split string scan functions from file i/o
 *  mm 990407		Changes to handle %ls and %lc
 *  mm 990408	Correction to invert char set map code.
 *  mm 990409	Correction to avoid using macro for iswspace() where there are side-effects.
 *  mm 990409a  Change to handle NULL source string.
 *  mm 990413	Corrected handling of case where source text quits early IL9904-0736
 */
