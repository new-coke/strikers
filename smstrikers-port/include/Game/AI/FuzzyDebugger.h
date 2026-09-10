#ifndef _FUZZYDEBUGGER_H_
#define _FUZZYDEBUGGER_H_

#include "Game/AI/FuzzyVariant.h"
#include "NL/nlDLListContainer.h"
#include "NL/nlDebug.h"
#include "NL/nlList.h"

class cFuzzyDebugNode;

enum eFuzzyNodeType
{
    FNODE_TYPE_NONE = -1,
    FNODE_TYPE_BEGIN = 0,
    FNODE_TYPE_CONTINUE = 1,
    FNODE_TYPE_FUNCTION = 2,
    FNODE_TYPE_IF = 3,
    FNODE_TYPE_TRUE = 4,
    FNODE_TYPE_FALSE = 5,
    FNODE_TYPE_QUESTION = 6,
    FNODE_TYPE_SETDESIRE = 7,
    FNODE_TYPE_SETPLAY = 8,
    FNODE_TYPE_SETRETURNVALUE = 9,
    FNODE_TYPE_SETACTIONFUNC = 10,
    FNODE_TYPE_GAMESTATE = 11,
    NUM_FNODE_TYPES = 12,
};

class cFuzzyDebugger
{
public:
    static void WriteSyncUpXML(float fGameTime);
    static void WriteXML();
    static void CreateGameStateLeafNode();
    static float CreateQuestionLeafNode(const char* name, float output, FuzzyVariant param1, FuzzyVariant param2);
    static FuzzyVariant CreateLeafNode(eFuzzyNodeType type, const char* name, FuzzyVariant output, FuzzyVariant param1, FuzzyVariant param2);
    static void AbortCurrentTree();
    static int CloseNode(eFuzzyNodeType type, FuzzyVariant output, FuzzyVariant param1, FuzzyVariant param2);
    static int OpenNode(eFuzzyNodeType type, const char* name, FuzzyVariant output, FuzzyVariant param1, FuzzyVariant param2);
    static void ClearAllTrees();
    static unsigned char IsOn();
    static unsigned char SetOn(unsigned char is_on, unsigned char bAbortCurrentTree);
    static cFuzzyDebugNode* GetLastDecisionTree() { return m_lDecisionTrees.m_pEnd; }

private:
    cFuzzyDebugger();
    static cFuzzyDebugNode* GetLastLeafNode(eFuzzyNodeType type);
    static cFuzzyDebugNode* CreateNewFuzzyNode(eFuzzyNodeType type, const char* name, cFuzzyDebugNode* parent, FuzzyVariant output, FuzzyVariant param1, FuzzyVariant param2);
    static void FilterDecisionTrees(nlDLListContainer<cFuzzyDebugNode*>* r_tree_list, FuzzyVariant param1);

    static cFuzzyDebugNode* m_pCurrentNode;
    static int m_iCurrentTreeNumNodes;
    static int m_iCurrentTreeLevel;
    static nlList<cFuzzyDebugNode> m_lDecisionTrees;
    static int m_iNumTrees;
    static unsigned char m_bIsOn;
};

#ifdef NDEBUG
#define nlAssert(condition) ((void)0)
#else
#define nlAssert(condition)                                     \
    do                                                          \
    {                                                           \
        if (!(condition))                                       \
        {                                                       \
            cFuzzyDebugger::WriteXML();                         \
            cFuzzyDebugger::ClearAllTrees();                    \
            nlAssertFail(#condition, __FILE__, __LINE__, true); \
        }                                                       \
    } while (0)
#endif

#endif // _FUZZYDEBUGGER_H_
