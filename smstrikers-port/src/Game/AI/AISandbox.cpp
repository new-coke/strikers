#include "Game/AI/AISandbox.h"
#include "Game/AI/Scripts/ScriptCaching.h"
#include "Game/AI/Scripts/ScriptQuestions.h"

template <>
AISandbox* nlSingleton<AISandbox>::s_pInstance = NULL;
