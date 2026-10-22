#ifndef _SCRIPTCACHING_H_
#define _SCRIPTCACHING_H_

#include "NL/nlSingleton.h"
#include "NL/nlAVLTree.h"
#include "Game/AI/FuzzyVariant.h"
#include "Game/AI/Scripts/ScriptQuestionKey.h"
#include <string.h>
#include "PowerPC_EABI_Support/MSL_C++/MSL_Common/msl_tree.h"

extern unsigned char g_bScriptQuestionCachingOn;
extern unsigned char g_bScriptQuestionCachingUseSTD;

// PORT: keys on the subject's tag and raw payload, since Variant::GetHash loses floats and vectors.
inline ScriptQuestionKey MakeScriptQuestionKey(uintptr_t question, const Variant& argument)
{
    ScriptQuestionKey key;
    key.question = question;
    key.tag = (int)argument.mType;
    if (argument.mType == FT_VECTOR)
    {
        // PORT: Reset writes the three floats and nothing past them, so the tail is left out.
        u32 x, y, z;
        memcpy(&x, &argument.mData.vector.x, sizeof x);
        memcpy(&y, &argument.mData.vector.y, sizeof y);
        memcpy(&z, &argument.mData.vector.z, sizeof z);
        key.subjectLo = ((u64)y << 32) | (u64)x;
        key.subjectHi = (uintptr_t)z;
    }
    else
    {
        key.subjectLo = argument.mData.u;
        key.subjectHi = 0;
    }
    return key;
}

typedef std::pair<const ScriptQuestionKey, FuzzyVariant> ScriptCachePair;
typedef std::map<ScriptQuestionKey, FuzzyVariant, std::less<ScriptQuestionKey>, std::allocator<ScriptCachePair> > ScriptCacheMap;
typedef std::__tree<ScriptCachePair, ScriptCacheMap::value_compare, std::allocator<ScriptCachePair> > ScriptCacheTree;

class ScriptQuestionCache : public nlSingleton<ScriptQuestionCache>
{
public:
    static ScriptQuestionCache* const* InstanceStorage() { return &s_pInstance; }

    ScriptQuestionCache()
        : mQuestionCacheMap(16, 16)
    {
    }

    ~ScriptQuestionCache();
    unsigned char Lookup(const ScriptQuestionKey& hash, FuzzyVariant& returnVal, const char* name)
    {
        struct MapNodeBase
        {
            void* left;
            void* right;
            void* parent;
        };

        struct MapTree
        {
            unsigned long x0;
            MapNodeBase x4;
        };

        struct MapNode
        {
            MapNodeBase base;
            ScriptQuestionKey key;
            FuzzyVariant value;
        };

        FuzzyVariant* pValue;

        mTotalLookups++;

        if (g_bScriptQuestionCachingUseSTD)
        {
            MapNode* stdFound = (MapNode*)mQuestionCacheMapSTD.find(hash).ptr_;
            if ((MapNodeBase*)stdFound != &((MapTree*)&mQuestionCacheMapSTD)->x4)
            {
                mCacheHits++;
                returnVal = stdFound->value;
                return 1;
            }
        }
        else if (mQuestionCacheMap.FindGet(hash, &pValue))
        {
            mCacheHits++;
            returnVal = *pValue;
            return 1;
        }

        return 0;
    }
    const FuzzyVariant& AddToCache(const ScriptQuestionKey&, const FuzzyVariant&, const char*);
    void Clear();

    /* 0x00 */ nlAVLTreeSlotPool<ScriptQuestionKey, FuzzyVariant, DefaultKeyCompare<ScriptQuestionKey> > mQuestionCacheMap;
    /* 0x28 */ ScriptCacheMap mQuestionCacheMapSTD;
    /* 0x38 */ int mTotalLookups;
    /* 0x3C */ int mCacheHits;
}; // total size: 0x40

inline ScriptQuestionCache::~ScriptQuestionCache()
{
    Clear();
}

inline void ScriptQuestionCache::Clear()
{
    mQuestionCacheMap.Clear();
    mQuestionCacheMapSTD.tree_.clear();
    mCacheHits = 0;
    mTotalLookups = 0;
}
inline const FuzzyVariant& ScriptQuestionCache::AddToCache(
    const ScriptQuestionKey& key, const FuzzyVariant& variant, const char* name)
{
    if (g_bScriptQuestionCachingOn)
    {
        const FuzzyVariant& cacheValue = variant;
        if (g_bScriptQuestionCachingUseSTD)
        {
            mQuestionCacheMapSTD.tree_.find_or_insert<ScriptQuestionKey, FuzzyVariant>(key).second = cacheValue;
        }
        else
        {
            mQuestionCacheMap.Add(key, cacheValue);
        }
    }
    return variant;
}
#endif // _SCRIPTCACHING_H_
