/*
 * Random population policy, adapted from mod-playerbots RandomPlayerbotFactory /
 * RandomPlayerbotMgr at d750b4b. See AUTHORS.md and PORTING.md.
 * Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_POLICY_H
#define CATA_RANDOM_PLAYERBOT_POLICY_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

// Hard upper bound for one realm's random population; accounts are created on demand.
constexpr uint32_t MaxRandomPlayerbots = 2000;
// Native account names are at most 16 characters (MAX_ACCOUNT_STR).
constexpr std::size_t MaxRandomPlayerbotPrefix = 10;

struct RandomPlayerbotSettings
{
    bool Enabled = false;
    uint32_t Count = 0;
    std::string AccountPrefix = "RNDBOT";
    uint32_t LoginPerInterval = 10;
    uint32_t IntervalMs = 20000;
    uint32_t AllianceRatio = 50; // percent
    uint32_t ClassMask = 0;      // bit (class - 1); 0 means every class in RandomPlayerbotClasses
};

// Uppercase Latin letters and digits only, so the generated names survive native normalization.
inline std::optional<std::string> NormalizeRandomPlayerbotPrefix(std::string const& prefix)
{
    if (prefix.empty() || prefix.size() > MaxRandomPlayerbotPrefix)
        return std::nullopt;
    std::string result;
    for (char c : prefix)
    {
        if (c >= 'a' && c <= 'z')
            c = char(c - 'a' + 'A');
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            return std::nullopt;
        result.push_back(c);
    }
    return result;
}

// Index is 1-based. Width grows past 999 without exceeding the native name length.
inline std::string RandomPlayerbotAccountName(std::string const& prefix, uint32_t index)
{
    char digits[8];
    std::snprintf(digits, sizeof(digits), "%03u", unsigned(index));
    return prefix + digits;
}

inline RandomPlayerbotSettings ClampRandomPlayerbotSettings(RandomPlayerbotSettings settings)
{
    settings.Count = std::min(settings.Count, MaxRandomPlayerbots);
    settings.LoginPerInterval = std::clamp<uint32_t>(settings.LoginPerInterval, 1, 100);
    settings.IntervalMs = std::clamp<uint32_t>(settings.IntervalMs, 1000, 600000);
    settings.AllianceRatio = std::min<uint32_t>(settings.AllianceRatio, 100);
    return settings;
}

// How many offline bots may be admitted this interval.
inline uint32_t RandomPlayerbotLoginBudget(uint32_t online, uint32_t loading, uint32_t ready, uint32_t target, uint32_t perInterval)
{
    uint32_t active = online + loading;
    if (active >= target)
        return 0;
    return std::min({target - active, perInterval, ready});
}

// Playable races without phased starting areas (Worgen 22 and Goblin 9 come later).
constexpr std::array<uint8_t, 5> RandomPlayerbotAllianceRaces = {1, 3, 4, 7, 11};
constexpr std::array<uint8_t, 5> RandomPlayerbotHordeRaces = {2, 5, 6, 8, 10};
// Classes with a combat AI: Warrior, Mage, Priest, Rogue, Hunter, Warlock, Paladin, Shaman, Druid.
constexpr std::array<uint8_t, 9> RandomPlayerbotClasses = {1, 8, 5, 4, 3, 9, 2, 7, 11};

// "4, 3,9" -> class mask. Unknown or unsupported ids are skipped; empty means all.
inline uint32_t ParseRandomPlayerbotClasses(std::string const& text)
{
    uint32_t mask = 0, value = 0;
    bool digits = false;
    auto flush = [&]()
    {
        if (digits && std::find(RandomPlayerbotClasses.begin(), RandomPlayerbotClasses.end(), value) !=
            RandomPlayerbotClasses.end())
            mask |= 1u << (value - 1);
        value = 0;
        digits = false;
    };
    for (char c : text)
    {
        if (c >= '0' && c <= '9')
        {
            value = std::min<uint32_t>(value * 10 + uint32_t(c - '0'), 100);
            digits = true;
        }
        else
            flush();
    }
    flush();
    return mask;
}

inline bool RandomPlayerbotClassEnabled(uint32_t classMask, uint8_t classId)
{
    return !classMask || (classMask & (1u << (classId - 1)));
}

struct RandomPlayerbotProfile
{
    uint8_t Race;
    uint8_t Class;
    uint8_t Gender;
};

// Roll returns a uniformly distributed value in [0, bound). The caller filters
// pairs without native creation data and retries with fresh rolls.
template<class Roll>
RandomPlayerbotProfile RollRandomPlayerbotProfile(uint32_t allianceRatio, Roll roll, uint32_t classMask = 0)
{
    bool alliance = roll(100u) < allianceRatio;
    uint8_t race = alliance ? RandomPlayerbotAllianceRaces[roll(uint32_t(RandomPlayerbotAllianceRaces.size()))] :
        RandomPlayerbotHordeRaces[roll(uint32_t(RandomPlayerbotHordeRaces.size()))];
    std::array<uint8_t, RandomPlayerbotClasses.size()> enabled{};
    uint32_t count = 0;
    for (uint8_t classId : RandomPlayerbotClasses)
        if (RandomPlayerbotClassEnabled(classMask, classId))
            enabled[count++] = classId;
    if (!count)
    {
        enabled = RandomPlayerbotClasses;
        count = uint32_t(enabled.size());
    }
    uint8_t classId = enabled[roll(count)];
    return {race, classId, uint8_t(roll(2u))};
}

// Two or three syllables, 4 to 12 letters, first letter uppercase. Native name
// checks (reserved, profanity, taken) stay authoritative.
template<class Roll>
std::string RollRandomPlayerbotName(Roll roll)
{
    static constexpr std::array<char const*, 24> Starts = {"Al", "Bar", "Cor", "Dra", "El", "Fen", "Gor", "Hal",
        "Ith", "Jor", "Kel", "Lor", "Mar", "Nor", "Or", "Per", "Quel", "Ral", "Sar", "Tor", "Ul", "Val", "Wyn", "Zar"};
    static constexpr std::array<char const*, 16> Middles = {"a", "e", "i", "o", "u", "ae", "ia", "or",
        "an", "el", "is", "un", "ar", "en", "ir", "os"};
    static constexpr std::array<char const*, 20> Ends = {"dor", "wen", "mir", "thas", "rin", "dan", "goth", "lia",
        "nor", "ric", "wyn", "ius", "mar", "ros", "tan", "vek", "zul", "dris", "las", "mon"};
    std::string name = Starts[roll(uint32_t(Starts.size()))];
    if (roll(2u))
        name += Middles[roll(uint32_t(Middles.size()))];
    name += Ends[roll(uint32_t(Ends.size()))];
    if (name.size() > 12)
        name.resize(12);
    return name;
}

#endif
