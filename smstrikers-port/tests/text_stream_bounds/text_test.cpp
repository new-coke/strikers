#include "fixture_support.hpp"
#include "copy_sites.inc"

static std::vector<unsigned short> bounded_text(size_t count)
{
    std::vector<unsigned short> text(count);
    for (size_t i = 0; i < count; ++i)
        text[i] = static_cast<unsigned short>('A' + i % 23);
    return text;
}

template <class Site>
static void initialize(Site& site)
{
    std::memset(&site, 0xa5, sizeof site);
    site.before = UINT64_C(0x1029384756abcdef);
    site.after = UINT64_C(0xfedcba6547382910);
}

template <class Site>
static void verify_copy(Site& site, int row, const WideBasicString& source,
                        const std::vector<unsigned short>& value, TLTextInstance& text)
{
    unsigned char before[sizeof(Site)];
    std::memcpy(before, &site, sizeof site);
    unsigned short* destination = site.buffer(row);
    const size_t capacity = site.capacity(row);
    const size_t offset = reinterpret_cast<unsigned char*>(destination)
        - reinterpret_cast<unsigned char*>(&site);
    assert(capacity > 0 && offset + capacity * sizeof(*destination) <= sizeof site);
    const size_t priorLive = liveCount;
    site.copy(source, row);
    assert(liveCount == priorLive);
    // The next owner-publication operation uses the unmodified real SetString body.
    text.m_pFontString = reinterpret_cast<FontCharString*>(uintptr_t(16));
    text.m_OverloadFlags = 0xaf;
    text.SetString(destination);
    assert(text.m_wcUserString == destination && text.m_pFontString == nullptr);
    assert(text.m_OverloadFlags == 0xa7);
    const size_t copied = value.size() < capacity ? value.size() : capacity - 1;
    for (size_t i = 0; i < copied; ++i) assert(destination[i] == value[i]);
    assert(destination[copied] == 0 && destination[capacity - 1] == 0);
    assert(nlStrLen(destination) == copied);
    const unsigned char* now = reinterpret_cast<const unsigned char*>(&site);
    for (size_t i = 0; i < sizeof site; ++i)
        if (i < offset || i >= offset + capacity * sizeof(*destination))
            assert(now[i] == before[i]);
    assert(source.size() == static_cast<int>(value.size()));
    for (size_t i = 0; i < value.size(); ++i) assert(source.c_str()[i] == value[i]);
    assert(source.c_str()[value.size()] == 0);
}

template <class Site>
static void copy_case(Site& site, int row, const char* name, const char* suffix,
                      const std::vector<unsigned short>& value, bool defaultEmpty)
{
    case_begin(name, suffix);
    TLTextInstance text{};
    const size_t beforeLive = liveCount;
    {
        WideBasicString source;
        if (defaultEmpty)
        {
            assert(value.empty() && source.mData == nullptr);
        }
        else
        {
            const unsigned short empty[] = {0};
            const unsigned short* start = value.empty() ? empty : value.data();
            source = WideBasicString(start, start + value.size());
            assert(source.mData != nullptr);
            assert(source.mData->mData.mCapacity == static_cast<int>(value.size() + 1));
        }
        verify_copy(site, row, source, value, text);
    }
    assert(liveCount == beforeLive);
    assert(text.m_wcUserString == site.buffer(row));
    const size_t capacity = site.capacity(row);
    const size_t copied = value.size() < capacity ? value.size() : capacity - 1;
    for (size_t i = 0; i < copied; ++i) assert(text.m_wcUserString[i] == value[i]);
    assert(text.m_wcUserString[copied] == 0);
    case_pass();
}

template <class Site>
static void format_case(Site& site, int row, const char* name)
{
    case_begin(name, "format");
    const unsigned short pattern[] = {'C','u','p',' ','{','0','}',':',' ','{','1','}',0};
    const unsigned short captain[] = {0x30eb,0x30a4,0x30fc,0x30b8,0};
    const unsigned short rank[] = {'3',0};
    const std::vector<unsigned short> expected = {'C','u','p',' ',0x30eb,0x30a4,0x30fc,0x30b8,':',' ','3'};
    const size_t beforeLive = liveCount;
    TLTextInstance text{};
    {
        const WideBasicString source = Format(WideBasicString(pattern), captain, rank);
        verify_copy(site, row, source, expected, text);
    }
    assert(liveCount == beforeLive);
    for (size_t i = 0; i < expected.size(); ++i) assert(text.m_wcUserString[i] == expected[i]);
    assert(text.m_wcUserString[expected.size()] == 0);
    case_pass();
}

template <class Site>
static void run_copy_site(const char* name, int rows)
{
    Site site;
    for (int row = 0; row < rows; row += rows > 1 ? rows - 1 : 1)
    {
        char rowName[140];
        std::snprintf(rowName, sizeof rowName, "%s.row%d", name, row);
        const size_t capacity = site.capacity(row);
        initialize(site); copy_case(site, row, rowName, "default-empty", {}, true);
        initialize(site); copy_case(site, row, rowName, "allocated-empty", {}, false);
        initialize(site); copy_case(site, row, rowName, "short", {0x30eb, 0x00e8}, false);
        initialize(site); copy_case(site, row, rowName, "exact-fit", bounded_text(capacity - 1), false);
        initialize(site); copy_case(site, row, rowName, "at-capacity", bounded_text(capacity), false);
        initialize(site); copy_case(site, row, rowName, "long", bounded_text(capacity + 17), false);
        copy_case(site, row, rowName, "reuse-empty", {}, true);
        initialize(site); format_case(site, row, rowName);
    }
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    if (std::strcmp(argv[1], "all") == 0)
    {
#include "run_sites.inc"
        assert(completedCases == 200);
    }
    else if (std::strcmp(argv[1], "empty-witness") == 0)
    {
        CopySite7 site;
        initialize(site);
        copy_case(site, 4, "BraggingRights:664", "default-empty", {}, true);
    }
    else if (std::strcmp(argv[1], "short-witness") == 0)
    {
        CopySite7 site;
        initialize(site);
        copy_case(site, 4, "BraggingRights:664", "short", {0x30eb, 0x00e8}, false);
    }
    else
        return 2;
    assert(!openCase && liveCount == 0);
    std::printf("{\"event\":\"group-pass\",\"cases\":%u,\"remaining_allocations\":%zu}\n",
                completedCases, liveCount);
    return 0;
}
