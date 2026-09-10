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

#include <ode/common.h>
#include <ode/odemath.h>

#include "math.h"

// this may be called for vectors `a' with extremely small magnitude, for
// example the result of a cross product on two nearly perpendicular vectors.
// we must be robust to these small vectors. to prevent numerical error,
// first find the component a[i] with the largest magnitude and then scale
// all the components by 1/a[i]. then we can compute the length of `a' and
// scale the components by 1/l. this has been verified to work with vectors
// containing the smallest representable numbers.

float nlRecipSqrt(float, bool);

/**
 * Offset/Address/Size: 0x1F4 | 0x802206FC | size: 0x198
 */
void dNormalize3(dVector3 a)
{
    dReal aa0, aa1, aa2, a0, a1, a2, l;
    dAASSERT(a);
    a0 = a[0];
    a1 = a[1];
    a2 = a[2];
    aa0 = dFabs(a0);
    aa2 = (float)__fabs(a2);
    aa1 = dFabs(a1);
    if (aa1 > aa0)
    {
        if (aa2 > aa1)
        {
            goto aa2_largest;
        }
        else
        { // aa1 is largest
            a0 /= aa1;
            a2 /= aa1;
            // l = dRecipSqrt(a0 * a0 + a2 * a2 + 1);
            l = nlRecipSqrt(a0 * a0 + a2 * a2 + 1, true);
            a[0] = a0 * l;
            // a[1] = dCopySign(l, a1);
            a[1] = copysign(l, a1);
            a[2] = a2 * l;
        }
    }
    else
    {
        if (aa2 > aa0)
        {
        aa2_largest: // aa2 is largest
            a0 /= aa2;
            a1 /= aa2;
            // l = dRecipSqrt(a0 * a0 + a1 * a1 + 1);
            l = nlRecipSqrt(a0 * a0 + a1 * a1 + 1, true);
            a[0] = a0 * l;
            a[1] = a1 * l;
            // a[2] = dCopySign(l, a2);
            a[2] = copysign(l, a2);
        }
        else
        { // aa0 is largest
            if (aa0 <= 0)
            {
                // dDEBUGMSG ("vector has zero size"); ... this messace is annoying
                a[0] = 1; // if all a's are zero, this is where we'll end up.
                a[1] = 0; // return a default unit length vector.
                a[2] = 0;
                return;
            }
            a1 /= aa0;
            a2 /= aa0;
            // l = dRecipSqrt(a1 * a1 + a2 * a2 + 1);
            l = nlRecipSqrt(a1 * a1 + a2 * a2 + 1, true);
            // a[0] = dCopySign(l, a0);
            a[0] = copysign(l, a0);
            a[1] = a1 * l;
            a[2] = a2 * l;
        }
    }
}

/* OLD VERSION */
/*
void dNormalize3 (dVector3 a)
{
  dAASSERT (a);
  dReal l = dDOT(a,a);
  if (l > 0) {
    l = dRecipSqrt(l);
    a[0] *= l;
    a[1] *= l;
    a[2] *= l;
  }
  else {
    a[0] = 1;
    a[1] = 0;
    a[2] = 0;
  }
}
*/

/**
 * Offset/Address/Size: 0x144 | 0x8022064C | size: 0xB0
 */
void dNormalize4(dVector4 a)
{
    dReal l = dDOT(a, a) + a[3] * a[3];

    if (l > 0.0f)
    {
        l = nlRecipSqrt(l, true);
        a[0] *= l;
        a[1] *= l;
        a[2] *= l;
        a[3] *= l;
    }
    else
    {
        a[0] = 1.0f;
        a[1] = 0.0f;
        a[2] = 0.0f;
        a[3] = 0.0f;
    }
}

/**
 * Offset/Address/Size: 0x0 | 0x80220508 | size: 0x144
 */
void dPlaneSpace(const dVector3 n, dVector3 p, dVector3 q)
{
    dAASSERT(n && p && q);
    if (dFabs(((volatile const dReal*)n)[2]) > M_SQRT1_2)
    {
        dReal a = n[1] * n[1] + n[2] * n[2];
        dReal k = nlRecipSqrt(a, true);
        p[0] = 0.0f;
        p[1] = -n[2] * k;
        p[2] = n[1] * k;
        q[0] = a * k;
        q[1] = -n[0] * p[2];
        q[2] = n[0] * p[1];
    }
    else
    {
        dReal a = n[0] * n[0] + n[1] * n[1];
        dReal k = nlRecipSqrt(a, true);
        p[0] = -n[1] * k;
        p[1] = n[0] * k;
        p[2] = 0.0f;
        q[0] = -n[2] * p[1];
        q[1] = n[2] * p[0];
        q[2] = a * k;
    }
}
