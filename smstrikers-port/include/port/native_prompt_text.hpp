#pragma once

#include "NL/nlLocalization.h"
#include "port/endian.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace port { namespace prompttext {
// Only these confirmed retail instructions differ from the native PAD mapping.
// Preserve the language, every surrounding word/escape, and all other LOC IDs.
struct Rule
{
    std::uint32_t hash;
    const char16_t* from;
    const char16_t* to;
};
static constexpr Rule kRules[7][2] = {
    {{0xaccb0957u, u"THE A BUTTON", u"THE @ BUTTON"},
     {0x9f9d0419u, u"the C Stick", u"the right stick"}},
    {{0xaccb0957u, u"BOUTON A", u"BOUTON @"},
     {0x9f9d0419u, u"le Stick C", u"le stick droit"}},
    {{0xaccb0957u, u"A-KNOPF", u"@-KNOPF"},
     {0x9f9d0419u, u"den C-Stick", u"den rechten Stick"}},
    {{0xaccb0957u, u"BOTÓN A", u"BOTÓN @"},
     {0x9f9d0419u, u"el Stick C", u"el stick derecho"}},
    {{0xaccb0957u, u"PULSANTE A", u"PULSANTE @"},
     {0x9f9d0419u, u"lo Stick C", u"lo stick destro"}},
    {{0xaccb0957u, u"Aボタン", u"@ボタン"},
     {0x9f9d0416u, u"Zで切り替えられる", u"SELECTで切り替えられる"}},
    {{0xaccb0957u, u"THE A BUTTON", u"THE @ BUTTON"},
     {0x9f9d0419u, u"the C Stick", u"the right stick"}}
};
static_assert(nlLocalization::LangEnglish == 0 && nlLocalization::LangFrench == 1 &&
              nlLocalization::LangGerman == 2 && nlLocalization::LangSpanish == 3 &&
              nlLocalization::LangItalian == 4 && nlLocalization::LangJapanese == 5 &&
              nlLocalization::LangUKEnglish == 6 && sizeof(char16_t) == sizeof(u16),
              "Prompt rules must follow the LOC language/UTF-16 contract");

struct Change
{
    const Rule* rule = nullptr;
    std::uint32_t entry = 0, source = 0, length = 0, at = 0;
    std::uint32_t fromWords = 0, toWords = 0;
};
struct Plan
{
    Change changes[2]{};
    std::uint32_t count = 0, extraWords = 0;
};
inline std::uint32_t length(const char16_t* text)
{
    std::uint32_t n = 0;
    while (text[n]) ++n;
    return n;
}

// Called after Load proves the table and all string extents. No heap or engine
// state is used. Unknown payloads/duplicate IDs or matches keep their original.
inline Plan plan(nlLocalization::nlLanguage language, const u8* lookup,
                 std::uint32_t count, const u8* strings, std::size_t words)
{
    Plan result;
    if (static_cast<unsigned>(language) >= 7 || words > UINT32_MAX)
        return result;
    for (const auto& rule : kRules[static_cast<unsigned>(language)])
    {
        std::uint32_t index = 0, occurrences = 0;
        for (std::uint32_t i = 0; i < count; ++i)
            if (port_be32(lookup + std::size_t(i) * sizeof(nlLocalization::StringLookup)) == rule.hash)
            {
                index = i;
                if (++occurrences > 1) break;
            }
        if (occurrences != 1) continue;
        const auto offset = port_be32(lookup + std::size_t(index) * sizeof(nlLocalization::StringLookup) + 4);
        if (offset >= words) continue;
        std::size_t size = 0;
        while (size < words - offset && port_be16(strings + (offset + size) * 2)) ++size;
        if (size == words - offset) continue;
        const auto fromWords = length(rule.from), toWords = length(rule.to);
        if (size < fromWords) continue;
        std::uint32_t at = 0, matches = 0;
        for (std::size_t p = 0; p <= size - fromWords; ++p)
        {
            bool equal = true;
            for (std::uint32_t c = 0; c < fromWords; ++c)
                if (port_be16(strings + (offset + p + c) * 2) != rule.from[c])
                { equal = false; break; }
            if (equal)
            {
                at = static_cast<std::uint32_t>(p);
                if (++matches > 1) break;
            }
        }
        if (matches != 1) continue;
        const std::uint64_t appended = std::uint64_t(size) - fromWords + toWords + 1;
        if (appended > UINT32_MAX - std::uint64_t(words) - result.extraWords)
            return Plan{}; // Preserve the original table if appended offsets cannot fit.
        auto& change = result.changes[result.count++];
        change = { &rule, index, offset, static_cast<std::uint32_t>(size), at, fromWords, toWords };
        result.extraWords += static_cast<std::uint32_t>(appended);
    }
    return result;
}

// All destination data is appended inside the one final LOC allocation. Old
// strings (including shared offsets) are immutable and lookup ordering is kept.
inline void apply(const Plan& plan, nlLocalization::StringLookup* lookup,
                  u16* strings, std::size_t originalWords)
{
    std::size_t tail = originalWords;
    for (std::uint32_t i = 0; i < plan.count; ++i)
    {
        const auto& change = plan.changes[i];
        const auto* source = strings + change.source;
        auto* target = strings + tail;
        std::memcpy(target, source, std::size_t(change.at) * sizeof(u16));
        for (std::uint32_t c = 0; c < change.toWords; ++c)
            target[change.at + c] = static_cast<u16>(change.rule->to[c]);
        std::memcpy(target + change.at + change.toWords, source + change.at + change.fromWords,
                    std::size_t(change.length - change.at - change.fromWords + 1) * sizeof(u16));
        lookup[change.entry].StringOffset = static_cast<std::uint32_t>(tail);
        tail += change.length - change.fromWords + change.toWords + 1;
    }
}
}} // namespace port::prompttext
