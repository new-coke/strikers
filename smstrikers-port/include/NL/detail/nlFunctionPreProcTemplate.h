/* Intentionally no include guard: nlFunction.h includes this generator once
 * for each supported arity and once for the Function<T> shorthand. */

#ifdef NLF_GENERATE_PRIMARY_WRAPPER

template <typename P1>
class Function : public Function1<void, P1>
{
    typedef Function1<void, P1> Base;

public:
    Function()
        : Base()
    {
    }

    Function(void (*function)(P1))
        : Base(function)
    {
    }

    template <typename Callable>
    Function(Callable callable)
        : Base(callable)
    {
    }

    operator bool() const
    {
        return this->mTag != EMPTY;
    }

    Function& operator=(const Function& other)
    {
        Base::operator=(other);
        return *this;
    }
};

#else

#ifndef NLF_CLASS
#error NLF_CLASS must name the generated FunctionN class
#endif

template <NLF_TEMPLATE_PARAMETERS>
class NLF_CLASS
{
#if NLF_ARITY == 0
    friend class Function<FnVoidVoid>;
#elif NLF_ARITY == 1
    friend class Function<P1>;
#endif

public:
    struct FunctorBase
    {
        virtual ~FunctorBase() { }
        virtual ReturnType operator()(NLF_PARAMETER_DECLARATIONS) = 0;
        virtual FunctorBase* Clone() const = 0;
    };

    template <typename Callable>
    struct FunctorImpl : public FunctorBase
    {
    private:
        Callable mFunctor;

    public:
        FunctorImpl(const Callable& callable);

        /* No explicit destructor. The original uses the implicit virtual
         * destructor generated from FunctorBase. */

        virtual ReturnType operator()(NLF_PARAMETER_DECLARATIONS);
        virtual FunctorBase* Clone() const;

    private:
        ReturnType Call(BoolToType<false> NLF_COMMA_PARAMETER_DECLARATIONS)
        {
            return mFunctor(NLF_ARGUMENT_NAMES);
        }

        void Call(BoolToType<true> NLF_COMMA_PARAMETER_DECLARATIONS)
        {
            mFunctor(NLF_ARGUMENT_NAMES);
        }
    };

    NLF_CLASS()
        : mTag(EMPTY)
    {
    }

    NLF_CLASS(ReturnType (*function)(NLF_PARAMETER_TYPES))
        : mTag(FREE_FUNCTION)
        , mFreeFunction(function)
    {
    }

    template <typename Callable>
    NLF_CLASS(const Callable& callable)
        : mTag(FUNCTOR)
    {
        typedef FunctorImpl<Callable> Impl;
        mFunctor = new (nlMalloc(sizeof(Impl), 8, false)) Impl(callable);
    }

    NLF_CLASS(const NLF_CLASS& other)
        : mTag(other.mTag)
    {
        if (mTag == FREE_FUNCTION)
        {
            mFreeFunction = other.mFreeFunction;
        }
        else if (mTag == FUNCTOR)
        {
            mFunctor = other.mFunctor->Clone();
        }
    }

    ~NLF_CLASS()
    {
        Clear();
    }

    NLF_CLASS& operator=(const NLF_CLASS& other)
    {
        Clear();
        mTag = other.mTag;
        if (mTag == FREE_FUNCTION)
        {
            mFreeFunction = other.mFreeFunction;
        }
        else if (mTag == FUNCTOR)
        {
            mFunctor = other.mFunctor->Clone();
        }
        return *this;
    }

    NLF_CLASS& operator=(ReturnType (*function)(NLF_PARAMETER_TYPES))
    {
        Clear();
        mTag = FREE_FUNCTION;
        mFreeFunction = function;
        return *this;
    }

    bool Empty() const
    {
        return mTag == EMPTY;
    }

    void Clear()
    {
        if (mTag == FUNCTOR)
        {
            // PORT: paired with the placement new over nlMalloc above, rather than relying on ::operator delete being nlFree.
            mFunctor->~FunctorBase();
            nlFree(mFunctor);
        }
        mTag = EMPTY;
    }

    operator bool() const
    {
        return mTag != EMPTY;
    }

    ReturnType (*GetFreeFunction() const)(NLF_PARAMETER_TYPES)
    {
        return mTag == FREE_FUNCTION ? mFreeFunction : NULL;
    }

    void SetFreeFunction(ReturnType (*function)(NLF_PARAMETER_TYPES))
    {
        mTag = FREE_FUNCTION;
        mFreeFunction = function;
    }

    ReturnType operator()(NLF_PARAMETER_DECLARATIONS) const
    {
        if (mTag == FREE_FUNCTION)
        {
            return mFreeFunction(NLF_ARGUMENT_NAMES);
        }
        return (*mFunctor)(NLF_ARGUMENT_NAMES);
    }

private:
    Tag mTag;
    union
    {
        ReturnType (*mFreeFunction)(NLF_PARAMETER_TYPES);
        FunctorBase* mFunctor;
    };
};

template <NLF_TEMPLATE_PARAMETERS>
template <typename Callable>
inline NLF_CLASS<NLF_TEMPLATE_ARGUMENTS>::FunctorImpl<Callable>::FunctorImpl(const Callable& callable)
    : mFunctor(callable)
{
}

template <NLF_TEMPLATE_PARAMETERS>
template <typename Callable>
inline ReturnType NLF_CLASS<NLF_TEMPLATE_ARGUMENTS>::FunctorImpl<Callable>::operator()(NLF_PARAMETER_DECLARATIONS)
{
    return Call(BoolToType<IsVoid<ReturnType>::value>() NLF_COMMA_ARGUMENT_NAMES);
}

template <NLF_TEMPLATE_PARAMETERS>
template <typename Callable>
inline typename NLF_CLASS<NLF_TEMPLATE_ARGUMENTS>::FunctorBase*
NLF_CLASS<NLF_TEMPLATE_ARGUMENTS>::FunctorImpl<Callable>::Clone() const
{
    return new (nlMalloc(sizeof(FunctorImpl), 8, false)) FunctorImpl(*this);
}

#if NLF_ARITY == 0
template <>
class Function<FnVoidVoid> : public Function0<void>
{
    typedef Function0<void> Base;

public:
    Function()
        : Base()
    {
    }

    Function(void (*function)())
        : Base(function)
    {
    }

    template <typename Callable>
    Function(Callable callable)
        : Base(callable)
    {
    }

    operator bool() const
    {
        return this->mTag != EMPTY;
    }

    template <typename Other>
    Function& operator=(const Other& other)
    {
        Base::operator=(other);
        return *this;
    }
};
#elif NLF_ARITY == 1
template <NLF_TEMPLATE_PARAMETERS>
class Function<ReturnType(P1)> : public NLF_CLASS<NLF_TEMPLATE_ARGUMENTS>
#elif NLF_ARITY == 3
template <NLF_TEMPLATE_PARAMETERS>
class Function<ReturnType(P1, P2, P3)> : public NLF_CLASS<NLF_TEMPLATE_ARGUMENTS>
#else
#error Unsupported nlFunction arity
#endif
#if NLF_ARITY != 0
{
    typedef NLF_CLASS<NLF_TEMPLATE_ARGUMENTS> Base;

public:
    Function()
        : Base()
    {
    }

    Function(ReturnType(*function)(NLF_PARAMETER_TYPES))
        : Base(function)
    {
    }

    template <typename Callable>
    Function(Callable callable)
        : Base(callable)
    {
    }

    Function& operator=(const Function& other)
    {
        Base::operator=(other);
        return *this;
    }
};
#endif

#endif
