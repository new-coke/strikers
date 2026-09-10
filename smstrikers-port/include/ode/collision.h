/*************************************************************************
 *                                                                       *
 * Open Dynamics Engine, Copyright (C) 2001-2003 Russell L. Smith.       *
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

#ifndef _ODE_COLLISION_H_
#define _ODE_COLLISION_H_

#include <ode/common.h>
#include <ode/collision_space.h>
#include <ode/contact.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* ************************************************************************ */
    /* general functions */

    void dGeomDestroy(dGeomID g);
    void dGeomSetData(dGeomID g, void* data);
    void* dGeomGetData(dGeomID g);
    void dGeomSetBody(dGeomID g, dBodyID b);
    dBodyID dGeomGetBody(dGeomID g);
    void dGeomSetPosition(dGeomID g, dReal x, dReal y, dReal z);
    void dGeomSetRotation(dGeomID g, const dMatrix3 R);
    void dGeomSetQuaternion(dGeomID g, const dQuaternion quat);
    const dReal* dGeomGetPosition(dGeomID g);
    const dReal* dGeomGetRotation(dGeomID g);
    void dGeomGetQuaternion(dGeomID g, dQuaternion quat);
    void dGeomGetAABB(dGeomID g, dReal aabb[6]);
    int dGeomIsSpace(dGeomID g);
    dSpaceID dGeomGetSpace(dGeomID g);
    int dGeomGetClass(dGeomID g);
    void dGeomSetCategoryBits(dGeomID g, unsigned long bits);
    void dGeomSetCollideBits(dGeomID g, unsigned long bits);
    unsigned long dGeomGetCategoryBits(dGeomID g);
    unsigned long dGeomGetCollideBits(dGeomID g);
    void dGeomEnable(dGeomID g);
    void dGeomDisable(dGeomID g);
    int dGeomIsEnabled(dGeomID g);

    /* ************************************************************************ */
    /* collision detection */

    int dCollide(dGeomID o1, dGeomID o2, int flags, dContactGeom* contact, int skip);
    void dSpaceCollide(dSpaceID space, void* data, dNearCallback* callback);
    void dSpaceCollide2(dGeomID g1, dGeomID g2, void* data,
        dNearCallback* callback);

    /* ************************************************************************ */
    /* standard classes */

    /* the maximum number of user classes that are supported */
    enum
    {
        dMaxUserClasses = 4
    };

    /* class numbers - each geometry object needs a unique number */
    enum
    {
        dSphereClass = 0,
        dBoxClass,
        dCCylinderClass,
        dCylinderClass,
        dPlaneClass,
        dRayClass,
        dGeomTransformClass,
        dTriMeshClass,

        dFirstSpaceClass,
        dSimpleSpaceClass = dFirstSpaceClass,
        dHashSpaceClass,
        dQuadTreeSpaceClass,
        dLastSpaceClass = dQuadTreeSpaceClass,

        dFirstUserClass,
        dLastUserClass = dFirstUserClass + dMaxUserClasses - 1,
        dGeomNumClasses
    };

    dGeomID dCreateSphere(dSpaceID space, dReal radius);
    void dGeomSphereSetRadius(dGeomID g, dReal radius);
    dReal dGeomSphereGetRadius(dGeomID g);
    dReal dGeomSpherePointDepth(dGeomID g, dReal x, dReal y, dReal z);

    dGeomID dCreateBox(dSpaceID space, dReal lx, dReal ly, dReal lz);
    void dGeomBoxSetLengths(dGeomID g, dReal lx, dReal ly, dReal lz);
    void dGeomBoxGetLengths(dGeomID g, dVector3 result);
    dReal dGeomBoxPointDepth(dGeomID g, dReal x, dReal y, dReal z);

    dGeomID dCreatePlane(dSpaceID space, dReal a, dReal b, dReal c, dReal d);
    void dGeomPlaneSetParams(dGeomID g, dReal a, dReal b, dReal c, dReal d);
    void dGeomPlaneGetParams(dGeomID g, dVector4 result);
    dReal dGeomPlanePointDepth(dGeomID g, dReal x, dReal y, dReal z);

    // dGeomID dCreateFinitePlane(dSpaceID space, dReal a, dReal b, dReal c, dReal d, bool flag, float param); // added

    dGeomID dCreateCCylinder(dSpaceID space, dReal radius, dReal length);
    void dGeomCCylinderSetParams(dGeomID g, dReal radius, dReal length);
    void dGeomCCylinderGetParams(dGeomID g, dReal* radius, dReal* length);
    dReal dGeomCCylinderPointDepth(dGeomID g, dReal x, dReal y, dReal z);

    dGeomID dCreateRay(dSpaceID space, dReal length);
    void dGeomRaySetLength(dGeomID g, dReal length);
    dReal dGeomRayGetLength(dGeomID g);
    void dGeomRaySet(dGeomID g, dReal px, dReal py, dReal pz,
        dReal dx, dReal dy, dReal dz);
    void dGeomRayGet(dGeomID g, dVector3 start, dVector3 dir);

    /*
     * Set/get ray flags that influence ray collision detection.
     * These flags are currently only noticed by the trimesh collider, because
     * they can make a major differences there.
     */
    void dGeomRaySetParams(dGeomID g, int FirstContact, int BackfaceCull);
    void dGeomRayGetParams(dGeomID g, int* FirstContact, int* BackfaceCull);
    void dGeomRaySetClosestHit(dGeomID g, int closestHit);
    int dGeomRayGetClosestHit(dGeomID g);

#include "ode/collision_trimesh.h"

    dGeomID dCreateGeomTransform(dSpaceID space);
    void dGeomTransformSetGeom(dGeomID g, dGeomID obj);
    dGeomID dGeomTransformGetGeom(dGeomID g);
    void dGeomTransformSetCleanup(dGeomID g, int mode);
    int dGeomTransformGetCleanup(dGeomID g);
    void dGeomTransformSetInfo(dGeomID g, int mode);
    int dGeomTransformGetInfo(dGeomID g);

    /* ************************************************************************ */
    /* utility functions */

    void dClosestLineSegmentPoints(const dVector3 a1, const dVector3 a2,
        const dVector3 b1, const dVector3 b2,
        dVector3 cp1, dVector3 cp2);

    int dBoxTouchesBox(const dVector3 p1, const dMatrix3 R1,
        const dVector3 side1, const dVector3 p2,
        const dMatrix3 R2, const dVector3 side2);

    void dInfiniteAABB(dGeomID geom, dReal aabb[6]);
    void dCloseODE();

    /* ************************************************************************ */
    /* custom classes */

    typedef void dGetAABBFn(dGeomID, dReal aabb[6]);
    typedef int dColliderFn(dGeomID o1, dGeomID o2,
        int flags, dContactGeom* contact, int skip);
    typedef dColliderFn* dGetColliderFnFn(int num);
    typedef void dGeomDtorFn(dGeomID o);
    typedef int dAABBTestFn(dGeomID o1, dGeomID o2, dReal aabb[6]);

    typedef struct dGeomClass
    {
        int bytes;
        dGetColliderFnFn* collider;
        dGetAABBFn* aabb;
        dAABBTestFn* aabb_test;
        dGeomDtorFn* dtor;
    } dGeomClass;

    int dCreateGeomClass(const dGeomClass* c);
    void* dGeomGetClassData(dGeomID g);
    dGeomID dCreateGeom(int classnum);

    /* ************************************************************************ */

#ifdef __cplusplus
}
#endif

#endif
