#ifndef _NLTOKENIZER_H_
#define _NLTOKENIZER_H_

#include "NL/nlBasicString.h"

template <typename StringType>
class Tokenizer
{
public:
    typedef typename StringType::value_type value_type;

    class iterator
    {
    public:
        iterator(const Tokenizer& tokenizer, const value_type* iter)
            : mTokenizer(&tokenizer)
            , mIter(iter)
            , mEnd(0)
        {
            FindNextToken();
        }

        iterator& operator++()
        {
            mIter = mEnd;
            FindNextToken();
            return *this;
        }

        bool operator!=(const iterator& other) const
        {
            return mIter != other.mIter;
        }

        const Tokenizer* mTokenizer;
        const value_type* mIter;
        const value_type* mEnd;
        StringType mToken;

    private:
        const value_type* SkipSeperators(const value_type* iter) const
        {
            while (iter != mTokenizer->mString.end())
            {
                int i = 0;
                while (i < mTokenizer->mSeperators.size())
                {
                    if (*iter == mTokenizer->mSeperators[i])
                    {
                        break;
                    }
                    i++;
                }
                if (i == mTokenizer->mSeperators.size())
                {
                    break;
                }
                iter++;
            }
            return iter;
        }

        void FindNextToken()
        {
            mIter = SkipSeperators(mIter);
            mEnd = FindSeperator(mIter);
            mToken = StringType(mIter, mEnd);
        }
        const value_type* FindSeperator(const value_type* iter) const
        {
            while (iter != mTokenizer->mString.end())
            {
                int i = 0;
                while (i < mTokenizer->mSeperators.size())
                {
                    if (*iter == mTokenizer->mSeperators[i])
                    {
                        break;
                    }
                    i++;
                }
                if (i != mTokenizer->mSeperators.size())
                {
                    break;
                }
                iter++;
            }
            return iter;
        }

    };

    Tokenizer(const StringType& string, const StringType& seperators)
        : mString(string)
        , mSeperators(seperators)
    {
    }

    iterator begin() const { return iterator(*this, mString.begin()); }
    iterator end() const { return iterator(*this, mString.end()); }

    StringType mString;
    StringType mSeperators;
};

#endif // _NLTOKENIZER_H_
