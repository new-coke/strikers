#ifndef _MSL_COMMON_ANSI_FP_H
#define _MSL_COMMON_ANSI_FP_H

#define SIGDIGLEN 36

#include "types.h"

typedef struct decimal
{
    char sign;
    char unk1;
    short exp;
    struct
    {
        unsigned char length;
        unsigned char text[36];
        unsigned char unk41;
    } sig;
} decimal;

typedef struct decform
{
    char style;
    char unk1;
    short digits;
} decform;

void __timesdec(decimal*, const decimal*, const decimal*);
void __two_exp(decimal*, s32);
BOOL __equals_dec(const decimal*, const decimal*);
void __minus_dec(decimal*, const decimal*, const decimal*);
void __num2dec_internal(decimal*, f64);
void __num2dec(const decform* f, f64 x, decimal* d);
f64 __dec2num(const decimal* d);

#endif
