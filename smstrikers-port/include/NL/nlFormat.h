#ifndef _NLFORMAT_H_
#define _NLFORMAT_H_

#include "NL/nlBasicString.h"
#include "NL/nlLexicalCast.h"

template <typename StringType>
class FormatImpl
{
    StringType mString;
    int mCurrentPos;

public:
    FormatImpl(const StringType& string)
        : mString(string)
        , mCurrentPos(0)
    {
    }

    operator StringType() const
    {
        return mString;
    }

    template <typename T>
    FormatImpl& operator%(const T& t)
    {
        StringType insert = LexicalCast<StringType>(t);

        for (int i = 0; i < mString.size(); i++)
        {
            if (mString[i] == (typename StringType::value_type)'{'
                && i + 1 < mString.size()
                && mString[i + 1] - '0' == mCurrentPos
                && i + 2 < mString.size()
                && mString[i + 2] == (typename StringType::value_type)'}')
            {
                mString.erase(mString.begin() + i, mString.begin() + i + 3);
                mString.insert(mString.begin() + i, insert.begin(), insert.end());
            }
        }

        mCurrentPos++;
        return *this;
    }
};


template <typename StringType, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
StringType Format(const StringType& string, const T0& t0, const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5, const T6& t6, const T7& t7)
{
    return FormatImpl<StringType>(string) % t0 % t1 % t2 % t3 % t4 % t5 % t6 % t7;
}

template <typename StringType, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
StringType Format(const StringType& string, const T0& t0, const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5, const T6& t6)
{
    return FormatImpl<StringType>(string) % t0 % t1 % t2 % t3 % t4 % t5 % t6;
}

template <typename StringType, typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
StringType Format(const StringType& string, const T0& t0, const T1& t1, const T2& t2, const T3& t3, const T4& t4, const T5& t5)
{
    return FormatImpl<StringType>(string) % t0 % t1 % t2 % t3 % t4 % t5;
}

template <typename StringType, typename T0, typename T1, typename T2, typename T3, typename T4>
StringType Format(const StringType& string, const T0& t0, const T1& t1, const T2& t2, const T3& t3, const T4& t4)
{
    return FormatImpl<StringType>(string) % t0 % t1 % t2 % t3 % t4;
}

template <typename StringType, typename T0, typename T1, typename T2, typename T3>
StringType Format(const StringType& string, const T0& t0, const T1& t1, const T2& t2, const T3& t3)
{
    return FormatImpl<StringType>(string) % t0 % t1 % t2 % t3;
}

template <typename StringType, typename T0, typename T1, typename T2>
StringType Format(const StringType& string, const T0& t0, const T1& t1, const T2& t2)
{
    return FormatImpl<StringType>(string) % t0 % t1 % t2;
}

template <typename StringType, typename T0, typename T1>
StringType Format(const StringType& string, const T0& t0, const T1& t1)
{
    return FormatImpl<StringType>(string) % t0 % t1;
}

template <typename StringType, typename T0>
StringType Format(const StringType& string, const T0& t0)
{
    return FormatImpl<StringType>(string) % t0;
}

#endif // _NLFORMAT_H_
