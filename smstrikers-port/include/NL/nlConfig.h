#ifndef _NLCONFIG_H_
#define _NLCONFIG_H_

#include "types.h"
#include "NL/nlLexicalCast.h"
#include "NL/nlBasicString.h"

enum Type
{
    _BOOL = 0,
    _INT = 1,
    _FLOAT = 2,
    _STRING = 3,
};

union Value
{
    /* 0x0 */ const char* s;
    /* 0x0 */ int i;
    /* 0x0 */ bool b;
    /* 0x0 */ float f;
};

class Config
{
public:
    struct TagValuePair
    {
        TagValuePair()
        {
            tag = NULL;
        }

        template <typename T>
        T Get() const
        {
            if (type == _BOOL)
            {
                return LexicalCast<T, bool>(value.b);
            }
            else if (type == _INT)
            {
                return LexicalCast<T, int>(value.i);
            }
            else if (type == _FLOAT)
            {
                return LexicalCast<T, float>(value.f);
            }
            else if (type == _STRING)
            {
                return LexicalCast<T, const char*>(value.s);
            }
            return T();
        }

        /* 0x00 */ const char* tag;
        /* 0x04 */ Type type;
        /* 0x08 */ Value value;
    };

    class IteratorBase
    {
    public:
        const char* Tag() const;
        bool IsValid() const;
        void Next();
        IteratorBase(Config&, Type);

    protected:
        TagValuePair* Current() const { return mCurrentTvp; }

    private:
        /* 0x0 */ TagValuePair* mCurrentTvp;
        /* 0x4 */ TagValuePair* mLastTvp;
        /* 0x8 */ Type mType;
    };

    template <typename T>
    class Iterator : public IteratorBase
    {
    public:
        Iterator(Config& config)
            : IteratorBase(config, GetType())
        {
        }

        T Current() const
        {
            return IteratorBase::Current()->template Get<T>();
        }

    private:
        Type GetType()
        {
            return GetType(T());
        }

        Type GetType(
            const BasicString<char, Detail::TempStringAllocator>& value)
        {
            return _STRING;
        }

        Type GetType(const char* value) { return _STRING; }
        Type GetType(int value) { return _INT; }
        Type GetType(unsigned char value) { return _BOOL; }
        Type GetType(float value) { return _FLOAT; }
    };

    enum AllocateWhere
    {
        ALLOCATE_LOW = 0,
        ALLOCATE_HIGH = 1,
    };

    struct Parser
    {
        virtual void EmptyLine() { }
        virtual void Comment(const BasicString<char, Detail::TempStringAllocator>&) { }
        virtual void Section(const BasicString<char, Detail::TempStringAllocator>&) { }
        virtual void TagValuePair(const BasicString<char, Detail::TempStringAllocator>&, const BasicString<char, Detail::TempStringAllocator>&) { }
    }; // total size: 0x4

    void Parse(const char* s, Parser& parser);
    static BasicString<char, Detail::TempStringAllocator> LoadFileAsString(const char* filename);
    void Set(const char* key, const BasicString<char, Detail::TempStringAllocator>& value);
    void Set(const char* tag, const char* value);
    void Set(const char* tag, float value);
    void Set(const char* tag, bool value);
    void Set(const char* tag, int value);
    char* CopyString(const char* str, bool makeUpperCase);
    u32 Hash(const char* str) const;

    template <typename T>
    void Set(const char* key, T value);
    TagValuePair& FindTvp(const char* tag);
    bool IsBool(const char* str, bool& b) const;
    bool Exists(const char* tag) const;
    void LoadFromFile(const char* filename);
    void LoadFromString(const char*);
    void ApplyToFile(const char*, const char*);

    template <typename T>
    T Get(const char* key, T defaultValue)
    {
        TagValuePair& tvp = FindTvp(key);
        if (tvp.tag == NULL)
        {
            Set(key, defaultValue);
            return defaultValue;
        }
        return tvp.Get<T>();
    }

    static Config& Global();

    ~Config();
    Config() { };
    Config(AllocateWhere allocateWhere);

    /* 0x0 */ TagValuePair* mTvpHash;
    /* 0x4 */ char* mStringMemory;
    /* 0x8 */ char* mStringEnd;
}; // total size: 0xC

inline Config::IteratorBase::IteratorBase(Config& config, Type type)
    : mCurrentTvp(config.mTvpHash)
    , mLastTvp(config.mTvpHash + 1024)
    , mType(type)
{
    while ((mCurrentTvp->tag == NULL || mCurrentTvp->type != mType)
        && mCurrentTvp < mLastTvp)
    {
        ++mCurrentTvp;
    }
}

inline bool Config::IteratorBase::IsValid() const
{
    return mCurrentTvp < mLastTvp;
}

inline void Config::IteratorBase::Next()
{
    ++mCurrentTvp;
    while ((mCurrentTvp->tag == NULL || mCurrentTvp->type != mType)
        && mCurrentTvp < mLastTvp)
    {
        ++mCurrentTvp;
    }
}

inline const char* Config::IteratorBase::Tag() const
{
    return mCurrentTvp->tag;
}

typedef Config::TagValuePair TagValuePair;

template <>
inline BasicString<char, Detail::TempStringAllocator> Config::Get<BasicString<char, Detail::TempStringAllocator> >(
    const char* key, BasicString<char, Detail::TempStringAllocator> defaultValue)
{
    TagValuePair& tvp = FindTvp(key);
    if (tvp.tag == NULL)
    {
        Set(key, defaultValue);
        BasicString<char, Detail::TempStringAllocator>::Data* data = defaultValue.mData;
        if (data != 0)
        {
            data->mRefCount++;
        }
        else
        {
            data = 0;
        }
        return BasicString<char, Detail::TempStringAllocator>(data);
    }
    return tvp.Get<BasicString<char, Detail::TempStringAllocator> >();
}

inline float GetConfigFloat(Config& cfg, const char* key, float defaultValue)
{
    TagValuePair& tvp = cfg.FindTvp(key);
    if (tvp.tag == NULL)
    {
        cfg.Set(key, defaultValue);
        return defaultValue;
    }

    float val;
    if (tvp.type == _BOOL)
    {
        val = LexicalCast<float, bool>(tvp.value.b);
    }
    else if (tvp.type == _INT)
    {
        val = LexicalCast<float, int>(tvp.value.i);
    }
    else if (tvp.type == _FLOAT)
    {
        val = LexicalCast<float, float>(tvp.value.f);
    }
    else if (tvp.type == _STRING)
    {
        val = LexicalCast<float, const char*>(tvp.value.s);
    }
    else
    {
        val = 0.f;
    }
    return val;
}

inline s32 GetConfigInt(Config& cfg, const char* key, s32 defaultValue)
{
    TagValuePair& tvp = cfg.FindTvp(key);
    if (tvp.tag == NULL)
    {
        cfg.Set(key, (int)defaultValue);
        return defaultValue;
    }

    int val;
    if (tvp.type == _BOOL)
    {
        val = LexicalCast<int, bool>(tvp.value.b);
    }
    else if (tvp.type == _INT)
    {
        val = LexicalCast<int, int>(tvp.value.i);
    }
    else if (tvp.type == _FLOAT)
    {
        val = LexicalCast<int, float>(tvp.value.f);
    }
    else if (tvp.type == _STRING)
    {
        val = LexicalCast<int, const char*>(tvp.value.s);
    }
    else
    {
        val = 0;
    }
    return val;
}

inline bool GetConfigBool(Config& cfg, const char* key, bool defaultValue)
{
    TagValuePair& tvp = cfg.FindTvp(key);
    bool val;
    if (tvp.tag == NULL)
    {
        cfg.Set(key, defaultValue);
        return defaultValue;
    }

    if (tvp.type == _BOOL)
    {
        val = LexicalCast<bool, bool>(tvp.value.b);
    }
    else if (tvp.type == _INT)
    {
        val = LexicalCast<bool, int>(tvp.value.i);
    }
    else if (tvp.type == _FLOAT)
    {
        val = LexicalCast<bool, float>(tvp.value.f);
    }
    else if (tvp.type == _STRING)
    {
        val = LexicalCast<bool, const char*>(tvp.value.s);
    }
    else
    {
        val = 0.f;
    }
    return val;
}

#endif // _NLCONFIG_H_
