#ifndef _FUZZY_H_
#define _FUZZY_H_

class SaveConfidence
{
public:
    SaveConfidence(float* pVal)
        : m_fSavedVal(*pVal)
        , m_pVal(pVal)
    {
    }

    ~SaveConfidence()
    {
        *m_pVal = m_fSavedVal;
    }

    float m_fSavedVal;
    float* m_pVal;
};

inline float FMIN(float f1, float f2)
{
    return (f1 <= f2) ? f1 : f2;
}

inline float FMAX(float f1, float f2)
{
    return (f1 >= f2) ? f1 : f2;
}

float FLESS(float f1, float f2);
float FGREATER(float f1, float f2);
float RandomChance(float fChance);
float GenerateFilteredRandom();

#endif // _FUZZY_H_
