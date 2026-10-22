#ifndef _SCRIPTQUESTIONKEY_H_
#define _SCRIPTQUESTIONKEY_H_

#include "types.h"

// PORT: retail sums question and subject into one key, which two different requests can share.
struct ScriptQuestionKey
{
    uintptr_t question;
    u64 subjectLo;
    uintptr_t subjectHi;
    int tag;

    bool operator==(const ScriptQuestionKey& other) const
    {
        return question == other.question && subjectLo == other.subjectLo
            && subjectHi == other.subjectHi && tag == other.tag;
    }

    bool operator<(const ScriptQuestionKey& other) const
    {
        if (question != other.question)
            return question < other.question;
        if (subjectLo != other.subjectLo)
            return subjectLo < other.subjectLo;
        if (subjectHi != other.subjectHi)
            return subjectHi < other.subjectHi;
        return tag < other.tag;
    }
};

#endif // _SCRIPTQUESTIONKEY_H_
