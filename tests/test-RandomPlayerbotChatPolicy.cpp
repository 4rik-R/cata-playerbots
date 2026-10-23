/*
 * This file is part of the Cataclysm Playerbots port. See AUTHORS.md.
 * Released under GNU GPL v2 or any later version.
 */
#include "../src/Bot/Random/RandomPlayerbotChatPolicy.h"
#include "../src/Bot/Random/RandomPlayerbotChatTexts.h"
#include <catch2/catch.hpp>

using namespace RandomPlayerbotChatPolicy;

TEST_CASE("German messages are lowered and split into words", "[playerbots][random][chat]")
{
    CHECK(Lower("ÄÖÜ Moin") == "äöü moin");
    CHECK(Words("na, wie gehts?") == " na wie gehts ");
    CHECK(WordCount(Words("hi du da")) == 3);
}

TEST_CASE("Chat intents are recognised from German keywords", "[playerbots][random][chat]")
{
    CHECK(Classify("moin").Kind == Intent::Greeting);
    CHECK(Classify("Servus!").Kind == Intent::Greeting);
    CHECK(Classify("hi, was machst du grad?").Kind == Intent::WhereWhat);
    CHECK(Classify("hi, was machst du grad?").Greeted);
    CHECK(Classify("wo bist du?").Kind == Intent::WhereWhat);
    CHECK(Classify("welches lvl bist du").Kind == Intent::LevelClass);
    CHECK(Classify("inv?").Kind == Intent::Invite);
    CHECK(Classify("Kommst du mit questen?").Kind == Intent::Invite);
    CHECK(Classify("lfg todesminen").Kind == Intent::LookingForGroup);
    CHECK(Classify("bist du ein bot?").Kind == Intent::BotQuestion);
    CHECK(Classify("danke dir").Kind == Intent::Thanks);
    CHECK(Classify("thx").Kind == Intent::Thanks);
    CHECK(Classify("bis später").Kind == Intent::Farewell);
    CHECK(Classify("du noob").Kind == Intent::Insult);
    CHECK(Classify("gut gemacht").Kind == Intent::Praise);
    CHECK(Classify("gz").Kind == Intent::Praise);
    CHECK(Classify("hahaha").Kind == Intent::Laugh);
    CHECK(Classify("ja klar").Kind == Intent::Yes);
    CHECK(Classify("nö").Kind == Intent::No);
    CHECK(Classify("Weißt du wo der Schmied ist").Kind == Intent::Question);
    CHECK(Classify("wetter ist schön heute").Kind == Intent::None);
    // "tag" inside a sentence is no greeting, only at the start.
    CHECK(Classify("jeden tag das gleiche hier").Kind == Intent::None);
    CHECK(Classify("Tag zusammen").Kind == Intent::Greeting);
}

TEST_CASE("A bot hears its name as a whole word", "[playerbots][random][chat]")
{
    CHECK(Mentions("hey Lorros, kommst du?", "Lorros"));
    CHECK(Mentions("LORROS!", "Lorros"));
    CHECK_FALSE(Mentions("Lorrosine ist da", "Lorros"));
    CHECK_FALSE(Mentions("hi", ""));
}

TEST_CASE("Templates are filled or rejected", "[playerbots][random][chat]")
{
    Values values { { "zone_name", "Dämmerwald" }, { "s", "Arik" }, { "my_level", "32" } };
    CHECK(*Fill("bin in %zone_name, lvl %my_level", values) == "bin in Dämmerwald, lvl 32");
    CHECK(*Fill("Was war das, %s?", values) == "Was war das, Arik?");
    CHECK(*Fill("100% sicher", values) == "100% sicher");
    CHECK_FALSE(Fill("Hilfe bei %quest_link?", values));
}

TEST_CASE("Every chat line can be filled with the values the server provides", "[playerbots][random][chat]")
{
    Values values { { "zone_name", "Z" }, { "my_role", "R" }, { "my_race", "V" }, { "my_class", "K" }, { "my_level", "1" },
        { "instance_name", "I" }, { "quest_link", "Q" }, { "category", "C" }, { "item_formatted_link", "L" },
        { "cost_gold", "G" }, { "item_link", "L" }, { "victim_name", "N" }, { "s", "S" }, { "other_name", "O" },
        { "activity", "A" } };
    for (uint8_t i = 0; i < uint8_t(RandomPlayerbotChatTexts::Lines::Count); ++i)
    {
        auto set = RandomPlayerbotChatTexts::Get(RandomPlayerbotChatTexts::Lines(i));
        REQUIRE(set.Size > 0);
        for (uint32_t k = 0; k < set.Size; ++k)
        {
            INFO(set.Items[k]);
            CHECK(Fill(set.Items[k], values));
            CHECK(std::string(set.Items[k]).size() < 200);
        }
    }
}

TEST_CASE("Reply chances and delays", "[playerbots][random][chat]")
{
    CHECK(ReplyChance(Source::Whisper, false, Intent::None, false) == 100);
    CHECK(ReplyChance(Source::Party, true, Intent::None, false) == 100);
    CHECK(ReplyChance(Source::Party, false, Intent::None, true) < 100);
    CHECK(ReplyChance(Source::Channel, false, Intent::None, false) <= 5);
    for (uint32_t seed = 1; seed < 40; ++seed)
    {
        uint32_t ms = ReplyDelayMs(seed, 2, 30, false);
        CHECK(ms >= ReplyMinMs);
        CHECK(ms <= ReplyMaxMs);
        CHECK(ReplyDelayMs(seed, 2, 30, true) == ms + ReplyCombatExtraMs);
    }
    CHECK(FollowUpChance(0) > 0);
    CHECK(FollowUpChance(MaxBotExchanges) == 0);
    CHECK(HumanActive(100000, 50000, true));
    CHECK_FALSE(HumanActive(400000, 50000, true));
    CHECK_FALSE(HumanActive(100000, 50000, false));
}

TEST_CASE("The AI budget is a one-minute window", "[playerbots][random][chat]")
{
    RateWindow window;
    CHECK(window.Allow(1000, 2));
    CHECK(window.Allow(2000, 2));
    CHECK_FALSE(window.Allow(3000, 2));
    CHECK(window.Allow(61001, 2));
}

TEST_CASE("Model answers become one clean chat line", "[playerbots][random][chat]")
{
    CHECK(CleanReply("Lorros: moin, bin in Dämmerwald", "Lorros") == "moin, bin in Dämmerwald");
    CHECK(CleanReply("\"klar, lad ein\"", "Lorros") == "klar, lad ein");
    CHECK(CleanReply("*grinst* jo passt \xF0\x9F\x98\x82", "Lorros") == "jo passt");
    CHECK(CleanReply("<think>hmm</think>\n\nhey du\nzweite Zeile", "Lorros") == "hey du");
    CHECK(CleanReply("Als KI kann ich das nicht", "Lorros").empty());
    CHECK(CleanReply("ich bin nur ein Bot", "Lorros").empty());
    CHECK(CleanReply("   ", "Lorros").empty());
    std::string longLine(150, 'a');
    longLine += ". " + std::string(150, 'b');
    std::string cut = CleanReply(longLine, "x");
    CHECK(cut.size() <= MaxLineBytes);
    CHECK(cut.back() == '.');
    CHECK(CutUtf8("ääää", 3) == "ä");
    CHECK(CleanReply("a|cffff0000rot", "x") == "acffff0000rot");
}

TEST_CASE("JSON for the Ollama chat API", "[playerbots][random][chat]")
{
    CHECK(JsonEscape("a\"b\\c\nd") == "a\\\"b\\\\c\\nd");
    std::string body = "{\"model\":\"gemma3:4b\",\"message\":{\"role\":\"assistant\",\"content\":\"moin \\u00e4\\\"x\\\" \\ud83d\\ude00\"},\"done\":true}";
    auto content = OllamaContent(body);
    REQUIRE(content);
    CHECK(*content == "moin \xC3\xA4\"x\" \xF0\x9F\x98\x80");
    CHECK_FALSE(OllamaContent("{\"error\":\"model not found\"}"));
    CHECK(*JsonField("{\"error\":\"model not found\"}", "error") == "model not found");
    std::string request = ChatRequestJson("gemma3:4b", "sys", { { false, "hi" }, { true, "moin" } }, 60, 0.8f, "30m");
    CHECK(request.find("\"model\":\"gemma3:4b\"") != std::string::npos);
    CHECK(request.find("\"stream\":false") != std::string::npos);
    CHECK(request.find("{\"role\":\"assistant\",\"content\":\"moin\"}") != std::string::npos);
    CHECK(request.find("\"temperature\":0.80") != std::string::npos);
}

TEST_CASE("HTTP answers, plain and chunked", "[playerbots][random][chat]")
{
    auto plain = ParseHttp("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"a\":1}");
    REQUIRE(plain);
    CHECK(plain->Status == 200);
    CHECK(plain->Body == "{\"a\":1}");
    auto chunked = ParseHttp("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\n{\"a\"\r\n3\r\n:1}\r\n0\r\n\r\n");
    REQUIRE(chunked);
    CHECK(chunked->Body == "{\"a\":1}");
    CHECK_FALSE(ParseHttp("garbage"));
    CHECK(ParseHttp("HTTP/1.0 404 Not Found\r\n\r\n")->Status == 404);
}

TEST_CASE("The system prompt carries the bot's facts", "[playerbots][random][chat]")
{
    Facts facts;
    facts.Name = "Lorros";
    facts.Class = "Krieger";
    facts.Race = "Mensch";
    facts.Role = "Tank";
    facts.Zone = "Dämmerwald";
    facts.Activity = "erledigst gerade Quests";
    facts.Partner = "Arik";
    facts.Level = 32;
    facts.Whisper = true;
    std::string prompt = SystemPrompt(facts, 5);
    CHECK(prompt.find("Lorros") != std::string::npos);
    CHECK(prompt.find("Stufe 32") != std::string::npos);
    CHECK(prompt.find("flüstert dir Arik") != std::string::npos);
    CHECK(std::string(Persona(5)) == Persona(5));
}
