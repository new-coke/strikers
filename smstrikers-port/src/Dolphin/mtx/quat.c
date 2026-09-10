#include <dolphin.h>
#include <dolphin/math.h>

void C_QUATAdd(const Quaternion* p, const Quaternion* q, Quaternion* r)
{
    ASSERTMSGLINE(77, p, "QUATAdd():  NULL QuaternionPtr 'p' ");
    ASSERTMSGLINE(78, q, "QUATAdd():  NULL QuaternionPtr 'q' ");
    ASSERTMSGLINE(79, r, "QUATAdd():  NULL QuaternionPtr 'r' ");

    r->x = p->x + q->x;
    r->y = p->y + q->y;
    r->z = p->z + q->z;
    r->w = p->w + q->w;
}

void PSQUATAdd(const register Quaternion* p, const register Quaternion* q, register Quaternion* r)
{
    // PORT: paired-single kernel replaced by its portable twin.
    C_QUATAdd(p, q, r);
}

void C_QUATSubtract(const Quaternion* p, const Quaternion* q, Quaternion* r)
{
    ASSERTMSGLINE(133, p, "QUATSubtract():  NULL QuaternionPtr 'p' ");
    ASSERTMSGLINE(134, q, "QUATSubtract():  NULL QuaternionPtr 'q' ");
    ASSERTMSGLINE(135, r, "QUATSubtract():  NULL QuaternionPtr 'r' ");

    r->x = p->x - q->x;
    r->y = p->y - q->y;
    r->z = p->z - q->z;
    r->w = p->w - q->w;
}

void PSQUATSubtract(const register Quaternion* p, const register Quaternion* q, register Quaternion* r)
{
    // PORT: paired-single kernel replaced by its portable twin.
    C_QUATSubtract(p, q, r);
}

void C_QUATMultiply(const Quaternion* p, const Quaternion* q, Quaternion* pq)
{
    Quaternion* r;
    Quaternion pqTmp;

    ASSERTMSGLINE(193, p, "QUATMultiply():  NULL QuaternionPtr 'p' ");
    ASSERTMSGLINE(194, q, "QUATMultiply():  NULL QuaternionPtr 'q' ");
    ASSERTMSGLINE(195, pq, "QUATMultiply():  NULL QuaternionPtr 'pq' ");

    if (p == pq || q == pq)
    {
        r = &pqTmp;
    }
    else
    {
        r = pq;
    }

    r->w = (p->w * q->w) - (p->x * q->x) - (p->y * q->y) - (p->z * q->z);
    r->x = (p->w * q->x) + (p->x * q->w) + (p->y * q->z) - (p->z * q->y);
    r->y = (p->w * q->y) + (p->y * q->w) + (p->z * q->x) - (p->x * q->z);
    r->z = (p->w * q->z) + (p->z * q->w) + (p->x * q->y) - (p->y * q->x);

    if (r == &pqTmp)
    {
        *pq = pqTmp;
    }
}

void PSQUATMultiply(register const Quaternion* a, register const Quaternion* b, register Quaternion* ab)
{
    // PORT: paired-single kernel replaced by its portable twin.
    C_QUATMultiply(a, b, ab);
}

void C_QUATScale(const Quaternion* q, Quaternion* r, f32 scale)
{
    ASSERTMSGLINE(306, q, "QUATScale():  NULL QuaternionPtr 'q' ");
    ASSERTMSGLINE(307, r, "QUATScale():  NULL QuaternionPtr 'r' ");

    r->x = q->x * scale;
    r->y = q->y * scale;
    r->z = q->z * scale;
    r->w = q->w * scale;
}

/**
 * Offset/Address/Size: 0x0 | 0x80252FDC | size: 0x1C
 */
void PSQUATScale(const register Quaternion* q, register Quaternion* r, register f32 scale)
{
    // PORT: paired-single kernel replaced by its portable twin.
    C_QUATScale(q, r, scale);
}

f32 C_QUATDotProduct(const Quaternion* p, const Quaternion* q)
{
    ASSERTMSGLINE(357, p, "QUATDotProduct():  NULL QuaternionPtr 'p' ");
    ASSERTMSGLINE(358, q, "QUATDotProduct():  NULL QuaternionPtr 'q' ");

    return (q->x * p->x) + (q->y * p->y) + (q->z * p->z) + (q->w * p->w);
}

/**
 * Offset/Address/Size: 0x1C | 0x80252FF8 | size: 0x20
 */
f32 PSQUATDotProduct(const register Quaternion* p, const register Quaternion* q)
{
    // PORT: paired-single kernel replaced by its portable twin.
    return C_QUATDotProduct(p, q);
}

void C_QUATNormalize(const Quaternion* src, Quaternion* unit)
{
    f32 mag;

    ASSERTMSGLINE(407, src, "QUATNormalize():  NULL QuaternionPtr 'src' ");
    ASSERTMSGLINE(408, unit, "QUATNormalize():  NULL QuaternionPtr 'unit' ");

    mag = (src->x * src->x) + (src->y * src->y) + (src->z * src->z) + (src->w * src->w);
    if (mag >= 0.00001f)
    {
        mag = 1.0f / sqrtf(mag);

        unit->x = src->x * mag;
        unit->y = src->y * mag;
        unit->z = src->z * mag;
        unit->w = src->w * mag;
    }
    else
    {
        unit->x = unit->y = unit->z = unit->w = 0.0f;
    }
}

void PSQUATNormalize(const register Quaternion* src, register Quaternion* unit)
{
    // PORT: paired-single kernel replaced by its portable twin.
    C_QUATNormalize(src, unit);
}

void C_QUATInverse(const Quaternion* src, Quaternion* inv)
{
    f32 mag, norminv;

    ASSERTMSGLINE(498, src, "QUATInverse():  NULL QuaternionPtr 'src' ");
    ASSERTMSGLINE(499, inv, "QUATInverse():  NULL QuaternionPtr 'inv' ");

    mag = (src->x * src->x) + (src->y * src->y) + (src->z * src->z) + (src->w * src->w);
    if (mag == 0.0f)
    {
        mag = 1.0f;
    }

    norminv = 1.0f / mag;
    inv->x = -src->x * norminv;
    inv->y = -src->y * norminv;
    inv->z = -src->z * norminv;
    inv->w = src->w * norminv;
}

void PSQUATInverse(const register Quaternion* src, register Quaternion* inv)
{
    // PORT: paired-single kernel replaced by its portable twin.
    C_QUATInverse(src, inv);
}

void C_QUATDivide(const Quaternion* p, const Quaternion* q, Quaternion* r)
{
    Quaternion qtmp;

    ASSERTMSGLINE(606, p, "QUATDivide():  NULL QuaternionPtr 'p' ");
    ASSERTMSGLINE(607, q, "QUATDivide():  NULL QuaternionPtr 'q' ");
    ASSERTMSGLINE(608, r, "QUATDivide():  NULL QuaternionPtr 'r' ");

    C_QUATInverse(q, &qtmp);
    C_QUATMultiply(&qtmp, p, r);
}

void PSQUATDivide(const Quaternion* p, const Quaternion* q, Quaternion* r)
{
    // PORT: paired-single kernel replaced by its portable twin.
    C_QUATDivide(p, q, r);
}

void C_QUATExp(const Quaternion* q, Quaternion* r)
{
    f32 theta, scale;

    ASSERTMSGLINE(643, q, "QUATExp():  NULL QuaternionPtr 'q' ");
    ASSERTMSGLINE(644, r, "QUATExp():  NULL QuaternionPtr 'r' ");
    ASSERTMSGLINE(647, q->w == 0.0f, "QUATExp():  'q' is not a pure quaternion. ");

    theta = sqrtf((q->x * q->x) + (q->y * q->y) + (q->z * q->z));
    scale = 1.0f;

    if (theta > 0.00001f)
    {
        scale = sinf(theta) / theta;
    }

    r->x = scale * q->x;
    r->y = scale * q->y;
    r->z = scale * q->z;
    r->w = cosf(theta);
}

void C_QUATLogN(const Quaternion* q, Quaternion* r)
{
    f32 theta, scale;

    ASSERTMSGLINE(676, q, "QUATLogN():  NULL QuaternionPtr 'q' ");
    ASSERTMSGLINE(677, r, "QUATLogN():  NULL QuaternionPtr 'r' ");

    scale = (q->x * q->x) + (q->y * q->y) + (q->z * q->z);
    scale = sqrtf(scale);
    theta = atan2f(scale, q->w);

    if (scale > 0.0f)
    {
        scale = theta / scale;
    }

    r->x = scale * q->x;
    r->y = scale * q->y;
    r->z = scale * q->z;
    r->w = 0.0f;
}

void C_QUATMakeClosest(const Quaternion* q, const Quaternion* qto, Quaternion* r)
{
    f32 dot;

    ASSERTMSGLINE(722, q, "QUATMakeClosest():  NULL QuaternionPtr 'q' ");
    ASSERTMSGLINE(723, qto, "QUATMakeClosest():  NULL QuaternionPtr 'qto' ");
    ASSERTMSGLINE(724, r, "QUATMakeClosest():  NULL QuaternionPtr 'r' ");

    dot = (q->x * qto->x) + (q->y * qto->y) + (q->z * qto->z) + (q->w * qto->w);
    if (dot < 0.0f)
    {
        r->x = -q->x;
        r->y = -q->y;
        r->z = -q->z;
        r->w = -q->w;
    }
    else
    {
        *r = *q;
    }
}

void C_QUATRotAxisRad(Quaternion* q, const Vec* axis, f32 rad)
{
    f32 tmp, tmp2, tmp3;
    Vec dst;

    tmp = rad;
    PSVECNormalize(axis, &dst);

    tmp2 = 0.5f * tmp;
    tmp3 = sinf(0.5f * tmp);
    tmp = tmp3;
    tmp3 = cosf(tmp2);

    q->x = tmp * dst.x;
    q->y = tmp * dst.y;
    q->z = tmp * dst.z;
    q->w = tmp3;
}

#pragma fp_contract off
void C_QUATMtx(Quaternion* r, const Mtx m)
{
    f32 vv0, vv1;
    s32 i, j, k;
    s32 idx[3] = { 1, 2, 0 };
    f32 vec[3];
    vv0 = m[0][0] + m[1][1] + m[2][2];
    if (vv0 > 0.0f)
    {
        vv1 = (f32)sqrtf(vv0 + 1.0f);
        r->w = vv1 * 0.5f;
        vv1 = 0.5f / vv1;
        r->x = (m[2][1] - m[1][2]) * vv1;
        r->y = (m[0][2] - m[2][0]) * vv1;
        r->z = (m[1][0] - m[0][1]) * vv1;
    }
    else
    {
        i = 0;
        if (m[1][1] > m[0][0])
            i = 1;
        if (m[2][2] > m[i][i])
            i = 2;
        j = idx[i];
        k = idx[j];
        vv1 = (f32)sqrtf((m[i][i] - (m[j][j] + m[k][k])) + 1.0f);
        vec[i] = vv1 * 0.5f;
        if (vv1 != 0.0f)
            vv1 = 0.5f / vv1;
        r->w = (m[k][j] - m[j][k]) * vv1;
        vec[j] = (m[i][j] + m[j][i]) * vv1;
        vec[k] = (m[i][k] + m[k][i]) * vv1;
        r->x = vec[0];
        r->y = vec[1];
        r->z = vec[2];
    }
}
#pragma fp_contract on

void C_QUATLerp(const Quaternion* p, const Quaternion* q, Quaternion* r, f32 t)
{
    f32 value;
    f32 pValue;

    pValue = p->x;
    value = t * (q->x - pValue);
    r->x = pValue + value;

    pValue = p->y;
    value = t * (q->y - pValue);
    r->y = pValue + value;

    pValue = p->z;
    value = t * (q->z - pValue);
    r->z = pValue + value;

    value = t * (q->w - p->w);
    r->w = p->w + value;
}

#pragma fp_contract off
/**
 * Offset/Address/Size: 0x3C | 0x80253018 | size: 0x174
 */
void C_QUATSlerp(const Quaternion* p, const Quaternion* q, Quaternion* r, f32 t)
{
    f32 theta, sin_th, cos_th;
    f32 tp, tq;

    cos_th = p->x * q->x + p->y * q->y + p->z * q->z + p->w * q->w;
    tq = 1.0f;

    if (cos_th < 0.0f)
    {
        cos_th = -cos_th;
        tq = -tq;
    }

    if (cos_th <= 0.99999f)
    {
        theta = acosf(cos_th);
        sin_th = sinf(theta);

        tp = sinf((1.0f - t) * theta) / sin_th;
        tq *= sinf(t * theta) / sin_th;
    }
    else
    {
        tp = 1.0f - t;
        tq *= t;
    }

    r->x = (tp * p->x) + (tq * q->x);
    r->y = (tp * p->y) + (tq * q->y);
    r->z = (tp * p->z) + (tq * q->z);
    r->w = (tp * p->w) + (tq * q->w);
}
#pragma fp_contract on

void C_QUATSquad(const Quaternion* p, const Quaternion* a, const Quaternion* b, const Quaternion* q, Quaternion* r, f32 t)
{
    Quaternion pq, ab;
    f32 t2;

    ASSERTMSGLINE(927, p, "QUATSquad():  NULL QuaternionPtr 'p' ");
    ASSERTMSGLINE(928, a, "QUATSquad():  NULL QuaternionPtr 'a' ");
    ASSERTMSGLINE(929, b, "QUATSquad():  NULL QuaternionPtr 'b' ");
    ASSERTMSGLINE(930, q, "QUATSquad():  NULL QuaternionPtr 'q' ");
    ASSERTMSGLINE(931, r, "QUATSquad():  NULL QuaternionPtr 'r' ");

    t2 = 2.0f * t * (1.0f - t);
    C_QUATSlerp(p, q, &pq, t);
    C_QUATSlerp(a, b, &ab, t);
    C_QUATSlerp(&pq, &ab, r, t2);
}

void C_QUATCompA(const Quaternion* qprev, const Quaternion* q, const Quaternion* qnext, Quaternion* a)
{
    Quaternion qm, qp, lqm, lqp, qpqm, exq;

    ASSERTMSGLINE(958, qprev, "QUATCompA():  NULL QuaternionPtr 'qprev' ");
    ASSERTMSGLINE(959, q, "QUATCompA():  NULL QuaternionPtr 'q' ");
    ASSERTMSGLINE(960, qnext, "QUATCompA():  NULL QuaternionPtr 'qnext' ");
    ASSERTMSGLINE(961, a, "QUATCompA():  NULL QuaternionPtr 'a' ");

    C_QUATDivide(qprev, q, &qm);
    C_QUATLogN(&qm, &lqm);
    C_QUATDivide(qnext, q, &qp);
    C_QUATLogN(&qp, &lqp);
    C_QUATAdd(&lqp, &lqm, &qpqm);
    C_QUATScale(&qpqm, &qpqm, -0.25f);
    C_QUATExp(&qpqm, &exq);
    C_QUATMultiply(q, &exq, a);
}
