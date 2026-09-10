#include "NL/nlConfig.h"
#include "Game/Sys/debug.h"
#include "NL/nlFileGC.h"
#include "NL/nlTokenizer.h"
#include <ctype.h>

/**
 * Offset/Address/Size: 0x0 | 0x801D2C64 | size: 0x13EC
 */
typedef BasicString<char, Detail::TempStringAllocator> BString;
typedef Tokenizer<BString> BTokenizer;

/**
 * Offset/Address/Size: 0x0 | 0x801D2C64 | size: 0x13EC
 */
void Config::Parse(const char* s, Config::Parser& parser)
{
    BTokenizer split(BString(s), BString("\n\r"));

    for (BTokenizer::iterator iter = split.begin(); iter != split.end(); ++iter)
    {
        BString line = iter.mToken;
        line.TrimInPlace(" \t\n");

        int lineLen = line.mData ? line.mData->mData.mSize - 1 : 0;
        if (lineLen == 0)
        {
            parser.EmptyLine();
            continue;
        }

        if (line[0] == '#')
        {
            parser.Comment(line);
            continue;
        }

        if (line[0] == '[')
        {
            if (line[(line.mData ? line.mData->mData.mSize - 1 : 0) - 1] == ']')
            {
                char sectionMarkers[3] = "[]";
                line.TrimInPlace(sectionMarkers);
                parser.Section(line);
                continue;
            }
        }

        BTokenizer tagValuePair(line, BString("="));
        BString tag;
        BString value;
        int numTokens = 0;

        for (BTokenizer::iterator jter = tagValuePair.begin(); jter != tagValuePair.end(); ++jter)
        {
            if (numTokens == 0)
            {
                tag = jter.mToken.Trim(" \t\"\r");
            }
            if (numTokens == 1)
            {
                value = jter.mToken.Trim(" \t\"\r");

                for (int i = 0; i < (int)(value.mData ? value.mData->mData.mSize - 1 : 0); i++)
                {
                    if (value[i] == '#')
                    {
                        value[i] = '\0';
                        value = BString(value.c_str());
                        value.TrimInPlace(" \t\"\r");
                    }
                }
            }
            numTokens++;
        }

        if (numTokens < 2)
        {
            continue;
        }
        parser.TagValuePair(tag, value);
    }
}

static inline void InitFileStringData(BString::Data* data, char* buffer, char* end)
{
    if (data != 0)
    {
        s32 size = end - buffer + 1;
        // PORT: through the allocator that will free it.
        data->mData.mData = Detail::TempStringAllocator::New<char>(size, 0);
        data->mData.mSize = size;
        data->mData.mCapacity = size;

        for (s32 i = 0; i < end - buffer + 1; i++)
        {
            data->mData.mData[i] = 0;
        }

        data->mRefCount = 1;
        for (s32 i = 0; i < data->mData.mSize - 1; i++)
        {
            data->mData.mData[i] = buffer[i];
        }
    }
}

static inline BString::Data* NewFileStringData(char* begin, char* end)
{
    BString::Data* data = (BString::Data*)nlMalloc(sizeof(BString::Data), 8, true);
    InitFileStringData(data, begin, end);
    return data;
}

/**
 * Offset/Address/Size: 0x13EC | 0x801D4050 | size: 0x21C
 */
BasicString<char, Detail::TempStringAllocator> Config::LoadFileAsString(const char* filename)
{
    tDebugPrintManager::Print(DC_CONFIG_SYSTEM, "reading config file: %s\n", filename);
    nlFlushFileCash();

    unsigned long fileSize = 0;
    char* buffer = (char*)nlLoadEntireFile(filename, &fileSize, 0x20, AllocateEnd);
    if (buffer != 0)
    {
        BasicString<char, Detail::TempStringAllocator> s(NewFileStringData(buffer, buffer + fileSize));
        nlFree(buffer);
        nlFlushFileCash();
        return s;
    }

    return BasicString<char, Detail::TempStringAllocator>();
}

/**
 * Offset/Address/Size: 0x1608 | 0x801D426C | size: 0x54
 */
void Config::Set(const char* key, const BasicString<char, Detail::TempStringAllocator>& value)
{
    Set(key, value.c_str());
}

static inline bool IsIntValue(const char* str, int& out)
{
    const char* s = str;
    while (*s != 0)
    {
        char c = *s;
        if (!isdigit(c) && c != '-')
        {
            return false;
        }
        s++;
    }

    out = LexicalCast<int, const char*>(str);
    return true;
}

static inline bool IsFloatValue(const char* str, float& out)
{
    bool seenPeriod = false;
    const char* s = str;
    while (*s != 0)
    {
        char c = *s;
        if (c == '.' || c == ',')
        {
            seenPeriod = true;
        }
        else if (!isdigit(c) && c != '-')
        {
            return false;
        }
        s++;
    }

    out = LexicalCast<float, const char*>(str);
    return seenPeriod;
}

u32 Config::Hash(const char* str) const
{
    u32 hash = 0x1505;
    while (*str != 0)
    {
        s8 c = (s8)nlToUpper<char>(const_cast<char&>(*str++));
        hash = hash + (hash << 5) + c;
    }
    return hash;
}

char* Config::CopyString(const char* str, bool makeUpperCase)
{
    const char* ret = mStringEnd;
    while (*str != 0)
    {
        if (mStringEnd - mStringMemory >= 0x27FF)
        {
            return (char*)ret;
        }

        if (makeUpperCase)
        {
            *mStringEnd = nlToUpper<char>(const_cast<char&>(*str));
        }
        else
        {
            *mStringEnd = *str;
        }
        str++;
        mStringEnd++;
    }

    *mStringEnd = 0;
    mStringEnd++;
    return (char*)ret;
}

static inline TagValuePair* FindConfigTvp(Config* config, const char* tag)
{
    u32 idx = config->Hash(tag) & 0x3FF;
    while (true)
    {
        if (config->mTvpHash[idx].tag == NULL || nlStrICmp(config->mTvpHash[idx].tag, tag) == 0)
        {
            return &config->mTvpHash[idx];
        }
        idx++;
        idx &= 0x3FF;
    }
}

/**
 * Offset/Address/Size: 0x165C | 0x801D42C0 | size: 0x56C
 */
void Config::Set(const char* tag, const char* value)
{
    TagValuePair* tvp;
    bool b = false;
    float f = 0.0f;
    int i = 0;

    if (IsIntValue(value, i))
    {
        tvp = FindConfigTvp(this, tag);
        tvp->type = _INT;
        tvp->value.i = i;

        if (tvp->tag == NULL)
        {
            tvp->tag = CopyString(tag, true);
        }
    }
    else if (IsFloatValue(value, f))
    {
        tvp = FindConfigTvp(this, tag);
        tvp->type = _FLOAT;
        tvp->value.f = f;

        if (tvp->tag == NULL)
        {
            tvp->tag = CopyString(tag, true);
        }
    }
    else if (IsBool(value, b))
    {
        const bool boolValue = b;
        tvp = FindConfigTvp(this, tag);
        tvp->type = _BOOL;
        tvp->value.b = boolValue;

        if (tvp->tag == NULL)
        {
            tvp->tag = CopyString(tag, true);
        }
    }
    else
    {
        tvp = FindConfigTvp(this, tag);
        tvp->type = _STRING;
        tvp->value.s = CopyString(value, false);

        if (tvp->tag == NULL)
        {
            tvp->tag = CopyString(tag, true);
        }
    }
}

static inline u32 ConfigHashFloat(const char* tag)
{
    const char* p = tag;
    u32 hash = 0x1505;
    while (*p != 0)
    {
        s8 c = (s8)nlToUpper<char>(const_cast<char&>(*p++));
        hash = (hash << 5) + hash + c;
    }
    return hash;
}

static inline TagValuePair& FindTvpFloat(Config* config, const char* tag)
{
    u32 idx = ConfigHashFloat(tag) & 0x3FF;
    while (true)
    {
        if (config->mTvpHash[idx].tag == NULL || nlStrICmp(config->mTvpHash[idx].tag, tag) == 0)
        {
            return config->mTvpHash[idx];
        }
        idx++;
        idx &= 0x3FF;
    }
}

static inline char* CopyConfigString(Config* config, const char* str)
{
    char* dest = config->mStringEnd;
    while (*str != 0)
    {
        if (config->mStringEnd - config->mStringMemory >= 0x27FF)
        {
            return dest;
        }
        *config->mStringEnd = nlToUpper<char>(const_cast<char&>(*str));
        str++;
        config->mStringEnd++;
    }
    *config->mStringEnd = 0;
    config->mStringEnd++;
    return dest;
}

/**
 * Offset/Address/Size: 0x1BC8 | 0x801D482C | size: 0x12C
 */
void Config::Set(const char* tag, float value)
{
    TagValuePair& tvp = FindTvpFloat(this, tag);
    tvp.type = _FLOAT;
    tvp.value.f = value;

    if (tvp.tag == NULL)
    {
        tvp.tag = CopyConfigString(this, tag);
    }
}

static inline TagValuePair& FindTvpChar(Config* config, const char* tag)
{
    unsigned int i = config->Hash(tag) & 0x3FF;

    while (true)
    {
        if (config->mTvpHash[i].tag == NULL || nlStrICmp(config->mTvpHash[i].tag, tag) == 0)
        {
            return config->mTvpHash[i];
        }
        i++;
        i &= 0x3FF;
    }
}

/**
 * Offset/Address/Size: 0x1CF4 | 0x801D4958 | size: 0x120
 */
void Config::Set(const char* tag, bool value)
{
    TagValuePair& tvp = FindTvpChar(this, tag);
    tvp.type = _BOOL;
    tvp.value.b = value;

    if (tvp.tag == 0)
    {
        tvp.tag = CopyConfigString(this, tag);
    }
}

/**
 * Offset/Address/Size: 0x1E14 | 0x801D4A78 | size: 0x120
 */
void Config::Set(const char* tag, int value)
{
    TagValuePair& tvp = FindTvpChar(this, tag);
    tvp.type = _INT;
    tvp.value.i = value;

    if (tvp.tag == NULL)
    {
        tvp.tag = CopyConfigString(this, tag);
    }
}

static inline u32 ConfigHash(const char* str)
{
    u32 hash = 0x1505;
    while (*str != 0)
    {
        s8 c = (s8)nlToUpper<char>(const_cast<char&>(*str++));
        hash = hash + (hash << 5) + c;
    }
    return hash;
}

/**
 * Offset/Address/Size: 0x1F34 | 0x801D4B98 | size: 0xB8
 */
TagValuePair& Config::FindTvp(const char* tag)
{
    unsigned int i = ConfigHash(tag) & 0x3FF;

    while (true)
    {
        if (mTvpHash[i].tag == NULL || nlStrICmp(mTvpHash[i].tag, tag) == 0)
        {
            return mTvpHash[i];
        }
        i++;
        i &= 0x3FF;
    }
}

/**
 * Offset/Address/Size: 0x1FEC | 0x801D4C50 | size: 0x534
 */
bool Config::IsBool(const char* str, bool& b) const
{
    BasicString<char, Detail::TempStringAllocator> s(str);
    for (int i = 0; i < (s.mData ? s.mData->mData.mSize - 1 : 0); i++)
    {
        s[i] = _tolower(s[i]);
    }
    if (s == "true" || s == "yes" || s == "on" || s == "enable")
    {
        b = true;
        return true;
    }
    if (s == "false" || s == "no" || s == "off" || s == "disable")
    {
        b = false;
        return false;
    }
    return false;
}

/**
 * Offset/Address/Size: 0x2520 | 0x801D5184 | size: 0xC8
 */
bool Config::Exists(const char* tag) const
{
    const char* p = tag;
    u32 hash = 0x1505;
    while (*p != 0)
    {
        s32 c = (s8)nlToUpper<char>(const_cast<char&>(*p++));
        u32 h = hash;
        hash = h + (h << 5) + c;
    }
    u32 startIdx = hash & 0x3FF;

    TagValuePair* tvpHash = mTvpHash;
    u32 idx = startIdx;
    do
    {
        if (tvpHash[idx].tag == NULL)
        {
            return false;
        }
        if (nlStrICmp(tvpHash[idx].tag, tag) == 0)
        {
            return true;
        }
        idx++;
        idx &= 0x3FF;
    } while (idx != startIdx);
    return false;
}

struct SetTagValuePair : public Config::Parser
{
    /* 0x04 */ BasicString<char, Detail::TempStringAllocator> mCurrentSection;
    /* 0x08 */ Config& mConfig;
    /* 0x0C */ bool mTweaked;
    /* 0x10 */ float mTweakMinValue;
    /* 0x14 */ float mTweakMaxValue;
    /* 0x18 */ float mTweakIncrement;

    SetTagValuePair(Config& cfg)
        : mConfig(cfg)
        , mTweaked(false)
    {
    }

    virtual inline void Comment(const BasicString<char, Detail::TempStringAllocator>&);
    virtual inline void Section(const BasicString<char, Detail::TempStringAllocator>&);
    virtual inline void TagValuePair(
        const BasicString<char, Detail::TempStringAllocator>&, const BasicString<char, Detail::TempStringAllocator>&);
}; // total size: 0x1C

/**
 * Offset/Address/Size: 0x25E8 | 0x801D524C | size: 0x138
 */
void Config::LoadFromFile(const char* filename)
{
    const BasicString<char, Detail::TempStringAllocator>& s = LoadFileAsString(filename);
    const char* str = s.c_str();
    SetTagValuePair stvp(*this);
    Parse(str, stvp);
}

/**
 * Offset/Address/Size: 0x2834 | 0x801D5498 | size: 0xCC
 */
Config::Config(Config::AllocateWhere allocateWhere)
{
#pragma defer_codegen on
    if (allocateWhere == ALLOCATE_LOW)
    {
        mTvpHash = new (nlMalloc(sizeof(TagValuePair) * 1024, 8, false)) TagValuePair[1024];
        mStringMemory = (char*)nlMalloc(0x2800, 8, false);
    }
    else
    {
        mTvpHash = new (nlMalloc(sizeof(TagValuePair) * 1024, 0x20, true)) TagValuePair[1024];
        mStringMemory = (char*)nlMalloc(0x2800, 0x20, true);
    }
    mStringEnd = mStringMemory;
    mStringMemory[0x27FF] = '\0';
}

/**
 * Offset/Address/Size: 0x27CC | 0x801D5430 | size: 0x68
 */
Config::~Config()
{
    if (mTvpHash != NULL)
    {
        ::operator delete[](mTvpHash);
    }

    ::operator delete[](mStringMemory);
}

/**
 * Offset/Address/Size: 0x2720 | 0x801D5384 | size: 0xAC
 */
Config& Config::Global()
{
    static Config sGlobal(ALLOCATE_LOW);
    return sGlobal;
}
#pragma defer_codegen off

/**
 * Offset/Address/Size: 0x2900 | 0x801D5564 | size: 0x4
 */
void SetTagValuePair::Comment(const BasicString<char, Detail::TempStringAllocator>&)
{
}

/**
 * Offset/Address/Size: 0x2904 | 0x801D5568 | size: 0x94
 */
void SetTagValuePair::Section(const BasicString<char, Detail::TempStringAllocator>& section)
{
    mCurrentSection = section;
}

/**
 * Offset/Address/Size: 0x2BCC | 0x801D5830 | size: 0x20
 */
#pragma dont_inline on
template <typename T>
void Config::Set(const char* key, T value)
{
    Set(key, value);
}
template void Config::Set<BasicString<char, Detail::TempStringAllocator> >(const char*, BasicString<char, Detail::TempStringAllocator>);
#pragma dont_inline reset

/**
 * Offset/Address/Size: 0x2998 | 0x801D55FC | size: 0x218
 */
void SetTagValuePair::TagValuePair(const BasicString<char, Detail::TempStringAllocator>& tag,
    const BasicString<char, Detail::TempStringAllocator>& rhs)
{
    BasicString<char, Detail::TempStringAllocator> tagWithSection(tag);

    s32 sectionLen = mCurrentSection.mData ? mCurrentSection.mData->mData.mSize - 1 : 0;
    if (sectionLen > 0)
    {
        tagWithSection = mCurrentSection.Append("/").Append(tag);
    }

    mConfig.Set<BasicString<char, Detail::TempStringAllocator> >(tagWithSection.c_str(), rhs);
}
