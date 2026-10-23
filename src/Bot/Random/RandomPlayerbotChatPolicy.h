/*
 * Pure rules for random-bot chat: German keyword intents, sentence templates,
 * reply chances and typing delays, and the small JSON/HTTP helpers the local
 * AI client needs. The placeholder names follow mod-playerbots' chat texts;
 * the AI part is written from scratch. Released under GNU GPL v2 or any later
 * version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_CHAT_POLICY_H
#define CATA_RANDOM_PLAYERBOT_CHAT_POLICY_H

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace RandomPlayerbotChatPolicy
{
// Typing delay of a reply.
constexpr uint32_t ReplyMinMs = 3000;
constexpr uint32_t ReplyMaxMs = 10000;
constexpr uint32_t ReplyCombatExtraMs = 5000;
// A human counts as "writing" this long after his last message; bots only talk among themselves then.
constexpr uint32_t HumanActiveMs = 180000;
// Bot-to-bot exchanges after one human line.
constexpr uint32_t MaxBotExchanges = 2;
// Remembered lines per conversation, for the AI prompt.
constexpr std::size_t HistoryLines = 5;
// Longest chat line a bot sends (the client limit is 255 bytes).
constexpr std::size_t MaxLineBytes = 200;

enum class Source : uint8_t { Whisper, Party, Say, Channel };

enum class Intent : uint8_t
{
    None, Greeting, Farewell, Thanks, WhereWhat, LevelClass, Invite, LookingForGroup, Help, BotQuestion,
    Insult, Praise, Laugh, Yes, No, Question
};

inline char const* IntentName(Intent intent)
{
    switch (intent)
    {
        case Intent::Greeting: return "greeting";
        case Intent::Farewell: return "farewell";
        case Intent::Thanks: return "thanks";
        case Intent::WhereWhat: return "where";
        case Intent::LevelClass: return "level";
        case Intent::Invite: return "invite";
        case Intent::LookingForGroup: return "lfg";
        case Intent::Help: return "help";
        case Intent::BotQuestion: return "bot";
        case Intent::Insult: return "insult";
        case Intent::Praise: return "praise";
        case Intent::Laugh: return "laugh";
        case Intent::Yes: return "yes";
        case Intent::No: return "no";
        case Intent::Question: return "question";
        default: return "none";
    }
}

// Lower case for ASCII and the German umlauts in UTF-8.
inline std::string Lower(std::string text)
{
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if (c >= 'A' && c <= 'Z')
            text[i] = char(c - 'A' + 'a');
        else if (c == 0xC3 && i + 1 < text.size())
        {
            unsigned char n = static_cast<unsigned char>(text[i + 1]);
            if (n == 0x84 || n == 0x96 || n == 0x9C) // Ä Ö Ü
                text[i + 1] = char(n + 0x20);
            ++i;
        }
    }
    return text;
}

// Words of a lowered message, joined as " w1 w2 ... " so phrases match on word borders.
inline std::string Words(std::string const& lowered)
{
    std::string out = " ";
    bool inWord = false;
    for (char ch : lowered)
    {
        unsigned char c = static_cast<unsigned char>(ch);
        bool letter = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c >= 0x80;
        if (letter)
        {
            out += ch;
            inWord = true;
        }
        else if (inWord)
        {
            out += ' ';
            inWord = false;
        }
    }
    if (inWord)
        out += ' ';
    return out;
}

inline uint32_t WordCount(std::string const& words)
{
    uint32_t count = 0;
    for (std::size_t i = 1; i < words.size(); ++i)
        if (words[i] == ' ')
            ++count;
    return count;
}

inline bool HasAny(std::string const& words, std::initializer_list<char const*> phrases)
{
    for (char const* phrase : phrases)
        if (words.find(std::string(" ") + phrase + " ") != std::string::npos)
            return true;
    return false;
}

// True if the first word (or the first two) is one of the phrases.
inline bool StartsWithAny(std::string const& words, std::initializer_list<char const*> phrases, uint32_t first = 1)
{
    std::size_t end = 0;
    for (uint32_t i = 0; i < first && end != std::string::npos; ++i)
        end = words.find(' ', end + 1);
    std::string head = end == std::string::npos ? words : words.substr(0, end + 1);
    return HasAny(head, phrases);
}

inline bool IsGreeting(std::string const& words)
{
    return StartsWithAny(words, { "hi", "hallo", "hey", "moin", "moinsen", "servus", "huhu", "nabend", "abend", "tach",
        "hallöchen", "hello", "yo", "na", "grüß", "grüss", "gruß", "gude", "morgen", "mahlzeit", "hiho", "huhu", "salut",
        "tag", "grüezi", "hoi", "guten" });
}

struct Understanding
{
    Intent Kind = Intent::None;
    bool Greeted = false;
};

inline Understanding Classify(std::string const& message)
{
    std::string lowered = Lower(message);
    std::string words = Words(lowered);
    uint32_t count = WordCount(words);
    bool asks = lowered.find('?') != std::string::npos;
    Understanding result;
    result.Greeted = IsGreeting(words);
    auto is = [&](Intent kind) { result.Kind = kind; return result; };

    if (HasAny(words, { "inv", "invite", "einladen", "einladung", "lad mich", "lade mich", "komm mit", "kommst du mit",
        "willst du mit", "mitkommen", "gruppe mit", "zusammen questen", "zusammen spielen", "zusammen leveln", "inviten",
        "grp", "party mit" }))
        return is(Intent::Invite);
    if (HasAny(words, { "bot", "bots", "ki", "npc", "computer", "mensch", "echter spieler", "programm" }))
        return is(Intent::BotQuestion);
    if (HasAny(words, { "lfg", "lfm", "suche gruppe", "sucht gruppe", "dungeon", "dungeonbrowser", "ini", "instanz",
        "inis", "heiler gesucht", "tank gesucht", "brauchen tank", "brauchen heiler", "brauche tank", "brauche heiler" }))
        return is(Intent::LookingForGroup);
    if (HasAny(words, { "wo bist", "wo steckst", "wo treibst", "was machst", "was treibst", "was tust", "was geht",
        "wo bist du", "was machste", "wat machst", "beschäftigt", "wohin", "wo gehts", "was gibts", "was so los",
        "wie gehts", "wie geht", "wie läufts", "alles klar", "alles gut", "na wie" }))
        return is(Intent::WhereWhat);
    if (HasAny(words, { "level", "lvl", "stufe", "welche klasse", "was spielst", "was bist du", "klasse", "skillung",
        "spec", "geskillt", "gear", "ausrüstung" }))
        return is(Intent::LevelClass);
    if (HasAny(words, { "hilfe", "help", "helfen", "hilf", "hilfst", "unterstützung", "kannst du mir", "zeig mir" }))
        return is(Intent::Help);
    if (HasAny(words, { "danke", "thx", "ty", "dank", "dankeschön", "merci", "thanks", "vielen dank", "dankö", "thanx" }))
        return is(Intent::Thanks);
    if (HasAny(words, { "bye", "tschüss", "tschüs", "tschau", "ciao", "cya", "bis dann", "bis später", "bis morgen",
        "gn8", "gute nacht", "machs gut", "byebye", "baba", "ade", "afk" }))
        return is(Intent::Farewell);
    if (HasAny(words, { "noob", "nub", "idiot", "depp", "trottel", "opfer", "lappen", "versager", "nervst", "nervig",
        "halt die klappe", "halts maul", "klappe", "dumm", "blöd", "doof", "lauch", "scheiß", "kacke", "mies" }))
        return is(Intent::Insult);
    if (HasAny(words, { "gut gemacht", "nice", "gg", "super", "toll", "stark", "geil", "wp", "well played", "respekt",
        "top", "klasse gemacht", "cool", "krass", "nett", "gz", "grats", "glückwunsch", "gratuliere", "perfekt" }))
        return is(Intent::Praise);
    if (result.Greeted && count <= 3 && !asks)
        return is(Intent::Greeting);
    if (HasAny(words, { "lol", "haha", "hahaha", "xd", "rofl", "lmao", "hehe", "hihi", "lul", "kek" }))
        return is(Intent::Laugh);
    if (count <= 4 && HasAny(words, { "ja", "jo", "jup", "jap", "yes", "klar", "ok", "okay", "jep", "gerne", "gern",
        "sicher", "safe", "logo", "yep", "jawohl", "passt" }))
        return is(Intent::Yes);
    if (count <= 4 && HasAny(words, { "nein", "nö", "nee", "no", "nope", "später", "keine lust", "kein bock", "lieber nicht" }))
        return is(Intent::No);
    if (result.Greeted)
        return is(Intent::Greeting);
    if (asks || StartsWithAny(words, { "wer", "wie", "was", "wann", "warum", "wieso", "weshalb", "welche", "welcher",
        "kannst", "hast", "bist", "weißt", "kennst", "gibts", "gibt" }))
        return is(Intent::Question);
    return result;
}

// True if the message names the bot (whole word, case-insensitive).
inline bool Mentions(std::string const& message, std::string const& name)
{
    if (name.empty())
        return false;
    return Words(Lower(message)).find(" " + Lower(name) + " ") != std::string::npos;
}

using Values = std::map<std::string, std::string>;

// Replaces %placeholders (letters and underscores) and a bare %s by their values.
// Returns nothing if a placeholder has no value, so the caller picks another line.
inline std::optional<std::string> Fill(std::string const& line, Values const& values)
{
    std::string out;
    for (std::size_t i = 0; i < line.size(); ++i)
    {
        if (line[i] != '%')
        {
            out += line[i];
            continue;
        }
        std::size_t end = i + 1;
        while (end < line.size() && ((line[end] >= 'a' && line[end] <= 'z') || line[end] == '_'))
            ++end;
        std::string key = line.substr(i + 1, end - i - 1);
        if (key.empty())
        {
            out += '%';
            continue;
        }
        auto it = values.find(key);
        if (it == values.end() || it->second.empty())
            return std::nullopt;
        out += it->second;
        i = end - 1;
    }
    return out;
}

inline uint32_t Hash(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

inline uint32_t Spread(uint32_t seed, uint32_t event, uint32_t lo, uint32_t hi)
{
    return hi <= lo ? lo : lo + Hash(seed, event) % (hi - lo + 1);
}

// Typing delay: a short reaction plus about 60 ms per letter, later during a fight.
inline uint32_t ReplyDelayMs(uint32_t seed, uint32_t event, std::size_t replyBytes, bool fighting)
{
    uint32_t ms = Spread(seed, event, 2000, 4500) + uint32_t(std::min<std::size_t>(replyBytes, 120) * 50);
    ms = std::clamp(ms, ReplyMinMs, ReplyMaxMs);
    return fighting ? ms + ReplyCombatExtraMs : ms;
}

// Percent chance that a bot answers a human line.
inline uint32_t ReplyChance(Source source, bool mentioned, Intent intent, bool onlyBotsAround)
{
    switch (source)
    {
        case Source::Whisper:
            return 100;
        case Source::Party:
            if (mentioned)
                return 100;
            if (intent == Intent::Greeting || intent == Intent::Farewell || intent == Intent::Thanks)
                return onlyBotsAround ? 70 : 40;
            return onlyBotsAround ? 45 : 20;
        case Source::Say:
            if (mentioned)
                return 100;
            return intent == Intent::Greeting || intent == Intent::Question ? 40 : 20;
        case Source::Channel:
            if (mentioned)
                return 80;
            return intent == Intent::Question || intent == Intent::LookingForGroup || intent == Intent::Help ? 10 : 3;
    }
    return 0;
}

// Percent chance that another bot picks up a bot's line, for a given exchange depth.
inline uint32_t FollowUpChance(uint32_t depth) { return depth >= MaxBotExchanges ? 0 : depth == 0 ? 30 : 15; }

// Bots only talk among themselves while a human writes.
inline bool HumanActive(uint32_t now, uint32_t lastHumanMs, bool everWrote)
{
    return everWrote && now - lastHumanMs <= HumanActiveMs;
}

// Sliding one-minute window for the AI budget.
struct RateWindow
{
    std::deque<uint32_t> Times;
    bool Allow(uint32_t now, uint32_t perMinute)
    {
        while (!Times.empty() && now - Times.front() >= 60000)
            Times.pop_front();
        if (Times.size() >= perMinute)
            return false;
        Times.push_back(now);
        return true;
    }
};

// ---- UTF-8 and reply cleanup ----

// Cuts at most `limit` bytes without splitting a UTF-8 character.
inline std::string CutUtf8(std::string const& text, std::size_t limit)
{
    if (text.size() <= limit)
        return text;
    std::size_t end = limit;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80)
        --end;
    return text.substr(0, end);
}

inline std::string Trim(std::string const& text)
{
    std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return {};
    std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

// Drops emoji and pictographs (4-byte UTF-8, U+2600-U+27BF, variation selectors) and control characters.
inline std::string StripSymbols(std::string const& text)
{
    std::string out;
    for (std::size_t i = 0; i < text.size();)
    {
        unsigned char c = static_cast<unsigned char>(text[i]);
        std::size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        if (i + len > text.size())
            break;
        bool drop = len == 4 || (c < 0x20 && c != '\t') || c == 0x7F || c == '|';
        if (len == 3)
        {
            unsigned char n = static_cast<unsigned char>(text[i + 1]);
            if (c == 0xE2 && (n == 0x98 || n == 0x99 || n == 0x9A || n == 0x9B || n == 0x9C || n == 0x9D || n == 0x9E))
                drop = true;
            if (c == 0xEF && n == 0xB8) // variation selectors
                drop = true;
            if (c == 0xE2 && n == 0x80 && static_cast<unsigned char>(text[i + 2]) == 0x8D) // zero width joiner
                drop = true;
        }
        if (len == 1 && (c & 0x80))
            drop = true; // stray continuation byte
        if (!drop)
            out.append(text, i, len);
        i += len;
    }
    return out;
}

// Makes one in-game line out of a model answer, or returns empty if it must not be said.
inline std::string CleanReply(std::string raw, std::string const& botName)
{
    // Reasoning models put their thoughts first.
    for (std::size_t open; (open = raw.find("<think>")) != std::string::npos;)
    {
        std::size_t close = raw.find("</think>", open);
        raw.erase(open, close == std::string::npos ? std::string::npos : close + 8 - open);
    }
    // First non-empty line.
    std::string line;
    std::size_t start = 0;
    while (start <= raw.size())
    {
        std::size_t end = raw.find('\n', start);
        std::string part = Trim(raw.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!part.empty())
        {
            line = part;
            break;
        }
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    // "Name: text" or "[Name]: text".
    for (std::string prefix : { botName + ":", "[" + botName + "]:", "[" + botName + "]", botName + " sagt:" })
        if (!botName.empty() && Lower(line).rfind(Lower(prefix), 0) == 0)
            line = Trim(line.substr(prefix.size()));
    // *grinst* and similar stage directions.
    for (std::size_t open; (open = line.find('*')) != std::string::npos;)
    {
        std::size_t close = line.find('*', open + 1);
        if (close == std::string::npos)
        {
            line.erase(open, 1);
            continue;
        }
        line.erase(open, close + 1 - open);
    }
    line = Trim(StripSymbols(line));
    // Surrounding quotes, also German ones.
    for (int pass = 0; pass < 2; ++pass)
    {
        if (line.size() >= 2 && line.front() == '"' && line.back() == '"')
            line = Trim(line.substr(1, line.size() - 2));
        if (line.rfind("\xE2\x80\x9E", 0) == 0) // „
            line = Trim(line.substr(3));
        if (line.size() >= 3 && line.compare(line.size() - 3, 3, "\xE2\x80\x9C") == 0) // “
            line = Trim(line.substr(0, line.size() - 3));
    }
    std::string words = Words(Lower(line));
    if (line.empty() || HasAny(words, { "ki", "sprachmodell", "assistent", "chatbot", "language model", "ai", "openai",
        "als bot", "ein bot", "rollenspiel", "systemprompt", "prompt" }))
        return {};
    if (line.size() > MaxLineBytes)
    {
        std::string cut = CutUtf8(line, MaxLineBytes);
        std::size_t stop = cut.find_last_of(".!?");
        std::size_t space = cut.find_last_of(' ');
        if (stop != std::string::npos && stop > MaxLineBytes / 2)
            cut = cut.substr(0, stop + 1);
        else if (space != std::string::npos && space > MaxLineBytes / 2)
            cut = cut.substr(0, space);
        line = Trim(cut);
    }
    return line;
}

// ---- JSON and HTTP for the local AI ----

inline std::string JsonEscape(std::string const& text)
{
    std::string out;
    static char const* hex = "0123456789abcdef";
    for (char ch : text)
    {
        unsigned char c = static_cast<unsigned char>(ch);
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20)
                {
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 15];
                }
                else
                    out += ch;
        }
    }
    return out;
}

inline void AppendUtf8(std::string& out, uint32_t cp)
{
    if (cp < 0x80)
        out += char(cp);
    else if (cp < 0x800)
    {
        out += char(0xC0 | (cp >> 6));
        out += char(0x80 | (cp & 0x3F));
    }
    else if (cp < 0x10000)
    {
        out += char(0xE0 | (cp >> 12));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    }
    else
    {
        out += char(0xF0 | (cp >> 18));
        out += char(0x80 | ((cp >> 12) & 0x3F));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    }
}

// Reads the JSON string that starts at the quote at `pos`; nothing if it is malformed.
inline std::optional<std::string> JsonStringAt(std::string const& json, std::size_t pos)
{
    if (pos >= json.size() || json[pos] != '"')
        return std::nullopt;
    std::string out;
    auto hex4 = [&](std::size_t at) -> std::optional<uint32_t>
    {
        if (at + 4 > json.size())
            return std::nullopt;
        uint32_t v = 0;
        for (std::size_t k = at; k < at + 4; ++k)
        {
            char h = json[k];
            v <<= 4;
            if (h >= '0' && h <= '9') v |= uint32_t(h - '0');
            else if (h >= 'a' && h <= 'f') v |= uint32_t(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') v |= uint32_t(h - 'A' + 10);
            else return std::nullopt;
        }
        return v;
    };
    for (std::size_t i = pos + 1; i < json.size(); ++i)
    {
        char c = json[i];
        if (c == '"')
            return out;
        if (c != '\\')
        {
            out += c;
            continue;
        }
        if (++i >= json.size())
            return std::nullopt;
        switch (json[i])
        {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u':
            {
                auto cp = hex4(i + 1);
                if (!cp)
                    return std::nullopt;
                i += 4;
                uint32_t code = *cp;
                if (code >= 0xD800 && code <= 0xDBFF && i + 6 < json.size() && json[i + 1] == '\\' && json[i + 2] == 'u')
                    if (auto low = hex4(i + 3); low && *low >= 0xDC00 && *low <= 0xDFFF)
                    {
                        code = 0x10000 + ((code - 0xD800) << 10) + (*low - 0xDC00);
                        i += 6;
                    }
                AppendUtf8(out, code);
                break;
            }
            default:
                return std::nullopt;
        }
    }
    return std::nullopt;
}

// The string value of the first `"key":` after `from`.
inline std::optional<std::string> JsonField(std::string const& json, std::string const& key, std::size_t from = 0)
{
    std::size_t at = json.find("\"" + key + "\"", from);
    if (at == std::string::npos)
        return std::nullopt;
    at = json.find(':', at + key.size() + 2);
    if (at == std::string::npos)
        return std::nullopt;
    at = json.find_first_not_of(" \t\r\n", at + 1);
    if (at == std::string::npos)
        return std::nullopt;
    return JsonStringAt(json, at);
}

// The assistant text of an Ollama /api/chat answer (non-streaming).
inline std::optional<std::string> OllamaContent(std::string const& body)
{
    std::size_t message = body.find("\"message\"");
    if (message == std::string::npos)
        return std::nullopt;
    return JsonField(body, "content", message);
}

struct ChatTurn
{
    bool FromBot = false; // assistant if true, user otherwise
    std::string Text;
};

inline std::string ChatRequestJson(std::string const& model, std::string const& system, std::vector<ChatTurn> const& turns,
    uint32_t maxTokens, float temperature, std::string const& keepAlive)
{
    std::string json = "{\"model\":\"" + JsonEscape(model) + "\",\"stream\":false,\"keep_alive\":\"" +
        JsonEscape(keepAlive) + "\",\"options\":{\"num_predict\":" + std::to_string(maxTokens) +
        ",\"temperature\":" + std::to_string(temperature).substr(0, 4) + "},\"messages\":[{\"role\":\"system\",\"content\":\"" +
        JsonEscape(system) + "\"}";
    for (ChatTurn const& turn : turns)
        json += std::string(",{\"role\":\"") + (turn.FromBot ? "assistant" : "user") + "\",\"content\":\"" +
            JsonEscape(turn.Text) + "\"}";
    return json + "]}";
}

struct HttpResponse
{
    int Status = 0;
    std::string Body;
};

// Splits a raw HTTP/1.x response; decodes a chunked body.
inline std::optional<HttpResponse> ParseHttp(std::string const& raw)
{
    if (raw.rfind("HTTP/1.", 0) != 0)
        return std::nullopt;
    std::size_t space = raw.find(' ');
    std::size_t headerEnd = raw.find("\r\n\r\n");
    if (space == std::string::npos || headerEnd == std::string::npos)
        return std::nullopt;
    HttpResponse response;
    response.Status = std::atoi(raw.c_str() + space + 1);
    std::string headers = Lower(raw.substr(0, headerEnd));
    std::string body = raw.substr(headerEnd + 4);
    if (headers.find("transfer-encoding: chunked") == std::string::npos)
    {
        response.Body = body;
        return response;
    }
    std::size_t at = 0;
    while (at < body.size())
    {
        std::size_t lineEnd = body.find("\r\n", at);
        if (lineEnd == std::string::npos)
            return std::nullopt;
        std::size_t size = std::strtoul(body.substr(at, lineEnd - at).c_str(), nullptr, 16);
        if (size == 0)
            return response;
        if (lineEnd + 2 + size > body.size())
            return std::nullopt;
        response.Body.append(body, lineEnd + 2, size);
        at = lineEnd + 2 + size + 2;
    }
    return std::nullopt;
}

// ---- Persona and prompt ----

inline char const* Persona(uint32_t seed)
{
    static char const* const personas[] = {
        "locker und freundlich, schreibt kurz und meist klein",
        "etwas mürrisch, aber hilfsbereit, wenig Worte",
        "gut gelaunt, macht gern kleine Witze",
        "ehrgeizig, redet gern über Ausrüstung und Dungeons",
        "entspannter Gelegenheitsspieler nach Feierabend",
        "neugierig und gesprächig, fragt gern zurück",
        "trocken und sarkastisch, aber nie gemein",
        "hilfsbereiter Veteran, gibt gern kurze Tipps",
        "verpeilt, vertippt sich manchmal",
        "sammelt gern Mounts und Haustiere, schwärmt davon",
    };
    return personas[Hash(seed, 77) % (sizeof(personas) / sizeof(personas[0]))];
}

struct Facts
{
    std::string Name;
    std::string Class;
    std::string Race;
    std::string Role;
    std::string Zone;
    std::string Activity;
    std::string Partner; // who the bot talks to
    uint32_t Level = 0;
    bool InGroupWithPartner = false;
    bool Whisper = false;
};

inline std::string SystemPrompt(Facts const& facts, uint32_t seed)
{
    std::string prompt =
        "Du bist " + facts.Name + ", ein Spieler in World of Warcraft (Cataclysm) auf einem deutschen Server. "
        "Du spielst einen " + facts.Race + " " + facts.Class + " auf Stufe " + std::to_string(facts.Level) +
        " (Rolle: " + facts.Role + "). Du bist gerade in " + facts.Zone + " und " + facts.Activity + ". "
        "Deine Art: " + Persona(seed) + ". ";
    prompt += facts.Whisper ? "Gerade flüstert dir " + facts.Partner + " privat. " :
        facts.InGroupWithPartner ? "Du bist mit " + facts.Partner + " in einer Gruppe und ihr schreibt im Gruppenchat. " :
        "Du schreibst im öffentlichen Chat mit " + facts.Partner + ". ";
    prompt +=
        "Antworte mit genau einer kurzen Chatzeile auf Deutsch, so wie echte Spieler im Spiel tippen: locker, kurz, "
        "gern Abkürzungen wie lvl, ini, thx, sry, kp. Höchstens 20 Wörter. Keine Emojis, keine Sternchen-Aktionen, "
        "keine Anführungszeichen, kein Name vor der Antwort. Sag nie, dass du ein Bot, eine KI oder ein Programm bist. "
        "Erfinde keine Gegenstände, Orte oder Quests, die du nicht kennst; bleib bei dem, was oben steht.";
    return prompt;
}
}

#endif
