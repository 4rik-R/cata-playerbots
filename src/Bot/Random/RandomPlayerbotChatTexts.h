/*
 * German chat lines for random bots. Lines marked "ported" are the deDE
 * texts (text_loc3) of mod-playerbots' ai_playerbot_texts table at
 * e88cc4099e69f35126b28942a412ca702d20453b, filtered to the placeholders
 * this port fills and without the toxic groups; the rest are written for this
 * port. Generated; see PORTING.md. Released under GNU GPL v2 or any later version.
 */
#ifndef CATA_RANDOM_PLAYERBOT_CHAT_TEXTS_H
#define CATA_RANDOM_PLAYERBOT_CHAT_TEXTS_H

#include <cstdint>

namespace RandomPlayerbotChatTexts
{
enum class Lines : uint8_t
{
    SuggestInstance,
    SuggestQuest,
    SuggestTrade,
    SuggestSomething,
    SuggestSell,
    Hello,
    Goodbye,
    LootRare,
    LootEpic,
    QuestTurnedIn,
    KilledRare,
    KilledWorldBoss,
    LevelUp,
    LevelUpTen,
    LevelUpMax,
    LowMana,
    LowHealth,
    CriticalHealth,
    Aoe,
    ReplyUnknown,
    ReplyGreeting,
    ReplyFarewell,
    ReplyThanks,
    ReplyWhere,
    ReplyLevel,
    ReplyInviteYes,
    ReplyInviteGrouped,
    ReplyInviteDungeon,
    ReplyInviteAlready,
    ReplyLfg,
    ReplyHelp,
    ReplyBot,
    ReplyInsult,
    ReplyPraise,
    ReplyLaugh,
    ReplyYes,
    ReplyNo,
    ReplyQuestion,
    Congrats,
    NiceLoot,
    PartyHello,
    PartyBye,
    Wipe,
    ActivityQuesting,
    ActivityGrinding,
    ActivityFighting,
    ActivityDungeon,
    ActivityCapital,
    ActivityFlying,
    ActivitySailing,
    ActivityDead,
    ActivityResting,
    Count
};

struct LineSet
{
    char const* const* Items;
    uint32_t Size;
};

// ported
inline char const* const SuggestInstanceLines[] = {
    "Wer hat Bock auf %instance_name?",
    "Gibts Gruppen für %instance_name?",
    "Brauch jemand Hilfe für %instance_name?",
    "Suche Gruppe: %instance_name.",
    "Braucht wer %my_role für %instance_name?",
    "Fehlt %my_role für %instance_name?",
    "Ich kann %my_role für %instance_name sein.",
    "Braucht jemand Hilfe bei %instance_name?",
    "Braucht ihr %my_role für %instance_name?",
    "Braucht wer Gear aus %instance_name?",
    "Kleiner Grind in %instance_name?",
    "Lust auf %instance_name",
    "Brauche Hilfe für %instance_name.",
    "Bock auf %instance_name.",
    "%my_role sucht Gruppe für %instance_name.",
    "Was ist mit %instance_name?",
    "Wer will %instance_name farmen?",
    "Rein in %instance_name?",
    "Suche Gruppe für %instance_name.",
    "Braucht wer Hilfe bei Quests in %instance_name?",
    "Bock auf Quests in %instance_name.",
    "Wer hat Quests in %instance_name?",
    "Kann bei Quests in %instance_name helfen.",
    "%my_role: Platz in Gruppe für %instance_name?",
    "Läuft überhaupt noch jemand %instance_name?",
    "%instance_name: nimmt wer einen %my_role mit?",
    "Lohnt es sich als %my_role in %instance_name?",
    "Lohnt es sich echt, nach %instance_name zu gehen?",
    "Braucht jemand noch Leute für %instance_name?",
    "Die Bosse in %instance_name droppen gutes Gear. Bock zu grinden?",
    "Braucht jemand %my_role?",
    "Braucht wer %my_role?",
    "Trefft mich in %instance_name",
    "Bock auf schnellen %instance_name Run",
    "Bock auf kompletten %instance_name Run",
    "Wie oft wart ihr schon in %instance_name?",
    "Noch ein %instance_name Run?",
    "Nehmt mich bitte mit nach %instance_name.",
    "Schneller %instance_name Run?",
    "Kompletter %instance_name Run?",
    "Wer nimmt %my_role mit nach %instance_name?",
    "LFG %instance_name, bin %my_role",
    "%my_role LFG %instance_name",
    "Sucht jemand eine Gruppe für %instance_name?",
    "Brauche einen DD für %instance_name",
    "Suche einen Heiler für %instance_name",
    "Braucht jemand einen Tank für %instance_name?",
    "Will %instance_name machen. Brauche eine Gruppe!",
    "Wer hat Lust auf %instance_name?",
    "Suche noch einen für %instance_name",
    "Will jemand mit nach %instance_name?",
    "Interesse an %instance_name?",
    "Suche einen schnellen Run durch %instance_name",
    "Wer ist dabei für %instance_name?",
    "Brauche Hilfe, um %instance_name zu clearen",
    "Wollt ihr %instance_name farmen?",
    "Will %instance_name abschließen",
    "Braucht jemand Items aus %instance_name?",
    "Will jemand %instance_name laufen?",
    "Laufe %instance_name, suche andere",
    "Hat jemand Zeit für %instance_name?",
    "Suche jemanden, der bei %instance_name hilft",
    "Brauche %my_role für %instance_name!",
    "Bock auf einen %instance_name Run?",
    "Brauche %my_role für %instance_name Run",
    "Will jemand Gear aus %instance_name farmen?",
    "Suche Gruppe für %instance_name Speedrun",
    "Brauche Gruppe, um %instance_name zu clearen",
    "Interesse, %instance_name zu farmen?",
    "Suche etwas Gear aus %instance_name",
    "Wer hat Lust auf einen schnellen %instance_name Run?",
    "Will jemand %instance_name zusammen angehen?",
    "Suche Leute, um %instance_name abzuschließen",
    "Kriegen wir eine Gruppe für %instance_name zusammen?",
    "Brauche einen schnellen Run für %instance_name",
    "Wer hat Lust, %instance_name zusammen zu farmen?",
    "Suche noch ein paar Leute für %instance_name",
    "Braucht jemand Hilfe bei den Mechaniken in %instance_name?",
    "Brauche schnell eine Gruppe für %instance_name",
    "Will jemand %instance_name als Speedrun laufen?",
    "Suche einen schnellen Abschluss von %instance_name",
    "Suche jemanden, der bei %instance_name mitkommt",
    "Wer hat Lust, %instance_name zu clearen?",
    "Brauche Hilfe beim Endboss von %instance_name",
    "Wer hat heute Lust, %instance_name zu laufen?",
    "Suche schnellen Run in %instance_name, um etwas Gear zu farmen",
    "Will jemand Mats aus %instance_name farmen?",
    "Lass uns eine Gruppe für %instance_name machen",
};
// ported
inline char const* const SuggestQuestLines[] = {
    "Braucht wer Hilfe bei %quest_link?",
    "Will jemand %quest_link teilen?",
    "Macht jemand %quest_link?",
    "Bock auf %quest_link.",
    "Suche Gruppe für %quest_link.",
    "Wer hilft bei %quest_link?",
    "Braucht jemand Unterstützung bei %quest_link?",
    "Interesse an %quest_link?",
    "Will jemand %quest_link abschließen?",
    "Braucht wer ein Team für %quest_link?",
    "Wer hat Bock auf %quest_link?",
    "Lass uns %quest_link zusammen machen.",
    "Brauche jemanden für %quest_link.",
    "Braucht jemand Hilfe bei %quest_link?",
    "Brauchst du Hilfe bei %quest_link? Ich helfe dir.",
    "Will %quest_link machen. Wer ist dabei?",
    "Hilfe gesucht für %quest_link.",
    "Wer ist bereit für %quest_link?",
    "Wer will bei %quest_link mitmachen?",
    "Wer ist bei %quest_link dabei?",
    "Brauche hier etwas Hilfe bei %quest_link.",
    "Jemand für %quest_link?",
    "Wer ist bereit, %quest_link zu machen?",
    "Suche etwas Hilfe für %quest_link.",
    "Wer hat Lust, %quest_link abzuschließen?",
    "Wer kann bei %quest_link helfen?",
    "Suche eine helfende Hand bei %quest_link.",
    "Ist jemand bereit für %quest_link?",
    "Kann jemand bei %quest_link dazukommen?",
    "Suche ein paar Mitstreiter für %quest_link.",
    "Will jemand sich für %quest_link zusammentun?",
    "Wer muss %quest_link noch abschließen?",
    "Will %quest_link abschließen.",
    "Wer kann bei %quest_link mitkommen?",
    "Interesse, %quest_link abzuschließen?",
    "Bock, bei %quest_link zu helfen?",
    "Ist jemand bereit, bei %quest_link zu helfen?",
    "Brauche jemanden, um %quest_link zu beenden.",
    "Will jemand %quest_link beenden?",
    "Gibt’s Hilfe für %quest_link?",
    "Wer ist dabei für %quest_link?",
    "Suche Partner zum Abschließen von %quest_link.",
    "Wer hat Lust, %quest_link zu beenden?",
    "Heute jemand Interesse an %quest_link?",
    "Macht gerade jemand %quest_link?",
    "Wer hat jetzt Bock auf %quest_link?",
    "Will jemand %quest_link starten?",
    "Bitte um Hilfe bei %quest_link.",
    "Wer kann mir bei %quest_link helfen?",
};
// ported
inline char const* const SuggestTradeLines[] = {
    "Jemand bereit zum %category farmen?",
    "Suche Hilfe beim %category farmen.",
    "%category sind verdammt teuer!",
    "Will %category.",
    "Brauche Hilfe mit %category.",
    "Kaufe: %category.",
    "Interesse an %category?",
    "Verkaufe: %category.",
    "Verkaufe %category günstiger als im AH.",
    "Wer will %category farmen?",
    "Will %category farmen.",
    "Suche Gruppe fürs %category farmen.",
    "%category sind willkommen.",
    "Kaufe alles von %category.",
    "Wow, alle farmen %category!",
    "%category gehen im AH weg wie warme Semmeln.",
    "AH ist heiß auf %category.",
    "%category sind auf dem Markt.",
    "Will etwas %category tauschen.",
    "Brauche mehr %category.",
    "Kann jemand etwas %category entbehren?",
    "Wer will %category?",
    "Etwas %category bitte?",
    "Hätte wohl den Skill für %category lernen sollen.",
    "Ich geiere auf %category.",
    "Für %category würden die Leute töten.",
    "%category ist ein Top-Deal!",
    "Alle sind verrückt nach %category!",
    "Wo farmt man am besten %category?",
    "Bin ready für %category.",
    "Lohnt es sich, %category zu verkaufen?",
    "Ich behalte meine ganzen %category wohl lieber.",
    "Brauchst du eine Gruppe? Vielleicht zum %category farmen?",
    "Denke immer noch über %category nach.",
    "Hab schon von %category gehört, aber meine Taschen sind leer.",
    "LFG für %category",
    "Würde mich der Verkauf von %category reich machen?",
    "Okay. Ich farme %category morgen.",
    "Alle reden über %category.",
    "Hab mindestens zehn Leute gesehen, die %category farmen.",
    "Hab gestern all meine %category verkauft. Bin komplett pleite!",
    "Will einer Gilde joinen, die %category farmt.",
    "Verkaufe %category, wer hat Interesse?",
    "Braucht jemand %category?",
    "Verkaufe %category, flüster mir.",
    "Brauche %category, verkauft das jemand?",
    "Hab %category zu verkaufen, meldet euch!",
    "Ist jemand bereit, %category zu farmen?",
    "Verkaufe %category, bester Preis am Markt.",
    "Hat jemand %category zum Tauschen?",
    "Wer braucht %category? Hab was zu verkaufen!",
    "Habe %category zu verkaufen, flüster mir für den Preis.",
    "Will %category verkaufen, flüster mir für Details.",
    "Kaufe: %category. Verkauft das jemand?",
    "Suche %category, bitte melden.",
    "Wer hat Interesse an %category?",
    "Kann jemand beim %category farmen helfen?",
    "Hat jemand %category zu verkaufen?",
    "Will %category kaufen, flüster mir einfach.",
    "Suche %category. Wer hat was?",
    "Kaufe: %category zum fairen Preis.",
    "Brauche dringend %category, hat wer was?",
    "Hab %category, verkaufe günstig.",
    "Verkaufe %category zum guten Kurs.",
    "Suche %category, verkauft wer günstig?",
    "Verkauft jemand %category? Hätte Interesse.",
    "Brauche %category, flüstert mir wenn ihr verkauft.",
    "Hat wer %category übrig zum Tauschen?",
    "Suche %category. Verkauft jemand?",
    "Hat wer Interesse, %category zu tauschen?",
    "Verkaufe %category, meldet euch für Tausch.",
    "Kaufe: %category, suche guten Kurs.",
    "Wer braucht %category? Hab was übrig.",
    "Will wer %category? Verkaufe billig.",
    "Suche %category, verkauft wer was?",
    "Braucht jemand %category? Hab was zum Tauschen.",
    "Kaufe: %category. Ist jemand günstiger als AH?",
    "Verkaufe %category billig, flüster mir für Details.",
    "Braucht wer %category? Hab was zu verkaufen.",
    "Tausche %category gegen was anderes.",
    "Kaufe: %category, hat wer was übrig?",
};
// ported
inline char const* const SuggestSomethingLines[] = {
    "Hab Bock auf Party in %zone_name.",
    "Sucht jemand %my_role?",
    "%my_role sucht Gilde.",
    "Suche Gold.",
    "%my_role will einer guten Gilde beitreten.",
    "Brauche einen Freund.",
    "Fühlt sich noch wer einsam?",
    "Langweilig...",
    "Wer will was abhaben?",
    "Vielleicht ein Duell in %zone_name?",
    "Macht hier grade jemand etwas?",
    "%zone_name: ist hier jemand?",
    "%zone_name: Wo sind alle?",
    "Scheint als wäre ich allein in %zone_name.",
    "Trefft mich in %zone_name.",
    "Lasst uns in %zone_name questen!",
    "%zone_name ist der beste Spot!",
    "Will nach %zone_name. Kommt wer mit?",
    "Wer will nach %zone_name?",
    "Ich mag %zone_name nicht, aber wohin sonst?",
    "Gibt’s gute Quests in %zone_name?",
    "Wohin soll ich nach %zone_name gehen?",
    "Wer ist in %zone_name?",
    "LFG in %zone_name.",
    "%zone_name ist der übelste Spot.",
    "Schnappt mich in %zone_name!",
    "Ab nach %zone_name!",
    "Bock zu questen in %zone_name",
    "Hat wer Quests in %zone_name?",
    "Kommt her nach %zone_name!",
    "Ich bin echt müde von %zone_name. Vielleicht sollte ich woanders hin?",
    "Viel Glück",
    "Ich will nach Hause und dann in die Kiste.",
    "Weiß wer, was man fürs Beidhändig-Kämpfen braucht?",
    "hi zusammen!",
    "%zone_name ist gemütlich",
    "Ich fühl mich super",
    "Was haltet ihr von meinem Build? %my_role",
    "schön zu sehen, dass der Chat sich noch erinnert",
    "HALLO?",
    "Zeit, mir den Weg nach %zone_name freizukämpfen",
    "%zone_name",
    "NEEEEEEEEEEEEIN",
    "hi, wie geht es euch",
    "hab mich grad ausgeloggt und wieder eingeloggt",
    "könnt ihr mal leiser sein, ich hab mich in %zone_name verlaufen",
    "wer will mit mir was trinken in %zone_name ...hicks!",
    "Gibts hier Gilden, die %my_role mitziehen?",
    "Sobald man höher kommt, ist Gold easy zu machen",
    "moin",
    "Ich hab das Gefühl, Willenskraft ist BiS zum Leveln",
    "KANN MICH JEMAND INVITEN",
    "brauche viele Drinks",
    "nun ja",
    "gibts hier Levelgilden?",
    "brb",
    "warum ist Schnee weiß, aber Eis klar? ist doch das Gleiche",
    "warum ist Schlagsahne fluffig und normale nicht",
    "warum riechen Füße, wenn sie keine Nase haben",
    "gibts hier auf dem Server PvP?",
    "eh klar",
    "puh... :)",
    "wusstet ihr schon, dass...",
    "Ich stell mir lieber nicht vor, wie sich die Viecher fühlen, während ich sie umboxe.",
    "der Grind macht Spaß",
    "WoW hält mich wachsam",
    "kurze Frage: wo krieg ich den Buff für mehr XP? bin in %zone_name",
    "inv mich. ich helfe",
    "welche Klasse ist gut für PvP?",
    "wo zum Teufel ist der Kochlehrer in %zone_name",
    "wisst ihr, was in %zone_name abgeht?",
    "muss was craften",
    "Bock auf Blödsinn in %zone_name?",
    "machen wir %zone_name zu unserem Spielplatz",
    "ich verirr mich gleich wieder in %zone_name",
    "kaum zu glauben, dass ich immer noch in %zone_name hocke",
    "Willst du dich für Chaos in %zone_name zusammentun?",
    "lasst uns zusammen Chaos in %zone_name anrichten",
    "hab Bock, in %zone_name absolut gar nix zu machen",
    "in %zone_name hört eh keiner zu",
    "schickt mal wer Hilfe nach %zone_name! ist wild hier!",
    "ich bin sicher, in %zone_name spukt es",
    "wenn %zone_name reden könnte, würde es wahrscheinlich schreien",
    "irgendwas ist faul an %zone_name",
    "gleich zerleg ich %zone_name mit meiner Awesomeness",
    "sagt %zone_name, dass ich da bin",
    "ich hab mich in %zone_name verlaufen, sendet Hilfe",
    "findet noch wer %zone_name verdächtig ruhig?",
    "ich wette, %zone_name hat Geheimnisse, von denen wir nix wissen",
    "langsam glaube ich, %zone_name ist nur ein Mythos",
    "wer hätte gedacht, dass %zone_name so abgeht?",
    "als wäre das Spiel in %zone_name kaputt",
    "ist %zone_name eigentlich ne Wüste, oder bin ich einfach nur lost?",
    "fühlt sich langsam an wie %zone_name im Mixer",
    "hat jemand ne Karte für %zone_name?",
    "Noch wer, der denkt das %zone_name uns umbringen will?",
    "Ich bin auf wilder Fahrt durch %zone_name!",
    "Bin das nur ich, oder wird %zone_name immer weirder?",
    "Ich bin davon überzeugt, das %zone_name heimlich ein Freizeitpark ist.",
};
// ported
inline char const* const SuggestSellLines[] = {
    "VK: %item_formatted_link für %cost_gold.",
    "Wer will %item_formatted_link für %cost_gold?",
    "Will wer %item_formatted_link? Nur %cost_gold.",
    "Nur %cost_gold für %item_formatted_link!",
    "Verkaufe %item_formatted_link für %cost_gold.",
    "%item_formatted_link gehört dir für nur %cost_gold!",
    "Lächerlicher Preis: %cost_gold für %item_formatted_link!",
    "Will %item_formatted_link für %cost_gold verkaufen.",
    "Wer braucht %item_formatted_link? Nur %cost_gold.",
    "Braucht jemand %item_formatted_link für %cost_gold?",
    "%cost_gold für %item_formatted_link. Günstiger als im AH!",
    "%item_formatted_link ist Wertvoll, aber ich würd es für %cost_gold verkaufen.",
    "Billiger als %cost_gold kriegst du %item_formatted_link nicht!",
    "Habe %item_formatted_link. Wer kauft für %cost_gold?",
    "Will jemand %item_formatted_link für %cost_gold kaufen?",
    "Wie wärs mit %item_formatted_link? Für %cost_gold.",
    "Verkaufe %item_formatted_link? Nur %cost_gold.",
    "LFG zum Farmen. %item_formatted_link gibts von mir noch für %cost_gold.",
    "Heute fast alles verkauft. %item_formatted_link hab ich noch für %cost_gold.",
    "Wozu Handelschat? Klar, um %item_formatted_link für %cost_gold zu verkaufen.",
    "Kann jemand den Preis %cost_gold für %item_formatted_link unterbieten?",
    "Wollt ihr den Handelschat stoppen? Kauft einfach %item_formatted_link! Für %cost_gold!",
    "Alle spammen im Handelschat. Ich auch - %cost_gold für %item_formatted_link!",
    "Taugt %item_formatted_link was? Verkaufe es für %cost_gold.",
    "Hab %item_formatted_link, verkaufe an dich für %cost_gold.",
    "Gestern nix geschafft, aber %item_formatted_link bekommen. Verkaufe für %cost_gold.",
    "Gestern gefarmt und %item_formatted_link gezogen. Will jemand für %cost_gold kaufen?",
    "Gestern %item_formatted_link gekauft. Brauchts wer für %cost_gold?",
    "Wer hatte %item_formatted_link gesucht? Preis bleibt - %cost_gold.",
    "Hab immer noch %item_formatted_link. Kauft wer für %cost_gold?",
    "Früher war ich besser bei Kasse. Jetzt verkauf ich %item_formatted_link für %cost_gold.",
    "Ich Wünschte, ich hätte mehr als %item_formatted_link. Kaufen könnt ihrs trotzdem für %cost_gold.",
    "Wozu ist euer Gold gut? Um mein %item_formatted_link für %cost_gold zu kaufen.",
    "Gönnt mir etwas Gold. Kauft %item_formatted_link für %cost_gold.",
    "Sind %cost_gold ein guter Preis für %item_formatted_link?",
    "Hab gestern erst %item_formatted_link gekauft, brauch es aber nicht mehr. Will wer für %cost_gold?",
    "Stell %item_formatted_link gleich ins AH, aber ihr könnts jetzt günstiger haben: nur %cost_gold.",
    "VK: %item_formatted_link für %cost_gold. flüster mir bei Interesse.",
    "Muss %item_formatted_link loswerden, %cost_gold oder bestes Angebot.",
    "%item_formatted_link für %cost_gold verfügbar. Schlagt zu!",
    "Habe %item_formatted_link, will %cost_gold.",
    "Verkaufe %item_formatted_link, nur %cost_gold.",
    "Will jemand %item_formatted_link für %cost_gold? flüster mir!",
    "Verkaufe %item_formatted_link für %cost_gold, greift zu solange es geht!",
    "%item_formatted_link für %cost_gold, schnell sein bevor es weg ist!",
    "Verkaufe %item_formatted_link für %cost_gold, wer ist dabei?",
    "Verkaufe %item_formatted_link für %cost_gold, wer zuerst kommt wird auch zuerst bedient!",
    "Nur %cost_gold für %item_formatted_link, nur für kurze Zeit!",
    "%item_formatted_link für %cost_gold verfügbar, meldet euch wenn ihrs braucht!",
    "Interesse an %item_formatted_link für %cost_gold?",
    "Verkaufe %item_formatted_link für %cost_gold, muss meine Taschen leeren.",
    "Hol dir %item_formatted_link für %cost_gold, Top-Deal!",
    "%item_formatted_link für %cost_gold, greift zu solange der Vorrat reicht!",
    "Verkaufe %item_formatted_link für %cost_gold, nicht verpassen!",
    "Nur %cost_gold für %item_formatted_link, flüster mir für Details!",
    "%item_formatted_link gibts für %cost_gold, lasst uns handeln!",
    "Verkaufe %item_formatted_link für %cost_gold, flüster mir für Infos.",
    "Will %item_formatted_link verkaufen, für %cost_gold gehört es dir!",
    "Hab %item_formatted_link für %cost_gold, machen wir nen Deal!",
    "Verkaufe %item_formatted_link, für %cost_gold nimmst du es mit!",
    "VK: %item_formatted_link für %cost_gold, Interesse?",
    "Muss %item_formatted_link schnell loswerden, %cost_gold.",
    "Verkaufe %item_formatted_link für %cost_gold, bester Preis weit und breit!",
    "%item_formatted_link für %cost_gold, wer kauft?",
    "%item_formatted_link für %cost_gold, machen wirs fix!",
    "Verkaufe %item_formatted_link für %cost_gold, schnappt zu bevor alles weg ist!",
    "Willst %item_formatted_link? Für %cost_gold kriegst du eins!",
    "%item_formatted_link für %cost_gold, schneller Verkauf!",
};
// ported
inline char const* const HelloLines[] = {
    "Hallo",
    "Hallo!",
    "Hi",
    "Hi!",
    "Hallo zusammen!",
};
// ported
inline char const* const GoodbyeLines[] = {
    "Auf Wiedersehen!",
    "Tschüss!",
    "Bis später!",
};
// ported
inline char const* const LootRareLines[] = {
    "%item_link ist BiS für Jäger",
    "%item_link ist BiS für %my_class",
    "RNG meint es heute gut: %item_link",
    "nice, %item_link frisch gelootet",
    "wow, hab gerade %item_link bekommen",
};
// ported
inline char const* const LootEpicLines[] = {
    "OMG, guckt mal, was ich gerade bekommen hab: %item_link!!!",
    "Keine @#$@% Chance! Ich hab %item_link bekommen, Wahnsinn.",
};
// ported
inline char const* const QuestTurnedInLines[] = {
    "Jaaa, endlich %quest_link abgegeben",
    "hab %quest_link abgegeben",
    "Hab %quest_link geschafft, gerade abgegeben",
    "gerade %quest_link abgegeben",
    "gerade %quest_link in %zone_name abgegeben",
    "Noch eine Quest abgeschlossen! %quest_link abgegeben",
    "Mission erfüllt! %quest_link ist abgegeben",
    "Endlich, %quest_link abgegeben! Was für ein Ritt!",
    "Grade %quest_link abgeschlossen und abgegeben, fühlt sich gut an!",
    "%quest_link in %zone_name abgegeben. Weiter zur nächsten Herausforderung!",
    "Hab %quest_link abgegeben. Wieder eine weniger!",
    "%quest_link erfolgreich abgegeben, das hat gedauert!",
    "%quest_link in %zone_name abgeschlossen und abgegeben. Fühlt sich gut an!",
    "Quest fertig! %quest_link abgegeben und bereit für die nächste!",
    "Endlich %quest_link in %zone_name abgegeben. Die war knackig!",
    "Das wars, %quest_link abgegeben! Jetzt Belohnung einsacken!",
    "%quest_link fertig! Jetzt abgeben und weiter.",
    "Gerade %quest_link abgegeben, fühlt sich gut an!",
    "Hab %quest_link abgegeben. Mal sehen, was als Nächstes kommt!",
    "%quest_link eben abgeschlossen und abgegeben. Weiter gehts!",
    "Wieder eine im Kasten, %quest_link abgegeben",
    "%quest_link abgegeben! Zeit, die Belohnung abzuholen.",
    "%quest_link abgeschlossen und abgegeben, was für eine Erleichterung!",
    "Das ging fix! %quest_link schon abgegeben!",
    "Gerade %quest_link in %zone_name abgegeben. Bereit fürs nächste Abenteuer!",
    "%quest_link erfolgreich abgegeben. Fühlt sich nach Fortschritt an!",
    "Mission komplett! %quest_link ist abgegeben!",
    "Hab gerade %quest_link beendet und abgegeben, weiter zur nächsten!",
};
// ported
inline char const* const KilledRareLines[] = {
    "Yo, hab gerade %victim_name gekillt!",
    "Seltener %victim_name down. War ein geiler Fight",
    "Hab gerade den seltenen %victim_name gekillt",
    "Seltener %victim_name in %zone_name besiegt",
    "Gerade seltenen %victim_name in %zone_name gekillt. Das ging fix!",
    "Puh, %victim_name doch noch gelegt!",
    "Seltener Elite %victim_name liegt",
    "%victim_name gibt es nicht mehr. Noch ein seltener Elite gelegt",
    "Seltener Elite %victim_name down. Ich bin aus Feuer!",
    "Hab gerade den seltenen Elite %victim_name gekillt",
};
// ported
inline char const* const KilledWorldBossLines[] = {
    "Das war geil! %victim_name gerade gekillt! Was ein Kampf",
    "Weltenboss %victim_name ist down! Das war episch",
    "Hab den Weltenboss %victim_name gelegt. Zeit zum Feiern!",
    "%victim_name gekillt! Der Weltenboss war kein Spaß!",
    "Gerade %victim_name gekillt, das war legendär",
};
// ported
inline char const* const LevelUpLines[] = {
    "Ding! %my_level",
    "Jaaa, ich bin Level %my_level!",
    "Gerade aufgelevelt",
    "Ding! Wieder ein Level up!",
    "Jetzt offiziell Level %my_level!",
    "Level %my_level, fühl mich jetzt schon stärker!",
    "Aufwärts! Level %my_level erreicht",
    "Das ging fix, schon Level %my_level!",
    "Gerade Level %my_level erreicht, auf gehts!",
    "Level %my_level, jetzt kommen die großen Sachen!",
    "Level %my_level, ich bin jetzt on fire!",
    "Level %my_level voll und weiter!",
    "Bin jetzt Level %my_level, was kommt als Nächstes?",
};
// ported
inline char const* const LevelUpTenLines[] = {
    "Ich bin Level %my_level!!!",
    "Werde stärker, schon Level %my_level!!!",
    "Gerade Level %my_level erreicht!!!",
    "Level %my_level!!! Es passiert!",
    "Gerade Level %my_level erreicht, fühlt sich mega an!",
    "Level %my_level und ich fange erst an!",
    "Wow, schon Level %my_level, unaufhaltbar!",
    "Neue Höhen mit Level %my_level!!!",
    "Level %my_level und weiter Vollgas!",
    "Level %my_level, jetzt bin ich zu stark!",
    "Level %my_level erreicht, fühlt sich so gut an!",
    "Bin jetzt Level %my_level! Los gehts!",
    "Level %my_level, noch viel vor mir!",
};
// ported
inline char const* const LevelUpMaxLines[] = {
    "OMG, endlich Level %my_level!!!",
    "Stufe %my_level!!! Jetzt kann ich Endgame-Content machen",
    "Frisch auf Level %my_level, %my_class!!!",
    "Noch ein Level: %my_level, %my_race %my_class!",
    "Endlich Level %my_level erreicht, ich kann alles!",
    "Maxlevel %my_level, Endgame ich komme!",
    "Level %my_level! Bereit fürs Endgame!",
    "Endgame freigeschaltet auf Level %my_level, her damit!",
    "Hab Level %my_level erreicht, Zeit zu glänzen!",
    "Level %my_level, lasst uns Endgame-Raids machen!",
    "Mit Level %my_level hält mich nix mehr auf!",
    "Level %my_level, bereit für High-Level-Content!",
    "Yes! Level %my_level, %my_class für alles bereit!",
    "Level %my_level, Zeit die Welt herauszufordern!",
    "Level %my_level erreicht, schnappen wir uns die Elitebosse!",
    "Jetzt bin ich Level %my_level, das echte Abenteuer beginnt!",
    "Level %my_level! Zeit für ernsthafte Action!",
    "Level %my_level und werde immer stärker!",
    "Endlich auf Level %my_level! Jetzt zeige ich, was ich kann!",
    "Level %my_level ist da! Bereit für die große Liga!",
    "Gerade Level %my_level erreicht, macht euch bereit für echte Herausforderungen!",
    "Level %my_level voll, Zeit für epischen Content!",
    "Frisch auf Level %my_level! Bereit für Endgame-Inis!",
    "Jetzt, wo ich Level %my_level bin, ist nichts mehr außerhalb meiner Reichweite!",
    "Jetzt ist es offiziell! Level %my_level, Zeit zu dominieren!",
    "Level %my_level geschafft! Zeit, der Welt meinen Stempel aufzudrücken!",
    "Level %my_level freigeschaltet! Jetzt beginnt der echte Spaß!",
    "Level %my_level, und die Welt ist mein Spielplatz!",
    "Level %my_level, bereit, der Welt meine Power zu zeigen!",
    "Gerade %my_level erreicht, jetzt geh ich all in!",
    "Level %my_level, endlich kann ich alles angehen!",
    "Level %my_level, fühl mich unaufhaltbar!",
    "Gerade Level %my_level erreicht, kanns kaum erwarten ins Endgame zu springen!",
};
// ported
inline char const* const LowManaLines[] = {
    "OOM",
    "Kein Mana mehr",
    "Verdammt, dafür hab ich mein ganzes Mana verballert",
    "Wartet bis ich trinke oder mein Mana wieder da ist",
    "Mana low",
    "Kein Mana. Schon wieder?",
    "Mana low. Muss trinken",
    "Haben wir nen Getränkeautomaten? Wieder OOM",
    "Mein Mana ist Geschichte",
    "Ich besorge mir nächstes mal Drinks. OOM",
    "Wo ist mein Mana?",
};
// ported
inline char const* const LowHealthLines[] = {
    "Brauche Heal",
    "HP low",
    "Gebt mal nen Heal. Bitte.",
    "Kann mir jemand nen Heal geben?",
    "Hey! Lieber jetzt heilen als später zu rezzen",
    "Sorry. Brauche noch einen Heal",
    "Verdammte Mobs. Heal mich bitte",
    "Noch ein Treffer und ich bin hinüber. Heal bitte",
    "Heiler da?",
    "Warum hauen die mir immer ins Gesicht? Brauche Heal",
    "Kann mich wer ein bisschen heilen?",
};
// ported
inline char const* const CriticalHealthLines[] = {
    "Heilt mich! Schnell!",
    "Fast tot! Heilt mich!",
    "Hilfe! Heilt mich!",
    "Irgendwer! Heilt mich!",
    "Heal! Heal! Heal!",
    "Ich sterbe! Heal! Aaaaarhg!",
    "Heal mich!",
    "Ich sterbe. Ich sterbe. Ich sterbe. Heal!",
    "Heiler, wo seid ihr? Ich sterbe!",
    "Oh, der Schmerz. Heilt mich schnell!",
};
// ported
inline char const* const AoeLines[] = {
    "Oh Gott!",
    "Ich habe Angst",
    "Das wars.",
    "Hier ist jetzt Schluss.",
    "Kann mal wer Blizzard anrufen?",
    "Verdammt. Der Tank hat alle Mobs gepullt.",
    "Wir sterben. Wir sterben. Wir sterben.",
    "Whoa! So viel Spielzeug zum Austoben.",
    "Ich kill sie alle!",
    "Wenn der Tank stirbt, sind wir Geschichte.",
    "Aaaaaargh!",
    "LEEEEERROOOYYYYYYYYYYYY JENNKINNNSSSSSS!!!!!!!",
    "Okay. Was haben wir an AoE?",
    "Jetzt wirds interessant.",
    "Cool. Zieht sie zusammen für nen schönen Flammenstoß.",
    "Kill! Kill! Kill!",
    "Wir sind Geschichte.",
    "Hoffentlich sind die Heiler ready. Leeeeroy!",
    "Hoffentlich kommen die nicht zu mir.",
    "Oh nein. Ich kann bei dem Gemetzel nicht hinsehen.",
};
// ported
inline char const* const ReplyUnknownLines[] = {
    "Was war das, %s?",
    "Bin nicht sicher, ob ich dich verstehe, %s?",
    "äh... keine Ahnung, wovon du redest",
    "Redest du mit mir, %s?",
    "whaaaa?",
    "hä?",
    "was?",
    "Was hast du gesagt, %s?",
    "Okay, ähm, lol",
    "Wovon redest du, %s",
    "jup",
    "Ja, %s",
    "interessant...",
    "lol",
    "ja",
    "ooooooh",
    "hmm",
    "ja klar",
    "ich versuche, diesen Kommentar zu verstehen",
    "oh ja, vielleicht",
    "ja und?",
    "haha",
    "hi",
    "was geht",
    "ja???",
    "äh... was",
    "redest du mit mir, %s?",
};
// own
inline char const* const ReplyGreetingLines[] = {
    "moin",
    "hi %other_name",
    "hey",
    "servus",
    "na?",
    "hallo :)",
    "moin moin",
    "hey %other_name, was geht?",
    "huhu",
    "hi",
    "yo",
    "hey du",
};
// own
inline char const* const ReplyFarewellLines[] = {
    "bis dann",
    "ciao",
    "cya",
    "machs gut",
    "tschau %other_name",
    "bis später",
    "hau rein",
    "bye",
};
// own
inline char const* const ReplyThanksLines[] = {
    "kein ding",
    "gerne",
    "np",
    "kein problem",
    "jo, gerne doch",
    "immer wieder",
    "passt schon",
};
// own
inline char const* const ReplyWhereLines[] = {
    "bin in %zone_name, %activity",
    "%activity in %zone_name",
    "grad in %zone_name. %activity",
    "%zone_name, %activity. und du?",
    "nix besonderes, %activity in %zone_name",
    "%activity. bin in %zone_name",
    "gut, %activity in %zone_name",
};
// own
inline char const* const ReplyLevelLines[] = {
    "lvl %my_level %my_class",
    "bin %my_level, %my_class",
    "%my_race %my_class, lvl %my_level",
    "%my_level, spiele %my_class als %my_role",
    "lvl %my_level. %my_role %my_class",
    "%my_class auf %my_level",
};
// own
inline char const* const ReplyInviteYesLines[] = {
    "klar, lad ein",
    "jo, inv",
    "gerne, schick inv",
    "klar, bin dabei",
    "ok, lad mich ein",
    "jo, gerne",
};
// own
inline char const* const ReplyInviteGroupedLines[] = {
    "sry, bin schon in ner gruppe",
    "grad schon in gruppe, später vllt",
    "bin schon in ner gruppe, sry",
};
// own
inline char const* const ReplyInviteDungeonLines[] = {
    "bin grad in ner ini, sry",
    "nach der ini gern",
    "bin im dungeon grad, später?",
};
// own
inline char const* const ReplyInviteAlreadyLines[] = {
    "bin doch schon bei dir in der gruppe :D",
    "sind doch schon zusammen",
    "bin doch schon da xD",
};
// own
inline char const* const ReplyLfgLines[] = {
    "nimm den dungeonbrowser, da bin ich auch drin",
    "meld dich im dungeonbrowser an, ich komm mit",
    "als %my_role bin ich dabei, geh in den dungeonbrowser",
    "dungeonbrowser, da findet man schnell leute",
    "geh in den dungeonbrowser, ich bin %my_role",
};
// own
inline char const* const ReplyHelpLines[] = {
    "klar, wobei?",
    "was brauchst du?",
    "kommt drauf an, was ist los?",
    "sag an",
    "wenn ich kann, gerne",
    "wo hängts?",
};
// own
inline char const* const ReplyBotLines[] = {
    "bot? ich? xD",
    "lol nein",
    "nö, bin nur müde",
    "manchmal fühl ich mich so",
    "nur wenn ich afk farme :D",
    "haha, nein",
    "sehr witzig",
};
// own
inline char const* const ReplyInsultLines[] = {
    "selber",
    "chill mal",
    "ok...",
    "wow, nett",
    "jaja",
    "entspann dich mal",
    "hast du nen schlechten tag?",
    "sag ich deiner mama",
};
// own
inline char const* const ReplyPraiseLines[] = {
    "danke :)",
    "thx",
    "hehe danke",
    "gg",
    "jo, lief gut",
    "danke dir",
    "thx, du auch",
};
// own
inline char const* const ReplyLaughLines[] = {
    "haha",
    "xD",
    "lol",
    "hehe",
    ":D",
    "^^",
    "hahaha",
};
// own
inline char const* const ReplyYesLines[] = {
    "jo",
    "ok",
    "alles klar",
    "cool",
    "passt",
    "gut",
};
// own
inline char const* const ReplyNoLines[] = {
    "ok, schade",
    "na gut",
    "kein ding",
    "ok",
    "schade",
};
// own
inline char const* const ReplyQuestionLines[] = {
    "kp",
    "keine ahnung ehrlich gesagt",
    "hm, gute frage",
    "weiß nicht, sry",
    "frag mich was leichteres",
    "kp, frag mal im allgemeinchat",
    "hm, keine ahnung",
};
// own
inline char const* const CongratsLines[] = {
    "gz",
    "grats",
    "gz %other_name",
    "glückwunsch",
    "nice, gz",
    "gz!",
    "grats %other_name",
    "gw",
};
// own
inline char const* const NiceLootLines[] = {
    "nice",
    "glückwunsch",
    "neid",
    "schick",
    "gz",
    "nicht schlecht",
};
// own
inline char const* const PartyHelloLines[] = {
    "hi",
    "moin zusammen",
    "hey",
    "hallo",
    "hi zusammen",
    "moin",
    "o/",
};
// own
inline char const* const PartyByeLines[] = {
    "danke für die gruppe",
    "thx, bye",
    "gg, bye",
    "danke euch",
    "bis dann",
    "war nett, ciao",
};
// own
inline char const* const WipeLines[] = {
    "nochmal?",
    "puh",
    "das war nix",
    "ok, nochmal von vorn",
    "rip",
    "naja, passiert",
};
// own
inline char const* const ActivityQuestingLines[] = {
    "mach quests",
    "quest hier rum",
    "bin am questen",
    "paar quests machen",
};
// own
inline char const* const ActivityGrindingLines[] = {
    "farm bisschen",
    "grinde mobs",
    "kloppe hier mobs",
    "farm XP",
};
// own
inline char const* const ActivityFightingLines[] = {
    "kämpf grad",
    "bin grad im kampf",
    "moment, kampf",
};
// own
inline char const* const ActivityDungeonLines[] = {
    "bin grad in ner ini",
    "dungeon grad",
    "in ner ini",
};
// own
inline char const* const ActivityCapitalLines[] = {
    "steh in der stadt rum",
    "reparieren und verkaufen",
    "bin in der stadt, kurz pause",
};
// own
inline char const* const ActivityFlyingLines[] = {
    "flieg grad",
    "bin am fliegen",
    "sitz aufm flugtier",
};
// own
inline char const* const ActivitySailingLines[] = {
    "bin aufm schiff",
    "fahr grad schiff",
    "warte aufs schiff",
};
// own
inline char const* const ActivityDeadLines[] = {
    "bin tot, lauf grad zurück",
    "liege tot rum lol",
    "bin grad gestorben",
};
// own
inline char const* const ActivityRestingLines[] = {
    "ess grad",
    "mach kurz pause",
    "trink grad",
};

inline LineSet Get(Lines lines)
{
    switch (lines)
    {
        case Lines::SuggestInstance: return { SuggestInstanceLines, 88 };
        case Lines::SuggestQuest: return { SuggestQuestLines, 49 };
        case Lines::SuggestTrade: return { SuggestTradeLines, 81 };
        case Lines::SuggestSomething: return { SuggestSomethingLines, 99 };
        case Lines::SuggestSell: return { SuggestSellLines, 68 };
        case Lines::Hello: return { HelloLines, 5 };
        case Lines::Goodbye: return { GoodbyeLines, 3 };
        case Lines::LootRare: return { LootRareLines, 5 };
        case Lines::LootEpic: return { LootEpicLines, 2 };
        case Lines::QuestTurnedIn: return { QuestTurnedInLines, 28 };
        case Lines::KilledRare: return { KilledRareLines, 10 };
        case Lines::KilledWorldBoss: return { KilledWorldBossLines, 5 };
        case Lines::LevelUp: return { LevelUpLines, 13 };
        case Lines::LevelUpTen: return { LevelUpTenLines, 13 };
        case Lines::LevelUpMax: return { LevelUpMaxLines, 33 };
        case Lines::LowMana: return { LowManaLines, 11 };
        case Lines::LowHealth: return { LowHealthLines, 11 };
        case Lines::CriticalHealth: return { CriticalHealthLines, 10 };
        case Lines::Aoe: return { AoeLines, 20 };
        case Lines::ReplyUnknown: return { ReplyUnknownLines, 27 };
        case Lines::ReplyGreeting: return { ReplyGreetingLines, 12 };
        case Lines::ReplyFarewell: return { ReplyFarewellLines, 8 };
        case Lines::ReplyThanks: return { ReplyThanksLines, 7 };
        case Lines::ReplyWhere: return { ReplyWhereLines, 7 };
        case Lines::ReplyLevel: return { ReplyLevelLines, 6 };
        case Lines::ReplyInviteYes: return { ReplyInviteYesLines, 6 };
        case Lines::ReplyInviteGrouped: return { ReplyInviteGroupedLines, 3 };
        case Lines::ReplyInviteDungeon: return { ReplyInviteDungeonLines, 3 };
        case Lines::ReplyInviteAlready: return { ReplyInviteAlreadyLines, 3 };
        case Lines::ReplyLfg: return { ReplyLfgLines, 5 };
        case Lines::ReplyHelp: return { ReplyHelpLines, 6 };
        case Lines::ReplyBot: return { ReplyBotLines, 7 };
        case Lines::ReplyInsult: return { ReplyInsultLines, 8 };
        case Lines::ReplyPraise: return { ReplyPraiseLines, 7 };
        case Lines::ReplyLaugh: return { ReplyLaughLines, 7 };
        case Lines::ReplyYes: return { ReplyYesLines, 6 };
        case Lines::ReplyNo: return { ReplyNoLines, 5 };
        case Lines::ReplyQuestion: return { ReplyQuestionLines, 7 };
        case Lines::Congrats: return { CongratsLines, 8 };
        case Lines::NiceLoot: return { NiceLootLines, 6 };
        case Lines::PartyHello: return { PartyHelloLines, 7 };
        case Lines::PartyBye: return { PartyByeLines, 6 };
        case Lines::Wipe: return { WipeLines, 6 };
        case Lines::ActivityQuesting: return { ActivityQuestingLines, 4 };
        case Lines::ActivityGrinding: return { ActivityGrindingLines, 4 };
        case Lines::ActivityFighting: return { ActivityFightingLines, 3 };
        case Lines::ActivityDungeon: return { ActivityDungeonLines, 3 };
        case Lines::ActivityCapital: return { ActivityCapitalLines, 3 };
        case Lines::ActivityFlying: return { ActivityFlyingLines, 3 };
        case Lines::ActivitySailing: return { ActivitySailingLines, 3 };
        case Lines::ActivityDead: return { ActivityDeadLines, 3 };
        case Lines::ActivityResting: return { ActivityRestingLines, 3 };
        default: return { nullptr, 0 };
    }
}
}

#endif
