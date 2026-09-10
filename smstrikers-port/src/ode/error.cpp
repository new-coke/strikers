/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001,2002 Russell L. Smith.       *
 * All rights reserved.  Email: russ@q12.org   Web: www.q12.org          *
 *                                                                       *
 * This library is free software; you can redistribute it and/or         *
 * modify it under the terms of EITHER:                                  *
 *   (1) The GNU Lesser General Public License as published by the Free  *
 *       Software Foundation; either version 2.1 of the License, or (at  *
 *       your option) any later version. The text of the GNU Lesser      *
 *       General Public License is included with this library in the     *
 *       file LICENSE.TXT.                                               *
 *   (2) The BSD-style license that is included with this library in     *
 *       the file LICENSE-BSD.TXT.                                       *
 *                                                                       *
 * This library is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
 * LICENSE.TXT and LICENSE-BSD.TXT for more details.                     *
 *                                                                       *
 *************************************************************************/

#include <ode/config.h>
#include <ode/error.h>

#include <stdlib.h>

// PORT: the definition lives here.
#include "NL/nlWare.h"
extern void nlBreak();

static dMessageFunction* error_function = 0;
static dMessageFunction* debug_function = 0;
static dMessageFunction* message_function = 0;

extern "C" void dSetErrorHandler(dMessageFunction* fn)
{
    error_function = fn;
}

extern "C" void dSetDebugHandler(dMessageFunction* fn)
{
    debug_function = fn;
}

extern "C" void dSetMessageHandler(dMessageFunction* fn)
{
    message_function = fn;
}

extern "C" dMessageFunction* dGetErrorHandler()
{
    return error_function;
}

extern "C" dMessageFunction* dGetDebugHandler()
{
    return debug_function;
}

extern "C" dMessageFunction* dGetMessageHandler()
{
    return message_function;
}

static void printMessage(int num, const char* msg1, const char* msg2, va_list ap)
{
    if (num)
        nlPrintf("\n%s %d: ", msg1, num);
    else
        nlPrintf("\n%s: ", msg1);

    nlPrintf(msg2, ap);

    nlPrintf("\n");
}

extern "C" void dError(int num, const char* msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    if (error_function)
    {
        error_function(num, msg, ap);
    }
    else
    {
        if (num)
        {
            nlPrintf("\n%s %d: ", "ODE Error", num);
        }
        else
        {
            nlPrintf("\n%s: ", "ODE Error");
        }
        nlPrintf("\n");
    }
    exit(1);
}

/**
 * Offset/Address/Size: 0xF4 | 0x8021E1B0 | size: 0xF8
 */
extern "C" void dDebug(int num, const char* msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    if (debug_function)
    {
        debug_function(num, msg, ap);
    }
    else
    {
        if (num)
        {
            nlPrintf("\n%s %d: ", "ODE INTERNAL ERROR", num);
        }
        else
        {
            nlPrintf("\n%s: ", "ODE INTERNAL ERROR");
        }
        nlPrintf("\n");
    }
    // *((char *)0) = 0;   ... commit SEGVicide
    nlBreak();
}

/**
 * Offset/Address/Size: 0x0 | 0x8021E0BC | size: 0xF4
 */
extern "C" void dMessage(int num, const char* msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    if (message_function)
    {
        message_function(num, msg, ap);
    }
    else
    {
        if (num)
        {
            nlPrintf("\n%s %d: ", "ODE Message", num);
        }
        else
        {
            nlPrintf("\n%s: ", "ODE Message");
        }
        nlPrintf("\n");
    }
}
