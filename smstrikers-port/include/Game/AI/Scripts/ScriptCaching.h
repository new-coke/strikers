#ifndef _SCRIPTCACHING_H_
#define _SCRIPTCACHING_H_

#include "NL/nlSingleton.h"
#include "NL/nlAVLTree.h"
#include "Game/AI/FuzzyVariant.h"
#include "PowerPC_EABI_Support/MSL_C++/MSL_Common/msl_tree.h"

extern unsigned char g_bScriptQuestionCachingOn;
extern unsigned char g_bScriptQuestionCachingUseSTD;

// PORT: uintptr_t keys, the key is two pointers summed, and is 32 bits on Windows otherwise.
typedef std::pair<const uintptr_t, FuzzyVariant> ScriptCachePair;
typedef std::map<uintptr_t, FuzzyVariant, std::less<uintptr_t>, std::allocator<ScriptCachePair> > ScriptCacheMap;
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
    unsigned char Lookup(uintptr_t hash, FuzzyVariant& returnVal, const char* name)
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
            unsigned long key;
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
    const FuzzyVariant& AddToCache(uintptr_t, const FuzzyVariant&, const char*);
    void Clear();

    /* 0x00 */ nlAVLTreeSlotPool<uintptr_t, FuzzyVariant, DefaultKeyCompare<uintptr_t> > mQuestionCacheMap;
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
    uintptr_t key, const FuzzyVariant& variant, const char* name)
{
    if (g_bScriptQuestionCachingOn)
    {
        const FuzzyVariant& cacheValue = variant;
        if (g_bScriptQuestionCachingUseSTD)
        {
            mQuestionCacheMapSTD.tree_.find_or_insert<uintptr_t, FuzzyVariant>(key).second = cacheValue;
        }
        else
        {
            mQuestionCacheMap.Add(key, cacheValue);
        }
    }
    return variant;
}
#endif // _SCRIPTCACHING_H_
