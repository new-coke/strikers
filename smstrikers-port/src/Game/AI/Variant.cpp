#include "Game/AI/Variant.h"
#include "NL/nlFormat.h"
#include "NL/nlLexicalCast.h"
#include "PowerPC_EABI_Support/Runtime/runtime.h"

Variant gvNotSet;

typedef BasicString<char, Detail::TempStringAllocator> NLString;

/**
 * Offset/Address/Size: 0xC4 | 0x8006632C | size: 0xF2C
 */
NLString Variant::ToString() const
{
    NLString toString;

    if (mType != FT_UNSPECIFIED)
    {
        NLString dataString = "???";

        switch (mType)
        {
        case FT_BOOL:
            dataString = mData.b ? "TRUE" : "FALSE";
            break;

        case FT_CHAR:
        {
            dataString = Format(NLString("{0}"), mData.c);
            break;
        }

        case FT_SHORT:
        {
            dataString = Format(NLString("{0}"), (int)mData.s);
            break;
        }

        case FT_INT:
        {
            dataString = Format(NLString("{0}"), mData.i);
            break;
        }

        case FT_U32:
        {
            dataString = Format(NLString("{0}"), mData.u);
            break;
        }

        case FT_FLOAT:
        {
            dataString = Format(NLString("{0}"), mData.f);
            break;
        }

        case FT_VECTOR:
        {
            dataString = Format(NLString("({0},{1},{2})"), mData.vector.x, mData.vector.y, mData.vector.z);
            break;
        }

        case FT_PLAYER:
            if (mData.pPlayer != 0)
            {
                dataString = Format(NLString("UPID={0}"), mData.pPlayer->GetUniqueID(-1));
            }
            break;

        case FT_TEAM:
            if (mData.pTeam != 0)
            {
                dataString = Format(NLString("Team={0}"), mData.pTeam->m_nSide == 0 ? "Home" : "Away");
            }
            break;
        }

        toString = dataString;
    }
    else
    {
        toString = "N/A";
    }

    return toString;
}

/**
 * Offset/Address/Size: 0x0 | 0x80066268 | size: 0xC4
 */
uintptr_t Variant::GetHash() const
{
    unsigned long hash = 0;

    switch (mType)
    {
    case FT_BOOL:
        return mData.b;

    case FT_CHAR:
        hash = mData.c;
        hash = (s8)hash;
        return hash;

    case FT_SHORT:
        return (uintptr_t)mData.s;

    case FT_INT:
        return (uintptr_t)mData.i;

    case FT_U32:
        return mData.u;

    case FT_FLOAT:
        return __cvt_fp2unsigned((f64)mData.f);

    case FT_PLAYER:
        return (uintptr_t)mData.pPlayer;

    case FT_TEAM:
        return (uintptr_t)mData.pTeam;

    case FT_VECTOR:
    {
        unsigned long hash1 = __cvt_fp2unsigned((f64)mData.vector.z);
        unsigned long hash2 = __cvt_fp2unsigned((f64)mData.vector.y);
        hash2 ^= hash1;
        unsigned long hash3 = __cvt_fp2unsigned((f64)mData.vector.x);
        hash = hash3 | hash2;
        break;
    }
    }

    return hash;
}
