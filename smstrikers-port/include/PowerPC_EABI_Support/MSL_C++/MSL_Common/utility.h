#ifndef MSL_UTILITY_H_
#define MSL_UTILITY_H_

namespace std
{
template <class T1, class T2>
struct pair
{
    T1 first;
    T2 second;

    pair()
        : first(T1())
    {
        second = T2();
    }
    pair(const T1& f, const T2& s)
        : first(f)
        , second(s)
    {
    }
};
} // namespace std

namespace Metrowerks
{
namespace details
{

template <class First, class Second, int tag>
class compressed_pair_imp
{
    First first_;
    Second second_;

public:
    compressed_pair_imp() { }
    compressed_pair_imp(const First& first)
        : first_(first)
        , second_()
    {
    }
    compressed_pair_imp(const First& first, const Second& second)
        : first_(first)
        , second_(second)
    {
    }

    First& first() { return first_; }
    Second& second() { return second_; }
};

template <class First, class Second>
class compressed_pair_imp<First, Second, 1> : private First
{
    Second second_;

public:
    compressed_pair_imp()
        : First()
        , second_()
    {
    }
    compressed_pair_imp(const Second& second)
        : First()
        , second_(second)
    {
    }

    inline First& first();
    Second& second() { return second_; }
};

template <class First, class Second>
inline First& compressed_pair_imp<First, Second, 1>::first()
{
    return *this;
}

} // namespace details
} // namespace Metrowerks

#endif
