#include "NL/nlSingleton.h"

// Hoisted above the includes: an explicit specialisation must be declared before anything implicitly instantiates the template.
class ScriptQuestionCache;
template <>
ScriptQuestionCache* nlSingleton<ScriptQuestionCache>::s_pInstance;

#include "Game/AI/Scripts/ScriptCaching.h"

unsigned char g_bScriptQuestionCachingOn = 1;
unsigned char g_bScriptQuestionCachingUseSTD;
template <>
ScriptQuestionCache* nlSingleton<ScriptQuestionCache>::s_pInstance = 0;
