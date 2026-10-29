#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <string>
#include <algorithm>
#include "renderer.h"
#include "audio.h"
#include "advisor_weights.h"
#include "version.h"
#include "messages.h"

enum GameState {
    STATE_MENU = 0,
    STATE_SHOP,
    STATE_HELP,
    STATE_SETTINGS,
    STATE_PLAYING,
    STATE_LEVELUP,
    STATE_CHEST,
    STATE_PAUSED,
    STATE_GAMEOVER,
    STATE_HALL,
    STATE_DEATH,
    STATE_PATCH_NOTES,
    STATE_CUTSCENE,
    STATE_EXTRACTION,
    STATE_CAMPAIGN_END,
    STATE_BOSS_MUSIC_TEST,
    STATE_MISSION_SELECT,
    STATE_MISSION_COMPLETE,
    STATE_HERO_SELECT
};

enum RunMode { MODE_STORY = 0, MODE_ENDLESS = 1 };

struct CampaignMissionDef {
    const char* title;
    const char* subtitle;
    const char* briefing;
    const char* objective;
    const char* finalBoss;
    int durationSeconds;
    int bossTimeSeconds;
    int bossType;
    int chapter;        // act number, 1..5
    uint32_t requiredSeals;
    bool finalTrio;
    int act;            // same as chapter, kept explicit for the mission list
    int kind;           // 0 = вылазка, 1 = трио, 2 = копия/двойник, 3 = выбор ветки
    int prereqA;        // previous mission that opens this one (-1 = none)
    int prereqB;        // alternative previous mission (a parallel track opens this one too)
    bool bossReinforced;// the same guardian returns stronger: the Archive raises what you broke
    int track;          // 0 = общее дело, 1 = след «СВИДЕТЕЛЬ», 2 = след «КОПИЯ»
    int brief;          // номер озвученного брифинга mission_brief_NN.wav, -1 = нет файла
};
// Five acts, twenty-eight cases. Data lives in tools/campaign.py, which generates
// these rows and the tables in STORY.md so the script and the build cannot drift.
static constexpr int CAMPAIGN_ACT_COUNT = 5;
static constexpr int CAMPAIGN_MISSION_COUNT = 30;
static constexpr int CAMPAIGN_ACT_MAX_ROWS = 6;
static const char* const CAMPAIGN_ACT_TITLES[CAMPAIGN_ACT_COUNT] = {
    "ПЕПЕЛ НА СТОЛЕ", "ГОЛОС В КОЛБЕ", "РЕЕСТР", "АРХИВ", "СЕРДЦЕ АРХИВА"
};
static const uint32_t CAMPAIGN_ACT_COLORS[CAMPAIGN_ACT_COUNT] = {
    0xFFFFC848, 0xFF80E58A, 0xFF5BD8EE, 0xFFFF8C43, 0xFFFFD278
};
static constexpr uint32_t STORY_ARCHIVE_MISSION_COMPLETE_BIT = 1u << 4;
static constexpr uint32_t STORY_HEART_MISSION_COMPLETE_BIT = 1u << 5;
// The old 6-mission save only knew these two extra bits; everything else now lives
// in SaveData::campaignMissionsDone.
static const CampaignMissionDef CAMPAIGN_MISSIONS[CAMPAIGN_MISSION_COUNT] = {
    {"ПЕПЕЛЬНЫЙ ПРОТОКОЛ", "УЧАСТОК У ВОРОТ", "Первое дело после бури. Пепел на участке лежит слишком ровно — кто-то накрыл место до меня. Дозор у ворот ещё двигается, но уже не отвечает.", "ОСМОТРИ УЧАСТОК • ДОПРОСИ ДОЗОР", "ПЕПЕЛЬНЫЙ ДОЗОР", 150, 120, 0, 1, 0x00u, false, 1, 0, -1, -1, false, 0, 0},
    {"СВИДЕТЕЛЬ БЕЗ ИМЕНИ", "ПРОТОКОЛ ДОПРОСА", "Меня нет в реестре. Для Архива я не существую — значит, могу задавать вопросы, на которые машина не отвечает. Писарь у стены готов говорить. Он просто не помнит, кому.", "ВЫВЕДИ ПИСАРЯ НА ЧИСТОСЕРДЕЧНУЮ", "ЭХО-ПИСАРЬ", 180, 150, 5, 1, 0x00u, false, 1, 0, 0, -1, false, 0, 1},
    {"ТРИ КОЛОКОЛА", "РЕЗОНАТОРЫ КОПИРУЮТ ПРИКАЗ", "Колокола бьют третий раз за ночь. Каждый удар печатает копию приказа о закрытии ворот — и ворота снова закрываются.", "СЛОМАЙ РЕЗОНАТОРЫ • ЗАГЛУШИ КОЛОКОЛА", "СТОРОЖ КОЛОКОЛЕН", 200, 160, 2, 1, 0x00u, false, 1, 0, 1, -1, false, 0, 2},
    {"ВОРОТА ДОЛГА", "ПЕЧАТЬ I • ДОЛГ", "Моргар держит ворота и слышит только собственный приказ. Баррикада сложена изнутри: он выигрывал время для эвакуации, которой не было.", "СЛОМАЙ ПЕЧАТЬ ДОЛГА", "МОРГАР", 240, 180, 0, 1, 0x00u, false, 1, 0, 2, -1, false, 0, 3},
    {"ПРОТОКОЛ ЭВАКУАЦИИ", "СЛЕД А • СВИДЕТЕЛЬ", "В караулке нашёлся список эвакуированных. Восемьдесят фамилий. Восемьдесят первая — моя, но почерк не мой.", "НАЙДИ СПИСОК • СОХРАНИ УЛИКУ", "АРХИВНЫЙ КУРЬЕР", 200, 150, 5, 2, 0x01u, false, 2, 0, 3, -1, false, 1, 4},
    {"СЕСТРА, КОТОРАЯ НЕ СЛЫШИТ", "СЛЕД А • СВИДЕТЕЛЬ", "Копия отвечает так, как её записали. Я спрашивал одно и то же восемь раз и восемь раз слышал тот же голос. Это не свидетель. Это протокол.", "ДОЖМИ КОПИЮ ДО РАСХОЖДЕНИЙ", "ГОЛОС ИЗ КОЛБЫ", 220, 170, 1, 2, 0x01u, false, 2, 0, 4, -1, false, 1, 7},
    {"ДЕЛО О СЕМИ СЕМЬЯХ", "СЛЕД А • СВИДЕТЕЛЬ", "Семь семей подали жалобу на Архив. Их показания совпадают дословно — значит, кто-то писал за них. Или они говорят одно, потому что видели одно.", "СОБЕРИ ПОКАЗАНИЯ • СВЕРЬ ГОЛОСА", "СТАРШИНА КВАРТАЛА", 220, 170, 0, 2, 0x01u, false, 2, 0, 5, -1, false, 1, 15},
    {"ПЕЧАТЬ ГОЛОСА", "СЛЕД А • СВИДЕТЕЛЬ", "Малакор бережёт копию родного голоса, потому что хоронить оригинал нечем. Я пришёл сказать ему то, что он не услышит от машины.", "СЛОМАЙ ПЕЧАТЬ ГОЛОСА", "МАЛАКОР", 240, 180, 1, 2, 0x01u, false, 2, 0, 6, -1, false, 1, 11},
    {"ЗАТОПЛЕННЫЙ РЕЕСТР", "СЛЕД А • СВИДЕТЕЛЬ", "Нижний ярус реестра затоплен. Чернила расплылись, но оттиск на обложках остался: семейные имена резали на камне, чтобы книга не решала за них.", "ПОДНИ КНИГИ • ПЕРЕПИШИ ИМЕНА", "ПЕРЕПИСЧИК", 230, 170, 4, 3, 0x03u, false, 3, 0, 7, -1, false, 1, 12},
    {"ИМЯ НА КАМНЕ", "СЛЕД А • СВИДЕТЕЛЬ", "На кладбище имена режут заново, поверх прежних. Кардинал вымарывал книги, люди резали камень. Кто-то стирал и камень.", "ЗАЩИТИ КАМЕНЬ • НАЙДИ РЕЗЧИКА", "РЕЗЧИК ИМЁН", 230, 170, 2, 3, 0x03u, false, 3, 0, 8, -1, false, 1, 13},
    {"ПЕЧАТЬ ИМЕНИ", "СЛЕД А • СВИДЕТЕЛЬ", "Кардинал вымарывал имена, называя забвение милосердием. Он не учёл одного: люди помнят друг друга без разрешения Архива.", "СЛОМАЙ ПЕЧАТЬ ИМЕНИ", "КАРДИНАЛ", 240, 180, 2, 3, 0x03u, false, 3, 0, 9, -1, false, 1, 17},
    {"ПЕПЕЛЬНЫЙ ТРОН", "СЛЕД А • СВИДЕТЕЛЬ", "Азарот запустил Архив, чтобы никто не исчез. Машина услышала его буквально: заперла ночь в цикле, а людей — в копиях.", "ВОЙДИ В КАБИНЕТ • ОТКРОЙ ДЕЛО АЗАРОТА", "СЕКРЕТАРЬ АРХИВА", 240, 180, 4, 4, 0x07u, false, 4, 0, 10, -1, false, 1, 18},
    {"ПИСАРЬ, КОТОРЫЙ СТЁР СЕБЯ", "СЛЕД А • СВИДЕТЕЛЬ", "В списке писцов одна строка вымарана дважды. Тот, кто стирал имена, стёр и своё имя.", "ВОССТАНОВИ СТРОКУ • ВЫЙДИ НА КАРДИНАЛА", "САМОЗВАНЫЙ ПИСАРЬ", 230, 170, 5, 4, 0x07u, false, 4, 0, 11, -1, false, 1, 14},
    {"ПЕЧАТЬ СВИДЕТЕЛЬСТВА", "СЛЕД А • СВИДЕТЕЛЬ", "Азарот хранит копии, а не людей. Печать Свидетельства — последний засов: пока она цела, Архив считает свои записи единственными свидетелями.", "СЛОМАЙ ПЕЧАТЬ СВИДЕТЕЛЬСТВА", "АЗАРОТ", 270, 200, 3, 4, 0x07u, false, 4, 0, 12, -1, false, 1, 21},
    {"ПУСТЫЕ НАРУЧНИКИ", "СЛЕД Б • КОПИЯ", "Другой след: склад наручников, все пустые. Сняли с погибших — или с тех, кто ушёл и не вернулся за ними.", "ВСКРОЙ КАМЕРУ • НАЙДИ ВЛАДЕЛЬЦЕВ", "СМОТРИТЕЛЬ СКЛАДА", 200, 150, 1, 2, 0x01u, false, 2, 0, 3, -1, false, 2, 5},
    {"КРИПТА ГОЛОСОВ", "СЛЕД Б • КОПИЯ", "В колбах последние слова. Их говорила сестра. Копия помнит фразу и не помнит, что было после неё.", "РАЗБЕЙ КОЛБЫ • СОБЕРИ ОБРАЗЦЫ", "ЭХО СЕСТРЫ", 220, 170, 5, 2, 0x01u, false, 2, 0, 14, -1, false, 2, 6},
    {"ТЁПЛЫЙ СЛЕД", "СЛЕД Б • КОПИЯ", "Морг цел. Буря его не задела: тела на месте, бирки на местах, и все даты смерти вчерашние. Кто-то ведёт учёт.", "СВЕРЬ БИРКИ • НАЙДИ УЧЁТЧИКА", "СЧЁТЧИК ТЕЛ", 210, 160, 0, 2, 0x01u, false, 2, 0, 15, -1, false, 2, 9},
    {"ПЕЧАТЬ ГОЛОСА", "СЛЕД Б • КОПИЯ", "Малакор бережёт копию родного голоса, потому что хоронить оригинал нечем. Я пришёл сказать ему то, что он не услышит от машины.", "СЛОМАЙ ПЕЧАТЬ ГОЛОСА", "МАЛАКОР", 240, 180, 1, 2, 0x01u, false, 2, 0, 16, -1, false, 2, 11},
    {"ДЕЛО О ПУСТЫХ СТРАНИЦАХ", "СЛЕД Б • КОПИЯ", "Из книги вырваны страницы — ровно те, где записаны свидетели. Пустые листы аккуратно сложены в папку с моим делом.", "НАЙДИ ЛИСТЫ • ПРОЧТИ КРАЯ", "ХРАНИТЕЛЬ ПАПОК", 220, 170, 3, 3, 0x03u, false, 3, 0, 17, -1, false, 2, 16},
    {"ХОЛОДНЫЙ СЛЕД", "СЛЕД Б • КОПИЯ", "В лаборатории пахнет озоном. На столе — журнал опытов: Архив учился говорить голосом мёртвых, пока город ещё был жив.", "ИЗЪЯТЬ ЖУРНАЛ • ЗАКРЫТЬ ОПЫТ", "ЛАБОРАНТ АРХИВА", 210, 160, 2, 3, 0x03u, false, 3, 0, 18, -1, false, 2, 8},
    {"КОЛБА № 9", "СЛЕД Б • КОПИЯ", "Колба девять пуста, но бирка цела. На бирке — мой голос. Я никогда не был здесь. Значит, был кто-то с моим голосом.", "НАЙДИ ПЛЁНКУ • ВОССТАНОВИ ЗАПИСЬ", "ХРАНИТЕЛЬ ПЛЁНКИ", 220, 170, 5, 3, 0x03u, false, 3, 0, 19, -1, false, 2, 10},
    {"КОПИЯ СВИДЕТЕЛЯ", "СЛЕД Б • КОПИЯ", "В деле подшита копия показаний — с моим голосом и моими словами. Копия утверждает, что она и есть Свидетель. Проверить можно только одним способом.", "ОТЛИЧИ КОПИЮ ОТ СЕБЯ", "КОПИЯ СВИДЕТЕЛЯ", 240, 180, 5, 4, 0x07u, false, 4, 2, 20, -1, false, 2, 19},
    {"ДОПРОС МАШИНЫ", "СЛЕД Б • КОПИЯ", "Машина не лжёт — она повторяет. Я спросил её о ночи бури и получил в ответ собственный рапорт, написанный до того, как я его написал.", "ДОПРОСИ АРХИВ • НАЙДИ АВТОРА", "ПРОТОКОЛИСТ", 240, 180, 4, 4, 0x07u, false, 4, 0, 21, -1, false, 2, 20},
    {"ПЕЧАТЬ СВИДЕТЕЛЬСТВА", "СЛЕД Б • КОПИЯ", "Азарот хранит копии, а не людей. Печать Свидетельства — последний засов: пока она цела, Архив считает свои записи единственными свидетелями.", "СЛОМАЙ ПЕЧАТЬ СВИДЕТЕЛЬСТВА", "АЗАРОТ", 270, 200, 3, 4, 0x07u, false, 4, 0, 22, -1, false, 2, 21},
    {"РЕЗЕРВНАЯ КОПИЯ ПРИКАЗА", "АРХИВ ЗАЩИЩАЕТ СЕБЯ", "В сердце Архива осталась запись последнего приказа Азарота. Это не воскресший Азарот — это машинная копия его команды, и она снова запускает цикл.", "НАЙДИ И УНИЧТОЖЬ КОПИЮ", "КОПИЯ АЗАРОТА", 260, 190, 5, 5, 0x0Fu, false, 5, 2, 13, 23, false, 0, 24},
    {"ТРИ ЭХО-ПРИКАЗА", "СТРАЖИ СЕРДЦА", "Три записи защищают сердце архива. Это не воскресшие стражи — это приказы, которые никто не отменил.", "ПОБЕДИ ТРИ ЭХО", "АРХИВНЫЙ ПРОТОКОЛ", 280, 210, 4, 5, 0x0Fu, true, 5, 1, 24, -1, false, 0, 25},
    {"ПОСЛЕДНЯЯ НОЧЬ", "КОЛЬЦО РАЗМЫКАЕТСЯ", "Контур разорван, но ночь ещё держится. Последний бой не вернёт погибших. Он оставит живым право помнить самим.", "УДЕРЖИ КОЛЬЦО • ДОЖДИСЬ РАССВЕТА", "ПОСЛЕДНИЙ ПРИКАЗ", 260, 190, 0, 5, 0x0Fu, false, 5, 0, 25, -1, false, 0, 26},
    {"СОХРАНИТЬ КОПИЮ", "ВЫБОР • ПАМЯТЬ ВМЕСТО ЧЕЛОВЕКА", "Копия голоса Малакора всё ещё в Архиве. Сжечь — и он потеряет последнее, что звучит его голосом. Сохранить — и Архив получит право решать, кого помнить.", "ЗАЩИТИ АРХИВ ОТ ЗАЧИСТКИ", "ИНСПЕКТОР ЗАЧИСТКИ", 230, 170, 0, 5, 0x0Fu, false, 5, 3, 26, -1, false, 0, 22},
    {"СЖЕЧЬ КОПИЮ", "ВЫБОР • ПРАВО НА УТРАТУ", "Копия голоса Малакора всё ещё в Архиве. Некоторые вещи должны умереть полностью, иначе живые не смогут отпустить.", "УНИЧТОЖЬ КОПИЮ • ВОЗЬМИ ПЕПЕЛ", "ХРАНИТЕЛЬ КОПИЙ", 230, 170, 5, 5, 0x0Fu, false, 5, 3, 26, -1, false, 0, 23},
    {"ПРАВО ПОМНИТЬ", "ДЕЛО ЗАКРЫТО", "Дело закрыто, а не выиграно. Мёртвые остались мёртвыми. Живые снова могут помнить их без разрешения машины.", "ЗАКРОЙ ДЕЛО", "ПОСЛЕДНИЙ СВИДЕТЕЛЬ", 240, 180, 5, 5, 0x0Fu, false, 5, 0, 27, 28, true, 0, 27},
};

// Every case has its own recorded narration: audio/voice/mission_brief_00…27.
// The clip name is derived from the case index, so adding a case can never silently
// reuse another case's voice line.
static std::string missionBriefKey(int mission) {
    // Файл записи привязан к делу, а не к его позиции в дереве: у каждого следа
    // свои дела, и номера в списке уже не совпадают с номерами записей.
    int clip = std::max(0, std::min(CAMPAIGN_MISSION_COUNT - 1, mission));
    const int brief = CAMPAIGN_MISSIONS[clip].brief;
    if (brief >= 0) clip = brief;
    char key[32];
    snprintf(key, sizeof(key), "mission_brief_%02d", clip);
    return std::string(key);
}

struct HeroDef {
    const char* name;
    const char* subtitle;
    const char* className;
    uint32_t classColor;
    int botDifficultyRank; // 1=easiest, 5=hardest; updated from the latest six learned-agent trials per hero
    const char* startWeaponName;
    const char* bonusDesc;
    int startWeaponId;
    float baseHp;
    float moveSpeed;
    float dmgMult;
    float cooldownMult;
    float areaMult;
    int bonusArmor;
};

static const HeroDef HERO_DEFS[5] = {
    {"ВИКТОР", "Охотник на нечисть", "ОХОТНИК", 0xFFFF6572, 4, "Оружие: Серебряные клинки", "+10% урон, +10% опыт, короткий рывок", 0, 120.0f, 185.0f, 1.05f, 0.90f, 1.0f, 0},
    {"СЕЛЕНА", "Архимаг Пепла", "МАГ", 0xFFC27AFF, 2, "Оружие: Огненный шар", "+20% радиус магии, +10% урон", 1, 105.0f, 175.0f, 1.12f, 0.95f, 1.20f, 0},
    {"БЬЁРН", "Страж Древних Руин", "СТРАЖ", 0xFFFFC45E, 1, "Оружие: Боевой топор", "+40 здоровья, +2 брони, +10% крит", 5, 165.0f, 166.0f, 1.10f, 0.98f, 1.05f, 2},
    {"АЛАРИК", "Инквизитор Солнца", "ИНКВИЗИТОР", 0xFF55DDE8, 3, "Оружие: Цепная молния", "+12% скорости, молнии цепляются дальше", 3, 115.0f, 190.0f, 1.06f, 0.90f, 1.0f, 0},
    {"НИКС", "Тень Бездны", "ТЕНЬ", 0xFF9A7BFF, 5, "Оружие: Рунический вихрь", "+15% крит, сбор самоцветов, хрупкий", 2, 95.0f, 198.0f, 1.08f, 0.95f, 1.05f, 0}
};

static constexpr int ITEM_COUNT = 40;
static constexpr int OVERKILL_SHARD_ATTACK_ID = 31;
static constexpr int CROSSFIRE_ATTACK_ID = 32;
static constexpr int EMBER_TICK_ATTACK_ID = 34;
static constexpr int FROST_SHATTER_ATTACK_ID = 35;
static constexpr int REFLECTED_PROJECTILE_ATTACK_ID = 36;
static constexpr int MAX_BUILD_ITEMS = 20;
static constexpr float RESONATOR_SPAWN_RATE_PER_SECOND = 0.07f; // +4.2 enemies/min per intact resonator; capped at eight live structures.
static constexpr int MAX_ACTIVE_RESONATORS = 8;
static constexpr int RESONATOR_DESTRUCTION_BURST_BASE = 24;
static constexpr int MATRYOSHKA_CHILDREN = 2;
static constexpr int MATRYOSHKA_ENEMY_TYPE = 10;
// A rat swarm does not hunt: it crosses the arena in one direction. It is a moving
// obstacle and a moving reward, not another pursuer — that is the point of it.
static constexpr int RAT_ENEMY_TYPE = 11;
static constexpr int MONSTER_TYPE_COUNT = 12;
static constexpr int STORY_INTRO_VERSION = 4;
static constexpr int PATCH_NOTES_VERSION = GAME_VERSION_NUM;
// Every released version has one entry in the chronicle (see versions[]/notes[]).
static constexpr int PATCH_NOTES_COUNT = 118;

// ── Arena identity ──────────────────────────────────────────────────────────────
// Each act is a different place, not the same floor with a new tint: stone, mortar,
// floor seals, fire colour, landmark props and the drifting air all change together.
// Two missions can never read as one arena re-skinned.
struct ArenaTheme {
    const char* name;
    uint32_t floorA, floorB, floorDetail, mortar;
    uint32_t runeOuter, runeInner;
    uint32_t flame, flameCore;
    uint32_t mote;         // drifting air colour
    uint32_t lightTint;    // broad ambient wash over the battlefield
    float moteSpeed;       // px/s vertical drift (negative = rises)
    int moteCount;
    int propKind;
};
static const ArenaTheme ARENA_THEMES[5] = {
    // I — Руины: cold mossy stone, violet seals, slow dust falling through brazier light.
    {"РУИНЫ", 0xFF141120, 0xFF1A1529, 0xFF211B33, 0xFF0B0813,
     0xFF765BC0, 0xFFAE91FF, 0xFFFF7018, 0xFFFFE050,
     0xFFC7BAFF, 0xFF2B2350, 34.0f, 26, 0},
    // II — Крипта: wet bone-stained brick, candlelight, dry ash sifting down.
    {"КРИПТА", 0xFF1A1018, 0xFF22131C, 0xFF301722, 0xFF0D070C,
     0xFFB54B62, 0xFFFF8290, 0xFF7CE4F0, 0xFFE8FFFF,
     0xFFE8D6C4, 0xFF3A1722, 22.0f, 30, 1},
    // III — Собор: teal glass and light shafts, cold blue fire, motes rising.
    {"СОБОР", 0xFF0D1A22, 0xFF122330, 0xFF193444, 0xFF05111A,
     0xFF38A5B8, 0xFF80E8F2, 0xFF6AD8FF, 0xFFE0FBFF,
     0xFF9FE8FF, 0xFF123A4E, -18.0f, 34, 2},
    // IV — Пепельный трон: charcoal and ember, fast rising sparks.
    {"ПЕПЕЛЬНЫЙ ТРОН", 0xFF1E120D, 0xFF27160E, 0xFF3C2112, 0xFF120A06,
     0xFFD48B3A, 0xFFFFC974, 0xFFFF6A18, 0xFFFFD070,
     0xFFFFB070, 0xFF48200E, -46.0f, 40, 3},
    // V — Сердце Архива: ink-blue stacks, pale glyph light, drifting pages.
    {"СЕРДЦЕ АРХИВА", 0xFF111027, 0xFF171439, 0xFF221D4C, 0xFF080617,
     0xFF6E7BE8, 0xFFB9C6FF, 0xFFA8C8FF, 0xFFFFFFFF,
     0xFFDCE2FF, 0xFF1B1A46, -8.0f, 28, 4},
};

// Monster rosters are per place, not global: the crypt sends shields and brutes, the
// cathedral sends shooters, the ash throne sends heavy elites. This is what makes one
// fight feel different from the next, not only the wallpaper behind it.
static int pickActMonsterType(int theme, int maxType) {
    static const int ROSTER[5][MONSTER_TYPE_COUNT] = {
        {50, 30, 20,  0,  0,  0,  0,  0,  0, 0, 0},
        {18, 26, 14, 20, 12, 10,  0,  0,  0, 0, 0},
        {10, 14,  8, 20, 10, 10, 16,  0, 12, 0, 0},
        {14,  8, 12, 12, 20, 10, 14, 10,  0, 0, 0},
        {10,  8, 10, 14, 16, 10, 10, 10, 10, 2, 0},
    };
    const int t = std::max(0, std::min(4, theme));
    const int cap = std::max(0, std::min(MONSTER_TYPE_COUNT - 1, maxType));
    int total = 0;
    for (int i = 0; i <= cap; ++i) total += ROSTER[t][i];
    if (total <= 0) return rand() % (cap + 1);
    int roll = rand() % total;
    for (int i = 0; i <= cap; ++i) {
        roll -= ROSTER[t][i];
        if (roll < 0) return i;
    }
    return cap;
}

// Each family dies in its own way: bone shards, slime splatter, ash, sparks, pages.
struct DeathBurst { uint32_t color; uint32_t accent; int count; };
static DeathBurst monsterDeathBurst(int type, bool elite) {
    static const DeathBurst bursts[MONSTER_TYPE_COUNT] = {
        {0xFFFF8B60, 0xFF7A3A2A, 10}, // 0 — ember imp
        {0xFFE8E0C8, 0xFF6B6478, 11}, // 1 — skeleton: bone shards
        {0xFF72E05C, 0xFF2F6B34, 10}, // 2 — slime: green splatter
        {0xFFB08CFF, 0xFF4A3570, 10}, // 3 — cultist: violet sparks
        {0xFFC8A070, 0xFF5A3F28, 12}, // 4 — brute
        {0xFF7CE8FF, 0xFF2A6C86, 10}, // 5 — frost wraith
        {0xFFFFD06A, 0xFF7A5A24, 10}, // 6 — warden
        {0xFFFF6A6A, 0xFF6B1F2A, 13}, // 7 — heavy knight
        {0xFF9AD8FF, 0xFF2C4C7A, 10}, // 8 — archer
        {0xFFFF9A3A, 0xFF6B3412, 14}, // 9 — ogre
        {0xFFFFC65E, 0xFF6B4A1C, 11}, // 10 — matryoshka
        {0xFFB8A48C, 0xFF573F32, 7},  // 11 — rat: dust and a thin tail of fur
    };
    DeathBurst b = bursts[std::max(0, std::min(MONSTER_TYPE_COUNT - 1, type))];
    if (elite) { b.count += 6; b.accent = 0xFFFFC64A; }
    return b;
}

struct ItemDef {
    int id;
    bool isWeapon;
    const char* name;
    const char* desc;
};

static const ItemDef ITEM_DEFS[ITEM_COUNT] = {
    // 0..6: Active Weapons
    {0, true,  "Серебряные клинки", "Выпускает быстрые самонаводящиеся клинки в ближайших врагов."},
    {1, true,  "Огненный шар",      "Взрывной снаряд, наносящий огромный урон по области вокруг цели."},
    {2, true,  "Рунический вихрь",  "Магические сферы вращаются вокруг героя, разрезая и отталкивая врагов."},
    {3, true,  "Цепная молния",     "Бьёт с небес разрядами молнии по самым опасным монстрам поблизости."},
    {4, true,  "Кровавая аура",     "Создаёт вокруг героя защитный круг, замедляющий и сжигающий врагов."},
    {5, true,  "Боевой топор",      "Тяжёлые топоры летят по дуге и пробивают насквозь целые толпы врагов."},
    {6, true,  "Ледяное кольцо",    "Выпускает расширяющуюся волну льда, замораживающую монстров вокруг."},
    // 7..12: Passive Artifacts
    {7, false, "Книга мудрости",    "+10% к получаемому опыту за ранг."},
    {8, false, "Сапоги ветра",      "+14% скорости за ранг и более быстрый рывок."},
    {9, false, "Магнитный амулет",  "+45% к радиусу сбора самоцветов и золота за ранг."},
    {10,false, "Сердце титана",     "За ранг: +40 макс. HP и +1.5 HP/с регенерации."},
    {11,false, "Перчатка берсерка", "За ранг: +12% урона и +8% шанса крита."},
    {12,false, "Часы вечности",     "За ранг: +9% частоты атак и +10% радиуса оружия."},
    {13,false, "Проклятая монета",  "+15% золота за ранг, но элитные враги опаснее."},
    {14,false, "Пепел феникса",    "Одно воскрешение за забег; ранг усиливает запас HP."},
    {15,false, "Око Бездны",       "За ранг: +6% заряда суперумения и +4% крита."},
    {16,false, "Печать Пепельного Сердца", "Тир II • за ранг: +11% урона и +5% к суперзаряду."},
    {17,false, "Кровавый Осколок Луны", "Тир III • +30% к криту за ранг. Избыток шанса — суперкрит (×4 урон)."},
    {18,false, "Линза Разлома", "Тир IV • за ранг: +14% радиуса и +10% частоты атак."},
    {19,false, "Клятва Последнего Рассвета", "Тир V • за ранг: +20% урона боссам и +5 брони."},
    {20,false, "Клеймо Охотника", "Тир II • за ранг: +12% урона элитным врагам."},
    {21,false, "Руна Возврата", "Тир III • каждые 10/8/6 убийств мгновенно возвращают рывок."},
    {22,false, "Сердце Последнего Эха", "Тир IV • убийство элиты или босса восстанавливает 2% макс. HP за ранг."},
    {23,false, "Покров Междумирья", "Тир V • щит делит следующий удар пополам; ранг ускоряет заряд."},
    {24,true, "Осколок Кометы", "Тир II • быстрый самонаводящийся выпад; ранг добавляет цель и силу."},
    {25,true, "Рунный Разворот", "Тир III • круговой импульс поражает ближайших врагов вокруг героя."},
    {26,true, "Копьё Разлома", "Тир IV • цепной разряд прыгает по нескольким целям."},
    {27,true, "Звезда Пепла", "Тир V • редкий мощный метеор взрывается в скоплении врагов."},
    {28,false,"Зеркало уклонения", "Тир III • уклонения рывком заряжают эхо; 4/3/2 заряда дают круговой удар."},
    {29,false,"Печать Разоблачения", "Тир IV • 4/3/2 удара по одной цели за 2,5 с: +10/14/18% урона на 1,9/2,2/2,5 с."},
    {30,false,"Чаша Переполнения", "Тир IV • избыток лечения даёт щит; тот поглощает урон раньше Покрова."},
    {31,false,"Осколок Последнего Удара", "Тир IV • перебор урона убийства бьёт до 3 врагов; цепочки нет."},
    {32,false,"Ключ Пепельного Ритма", "Тир IV • сбор самоцветов опыта заряжает всплеск, ускоряющий готовые атаки."},
    {33,false,"Призма Перекрёстного Огня", "Тир IV • разные активные атаки по одной цели запускают рикошет по толпе."},
    {34,false,"Угольный Фитиль", "Тир IV • крит активного оружия поджигает обычного врага; ожог не складывается."},
    {35,false,"Осколок Оттепели", "Тир IV • после оттепели: 6/8/10 урона по 2/3/4 соседям."},
    {36,false,"Печать Обратного Тока", "Тир IV • перехватывает обычный снаряд и направляет 8/12/16 урона во врага."},
    {37,false,"Знак Палача", "Тир IV • активное оружие казнит цель при 8/12/16% здоровья; боссы неуязвимы."},
    {38,false,"Печать Возмездия", "Потеря HP заряжает следующий активный удар: +35/50/65%."},
    {39,false,"Руна Свидетеля", "Тир IV • разрушение резонатора даёт +8/12/16% заряда суперумения."}
};

struct MetaUpgradeDef {
    const char* name;
    const char* desc;
    int maxRank;
    int baseCost;
    int costStep;
};

static const MetaUpgradeDef META_DEFS[6] = {
    {"Сила клинка",     "+4% ко всему наносимому урону за эффективный ранг; после XII вклад снижен", 50, 650, 0},
    {"Кровь титана",    "+10 к стартовому запасу здоровья за эффективный ранг; после XII вклад снижен", 50, 730, 0},
    {"Крылья ветра",    "+3% к скорости движения за эффективный ранг; после XII вклад снижен", 50, 700, 0},
    {"Древний магнит",  "+10% к радиусу сбора и +5% золота за эффективный ранг; после XII вклад снижен", 50, 660, 0},
    {"Благословение",   "+5% к опыту и +2% к криту за эффективный ранг; после XII вклад снижен", 50, 760, 0},
    {"Кованая броня",   "Снижает входящий урон на 0.8 за эффективный ранг; после XII вклад снижен", 50, 900, 0}
};

static constexpr int ENEMY_OATH_COUNT = 6;
static const MetaUpgradeDef ENEMY_OATHS[ENEMY_OATH_COUNT] = {
    {"Кровавый договор", "+12% HP врагов и +10% добычи золота за ранг", 10, 420, 0},
    {"Охотничий знак", "+8% скорости врагов и +8% добычи золота за ранг", 10, 390, 0},
    {"Чёрный арсенал", "+10% урона врагов и +12% добычи золота за ранг", 10, 450, 0},
    {"Марш голодной стаи", "+6% частоты появления врагов и +10% золота за ранг", 10, 520, 0},
    {"Знак добытчика", "+1 п.п. к шансу элиты и +12% золота за ранг", 10, 480, 0},
    {"Пепельная свора", "+5% HP, +3% скорости врагов и +18% золота за ранг", 10, 560, 0}
};

struct PendingMatryoshkaSplit { float x, y; };

struct Monster {
    int uniqueId;
    float x, y;
    float vx, vy;
    int facing = 1; // stable horizontal direction; prevents frame-to-frame sprite flipping
    int type;       // 0..10 regular, or bossType if isBoss
    bool isBoss;
    bool isElite;
    int bossType;   // 0..3
    bool isSmart = false; // Rare 1% tactical variant; offline bots live elsewhere.
    int smartTactic = 0;  // 0 lead-shot, 1 intercept, 2 projectile dodge
    bool isResonator = false;
    bool isMatryoshka = false;
    bool iceFractureArmed = false; // Item 35: only Frost Nova freezes can release one non-recursive shatter pulse.
    bool isTrioBoss = false;
    bool isMissionFinalBoss = false;
    bool fromHunt = false;   // spawned by the quiet-hunt pack (the only pack that can be "cleared")
    bool isRat = false;      // swarm runner: flies in one direction, never chases
    float ratDirX = 1.0f, ratDirY = 0.0f;
    float ratLife = 0.0f;    // seconds left before the swarm leaves the field
    float ratPhase = 0.0f;   // per-rat animation offset for the scurrying legs
    float ratHeading = 0.0f; // current heading in radians: the sprite turns with it
    int bossPhase = 1; // 1 hunt, 2 assault, 3 fury; determined by remaining health.
    bool bossReinforced = false; // returning guardian: same silhouette, harder fight.
    float phaseInvulnTimer = 0.0f; // Brief phase-transition shield, prevents same-frame burst skips.
    int exposeHitStacks = 0; // Repeated hits build a target-specific weak-point mark (item 29).
    float exposeBuildTimer = 0.0f;
    float exposeTimer = 0.0f;
    uint16_t crossfireWeaponMask = 0; // Distinct active weapons that struck during the short crossfire window (item 33).
    float crossfireWindowTimer = 0.0f;
    float crossfireCooldown = 0.0f;
    float emberTimer = 0.0f;
    float emberPulseTimer = 0.0f;
    float emberDamagePerTick = 0.0f;
    int emberTicksRemaining = 0;
    bool isShieldBearer = false;
    int shieldHits = 0;
    float shieldFacingX = 0.0f, shieldFacingY = 1.0f;
    float smartDodgeTimer = 0.0f;
    float smartThinkTimer = 0.0f;
    float smartDodgeX = 0.0f, smartDodgeY = 0.0f;
    float specialTimer = 0.0f;
    float abilityTimer = 0.0f;
    float abilityWindup = 0.0f;
    float abilityActive = 0.0f;
    float abilityDirX = 0.0f, abilityDirY = 0.0f;
    float abilityTargetX = 0.0f, abilityTargetY = 0.0f;
    float hp, maxHp;
    float speed;
    float damage;
    float radius;
    float hitFlashTimer;
    float freezeTimer;
    float shootTimer;
    float animOffset;
};

struct Projectile {
    float x, y;
    float vx, vy;
    int weaponId;   // 0=blade, 1=fireball, 5=axe, 99=enemy dark orb
    float damage;
    float radius;
    float life;
    int pierce;
    bool isEnemy;
    bool isCrit;
    int projectileKind = 0; // 0 dark orb, 1 bone splinter, 2 homing plague, 3 ash seed, 5 blood crescent
    bool isBossProjectile = false;
    float trailTimer = 0.0f;
    std::vector<int> hitIds;
};

struct Pickup {
    float x, y;
    int type;       // 0=blue gem, 1=green gem, 2=purple gem, 3=gold coin, 4=heal potion, 5=vacuum magnet, 6=boss chest, 7=rare extraction portal
    int value;
    float bobTime;
    bool magnetized;
    float age = 0.0f;
};

struct Particle {
    float x, y;
    float vx, vy;
    float life, maxLife;
    uint32_t color;
    int size;
    bool additive;
    int style = 0; // 0 spark, 1 shard, 2 ember, 3 rune mote, 4 ash/smoke, 5 trail streak
    float rotation = 0.0f;
    float spin = 0.0f;
    float stretch = 1.0f;
    float scale = 1.0f;
};

struct FloatingText {
    float x, y;
    float vy;
    float life, maxLife;
    std::string text;
    uint32_t color;
    bool big;
};

struct LightningArc {
    float x0, y0, x1, y1;
    float life;
};

struct FrostWave {
    float x, y;
    float radius, maxRadius;
    float life, maxLife;
};

struct Shockwave {
    float x, y;
    float radius, maxRadius;
    float life, maxLife;
    uint32_t color;
};

struct SkillImpactMark {
    float x, y;
    float life, maxLife;
    int attackId;
    bool critical;
};

struct BossHazard {
    float x, y;
    float radius;
    float warningTimer;
    float life;
    float tickTimer;
    float damage;
    int bossType;
    int hazardType = 0; // 0 poison, 1 ash fire, 2 frost
};

struct BalanceConfig {
    // Conservative shipped baseline; the more punishing lab winner is documented but not forced on players.
    float spawnMultiplier = 1.15f;
    float enemyHpMultiplier = 0.90f;
    float enemyDamageMultiplier = 0.90f;
    float playerDamageMultiplier = 1.0f;
    float experienceMultiplier = 1.0f;
};

struct GameOptions {
    uint32_t magic = 0x53484346; // 'SHCF'
    int sfxEnabled = 1;
    int musicEnabled = 1;
    int hitEffectsEnabled = 1;
    int screenShakeEnabled = 1;
    int advisorEnabled = 1;
    int hardModeEnabled = 0;
    uint32_t patchNotesHiddenVersion = 0;
    int projectileTrailsEnabled = 1;
    float sfxVolume = 0.65f;
    float musicVolume = 0.42f;
    int messageSpeechEnabled = 1; // read combat messages aloud (SAPI)
    int skillIconStyle = 0; // 0 = вектор, 1 = ИИ-арт, 2 = набор «цвет».
};

struct SaveData {
    uint32_t magic = 0x53484457; // 'SHDW'
    int gold = 0;
    int storyChapter = 0;
    int bossesDefeated = 0;
    int bestTimeSec = 0;
    int bestKills = 0;
    int totalRuns = 0;
    int metaRanks[6] = {0, 0, 0, 0, 0, 0};
    int seenIntroCutscene = 0;
    int seenTwistCutscene = 0;
    int seenEndingCutscene = 0;
    uint32_t saveVersion = 8;
    int enemyOathRanks[3] = {0, 0, 0};
    // Appended in v5 for binary compatibility: narrative evidence and permanently broken seals.
    uint32_t storySealMask = 0;
    // Low 4 bits: discovered evidence. Bits 4–5: completed Archive / Heart of Archive missions.
    uint32_t storyEntryMask = 0;
    // v6 adds three more contracts at the tail so existing saved story masks keep their offsets.
    int enemyOathRanksAdditional[3] = {0, 0, 0};
    // v7: campaign cases. Bit i = case i closed for good (30 cases, still one 32-bit word).
    // Appended at the very tail so every v6 offset above stays valid.
    uint32_t campaignMissionsDone = 0;
    // v7: bit (act-1) = the opening scene of that act has been shown.
    uint32_t seenActIntros = 0;
    // v7 (tail): сколько раз кампания начиналась заново через «НОВАЯ НОЧЬ».
    // Каждый ресет очищает маску дел и умножает золото в делах кампании.
    int campaignCycle = 0;
    // v8 (tail): какой след выбран в этом прохождении. 0 — ещё не выбран,
    // 1 — «СВИДЕТЕЛЬ», 2 — «КОПИЯ». Второй след закрыт до новой ночи.
    int campaignTrack = 0;
};

struct UIButton {
    int x, y, w, h;
    const char* label;
    int id;
};

struct ChestReward {
    int itemId = -1;
    int fromLevel = 0;
    int toLevel = 0;
};

struct Game {
    static int bossProjectileKind(int bossType) {
        switch (bossType % 6) {
            case 0: return 1; // Morgar: bone splinters
            case 1: return 2; // Malakor: homing plague orbs
            case 2: return 5; // Cardinal: blood crescents
            case 4: return 1; // Protocolist: ruled splinters that never curve
            case 5: return 5; // Double: mirrored glass crescents
            default: return 3; // Azaroth: delayed ash seeds
        }
    }

    GameState state = STATE_MENU;
    GameState settingsReturnState = STATE_MENU;
    int bossTestMusicMode = 2; // F12 test arena: 1=horde context, 2=boss context.
    SoundEngine sound;
    int gameMode = MODE_STORY;
    int mainMenuFocus = 0; // Radial menu: Story, Endless, Forge, Arsenal, Hall, Settings, Exit.
    int shopTab = 0; // 0 permanent forge, 1 enemy oaths
    SaveData save;
    GameOptions options;
    BalanceConfig balance;
    bool simulationMode = false; // Disables disk writes in the offline balance lab.

    // Input state
    bool keyW = false, keyA = false, keyS = false, keyD = false;
    bool keyUp = false, keyDown = false, keyLeft = false, keyRight = false;
    bool mouseLeftDown = false, mouseRightDown = false;
    bool mouseClicked = false;
    bool mouseMovedSinceLaunch = false;
    int mouseX = SCREEN_W / 2, mouseY = SCREEN_H / 2;
    int mouseWheelDelta = 0;
    int arsenalScrollRow = 0;
    bool requestExit = false;
    bool exitConfirmationOpen = false;
    int exitDialogFocus = 1; // Default safely to “Stay”.
    bool requestFullscreenToggle = false;
    bool isFullscreen = false;

    // Hero & Run state
    int selectedHero = 0;
    int selectedStoryMission = 0;
    int storyMissionCursor = 0;
    int briefPreviewMission = -1; // which case narration was last read out in the select screen
    float pendingObjectiveVoiceTimer = 0.0f; // speak the objective once the case narration ends
    int campaignActView = 1;   // which act the mission list is showing
    int helpPage = 0;          // 0 = арсенал, 1 = словарь терминов
    int glossaryScroll = 0;

    bool storyMissionBossSpawned = false;
    bool missionCompletePending = false;
    bool evolvedWeapons[7] = {false};
    float superCharge = 0.0f;
    bool superActive = false;
    float superTimer = 0.0f;
    std::string storyLine;
    float storyTimer = 0.0f;
    std::string storySpeaker;      // registry key of the message now on screen
    float storyDuration = 0.0f;    // full lifetime, used for the fade envelope
    std::string announceLastText;  // de-duplicates the speaker on repeated boss lines
    float announceLastAt = -100.0f;
    // Описание дела читается в экране выбора, а затем ещё раз при старте забега.
    // Запоминаем, какое дело и когда было озвучено, чтобы не читать его дважды.
    int briefSpokenMission = -1;
    float briefSpokenAt = -1000.0f;
    MessageRegistry messages;      // every shown message is accounted for here
    int voiceClips = 0;            // how many registry rows have a recorded phrase
    int voiceMissing = 0;

    // Cheap disk check (cached per key), so dropping a new clip into audio/voice/
    // and reopening the settings immediately updates the F3 counters.
    void refreshVoiceStats() {
        voiceClips = 0;
        for (int i = 0; i < GMSG_COUNT; ++i) {
            if (!GAME_MESSAGES[i].voiced) continue;
            if (sound.hasVoiceClip(GAME_MESSAGES[i].key)) ++voiceClips;
        }
        voiceMissing = MessageRegistry::voicedDefs() - voiceClips;
        messages.missingClips = voiceMissing;
    }
    bool debugOverlay = false;     // F3 technical read-out
    // Дебаг-меню (F9 или клавиша `). Раньше оно открывалось только по коду v404,
    // и со стороны выглядело так, будто отладки в игре нет вообще. Теперь F9
    // открывает меню сразу, а бой под ним замирает.
    bool cheatMenuOpen = false;
    int cheatSelection = 0;        // выбранная строка: стрелки + Enter
    int cheatBossCycle = 0;        // какой страж будет призван следующим
    bool cheatGodMode = false;
    float fpsSmooth = 60.0f;
    float frameMsSmooth = 16.7f;
    int debugPeakEntities = 0;
    float buildTierUpTimer = 0.0f;
    int buildTierUpValue = 1;
    float playerDeathTimer = 0.0f;
    bool runRecorded = false;
    float globalTime = 0.0f;
    float runTime = 0.0f;
    float resonatorSpawnTimer = 22.0f;
    int resonatorSpawnIndex = 3;
    float phoenixReviveTimer = 0.0f;
    static constexpr float PHOENIX_REVIVE_DURATION = 2.25f;
    float portalSpawnTimer = 0.0f;
    bool portalSpawned = false;
    float victoryMusicTimer = 0.0f;
    float magnetAudioTimer = 0.0f;
    int waveNumber = 1;
    int nextBreakthroughLevel = 8;
    int breakthroughCount = 0;
    bool breakthroughActive = false;
    bool hordeMusicActive = false; // hysteresis: dense crowds can trigger/hold the horde arrangement
    int musicCrowdTempoTier = 0; // 0 slow, 1 medium, 2 fast, 3 very fast; follows live monster count
    bool breakthroughEarlyHit = false;
    bool breakthroughQueued = false;
    float breakthroughTimer = 0.0f;
    float breakthroughEscalation = 0.0f;
    float recoveryTimer = 0.0f;
    int lastBreakthroughWarningLevel = -1;
    float hitFxWindowTimer = 0.0f;
    int hitFxCountInWindow = 0;
    int hitFxClusterEmitted = 0;
    int lastDifficultyCycle = -1;
    int lastDifficultySegment = -1;
    int lastSurgeWarningCycle = -1;
    std::string difficultyBanner;
    float difficultyBannerTimer = 0.0f;
    bool superResolvingUltimate = false;
    int kills = 0;
    int runGold = 0;
    int runBossKills = 0;
    int dashRuneKillCounter = 0;
    int echoStepCharges = 0;
    bool dashEvadeTriggered = false;
    float veilCooldown = 0.0f;
    float retaliationChargeTimer = 0.0f; // Item 38: one reactive empowered active-weapon hit.
    float totalDamageDealt = 0.0f;
    float dpsWindowDamage = 0.0f;
    float currentDps = 0.0f;
    float dpsTimer = 0.0f;
    float recentDpsSamples[60] = {};
    float recentDpsSum = 0.0f;
    int recentDpsCount = 0;
    int recentDpsIndex = 0;

    float playerX = 0.0f, playerY = 0.0f;
    float playerVelocityX = 0.0f, playerVelocityY = 0.0f;
    int playerFacing = 1;
    float playerHp = 120.0f, playerMaxHp = 120.0f;
    float playerWard = 0.0f, wardFlashTimer = 0.0f;
    float overkillShardCooldown = 0.0f;
    float crossfireGlobalCooldown = 0.0f;
    float emberGlobalCooldown = 0.0f;
    float frostShatterCooldown = 0.0f;
    float projectileParryCooldown = 0.0f;
    int ashRhythmSparkCount = 0;
    float ashRhythmFlashTimer = 0.0f;
    float playerHurtTimer = 0.0f;
    float playerSlowTimer = 0.0f;
    float dashTimer = 0.0f;
    float dashCooldown = 0.0f;
    float dashDirX = 1.0f, dashDirY = 0.0f;
    std::vector<int> dashHitIds;
    int nextMonsterId = 1;

    int playerLevel = 1;
    int playerXp = 0;
    int xpToNext = 25;

    int itemLevels[ITEM_COUNT] = {0};
    float weaponTimers[7] = {0};
    float tierWeaponTimers[4] = {0};
    float orbitalAngle = 0.0f;
    float auraPulse = 0.0f;

    // Upgrade choices (3 item indices)
    int upgradeChoices[4] = {0, 1, 2, 3};
    int hoveredCard = -1;
    bool upgradeRerollUsed = false;

    // Chest rewards
    std::vector<ChestReward> chestRewards;
    int smartOneTapCount[3] = {0, 0, 0};
    float smartSpawnChance[3] = {0.01f, 0.01f, 0.01f};
    float smartStrengthMultiplier[3] = {1.0f, 1.0f, 1.0f};
    int pendingResonatorBurst = 0;
    int killWindowStartCount = 0;
    float killWindowTimer = 0.0f;
    float measuredKillsPerSecond = 0.0f;
    float lowActionTimer = 0.0f;
    float eventCooldown = 35.0f;
    float squadTimer = 16.0f;      // next themed squad push
    float ratTimer = 34.0f;        // next rat swarm crossing
    // Quiet hunt ("тихая охота") is an *opportunity*, not a wall: a finite pack walks the
    // trail, drops coins as it dies, and pays a bounty if the whole pack is cleared.
    // The breakthrough is the wall: timed, unavoidable and it blocks progression.
    bool huntActive = false;
    int huntPackTotal = 0;
    int huntPackRemaining = 0;
    float huntTimer = 0.0f;
    int huntCoinsDropped = 0;
    int pendingStoryChapter = -1;
    int cutsceneId = 0;
    float cutsceneTimer = 0.0f;
    GameState cutsceneReturnState = STATE_PLAYING;
    bool patchNotesDoNotShowAgain = false;
    int patchNotesPage = 0;
    // Прокрутка внутри заметки: заметка 2.10.23 длиннее панели, а раньше текст
    // просто уезжал за нижнюю кромку и его нельзя было дочитать.
    float patchNotesScroll = 0.0f;
    int patchNotesScrollMax = 0;
    int hallPage = 0; // 0=Hall of Fame, 1=Witness Ledger
    int settingsVolumeFocus = 0; // 0=SFX, 1=music volume
    float bossDeferredTimer = 0.0f;
    int currentRunChapter = 0;
    bool bossTrioStarted = false;
    int bossTrioSpawned = 0;
    int trioBossesDefeated = 0;
    bool endingCutscenePending = false;
    bool campaignEndPending = false;
    bool storyFinaleBlockedNoticeShown = false;
    int buildLimitBonus = 0;
    int chestBonusGold = 0;

    // World entities
    std::vector<Monster> monsters;
    std::vector<PendingMatryoshkaSplit> pendingMatryoshkaSplits;
    std::vector<Projectile> projectiles;
    std::vector<Pickup> pickups;
    std::vector<Particle> particles;
    std::vector<FloatingText> floatTexts;
    std::vector<LightningArc> lightningArcs;
    std::vector<FrostWave> frostWaves;
    std::vector<Shockwave> shockwaves;
    std::vector<SkillImpactMark> skillImpactMarks;
    std::vector<BossHazard> bossHazards;

    float spawnAccumulator = 0.0f;
    int pendingSummons = 0;
    int pendingSummonType = 1;
    int nextBossIndex = 0;
    float bossWarningTimer = 0.0f;
    std::string bossWarningName;
    float bossPhaseEventTimer = 0.0f;
    // After a boss appears (or changes phase) the arena is cleared and mobs come back
    // five times as dense for one second: the boss entrance must read as a beat change.
    float bossRushTimer = 0.0f;
    static constexpr float BOSS_RUSH_SECONDS = 1.0f;
    static constexpr float BOSS_RUSH_MULTIPLIER = 5.0f;
    int bossPhaseEventType = 0;
    int bossPhaseEventNumber = 1;
    float screenShake = 0.0f;

    void init() {
        sound.init();
        loadOptions();
        // Recorded phrases live in audio/voice/; count what is actually there for F3.
        sound.setVoiceEnabled(options.messageSpeechEnabled != 0);
        refreshVoiceStats();
        loadSave();
        initRun(0, false);
        patchNotesPage = PATCH_NOTES_COUNT - 1;
        patchNotesScroll = 0.0f;
        state = options.patchNotesHiddenVersion == PATCH_NOTES_VERSION ? STATE_MENU : STATE_PATCH_NOTES;
    }


    FILE* openSaveFile(bool writeMode) {
#ifdef _WIN32
        wchar_t modulePath[MAX_PATH + 2] = {};
        DWORD length = GetModuleFileNameW(NULL, modulePath, MAX_PATH);
        if (length > 0 && length < MAX_PATH) {
            std::wstring path(modulePath, length);
            size_t slash = path.find_last_of(L"\\/");
            if (slash == std::wstring::npos) path.clear();
            else path.resize(slash + 1);
            path += L"shadow_survivors.sav";
            return _wfopen(path.c_str(), writeMode ? L"wb" : L"rb");
        }
#endif
        return fopen("shadow_survivors.sav", writeMode ? "wb" : "rb");
    }

    FILE* openOptionsFile(bool writeMode) {
#ifdef _WIN32
        wchar_t modulePath[MAX_PATH + 2] = {};
        DWORD length = GetModuleFileNameW(NULL, modulePath, MAX_PATH);
        if (length > 0 && length < MAX_PATH) {
            std::wstring path(modulePath, length);
            size_t slash = path.find_last_of(L"\\/");
            if (slash == std::wstring::npos) path.clear();
            else path.resize(slash + 1);
            path += L"shadow_survivors.cfg";
            return _wfopen(path.c_str(), writeMode ? L"wb" : L"rb");
        }
#endif
        return fopen("shadow_survivors.cfg", writeMode ? "wb" : "rb");
    }

    void loadOptions() {
        struct LegacyOptions27 { uint32_t magic; int sfx, music, hitFx, shake, advisor; };
        struct LegacyOptions28 { uint32_t magic; int sfx, music, hitFx, shake, advisor, abyss; uint32_t hiddenVersion; };
        struct LegacyOptions294 {
            uint32_t magic;
            int sfxEnabled, musicEnabled, hitEffectsEnabled, screenShakeEnabled, advisorEnabled, hardModeEnabled;
            uint32_t patchNotesHiddenVersion;
            int projectileTrailsEnabled;
            float sfxVolume, musicVolume;
        };
        struct LegacyOptions29 {
            uint32_t magic;
            int sfxEnabled, musicEnabled, hitEffectsEnabled, screenShakeEnabled, advisorEnabled, hardModeEnabled;
            uint32_t patchNotesHiddenVersion;
            int projectileTrailsEnabled;
        };
        // The immediately preceding 2.9.70 layout; keep its 48-byte settings files readable.
        struct LegacyOptionsCurrent {
            uint32_t magic;
            int sfxEnabled, musicEnabled, hitEffectsEnabled, screenShakeEnabled, advisorEnabled, hardModeEnabled;
            uint32_t patchNotesHiddenVersion;
            int projectileTrailsEnabled;
            float sfxVolume, musicVolume;
            int musicVariant;
        };
        static_assert(sizeof(LegacyOptionsCurrent) == 48, "2.9.70 options layout changed unexpectedly");
        FILE* f = openOptionsFile(false);
        if (f) {
            fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
            if (n == (long)sizeof(GameOptions)) {
                GameOptions saved{};
                if (fread(&saved, sizeof(saved), 1, f) == 1 && saved.magic == 0x53484346) options = saved;
            } else if (n == (long)sizeof(LegacyOptionsCurrent)) {
                LegacyOptionsCurrent old{};
                if (fread(&old, sizeof(old), 1, f) == 1 && old.magic == 0x53484346) {
                    options.sfxEnabled = old.sfxEnabled; options.musicEnabled = old.musicEnabled;
                    options.hitEffectsEnabled = old.hitEffectsEnabled; options.screenShakeEnabled = old.screenShakeEnabled;
                    options.advisorEnabled = old.advisorEnabled; options.hardModeEnabled = old.hardModeEnabled;
                    options.patchNotesHiddenVersion = old.patchNotesHiddenVersion;
                    options.projectileTrailsEnabled = old.projectileTrailsEnabled;
                    options.sfxVolume = old.sfxVolume; options.musicVolume = old.musicVolume;
                    options.messageSpeechEnabled = 1;
                    options.skillIconStyle = 0;
                }
            } else if (n == (long)sizeof(LegacyOptions294)) {
                LegacyOptions294 old{};
                if (fread(&old, sizeof(old), 1, f) == 1 && old.magic == 0x53484346) {
                    options.sfxEnabled = old.sfxEnabled; options.musicEnabled = old.musicEnabled;
                    options.hitEffectsEnabled = old.hitEffectsEnabled; options.screenShakeEnabled = old.screenShakeEnabled;
                    options.advisorEnabled = old.advisorEnabled; options.hardModeEnabled = old.hardModeEnabled;
                    options.patchNotesHiddenVersion = old.patchNotesHiddenVersion;
                    options.projectileTrailsEnabled = old.projectileTrailsEnabled;
                    options.sfxVolume = old.sfxVolume; options.musicVolume = old.musicVolume;
                }
            } else if (n == (long)sizeof(LegacyOptions29)) {
                LegacyOptions29 old{};
                if (fread(&old, sizeof(old), 1, f) == 1 && old.magic == 0x53484346) {
                    options.sfxEnabled = old.sfxEnabled; options.musicEnabled = old.musicEnabled;
                    options.hitEffectsEnabled = old.hitEffectsEnabled; options.screenShakeEnabled = old.screenShakeEnabled;
                    options.advisorEnabled = old.advisorEnabled; options.hardModeEnabled = old.hardModeEnabled;
                    options.patchNotesHiddenVersion = old.patchNotesHiddenVersion;
                    options.projectileTrailsEnabled = old.projectileTrailsEnabled;
                }
            } else if (n == (long)sizeof(LegacyOptions28)) {
                LegacyOptions28 old{};
                if (fread(&old, sizeof(old), 1, f) == 1 && old.magic == 0x53484346) {
                    options.sfxEnabled = old.sfx; options.musicEnabled = old.music;
                    options.hitEffectsEnabled = old.hitFx; options.screenShakeEnabled = old.shake;
                    options.advisorEnabled = old.advisor; options.hardModeEnabled = old.abyss;
                    options.patchNotesHiddenVersion = old.hiddenVersion;
                }
            } else if (n == (long)sizeof(LegacyOptions27)) {
                LegacyOptions27 old{};
                if (fread(&old, sizeof(old), 1, f) == 1 && old.magic == 0x53484346) {
                    options.sfxEnabled = old.sfx; options.musicEnabled = old.music;
                    options.hitEffectsEnabled = old.hitFx; options.screenShakeEnabled = old.shake;
                    options.advisorEnabled = old.advisor;
                }
            }
            fclose(f);
        }
        if (!std::isfinite(options.sfxVolume)) options.sfxVolume = 0.65f;
        if (!std::isfinite(options.musicVolume)) options.musicVolume = 0.42f;
        options.sfxVolume = std::max(0.0f, std::min(1.0f, options.sfxVolume));
        options.musicVolume = std::max(0.0f, std::min(1.0f, options.musicVolume));
        // Old builds stored the music-track index here; anything non-zero simply means "on".
        options.messageSpeechEnabled = options.messageSpeechEnabled != 0 ? 1 : 0;
        options.skillIconStyle = std::max(0, std::min(2, options.skillIconStyle));
        sound.musicVariant.store(0, std::memory_order_relaxed);
        sound.enabled.store(options.sfxEnabled != 0, std::memory_order_relaxed);
        sound.musicEnabled.store(options.musicEnabled != 0, std::memory_order_relaxed);
        sound.sfxVolume.store(options.sfxVolume, std::memory_order_relaxed);
        sound.musicVolume.store(options.musicVolume, std::memory_order_relaxed);
    }

    void saveOptions() {
        if (simulationMode) return;
        options.magic = 0x53484346;
        options.sfxEnabled = sound.enabled.load(std::memory_order_relaxed) ? 1 : 0;
        options.musicEnabled = sound.musicEnabled.load(std::memory_order_relaxed) ? 1 : 0;
        options.sfxVolume = sound.sfxVolume.load(std::memory_order_relaxed);
        options.musicVolume = sound.musicVolume.load(std::memory_order_relaxed);
        options.messageSpeechEnabled = options.messageSpeechEnabled != 0 ? 1 : 0;
        FILE* f = openOptionsFile(true);
        if (f) { fwrite(&options, sizeof(options), 1, f); fclose(f); }
    }

    void loadSave() {
        struct LegacySaveV1 {
            uint32_t magic;
            int gold, bestTimeSec, bestKills, totalRuns;
            int metaRanks[6];
        };
        struct LegacySaveV3 {
            uint32_t magic;
            int gold, storyChapter, bossesDefeated, bestTimeSec, bestKills, totalRuns;
            int metaRanks[6];
            int seenIntroCutscene, seenTwistCutscene, seenEndingCutscene;
            uint32_t saveVersion;
        };
        FILE* f = openSaveFile(false);
        bool migrated = false;
        auto migrateLegacyStory = [&](int oldChapter, int oldBossCount, int oldEndingSeen) {
            int recovered = std::max(0, std::min(4, oldBossCount));
            if (oldChapter >= 4 || oldEndingSeen) recovered = 4;
            const uint32_t mask = recovered > 0 ? ((1u << recovered) - 1u) : 0u;
            save.storySealMask = mask;
            save.storyEntryMask = mask;
        };
        // Old saves only knew six cases; carry those six onto the twenty-eight-case mask so
        // a finished campaign never reopens after the update.
        auto remapCampaignMask = [&]() {
            uint32_t mask = save.storySealMask & 0x0Fu;
            if (save.storyEntryMask & STORY_ARCHIVE_MISSION_COMPLETE_BIT) mask |= 1u << 4;
            if ((save.storyEntryMask & STORY_HEART_MISSION_COMPLETE_BIT) || save.seenEndingCutscene)
                mask |= 1u << 5;
            save.campaignMissionsDone = mask;
        };
        // v8: раскладка кампании стала двумя следами, и номера дел сместились.
        // Переносим старую маску по номеру озвученного брифинга — он у каждого дела
        // свой, поэтому закрытые дела не теряются, а выбранный след восстанавливается
        // по тому, какие дела у игрока были закрыты.
        auto remapCampaignToTracks = [&](uint32_t oldMask) {
            save.campaignMissionsDone = 0;
            save.campaignTrack = 0;
            int votes[3] = {0, 0, 0};
            for (int n = 0; n < CAMPAIGN_MISSION_COUNT; ++n) {
                const int brief = CAMPAIGN_MISSIONS[n].brief;
                if (brief < 0 || !(oldMask & (1u << brief))) continue;
                int sameBrief = 0;
                for (int k = 0; k < CAMPAIGN_MISSION_COUNT; ++k)
                    if (CAMPAIGN_MISSIONS[k].brief == brief) ++sameBrief;
                if (sameBrief == 1) ++votes[CAMPAIGN_MISSIONS[n].track];
            }
            if (votes[2] > votes[1]) save.campaignTrack = 2;
            else if (votes[1] > 0) save.campaignTrack = 1;
            const int target = save.campaignTrack ? save.campaignTrack : 1;
            for (int n = 0; n < CAMPAIGN_MISSION_COUNT; ++n) {
                const int brief = CAMPAIGN_MISSIONS[n].brief;
                if (brief < 0 || !(oldMask & (1u << brief))) continue;
                const int track = CAMPAIGN_MISSIONS[n].track;
                if (track == 0 || track == target) save.campaignMissionsDone |= (1u << n);
            }
        };
        if (f) {
            fseek(f, 0, SEEK_END);
            long fileSize = ftell(f);
            rewind(f);
            if (fileSize == (long)sizeof(LegacySaveV1)) {
                // v1 stored stats in a different field order; translate rather than
                // treating old best-time data as campaign progress.
                LegacySaveV1 old = {};
                if (fread(&old, sizeof(old), 1, f) == 1 && old.magic == 0x53484457) {
                    save.gold = std::min(250, std::max(0, old.gold / 4));
                    save.bestTimeSec = old.bestTimeSec;
                    save.bestKills = old.bestKills;
                    save.totalRuns = old.totalRuns;
                    for (int i = 0; i < 6; ++i) save.metaRanks[i] = 0;
                    save.storyChapter = 0;
                    save.bossesDefeated = 0;
                    save.saveVersion = 8;
                    migrated = true;
                }
            } else if (fileSize == (long)sizeof(LegacySaveV3)) {
                LegacySaveV3 old{};
                if (fread(&old, sizeof(old), 1, f) == 1 && old.magic == 0x53484457) {
                    save.gold = old.gold; save.storyChapter = old.storyChapter; save.bossesDefeated = old.bossesDefeated;
                    save.bestTimeSec = old.bestTimeSec; save.bestKills = old.bestKills; save.totalRuns = old.totalRuns;
                    for (int i = 0; i < 6; ++i) save.metaRanks[i] = old.metaRanks[i];
                    save.seenIntroCutscene = old.seenIntroCutscene;
                    save.seenTwistCutscene = old.seenTwistCutscene;
                    save.seenEndingCutscene = old.seenEndingCutscene;
                    migrateLegacyStory(old.storyChapter, old.bossesDefeated, old.seenEndingCutscene);
                    remapCampaignMask();
                    remapCampaignToTracks(save.campaignMissionsDone);
                    save.saveVersion = 8;
                    migrated = true;
                }
            } else {
                SaveData tmp;
                size_t got = fread(&tmp, 1, sizeof(SaveData), f);
                if (got >= sizeof(uint32_t) && tmp.magic == 0x53484457) {
                    save = tmp;
                    const uint32_t loadedVersion = save.saveVersion;
                    if (got < sizeof(SaveData) || save.saveVersion < 6) {
                        // V5 already contains story masks; v6 only appends contract ranks.
                        // Preserve masks when migrating v5 and recover story only from older prefixes.
                        const size_t storyMaskEnd = offsetof(SaveData, storyEntryMask) + sizeof(save.storyEntryMask);
                        if (save.saveVersion < 5 || got < storyMaskEnd)
                            migrateLegacyStory(save.storyChapter, save.bossesDefeated, save.seenEndingCutscene);
                        if (save.saveVersion < 3) {
                            save.seenIntroCutscene = 0;
                            save.seenTwistCutscene = 0;
                            save.seenEndingCutscene = 0;
                        }
                        save.saveVersion = 7;
                        migrated = true;
                    }
                    if (loadedVersion < 7 || got < offsetof(SaveData, campaignMissionsDone)) {
                        remapCampaignMask();
                        save.saveVersion = 8;
                        migrated = true;
                    }
                    if (loadedVersion < 8 || got < offsetof(SaveData, campaignTrack)) {
                        remapCampaignToTracks(save.campaignMissionsDone);
                        save.saveVersion = 8;
                        migrated = true;
                    }
                }
            }
            fclose(f);
        }
        if (migrated) writeSave();
    }

    void writeSave() {
        if (simulationMode) return;
        FILE* f = openSaveFile(true);
        if (f) {
            fwrite(&save, sizeof(SaveData), 1, f);
            fclose(f);
        }
    }

    int storySealCount() const {
        uint32_t mask = save.storySealMask & 0x0Fu;
        int count = 0;
        for (int i = 0; i < 4; ++i) if (mask & (1u << i)) ++count;
        return count;
    }

    int storyEntryCount() const {
        uint32_t mask = save.storyEntryMask & 0x0Fu;
        int count = 0;
        for (int i = 0; i < 4; ++i) if (mask & (1u << i)) ++count;
        return count;
    }

    static int campaignActOf(int mission) {
        if (mission < 0 || mission >= CAMPAIGN_MISSION_COUNT) return 1;
        return std::max(1, std::min(CAMPAIGN_ACT_COUNT, CAMPAIGN_MISSIONS[mission].act));
    }
    static int campaignActFirstMission(int act) {
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i)
            if (CAMPAIGN_MISSIONS[i].act >= act) return i;
        return CAMPAIGN_MISSION_COUNT - 1;
    }
    static int campaignActMissionCount(int act) {
        int n = 0;
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i) if (CAMPAIGN_MISSIONS[i].act == act) ++n;
        return n;
    }
    static int campaignActProgressIndex(int mission) {
        const int act = campaignActOf(mission);
        return mission - campaignActFirstMission(act);
    }

    // The archive's secret is on the record once the player has seen the openings of Acts II–V.
    bool archiveTruthKnown() const {
        return (save.seenActIntros & 0x1Eu) != 0 || save.seenEndingCutscene != 0;
    }

    bool isCampaignMissionCompleted(int mission) const {
        if (mission < 0 || mission >= CAMPAIGN_MISSION_COUNT) return false;
        return (save.campaignMissionsDone & (1u << mission)) != 0;
    }

    const CampaignMissionDef& selectedCampaignMission() const {
        return CAMPAIGN_MISSIONS[std::max(0, std::min(CAMPAIGN_MISSION_COUNT - 1, selectedStoryMission))];
    }

    // ── Ветки ───────────────────────────────────────────────────────────────────
    // Четыре пары дел — это выбор, а не два обязательных шага. Раньше вторая ветка
    // требовалась как пререквизит, поэтому игрок проходил обе подряд, не выбирая.
    // ── Два следа вместо развилки-обманки ────────────────────────────────────
    // Раньше «ветка А / ветка Б» была парой соседних дел: обе карточки сходились
    // уже в следующем деле, поэтому выбор ничего не решал. Теперь у дел есть
    // след (track): общий (0), «СВИДЕТЕЛЬ» (1) или «КОПИЯ» (2). След выбирается
    // один раз после акта I, и до новой ночи доступен только он.
    static const char* campaignTrackName(int track) {
        if (track == 1) return "СВИДЕТЕЛЬ";
        if (track == 2) return "КОПИЯ";
        return "ОБЩИЙ";
    }
    static const char* campaignTrackLetter(int track) {
        if (track == 1) return "А";
        if (track == 2) return "Б";
        return "•";
    }
    static int campaignTrackOf(int mission) {
        if (mission < 0 || mission >= CAMPAIGN_MISSION_COUNT) return 0;
        return CAMPAIGN_MISSIONS[mission].track;
    }
    // Дело чужого следа закрыто до новой ночи; дело без следа доступно всегда.
    bool isCampaignMissionLockedByTrack(int mission) const {
        const int track = campaignTrackOf(mission);
        return track != 0 && save.campaignTrack != track;
    }
    bool isCampaignTrackChosen() const { return save.campaignTrack == 1 || save.campaignTrack == 2; }
    int campaignTrackCompletedCount() const {
        if (!isCampaignTrackChosen()) return 0;
        int n = 0;
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i)
            if (CAMPAIGN_MISSIONS[i].track == save.campaignTrack && isCampaignMissionCompleted(i)) ++n;
        return n;
    }
    void chooseCampaignTrack(int track) {
        if (track != 1 && track != 2) return;
        if (isCampaignTrackChosen()) return;
        save.campaignTrack = track;
        writeSave();
        sound.play(SFX_LEVELUP, 0.05f);
    }
    // Два последних выбора (kind 3) взаимоисключающие: на ночь даётся один ответ.
    static int campaignChoiceSibling(int mission) {
        if (mission < 0 || mission >= CAMPAIGN_MISSION_COUNT) return -1;
        const CampaignMissionDef& def = CAMPAIGN_MISSIONS[mission];
        if (def.kind != 3) return -1;
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i) {
            if (i == mission) continue;
            const CampaignMissionDef& other = CAMPAIGN_MISSIONS[i];
            if (other.kind == 3 && other.act == def.act && other.prereqA == def.prereqA) return i;
        }
        return -1;
    }
    bool isCampaignMissionLockedByChoice(int mission) const {
        const int sibling = campaignChoiceSibling(mission);
        return sibling >= 0 && isCampaignMissionCompleted(sibling);
    }

    // Единая подпись статуса для дерева и досье: одна правда вместо трёх веток if.
    const char* campaignMissionStatus(int mission) const {
        if (mission < 0 || mission >= CAMPAIGN_MISSION_COUNT) return "НЕТ ДЕЛА";
        if (isCampaignMissionCompleted(mission)) return "ПРОЙДЕНА";
        if (isCampaignMissionLockedByChoice(mission)) return "ДРУГОЙ ВЫБОР СДЕЛАН";
        if (isCampaignMissionLockedByTrack(mission)) {
            const bool pickNow = !isCampaignTrackChosen();
            if (pickNow) return campaignTrackOf(mission) == 1 ? "СЛЕД А • ВЫБЕРИ" : "СЛЕД Б • ВЫБЕРИ";
            return "ЗАКРЫТО ДО НОВОЙ НОЧИ";
        }
        if (isCampaignMissionUnlocked(mission)) return "ДОСТУПНА";
        return "ЗАКРЫТА";
    }
    // Здесь кнопка не только запускает дело, но и фиксирует выбор следа.
    bool campaignCanChooseTrackHere(int mission) const {
        return !isCampaignTrackChosen() && campaignTrackOf(mission) != 0;
    }
    void confirmCampaignSelection(int mission) {
        // «Пойти следом» = выбрать след и сразу начать дело: одно нажатие, а не два.
        if (campaignCanChooseTrackHere(mission)) chooseCampaignTrack(campaignTrackOf(mission));
        if (isCampaignMissionUnlocked(mission)) launchStoryMission(mission);
    }
    static int campaignMissionWithBrief(int brief) {
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i)
            if (CAMPAIGN_MISSIONS[i].brief == brief) return i;
        return -1;
    }

    // «×2 золота» после первого ресета, дальше ×3 и ×4 — но не больше.
    int campaignGoldMultiplier() const {
        if (gameMode != MODE_STORY) return 1;
        return 1 + std::max(0, std::min(3, save.campaignCycle));
    }
    void awardGold(int amount) {
        if (amount == 0) return;
        const int scaled = amount * campaignGoldMultiplier();
        save.gold += scaled;
        runGold += scaled;
    }

    bool isCampaignMissionUnlocked(int mission) const {
        if (mission < 0 || mission >= CAMPAIGN_MISSION_COUNT) return false;
        const CampaignMissionDef& def = CAMPAIGN_MISSIONS[mission];
        if (isCampaignMissionLockedByTrack(mission)) return false; // второй след — в новую ночь
        if (isCampaignMissionLockedByChoice(mission)) return false; // финальный выбор уже сделан
        if (def.prereqA < 0) return true; // the case that opens an act is always reachable
        // Два пререквизита — слияние следов: достаточно ЛЮБОГО из них, потому что
        // до акта V игрок доходит только одним следом из двух.
        if (isCampaignMissionCompleted(def.prereqA)) return true;
        if (def.prereqB >= 0 && isCampaignMissionCompleted(def.prereqB)) return true;
        return false;
    }

    // Which case actually opens this one — used by the dossier line "нужно закрыть дело №".
    int campaignMissingPrereq(int mission) const {
        if (mission < 0 || mission >= CAMPAIGN_MISSION_COUNT) return -1;
        const CampaignMissionDef& def = CAMPAIGN_MISSIONS[mission];
        // Слияние следов: советуем дело того следа, который игрок проходит,
        // иначе подсказка отправит его в закрытую ветку.
        for (int pass = 0; pass < 2; ++pass) {
            const int pre[2] = {def.prereqA, def.prereqB};
            for (int k = 0; k < 2; ++k) {
                if (pre[k] < 0 || isCampaignMissionCompleted(pre[k])) continue;
                const int track = campaignTrackOf(pre[k]);
                if (pass == 1 || track == 0 || track == save.campaignTrack) return pre[k];
            }
        }
        return -1;
    }

    void markCampaignMissionDone(int mission) {
        if (mission < 0 || mission >= CAMPAIGN_MISSION_COUNT) return;
        save.campaignMissionsDone |= (1u << mission);
        // Печать снимается за закрытый акт: раньше биты брались из номеров дел,
        // а с двумя следами номера у каждого свои. Считаем по факту: акт II…V
        // пройден целиком по текущему следу — бит (акт-2) в старом ниббле.
        save.storySealMask = 0;
        for (int act = 2; act <= CAMPAIGN_ACT_COUNT; ++act) {
            int total = 0, done = 0;
            for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i) {
                if (CAMPAIGN_MISSIONS[i].act != act) continue;
                if (CAMPAIGN_MISSIONS[i].track != 0 && CAMPAIGN_MISSIONS[i].track != save.campaignTrack) continue;
                ++total;
                if (isCampaignMissionCompleted(i)) ++done;
            }
            if (total > 0 && done == total) save.storySealMask |= (1u << (act - 2));
        }
        // Старые биты «архив/сердце» по-прежнему нужны старым сборкам и хукам финала.
        if (CAMPAIGN_MISSIONS[mission].act >= 4) save.storyEntryMask |= STORY_ARCHIVE_MISSION_COMPLETE_BIT;
        if (CAMPAIGN_MISSIONS[mission].act >= 5) save.storyEntryMask |= STORY_HEART_MISSION_COMPLETE_BIT;
    }

    int campaignCompletedCount() const {
        int n = 0;
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i) if (isCampaignMissionCompleted(i)) ++n;
        return n;
    }
    int campaignActCompletedCount(int act) const {
        int n = 0;
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i)
            if (CAMPAIGN_MISSIONS[i].act == act && isCampaignMissionCompleted(i)) ++n;
        return n;
    }
    bool isCampaignActUnlocked(int act) const {
        if (act <= 1) return true;
        // Акт открыт, если открыто хоть одно его дело на СВОЕЙ полосе: первое дело
        // акта принадлежит следу А, и для игрока на следе Б оно закрыто.
        const int lane = campaignLaneTrack();
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i) {
            if (CAMPAIGN_MISSIONS[i].act != act) continue;
            if (CAMPAIGN_MISSIONS[i].track != 0 && CAMPAIGN_MISSIONS[i].track != lane) continue;
            if (isCampaignMissionUnlocked(i)) return true;
        }
        return false;
    }
    int campaignActSealCount(int act) const {
        int n = 0;
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i)
            if (CAMPAIGN_MISSIONS[i].act == act && CAMPAIGN_MISSIONS[i].bossType < 4 &&
                isCampaignMissionCompleted(i) && CAMPAIGN_MISSIONS[i].prereqB < 0) ++n;
        return n;
    }
    int campaignMissionMinutes(int mission) const {
        if (mission < 0 || mission >= CAMPAIGN_MISSION_COUNT) return 0;
        return (CAMPAIGN_MISSIONS[mission].durationSeconds + 59) / 60;
    }
    int campaignBossMinutes(int mission) const {
        if (mission < 0 || mission >= CAMPAIGN_MISSION_COUNT) return 0;
        return (CAMPAIGN_MISSIONS[mission].bossTimeSeconds + 59) / 60;
    }
    int campaignStartingTier(int mission) const {
        // Each act opens one tier higher; the last act keeps Tier V.
        return std::max(1, std::min(5, campaignActOf(mission)));
    }
    int campaignDifficultyPercent(int mission) const {
        if (mission < 0 || mission >= CAMPAIGN_MISSION_COUNT) return 0;
        return std::min(40, campaignActProgressIndex(mission) * 3 + (campaignActOf(mission) - 1) * 4);
    }
    float campaignDifficultyMultiplier() const {
        return gameMode == MODE_STORY ? 1.0f + campaignDifficultyPercent(selectedStoryMission) * 0.01f : 1.0f;
    }

    static const char* heroWitnessLine(int heroId) {
        static const char* lines[5] = {
            "Виктор записывает пропавших в полевой журнал: «Пока запись со мной — их не стереть».",
            "Селена узнаёт в Архиве собственную магическую работу и пришла исправить её, не скрывая своей вины.",
            "Бьёрн помнит закрытые ворота и хочет узнать, кого его приказ оставил внутри.",
            "Аларик вымарывал имена ради порядка; теперь он пришёл вернуть людям право свидетельствовать.",
            "Никс слышит имена, которых нет в реестре, и знает: незаписанный человек не становится пустотой."
        };
        return lines[std::max(0, std::min(4, heroId))];
    }

    // Permanent campaign milestones unlock the last two heroes.
    bool isHeroUnlocked(int heroId) const {
        if (heroId < 0 || heroId >= 5) return false;
        if (heroId < 3) return true;
        return save.bossesDefeated >= (heroId == 3 ? 1 : 3);
    }

    void cycleHero(int direction) {
        for (int n = 0; n < 5; ++n) {
            selectedHero = (selectedHero + (direction > 0 ? 1 : 4)) % 5;
            if (isHeroUnlocked(selectedHero)) break;
        }
    }

    float getMetaUpgradeEffectRank(int id) const {
        if (id < 0 || id >= 6) return 0.0f;
        const int rank = std::max(0, std::min(META_DEFS[id].maxRank, save.metaRanks[id]));
        return rank <= 12 ? (float)rank : 12.0f + (rank - 12) * 0.35f;
    }

    // Stat getters combine hero, passive items, and long-tail permanent forge ranks.
    float getDamageMultiplier() const {
        float m = HERO_DEFS[selectedHero].dmgMult;
        m += getMetaUpgradeEffectRank(0) * 0.04f;
        m += itemLevels[11] * 0.12f;
        m += itemLevels[15] * 0.05f; // Око Бездны
        m += itemLevels[16] * 0.11f; // Печать Пепельного Сердца
        return m;
    }

    float getCritChance() const {
        float c = 0.08f + getMetaUpgradeEffectRank(4) * 0.02f + itemLevels[11] * 0.08f;
        if (selectedHero == 2) c += 0.10f;
        c += itemLevels[15] * 0.04f; // Око Бездны
        c += itemLevels[17] * 0.30f; // Tier III: a powerful crit build; overflow is super-crit chance.
        return std::max(0.0f, c);
    }

    float getCooldownMultiplier() const {
        float cd = HERO_DEFS[selectedHero].cooldownMult;
        cd *= powf(0.91f, (float)itemLevels[12]); // Часы вечности
        cd *= powf(1.0f / 1.10f, (float)itemLevels[18]); // Линза Разлома
        return std::max(0.24f, cd);
    }

    float getAreaMultiplier() const {
        float a = HERO_DEFS[selectedHero].areaMult;
        a += itemLevels[12] * 0.10f;
        a += itemLevels[18] * 0.14f;
        return a;
    }

    float getMoveSpeed() const {
        float spd = HERO_DEFS[selectedHero].moveSpeed;
        spd *= (1.0f + getMetaUpgradeEffectRank(2) * 0.03f + itemLevels[8] * 0.14f);
        return spd;
    }

    float getPickupRadius() const {
        float r = 95.0f;
        r *= (1.0f + getMetaUpgradeEffectRank(3) * 0.10f + itemLevels[9] * 0.45f);
        return r;
    }

    float getXpMultiplier() const {
        float x = (selectedHero == 0) ? 1.15f : 1.0f;
        x += getMetaUpgradeEffectRank(4) * 0.05f + itemLevels[7] * 0.10f;
        return x * balance.experienceMultiplier;
    }

    int getXpRequirementForLevel(int level) const {
        const double lv = (double)std::max(1, level);
        if (lv <= 1.0) return 60; // The first choice should arrive early; later levels keep the measured curve.
        return std::max(1, (int)round(40.0 + 50.0 * lv + 4.0 * lv * lv));
    }

    float getEliteDamageMultiplier() const { return 1.0f + std::max(0, itemLevels[20]) * 0.12f; }
    int getDashRefreshKillsRequired() const { return std::max(6, 12 - std::max(0, itemLevels[21]) * 2); }
    float getEchoHealAmount() const { return playerMaxHp * std::max(0, itemLevels[22]) * 0.02f; }
    float getVeilCooldownSeconds() const { return std::max(16.0f, 30.0f - std::max(0, itemLevels[23]) * 4.0f); }
    int getEchoStepThreshold() const {
        static const int thresholds[4] = {0, 4, 3, 2};
        const int rank = std::max(0, std::min(3, itemLevels[28]));
        return thresholds[rank];
    }
    void triggerEchoStepPulse() {
        const int rank = std::max(1, std::min(3, itemLevels[28]));
        const float radius = (112.0f + rank * 14.0f) * getAreaMultiplier();
        const float damage = 23.0f + rank * 12.0f;
        echoStepCharges = 0;
        addShockwave(playerX, playerY, radius, 0xFF63E9E8, 0.46f);
        preserveParticleRandomBudget(16);
        spawnParticles(playerX, playerY, 16, 0xFF63E9E8, 150.0f, 3, true);
        addFloatingText(playerX, playerY - 58.0f, "ЗЕРКАЛЬНОЕ ЭХО!", 0xFF8FFFF4, true);
        bool hit = false;
        for (size_t i = 0; i < monsters.size(); ++i) {
            Monster& m = monsters[i];
            if (m.hp <= 0.0f || m.isResonator) continue;
            const float dx = m.x - playerX, dy = m.y - playerY;
            const float dist2 = dx * dx + dy * dy;
            if (dist2 > radius * radius) continue;
            const float dist = std::max(1.0f, sqrtf(dist2));
            damageMonster(i, damage, dx / dist * 18.0f, dy / dist * 18.0f, 29);
            hit = true;
        }
        sound.play(hit ? SFX_LIGHTNING : SFX_DASH, hit ? 0.18f : 0.08f);
    }
    void registerDashEvade() {
        if (itemLevels[28] <= 0 || dashTimer <= 0.0f || dashEvadeTriggered) return;
        dashEvadeTriggered = true; // at most one charge per dash, even if a whole volley overlaps
        const int threshold = getEchoStepThreshold();
        if (threshold <= 0) return;
        ++echoStepCharges;
        if (echoStepCharges >= threshold) triggerEchoStepPulse();
        else {
            char charge[48];
            snprintf(charge, sizeof(charge), "ЭХО ШАГА %d/%d", echoStepCharges, threshold);
            addFloatingText(playerX, playerY - 48.0f, charge, 0xFF8FFFF4, false);
            sound.play(SFX_DASH, 0.05f);
        }
    }
    float applyVeilMitigation(float damage) {
        if (damage <= 0.0f) return damage;
        if (itemLevels[30] > 0 && playerWard > 0.0f) {
            const float absorbed = std::min(playerWard, damage);
            playerWard = std::max(0.0f, playerWard - absorbed);
            damage -= absorbed;
            wardFlashTimer = std::max(wardFlashTimer, 0.30f);
            if (options.hitEffectsEnabled) {
                preserveParticleRandomBudget(8);
                spawnParticles(playerX, playerY, 8, 0xFFFFD878, 95.0f, 3, true);
                addShockwave(playerX, playerY, 48.0f + absorbed, 0xFFFFD878, 0.28f);
            }
            if (damage <= 0.0f) return 0.0f;
        }
        if (itemLevels[23] <= 0 || veilCooldown > 0.0f) return damage;
        veilCooldown = getVeilCooldownSeconds();
        spawnParticles(playerX, playerY, 16, 0xFF9A86FF, 105.0f, 3, true);
        addShockwave(playerX, playerY, 64.0f, 0xFF9A86FF, 0.38f);
        addFloatingText(playerX, playerY - 30.0f, "ПОКРОВ ПОГЛОТИЛ ПОЛОВИНУ УДАРА", 0xFFC6B6FF, true);
        return damage * 0.5f;
    }

    float getArmor() const {
        return (float)HERO_DEFS[selectedHero].bonusArmor + getMetaUpgradeEffectRank(5) * 0.8f + itemLevels[19] * 5.0f;
    }


    int getEnemyOathRank(int id) const {
        if (id < 0 || id >= ENEMY_OATH_COUNT) return 0;
        return std::max(0, id < 3 ? save.enemyOathRanks[id] : save.enemyOathRanksAdditional[id - 3]);
    }
    void setEnemyOathRank(int id, int rank) {
        if (id < 0 || id >= ENEMY_OATH_COUNT) return;
        if (id < 3) save.enemyOathRanks[id] = std::max(0, rank);
        else save.enemyOathRanksAdditional[id - 3] = std::max(0, rank);
    }
    float getEnemyOathHpMultiplier() const {
        return 1.0f + getEnemyOathRank(0) * 0.12f + getEnemyOathRank(5) * 0.05f;
    }
    float getEnemyOathSpeedMultiplier() const {
        return 1.0f + getEnemyOathRank(1) * 0.08f + getEnemyOathRank(5) * 0.03f;
    }
    float getEnemyOathDamageMultiplier() const { return 1.0f + getEnemyOathRank(2) * 0.10f; }
    float getEnemyOathSpawnMultiplier() const { return 1.0f + getEnemyOathRank(3) * 0.06f; }
    float getEnemyOathEliteChancePercent() const { return getEnemyOathRank(4) * 1.0f; }
    float getGoldMultiplier() const {
        static const float contractGold[ENEMY_OATH_COUNT] = {0.10f, 0.08f, 0.12f, 0.10f, 0.12f, 0.18f};
        float contractBonus = 0.0f;
        for (int i = 0; i < ENEMY_OATH_COUNT; ++i) contractBonus += getEnemyOathRank(i) * contractGold[i];
        return 1.0f + itemLevels[13] * 0.15f + getMetaUpgradeEffectRank(3) * 0.05f + contractBonus;
    }
    int getEnemyOathCost(int id) const {
        if (id < 0 || id >= ENEMY_OATH_COUNT) return 0;
        return (int)roundf(ENEMY_OATHS[id].baseCost * powf(1.55f, (float)getEnemyOathRank(id)));
    }
    float getSuperGainMultiplier() const { return 1.0f + itemLevels[15] * 0.06f + itemLevels[16] * 0.05f; }
    float getBossDamageMultiplier() const { return 1.0f + itemLevels[19] * 0.20f; }
    void recordRecentDpsSample(float dps) {
        dps = std::max(0.0f, dps);
        if (recentDpsCount == 60) recentDpsSum -= recentDpsSamples[recentDpsIndex];
        else ++recentDpsCount;
        recentDpsSamples[recentDpsIndex] = dps;
        recentDpsSum += dps;
        recentDpsIndex = (recentDpsIndex + 1) % 60;
    }
    float getAverageDpsLastMinute() const {
        // At run start there is no measured minute yet; use a 100 DPS calibration baseline.
        return recentDpsCount > 0 ? recentDpsSum / recentDpsCount : 100.0f;
    }
    int getHealingDropChancePermille() const {
        const float hpRatio = playerMaxHp > 0.0f ? playerHp / playerMaxHp : 1.0f;
        if (hpRatio >= 0.90f) return 0;
        if (hpRatio <= 0.30f) return 160;
        if (hpRatio <= 0.60f) return 100;
        return 50;
    }
    int getCriticalTierForRoll(float roll) const {
        const float chance = getCritChance();
        if (roll < std::max(0.0f, chance - 1.0f)) return 2;
        if (roll < std::min(1.0f, chance)) return 1;
        return 0;
    }
    float getCriticalDamageMultiplier(int tier) const {
        return tier >= 2 ? 4.0f : (tier == 1 ? 1.85f : 1.0f);
    }
    float getEvoMultiplier(int weaponId) const { return evolvedWeapons[weaponId] ? 1.65f : 1.0f; }
    float getWeaponRankDamageMultiplier(int weaponId) const {
        static const float rankGrowth[7] = {1.10f, 1.13f, 1.08f, 1.12f, 1.09f, 1.14f, 1.11f};
        if (weaponId < 0 || weaponId >= 7) return 1.0f;
        float multiplier = 1.0f;
        for (int rank = 0; rank < std::max(0, itemLevels[weaponId] - 1); ++rank)
            multiplier *= rankGrowth[weaponId] + 0.003f * rank; // later ranks hit harder than early ranks
        return multiplier;
    }
    float getWeaponRankCooldownMultiplier(int weaponId) const {
        static const float rankCadence[7] = {0.970f, 0.985f, 0.960f, 0.975f, 0.950f, 0.985f, 0.955f};
        if (weaponId < 0 || weaponId >= 7) return 1.0f;
        float multiplier = 1.0f;
        for (int rank = 0; rank < std::max(0, itemLevels[weaponId] - 1); ++rank)
            multiplier *= std::max(0.88f, rankCadence[weaponId] - 0.0015f * rank);
        return multiplier;
    }
    float getWeaponRankAreaMultiplier(int weaponId) const {
        static const float rankArea[7] = {0.025f, 0.080f, 0.045f, 0.075f, 0.095f, 0.035f, 0.105f};
        if (weaponId < 0 || weaponId >= 7) return 1.0f;
        float areaBonus = 0.0f;
        for (int rank = 0; rank < std::max(0, itemLevels[weaponId] - 1); ++rank)
            areaBonus += rankArea[weaponId] + 0.004f * rank;
        return 1.0f + areaBonus;
    }
    int getWeaponProjectileCountAtLevel(int weaponId, int level) const {
        if (weaponId < 0 || weaponId >= 7 || level <= 0) return 0;
        const int evolvedBonus = evolvedWeapons[weaponId] ? (weaponId == 1 ? 1 : (weaponId == 3 ? 3 : 2)) : 0;
        switch (weaponId) {
            case 0: return 1 + level / 2 + evolvedBonus;
            case 1: return 1 + (level >= 4 ? 1 : 0) + (level >= 7 ? 1 : 0) + evolvedBonus;
            case 2: return 3 + level / 2 + evolvedBonus;
            case 3: return 3 + level / 2 + evolvedBonus;
            case 5: return 1 + level / 3 + (level >= 7 ? 1 : 0) + evolvedBonus;
            default: return 0;
        }
    }
    int getNextWeaponProjectileRank(int weaponId) const {
        if (weaponId < 0 || weaponId >= 7 || weaponId == 4 || weaponId == 6) return 0;
        const int current = itemLevels[weaponId];
        const int currentCount = getWeaponProjectileCountAtLevel(weaponId, current);
        for (int level = std::max(1, current + 1); level <= getItemMaxLevel(weaponId); ++level)
            if (getWeaponProjectileCountAtLevel(weaponId, level) > currentCount) return level;
        return 0;
    }
    std::string getWeaponUpgradePreviewText(int weaponId) const {
        if (weaponId >= 24 && weaponId <= 27) {
            const int level = itemLevels[weaponId];
            char line[224];
            if (weaponId == 24) {
                if (level <= 0) snprintf(line, sizeof(line), "САМОНАВОДЯЩИЙСЯ КРИСТАЛЬНЫЙ ЗАЛП\nРанги усиливают урон; на 4-м и 7-м появляются дополнительные осколки.");
                else snprintf(line, sizeof(line), "+13 урона • быстрее перезарядка • залп %d\nСледующий дополнительный осколок: %s.", 1 + (level >= 4) + (level >= 7), level < 4 ? "4-й ранг" : (level < 7 ? "7-й ранг" : "максимум"));
            } else if (weaponId == 25) {
                if (level <= 0) snprintf(line, sizeof(line), "РУНИЧЕСКИЙ ИМПУЛЬС ВОКРУГ ГЕРОЯ\nКаждый ранг усиливает урон, радиус и частоту волн.");
                else snprintf(line, sizeof(line), "+14 урона • +22 радиуса • короче перезарядка\nПоражает всех врагов в круге вокруг героя.");
            } else if (weaponId == 26) {
                if (level <= 0) snprintf(line, sizeof(line), "РАЗРЯД ПРЫГАЕТ МЕЖДУ ВРАГАМИ\nКаждые два ранга добавляют звено цепи.");
                else snprintf(line, sizeof(line), "+15 урона • цепь до %d целей • короче перезарядка", 3 + level / 2);
            } else {
                if (level <= 0) snprintf(line, sizeof(line), "МОЩНЫЙ МЕТЕОР ПО СКОПЛЕНИЮ ВРАГОВ\nКаждый ранг увеличивает урон и радиус взрыва.");
                else snprintf(line, sizeof(line), "+34 урона • +22 радиуса взрыва • быстрее перезарядка");
            }
            return std::string(line);
        }
        if (weaponId < 0 || weaponId >= 7) return std::string();
        static const char* signature[7] = {
            "КЛИНКИ: чаще залп; пробой на 4-м и 7-м ранге",
            "ОГОНЬ: большой взрыв; новые снаряды на 4-м и 7-м",
            "ВИХРЬ: сферы вращаются быстрее и множатся",
            "МОЛНИЯ: +удар каждые 2 ранга, цепь ищет дальше",
            "АУРА: шире и чаще; убийства лечат сильнее",
            "ТОПОР: тяжёлый сквозной залп, новые топоры на 3/6/7",
            "ЛЁД: шире волна и дольше заморозка с каждым рангом"
        };
        const int level = itemLevels[weaponId];
        char line[240];
        static const int resonanceRelicForWeapon[7] = {7, 8, 9, 10, 11, 12, 13};
        if (level == 8 && itemLevels[resonanceRelicForWeapon[weaponId]] >= 4 && !evolvedWeapons[weaponId]) {
            snprintf(line, sizeof(line), "РАНГ 9: СУПЕРПРЕДМЕТ • ЭВОЛЮЦИЯ\nРезонанс готов — выбери карту, чтобы пробудить супероружие.");
            return std::string(line);
        }
        if (level >= 8 && !evolvedWeapons[weaponId]) {
            snprintf(line, sizeof(line), "ОРУЖИЕ ДОСТИГЛО 8-ГО РАНГА\nНужен соответствующий артефакт IV ранга для суперпредмета.");
            return std::string(line);
        }
        if (level <= 0) {
            snprintf(line, sizeof(line), "ОТКРЫВАЕТ АТАКУ\n%s", signature[weaponId]);
        } else {
            const int nextLevel = level + 1;
            const int now = getWeaponProjectileCountAtLevel(weaponId, level);
            const int next = getWeaponProjectileCountAtLevel(weaponId, nextLevel);
            char countLine[64] = "";
            if (now > 0 && next > now) snprintf(countLine, sizeof(countLine), " • ЗАЛП %d→%d", now, next);
            else if (weaponId == 4) snprintf(countLine, sizeof(countLine), " • АУРА +радиус/частота");
            else if (weaponId == 6) snprintf(countLine, sizeof(countLine), " • ЗАМОРОЗКА %.1f→%.1f с", 1.6f + level * 0.2f, 1.6f + nextLevel * 0.2f);
            else snprintf(countLine, sizeof(countLine), " • СЛЕД. ПИК %d", getNextWeaponProjectileRank(weaponId));
            static const float damageGrowth[7] = {1.10f, 1.13f, 1.08f, 1.12f, 1.09f, 1.14f, 1.11f};
            static const float cadence[7] = {0.970f, 0.985f, 0.960f, 0.975f, 0.950f, 0.985f, 0.955f};
            static const float areaGrowth[7] = {0.025f, 0.080f, 0.045f, 0.075f, 0.095f, 0.035f, 0.105f};
            const int rank = std::max(0, level - 1);
            const int dmgPct = (int)roundf((damageGrowth[weaponId] + 0.003f * rank - 1.0f) * 100.0f);
            const int cdPct = (int)roundf((1.0f - std::max(0.88f, cadence[weaponId] - 0.0015f * rank)) * 100.0f);
            const int areaPct = (int)roundf((areaGrowth[weaponId] + 0.004f * rank) * 100.0f);
            snprintf(line, sizeof(line), "+%d%% УРОН • -%d%% КД • +%d%% РАДИУС%s\n%s", dmgPct, cdPct, areaPct, countLine, signature[weaponId]);
        }
        return std::string(line);
    }

    std::string getPassiveUpgradePreviewText(int itemId) const {
        if (itemId == 28) return "+1 эхо за рывок сквозь угрозу; накапливай 4/3/2 заряда для контрудара по кругу";
        if (itemId == 39) {
            const int rank = std::max(0, std::min(3, itemLevels[39]));
            char text[176];
            if (rank == 0) snprintf(text, sizeof(text), "Разрушение резонатора даёт 8/12/16%% заряда суперумения.");
            else if (rank < 3) snprintf(text, sizeof(text), "Резонатор: +%d%% • следующий ранг: +%d%% к суперзаряду.", getResonatorSuperChargeBonus(rank), getResonatorSuperChargeBonus(rank + 1));
            else snprintf(text, sizeof(text), "Максимум: разрушение резонатора даёт +16%% суперзаряда.");
            return std::string(text);
        }
        if (itemId == 38) {
            const int rank = std::max(0, std::min(3, itemLevels[38]));
            char text[176];
            if (rank == 0) snprintf(text, sizeof(text), "Потеря HP → активный удар +35/50/65%% за 4,5 с.");
            else if (rank < 3) snprintf(text, sizeof(text), "Сейчас +%.0f%% • дальше +%.0f%%. Заряд 4,5 с, не складывается.", getRetaliationBonusPercent(rank) * 100.0f, getRetaliationBonusPercent(rank + 1) * 100.0f);
            else snprintf(text, sizeof(text), "Активный удар: +65%% урона за 4,5 с. Один заряд.");
            return std::string(text);
        }
        if (itemId == 37) {
            const int rank = std::max(0, std::min(3, itemLevels[37]));
            char text[176];
            if (rank == 0) snprintf(text, sizeof(text), "Добивание активным оружием ниже 8/12/16%% HP • обычные враги и элиты, не боссы.");
            else if (rank < 3) snprintf(text, sizeof(text), "Порог казни: %.0f%% HP • следующий ранг: %.0f%%.", getExecutionThresholdPercent(rank), getExecutionThresholdPercent(rank + 1));
            else snprintf(text, sizeof(text), "Максимум: удар активного оружия казнит цель ниже 16%% HP. Боссы исключены.");
            return std::string(text);
        }
        if (itemId == 36) {
            const int rank = std::max(0, std::min(3, itemLevels[36]));
            char text[176];
            if (rank == 0) snprintf(text, sizeof(text), "Перехват снаряда врага: 8/12/16 урона ближайшей цели • повтор через 2,2/1,8/1,4 с.");
            else if (rank < 3) snprintf(text, sizeof(text), "Отражение: %d урона • готовность через %.1f с; следующий ранг: %d урона, %.1f с.", getProjectileParryDamage(rank), getProjectileParryCooldown(rank), getProjectileParryDamage(rank + 1), getProjectileParryCooldown(rank + 1));
            else snprintf(text, sizeof(text), "Максимум: 16 урона по врагу каждые 1,4 с. Снаряды боссов и атаки по площади не отражаются.");
            return std::string(text);
        }
        if (itemId == 35) {
            const int rank = std::max(0, std::min(3, itemLevels[35]));
            char text[176];
            if (rank == 0) snprintf(text, sizeof(text), "Переживший заморозку выпускает осколки: 6/8/10 урона по 2/3/4 соседям.");
            else if (rank < 3) snprintf(text, sizeof(text), "Откол: %d цели в радиусе %.0f, %d урона • следующий ранг: %d цели, %d урона.", getFrostShatterTargetCount(rank), getFrostShatterRadius(rank), (int)getFrostShatterDamage(rank), getFrostShatterTargetCount(rank + 1), (int)getFrostShatterDamage(rank + 1));
            else snprintf(text, sizeof(text), "Максимум: 4 цели в радиусе 108, по 10 урона • перезарядка 0,56 с; откол не цепляется.");
            return std::string(text);
        }
        if (itemId == 34) {
            const int rank = std::max(0, std::min(3, itemLevels[34]));
            char text[160];
            if (rank == 0) snprintf(text, sizeof(text), "Крит активного оружия поджигает цель: 4/5/6 тиков по 3/5/7 урона • ожог не складывается.");
            else if (rank < 3) snprintf(text, sizeof(text), "Ожог: %d × %.0f урона • следующий ранг: %d тиков по %.0f.", getEmberTickCount(rank), getEmberTickDamage(rank), getEmberTickCount(rank + 1), getEmberTickDamage(rank + 1));
            else snprintf(text, sizeof(text), "Максимум: 6 тиков по 7 урона за 3,66 с. Обычные цели и элиты; повтор не складывается.");
            return std::string(text);
        }
        if (itemId == 33) {
            const int rank = std::max(0, std::min(3, itemLevels[33]));
            char text[176];
            if (rank == 0) snprintf(text, sizeof(text), "4/3/3 разных активных оружия бьют одну цель за 2,2/2,5/2,8 с: рикошет поражает 2/3/4 соседа.");
            else if (rank < 3) snprintf(text, sizeof(text), "%d оружия / %.1f с → %d цели, %d урона. Следом: %d оружия / %d цели.", getCrossfireDistinctWeaponThreshold(rank), getCrossfireWindowSeconds(rank), getCrossfireTargetCount(rank), (int)getCrossfireDamage(rank), getCrossfireDistinctWeaponThreshold(rank + 1), getCrossfireTargetCount(rank + 1));
            else snprintf(text, sizeof(text), "Макс.: 3 оружия / 2,8 с → 4 цели, 24 урона. Радиус 140; кд цели/общий 2/0,6 с.");
            return std::string(text);
        }
        if (itemId == 32) {
            const int rank = std::max(0, std::min(3, itemLevels[32]));
            const int threshold = getAshRhythmSparkThreshold(std::max(1, rank));
            char text[176];
            if (rank == 0) snprintf(text, sizeof(text), "Каждые %d/%d/%d XP-самоцветов: активные атаки ускоряются на 0,15/0,22/0,30 с.", getAshRhythmSparkThreshold(1), getAshRhythmSparkThreshold(2), getAshRhythmSparkThreshold(3));
            else if (rank < 3) snprintf(text, sizeof(text), "Искры %d/%d • следующий ранг: каждые %d самоцветов, оружие -%.2f с.", ashRhythmSparkCount, threshold, getAshRhythmSparkThreshold(rank + 1), getAshRhythmCooldownAdvance(rank + 1));
            else snprintf(text, sizeof(text), "Искры %d/10 • каждые 10 XP-самоцветов: активное оружие -0,30 с.", ashRhythmSparkCount);
            return std::string(text);
        }
        if (itemId == 31) {
            const int rank = std::max(0, std::min(3, itemLevels[31]));
            char text[192];
            if (rank == 0) snprintf(text, sizeof(text), "Перебор %d+ урона при убийстве поражает до %d врагов: %d%% избытка, максимум %d за цель.", (int)getOverkillShardMinimumExcess(1), getOverkillShardTargetCount(1), (int)roundf(getOverkillShardRatio(1) * 100.0f), (int)getOverkillShardDamageCap(1));
            else if (rank < 3) snprintf(text, sizeof(text), "Сейчас: %d цели, радиус %.0f, до %d урона • следующий ранг: %d цели, радиус %.0f, до %d.", getOverkillShardTargetCount(rank), getOverkillShardRadius(rank), (int)getOverkillShardDamageCap(rank), getOverkillShardTargetCount(rank + 1), getOverkillShardRadius(rank + 1), (int)getOverkillShardDamageCap(rank + 1));
            else snprintf(text, sizeof(text), "Максимум: 3 цели в радиусе 124; 24%% избытка, не более 22 урона каждой. Перезарядка 0,85 с.");
            return std::string(text);
        }
        if (itemId == 30) {
            const int rank = std::max(0, std::min(3, itemLevels[30]));
            char text[192];
            if (rank == 0) snprintf(text, sizeof(text), "30/35/40%% избыточного лечения создаёт щит до 16/26/36 ед. • поглощает урон раньше Покрова.");
            else if (rank < 3) snprintf(text, sizeof(text), "Щит %d/%d • следующий ранг: %d%% избытка, предел %d.", (int)roundf(playerWard), (int)getOverhealWardCapacity(rank), (int)roundf(getOverhealWardRate(rank + 1) * 100.0f), (int)getOverhealWardCapacity(rank + 1));
            else snprintf(text, sizeof(text), "Максимум: 40%% избытка лечения превращается в щит до 36 ед.");
            return std::string(text);
        }
        if (itemId == 29) {
            const int rank = std::max(0, std::min(3, itemLevels[29]));
            char text[192];
            if (rank == 0) snprintf(text, sizeof(text), "4/3/2 попадания по одной цели за 2,5 с: +10/14/18%% входящего урона на 1,9/2,2/2,5 с.");
            else if (rank < 3) snprintf(text, sizeof(text), "Сейчас: +%d%% на %.1f с • следующий ранг: %d попадания, +%d%% на %.1f с.",
                (int)roundf(getExposureDamageBonus(rank) * 100.0f), getExposureDuration(rank),
                getExposureHitsRequired(rank + 1), (int)roundf(getExposureDamageBonus(rank + 1) * 100.0f), getExposureDuration(rank + 1));
            else snprintf(text, sizeof(text), "Максимум: 2 попадания раскрывают цель на 2,5 с; она получает +18%% урона.");
            return std::string(text);
        }
        if (itemId < 7 || (itemId >= 24 && itemId <= 27) || itemId > 37) return std::string();
        static const char* effects[17] = {
            "+10% опыта за ранг — быстрее открываются сборки",
            "+14% скорости; рывок возвращается быстрее",
            "+45% радиуса сбора — собирай добычу не сходя с пути",
            "+40 макс. HP и +1.5 HP/с за ранг",
            "+12% урона и +8% крит. шанса за ранг",
            "+9% частоты атак и +10% радиуса за ранг",
            "+15% золота за ранг — риск усиливает элиту",
            "Воскрешение: ранг усиливает запас HP после спасения",
            "+6% заряда суперумения и +4% крит. шанса",
            "+11% урона и +5% заряда суперумения",
            "+30% крит. шанса; избыток — суперкрит ×4",
            "+14% радиуса и +10% частоты атак",
            "+20% урона боссам и +5 брони",
            "+12% урона элитным врагам за ранг",
            "каждые 10/8/6 убийств возвращают рывок",
            "элита и босс восстанавливают 2% макс. HP за ранг",
            "готовый покров делит следующий входящий урон пополам"
        };
        return std::string(effects[itemId - 7]);
    }

    float getSuperSacrificeCost() const { return std::max(8.0f, playerMaxHp * 0.12f); }

    int getItemMaxLevel(int id) const {
        if (id < 0 || id >= ITEM_COUNT) return 0;
        if (id >= 0 && id < 7) {
            static const int resonanceRelicForWeapon[7] = {7, 8, 9, 10, 11, 12, 13};
            return itemLevels[resonanceRelicForWeapon[id]] >= 4 ? 9 : 8;
        }
        if (id >= 24 && id < 28) return 8;
        if (id < 13) return 6;
        if (id < 16) return 5;
        return 3;
    }
    int getMetaUpgradeCost(int id) const {
        if (id < 0 || id >= 6) return 0;
        const int rank = std::max(0, std::min(META_DEFS[id].maxRank, save.metaRanks[id]));
        const double earlyCurve = pow(1.58, std::min(rank, 12));
        const double longTailCurve = pow(1.075, std::max(0, rank - 12));
        const double cost = META_DEFS[id].baseCost * earlyCurve * longTailCurve;
        return (int)std::max(1.0, std::min(2000000000.0, floor(cost + 0.5)));
    }
    bool isItemUnlocked(int id) const {
        if (id < 0 || id >= ITEM_COUNT) return false;
        if (id < 16) return true;
        // Story progression is mission-based: the current mission opens its tier from the first draft.
        // Defeating its boss advances the in-run tier by one; Endless retains the original time gates.
        if (gameMode == MODE_STORY) return getItemTier(id) <= getBuildTier();
        static const float unlockTimes[24] = {180.0f, 300.0f, 420.0f, 540.0f, 360.0f, 420.0f, 480.0f, 600.0f,
                                               420.0f, 540.0f, 660.0f, 780.0f, 300.0f, 600.0f, 480.0f, 540.0f, 510.0f, 450.0f, 480.0f, 570.0f, 525.0f, 585.0f, 600.0f, 615.0f};
        return runTime >= unlockTimes[id - 16];
    }

    int getItemTier(int id) const {
        if (id < 0 || id >= ITEM_COUNT) return 0;
        if (id < 16) return 1;
        if (id == 16 || id == 20 || id == 24) return 2;
        if (id == 17 || id == 21 || id == 25) return 3;
        if (id == 18 || id == 22 || id == 26) return 4;
        if (id == 28) return 3;
        if (id == 29 || id == 30 || id == 31 || id == 32 || id == 33 || id == 34 || id == 35 || id == 36 || id == 37 || id == 38 || id == 39) return 4;
        return 5; // 19, 23, and 27
    }

    int getExposureHitsRequired(int rank) const {
        static const int hits[4] = {0, 4, 3, 2};
        return hits[std::max(0, std::min(3, rank))];
    }
    float getExposureDamageBonus(int rank) const {
        static const float bonus[4] = {0.0f, 0.10f, 0.14f, 0.18f};
        return bonus[std::max(0, std::min(3, rank))];
    }
    float getExposureDuration(int rank) const {
        static const float duration[4] = {0.0f, 1.9f, 2.2f, 2.5f};
        return duration[std::max(0, std::min(3, rank))];
    }
    static constexpr float EXPOSURE_BUILD_WINDOW = 2.5f;
    float getOverhealWardRate(int rank) const {
        static const float rate[4] = {0.0f, 0.30f, 0.35f, 0.40f};
        return rate[std::max(0, std::min(3, rank))];
    }
    float getOverhealWardCapacity(int rank) const {
        static const float capacity[4] = {0.0f, 16.0f, 26.0f, 36.0f};
        return capacity[std::max(0, std::min(3, rank))];
    }
    void restorePlayerHp(float amount) {
        if (amount <= 0.0f) return;
        const float hpGain = std::min(amount, std::max(0.0f, playerMaxHp - playerHp));
        playerHp += hpGain;
        const float overflow = amount - hpGain;
        const int rank = std::max(0, std::min(3, itemLevels[30]));
        if (rank > 0 && overflow > 0.0f) {
            const float beforeWard = playerWard;
            playerWard = std::min(getOverhealWardCapacity(rank), playerWard + overflow * getOverhealWardRate(rank));
            if (playerWard > beforeWard) wardFlashTimer = std::max(wardFlashTimer, 0.22f);
        }
    }
    void updateExposureTimers(Monster& m, float dt) {
        const float elapsed = std::max(0.0f, dt);
        m.exposeTimer = std::max(0.0f, m.exposeTimer - elapsed);
        m.exposeBuildTimer = std::max(0.0f, m.exposeBuildTimer - elapsed);
        updateCrossfireTimers(m, elapsed);
        if (m.exposeBuildTimer <= 0.0f) m.exposeHitStacks = 0;
    }

    const std::vector<int>& getArsenalItemOrder() const {
        // Tier and type are fixed item metadata, so build the display order once rather than sorting every frame.
        static const std::vector<int> order = [this]() {
            std::vector<int> sorted(ITEM_COUNT);
            for (int id = 0; id < ITEM_COUNT; ++id) sorted[id] = id;
            std::sort(sorted.begin(), sorted.end(), [this](int a, int b) {
                const int tierA = getItemTier(a), tierB = getItemTier(b);
                if (tierA != tierB) return tierA > tierB; // rarest items first
                if (ITEM_DEFS[a].isWeapon != ITEM_DEFS[b].isWeapon) return ITEM_DEFS[a].isWeapon; // weapon, then artifact
                return a < b; // stable, predictable ordering within each tier/type group
            });
            return sorted;
        }();
        return order;
    }

    uint32_t getItemTierColor(int id) const {
        static const uint32_t tierColors[6] = {
            0xFF777185, // invalid/none
            0xFF7CA9B8, // I — common, cool slate
            0xFF62D58C, // II — green
            0xFF56A9FF, // III — blue
            0xFFBD79F5, // IV — violet
            0xFFFFC65C  // V — amber
        };
        const int tier = std::max(0, std::min(5, getItemTier(id)));
        return tierColors[tier];
    }

    float getItemDraftWeight(int id) const {
        static const float tierWeights[6] = {0.0f, 1.0f, 1.20f, 1.35f, 1.80f, 2.00f};
        const int tier = getItemTier(id);
        return tier >= 1 && tier <= 5 ? tierWeights[tier] : 0.0f;
    }

    int countEquippedItems() const {
        int count = 0;
        for (int i = 0; i < ITEM_COUNT; ++i) count += itemLevels[i] > 0;
        return count;
    }

    int getBuildTier() const {
        const int startingTier = gameMode == MODE_STORY ? campaignStartingTier(selectedStoryMission) : 1;
        return std::min(5, startingTier + std::max(0, runBossKills));
    }

    static int bossPhaseForHealth(float hp, float maxHp) {
        const float ratio = maxHp > 0.0f ? std::max(0.0f, hp / maxHp) : 1.0f;
        return ratio <= 0.33f ? 3 : (ratio <= 0.66f ? 2 : 1);
    }
    void handleBossPhaseTransition(Monster& m) {
        if (!m.isBoss || m.hp <= 0.0f) return;
        const int hpPhase = bossPhaseForHealth(m.hp, m.maxHp);
        if (hpPhase <= m.bossPhase) return;
        m.bossPhase = hpPhase;
        m.phaseInvulnTimer = 0.50f;
        static const char* phaseRoman[3] = {"I", "II", "III"};
        static const char* phaseTitle[3] = {"ОХОТА", "НАТИСК", "ЯРОСТЬ"};
        static const char* bossShort[6] = {"МОРГАР", "МАЛАКОР", "КАРДИНАЛ", "АЗАРОТ", "ПРОТОКОЛИСТ", "ДВОЙНИК"};
        announce(GMSG_BOSS_PHASE, std::string(bossShort[m.bossType % 6]) + " • ФАЗА " + phaseRoman[m.bossPhase - 1] + " — " + phaseTitle[m.bossPhase - 1], 3.2f);
        bossPhaseEventTimer = 3.2f;
        clearArenaForBoss();
        bossPhaseEventType = m.bossType % 6;
        bossPhaseEventNumber = m.bossPhase;
        sound.play(SFX_BOSS_PHASE, 0.18f);
        const uint32_t phaseColor = m.bossPhase == 2 ? 0xFFFFA544 : 0xFFFF4F75;
        spawnParticles(m.x, m.y, 46, phaseColor, 225.0f, 6, true);
        spawnParticles(m.x, m.y, 22, 0xFFE9D7FF, 165.0f, 4, true);
        addShockwave(m.x, m.y, 390.0f, phaseColor, 1.10f);
        addShockwave(m.x, m.y, 235.0f, 0xFFFFFFFF, 0.85f);
        addShockwave(m.x, m.y, 95.0f, phaseColor, 0.70f);
        if (options.screenShakeEnabled) screenShake = std::max(screenShake, 0.62f);
    }

    int getBuildLimit() const {
        const int guardianSlots = std::min(4, std::max(0, runBossKills));
        const int levelSlots = std::max(0, playerLevel - 1) / 10;
        return std::min(MAX_BUILD_ITEMS, 5 + guardianSlots + levelSlots + buildLimitBonus);
    }

    int chooseWeightedItem(const std::vector<int>& candidates) const {
        if (candidates.empty()) return -1;
        float totalWeight = 0.0f;
        for (int id : candidates) totalWeight += getItemDraftWeight(id);
        if (totalWeight <= 0.0f) return candidates[0];
        float roll = ((float)rand() / ((float)RAND_MAX + 1.0f)) * totalWeight;
        for (int id : candidates) {
            roll -= getItemDraftWeight(id);
            if (roll < 0.0f) return id;
        }
        return candidates.back();
    }
    int countActiveEnemies() const {
        int count = 0;
        for (const Monster& m : monsters) if (m.hp > 0.0f && !m.isResonator) ++count;
        return count;
    }
    float getThreatPressure() const {
        float crowd = std::max(0, countActiveEnemies() - 55) / 100.0f;
        float hpStress = playerMaxHp > 0.0f ? std::max(0.0f, (0.55f - playerHp / playerMaxHp) * 1.8f) : 0.0f;
        return std::min(1.6f, crowd + hpStress);
    }
    float getPopulationAggression() const {
        const int enemies = countActiveEnemies();
        return enemies < 25 ? 1.18f : (enemies < 45 ? 1.08f : 1.0f);
    }

    int getDifficultyCycle() const { return breakthroughCount; }
    // Which place this fight happens in. Campaign missions follow their act; Endless
    // walks through the same five places as the night goes on, so the arena itself
    // changes under the player instead of repeating one palette forever.
    int currentArenaTheme() const {
        if (gameMode == MODE_STORY) {
            int act = 1;
            if (selectedStoryMission >= 0 && selectedStoryMission < CAMPAIGN_MISSION_COUNT)
                act = CAMPAIGN_MISSIONS[selectedStoryMission].act;
            return std::max(0, std::min(4, act - 1));
        }
        return std::max(0, std::min(4, (int)(runTime / 150.0f)));
    }

    int getDifficultySegment() const {
        if (breakthroughActive) return 1;
        if (recoveryTimer > 0.0f) return 2;
        return 0;
    }
    float getLongTermEnemyTier() const {
        const float minutes = std::max(0.0f, runTime) / 60.0f;
        const int milestones = (int)(std::max(0.0f, runTime) / 90.0f);
        // Enemy durability rises continuously, with a visible chapter step every 90 seconds.
        return 1.0f + std::min(2.0f, minutes * 0.12f) + std::min(1.0f, milestones * 0.12f);
    }
    int getResonatorBurstCount() const {
        const int elapsedSteps = (int)(std::max(0.0f, runTime) / 90.0f);
        return RESONATOR_DESTRUCTION_BURST_BASE + std::min(72, elapsedSteps * 6);
    }
    float getSublevelSpawnMultiplier() const {
        float multiplier = breakthroughActive ? 1.32f + std::min(2.2f, breakthroughEscalation * 0.12f) : (recoveryTimer > 0.0f ? 0.84f : 1.0f);
        if (options.hardModeEnabled) multiplier *= 1.22f;
        else multiplier *= std::max(0.48f, 1.0f - std::min(0.42f, getThreatPressure() * 0.30f));
        if (countActiveEnemies() < 25) multiplier *= 1.16f;
        return multiplier;
    }
    float getSublevelEnemyHpMultiplier() const {
        float multiplier = breakthroughActive ? 1.05f + std::min(0.75f, breakthroughEscalation * 0.025f) : (recoveryTimer > 0.0f ? 0.95f : 1.0f);
        return multiplier * (options.hardModeEnabled ? 1.12f : 1.0f) * campaignDifficultyMultiplier();
    }
    float getSublevelEnemySpeedMultiplier() const {
        float multiplier = breakthroughActive ? 1.05f + std::min(0.42f, breakthroughEscalation * 0.018f) : (recoveryTimer > 0.0f ? 0.92f : 1.0f);
        if (countActiveEnemies() < 25) multiplier *= 1.18f;
        return multiplier * (options.hardModeEnabled ? 1.10f : 1.0f) * campaignDifficultyMultiplier();
    }
    float getSublevelEnemyDamageMultiplier() const {
        float multiplier = breakthroughActive ? 1.08f + std::min(0.7f, breakthroughEscalation * 0.035f) : (recoveryTimer > 0.0f ? 0.92f : 1.0f);
        return multiplier * (options.hardModeEnabled ? 1.10f : 1.0f) * campaignDifficultyMultiplier();
    }

    void startBreakthrough() {
        if (breakthroughActive) return;
        breakthroughCount++;
        breakthroughActive = true;
        breakthroughEarlyHit = false;
        breakthroughTimer = 18.0f;
        breakthroughEscalation = 0.0f;
        recoveryTimer = 0.0f;
        lastBreakthroughWarningLevel = nextBreakthroughLevel;
        nextBreakthroughLevel = playerLevel + 7 + (rand() % 9);
        difficultyBanner = "ПРОРЫВ! ОРДА — УРОВЕНЬ " + std::to_string(playerLevel);
        difficultyBannerTimer = 4.0f;
        announce(GMSG_BREAKTHROUGH_START, "НА ТЕБЯ ИДЁТ ОСОБАЯ ОРДА!", 3.0f);
        speakOnly(GMSG_HOSTILE_HORDE, "НА ТЕБЯ ИДЁТ ОСОБАЯ ОРДА!");
        if (options.screenShakeEnabled) screenShake = std::max(screenShake, 0.18f);
        sound.play(SFX_BOSS, 0.08f);

        const int waveKind = (breakthroughCount - 1) % 3;
        const int skeletons = waveKind == 1 ? 8 : 10;
        const int ranged = waveKind == 2 ? 4 : 3;
        const int elites = waveKind == 0 ? 2 : 3;
        for (int i = 0; i < skeletons; ++i) spawnMonster(1, false, false);
        for (int i = 0; i < ranged; ++i) spawnMonster(waveKind == 1 ? 8 : 3, false, false);
        for (int i = 0; i < elites; ++i) spawnMonster(waveKind == 2 ? 7 : 4, true, false);
        if (waveKind == 2) for (int i = 0; i < 2; ++i) spawnMonster(9, false, false);
        const uint32_t surgeColor = waveKind == 0 ? 0xFFFF6048 : (waveKind == 1 ? 0xFFB66CFF : 0xFFFFB638);
        spawnParticles(playerX, playerY, 34, surgeColor, 225.0f, 4, true);
        addShockwave(playerX, playerY, 420.0f, surgeColor, 0.90f);
    }

    void finishBreakthrough() {
        if (!breakthroughActive) return;
        breakthroughActive = false;
        breakthroughTimer = 0.0f;
        breakthroughEscalation = 0.0f;
        breakthroughEarlyHit = false;
        recoveryTimer = 18.0f;
        difficultyBanner = "ПРОРЫВ ОТБИТ! ВРАГИ ОТСТУПАЮТ";
        speakOnly(GMSG_BREAKTHROUGH_HELD2, difficultyBanner);
        difficultyBannerTimer = 4.5f;
        sound.play(SFX_LEVELUP, 0.08f);
    }

    void notePlayerDamaged() {
        if (!breakthroughActive) return;
        if (breakthroughTimer > 0.0f) breakthroughEarlyHit = true;
        else finishBreakthrough();
    }

    void applyBossAbilityHit(float rawDamage, bool poisonSlow = false) {
        if (dashTimer > 0.0f) { registerDashEvade(); return; }
        if (playerHurtTimer > 0.0f) return;
        const float dealt = applyVeilMitigation(std::max(3.0f, rawDamage - getArmor()));
        playerHp -= dealt;
        registerRetaliationCharge(dealt);
        notePlayerDamaged();
        playerHurtTimer = 0.42f;
        if (poisonSlow) playerSlowTimer = std::max(playerSlowTimer, 1.0f);
        if (options.screenShakeEnabled) screenShake = std::max(screenShake, 0.18f);
        char hitText[32];
        snprintf(hitText, sizeof(hitText), "-%d HP", (int)roundf(dealt));
        addFloatingText(playerX, playerY - 24.0f, hitText, 0xFFFF3F66, true);
        if (options.hitEffectsEnabled) spawnParticles(playerX, playerY, 8, poisonSlow ? 0xFF72D969 : 0xFFFF7748, 130.0f, 3, true);
        sound.play(SFX_BOSS_IMPACT, 0.12f);
    }

    void spawnEnemyHazard(float x, float y, float damage, int hazardType) {
        if (bossHazards.size() >= 8) bossHazards.erase(bossHazards.begin());
        bossHazards.push_back({x, y, hazardType == 2 ? 84.0f : 72.0f, 0.62f, 2.2f, 0.25f, damage, 2, hazardType});
        const uint32_t color = hazardType == 2 ? 0xFF5CE8FF : (hazardType == 1 ? 0xFFFF6738 : 0xFFB65BFF);
        addShockwave(x, y, 78.0f, color, 0.38f);
    }

    void checkSymbiosis() {
        static const int relicForWeapon[7] = {7, 8, 9, 10, 11, 12, 13};
        static const char* evoNames[7] = {"ТЫСЯЧА КЛИНКОВ БУРИ", "МЕТЕОРИТНЫЙ КАТАКЛИЗМ", "КОЛЕСО АРКАНЫ", "ГНЕВ ГРОМОВЕРЖЦА", "ПОЖИРАТЕЛЬ ДУШ", "ЖНЕЦ СМЕРТИ", "АБСОЛЮТНЫЙ НОЛЬ"};
        for (int w = 0; w < 7; ++w) {
            if (!evolvedWeapons[w] && itemLevels[w] >= 9 && itemLevels[relicForWeapon[w]] >= 4) {
                evolvedWeapons[w] = true;
                sound.play(SFX_EVOLVE, 0.01f);
                announce(GMSG_EVOLUTION, std::string("СИМБИОЗ: ") + evoNames[w] + " пробудился!", 5.0f);
                superCharge = std::min(100.0f, superCharge + 8.0f);
            }
        }
    }

    void triggerUltimate() {
        float healthCost = getSuperSacrificeCost();
        if (state != STATE_PLAYING || superCharge < 100.0f || playerHp <= healthCost + 1.0f) return;
        // The emergency spell has a real price: spend 12% max HP and cannot cast at critical health.
        playerHp -= healthCost;
        superCharge = 0.0f;
        superActive = true;
        superTimer = 0.65f;
        screenShake = 0.38f;
        sound.play(SFX_ULTIMATE, 0.01f);
        spawnParticles(playerX, playerY, 70, 0xFFFFD850, 270.0f, 5, true);
        float radius = 365.0f * getAreaMultiplier();
        superResolvingUltimate = true;
        for (size_t i = 0; i < monsters.size(); ++i) {
            float dx = monsters[i].x - playerX, dy = monsters[i].y - playerY;
            if (dx*dx + dy*dy <= radius*radius) {
                monsters[i].freezeTimer = std::max(monsters[i].freezeTimer, monsters[i].isBoss ? 0.25f : 1.0f);
                damageMonster(i, 68.0f + playerLevel * 3.4f, dx * 0.06f, dy * 0.06f, 28);
            }
        }
        superResolvingUltimate = false;
    }

    void initRun(int heroIdx, bool speakBanners = true) {
        selectedHero = std::max(0, std::min(4, heroIdx));
        runTime = 0.0f;
        resonatorSpawnTimer = 22.0f;
        resonatorSpawnIndex = 3;
        phoenixReviveTimer = 0.0f;
        portalSpawnTimer = 210.0f + (rand() % 150);
        portalSpawned = false;
        victoryMusicTimer = 0.0f;
        magnetAudioTimer = 0.0f;
        sound.musicMode = 0;
        waveNumber = 1;
        nextBreakthroughLevel = 7 + (rand() % 9);
        breakthroughCount = 0;
        breakthroughTimer = recoveryTimer = 0.0f;
        breakthroughActive = hordeMusicActive = breakthroughEarlyHit = breakthroughQueued = false;
        musicCrowdTempoTier = 0;
        sound.musicTempoTier.store(0,std::memory_order_relaxed);
        breakthroughEscalation = 0.0f;
        currentRunChapter = gameMode == MODE_STORY ? selectedCampaignMission().chapter : 0;
        // Разным боям — разная музыка: в кампании запись выбирается по номеру дела
        // (одно и то же дело всегда звучит одинаково), в эндлесе — случайный плейлист,
        // который переключается на следующий трек, когда предыдущий доиграл.
        // Без смещения: дело 01 берёт первый файл плейлиста, дело 28 — двадцать восьмой.
        // Имена файлов ss_gameplay_caseNN.mp3 идут по алфавиту ровно в порядке дел.
        sound.musicVariantSeed.store(gameMode == MODE_STORY ? selectedStoryMission : -1);
        sound.musicShuffle.store(gameMode == MODE_ENDLESS);
        bossTrioStarted = false; bossTrioSpawned = trioBossesDefeated = 0;
        storyMissionBossSpawned = false; missionCompletePending = false;
        buildTierUpTimer = 0.0f; buildTierUpValue = 1;
        endingCutscenePending = false; campaignEndPending = false; storyFinaleBlockedNoticeShown = false; bossDeferredTimer = 0.0f;
        buildLimitBonus = 0;
        lastBreakthroughWarningLevel = -1;
        hitFxWindowTimer = 0.0f;
        hitFxCountInWindow = hitFxClusterEmitted = 0;
        lastDifficultyCycle = -1;
        lastDifficultySegment = -1;
        lastSurgeWarningCycle = -1;
        difficultyBanner.clear();
        difficultyBannerTimer = 0.0f;
        superResolvingUltimate = false;
        kills = 0;
        killWindowStartCount = 0; killWindowTimer = 0.0f; measuredKillsPerSecond = 0.0f; lowActionTimer = 0.0f;
        runGold = 0;
        runBossKills = 0;
        buildTierUpValue = getBuildTier();
        dashRuneKillCounter = 0; veilCooldown = 0.0f; retaliationChargeTimer = 0.0f; overkillShardCooldown = 0.0f; crossfireGlobalCooldown = 0.0f; emberGlobalCooldown = 0.0f; frostShatterCooldown = 0.0f; projectileParryCooldown = 0.0f;
        ashRhythmSparkCount = 0; ashRhythmFlashTimer = 0.0f;
        totalDamageDealt = 0.0f;
        dpsWindowDamage = 0.0f;
        currentDps = 0.0f;
        dpsTimer = 0.0f;
        memset(recentDpsSamples, 0, sizeof(recentDpsSamples));
        recentDpsSum = 0.0f; recentDpsCount = 0; recentDpsIndex = 0;

        playerX = 0.0f;
        playerY = 0.0f;
        playerVelocityX = playerVelocityY = 0.0f;
        playerFacing = 1;
        playerMaxHp = HERO_DEFS[selectedHero].baseHp + getMetaUpgradeEffectRank(1) * 10.0f;
        playerHp = playerMaxHp;
        playerWard = wardFlashTimer = 0.0f;
        playerHurtTimer = 0.0f;
        playerSlowTimer = 0.0f;
        playerDeathTimer = 0.0f;
        runRecorded = false;
        dashTimer = 0.0f;
        dashCooldown = 0.0f;
        echoStepCharges = 0; dashEvadeTriggered = false;

        playerLevel = 1;
        playerXp = 0;
        xpToNext = getXpRequirementForLevel(playerLevel);

        memset(itemLevels, 0, sizeof(itemLevels));
        memset(evolvedWeapons, 0, sizeof(evolvedWeapons));
        superCharge = 0.0f; superActive = false; superTimer = 0.0f;
        if (!speakBanners) {
            // Игра готовит сцену для меню: озвучка и плашки боя здесь не звучат.
            // Раньше init() вызывал полноценный initRun(0), и при запуске игры ещё до
            // меню звучало «Пепел после бури… Дозор не отвечает».
            difficultyBanner.clear();
            difficultyBannerTimer = 0.0f;
            pendingObjectiveVoiceTimer = 0.0f;
        } else if (gameMode == MODE_STORY) {
            // When this case has its own recording, the generic "миссия начинается" line
            // stays silent so it cannot clip the narration by a fraction of a second.
            const std::string briefKey = missionBriefKey(selectedStoryMission);
            const bool ownNarration = sound.voice.hasClip(briefKey);
            if (ownNarration) announceVisual(GMSG_MISSION_BRIEFING, selectedCampaignMission().briefing, 8.0f);
            else announce(GMSG_MISSION_BRIEFING, selectedCampaignMission().briefing, 8.0f);
            const float clipLength = ownNarration ? sound.voice.clipSeconds(briefKey) : 0.0f;
            // В экране выбора дело уже прочитано вслух: старт забега не повторяет
            // ту же запись. Если она ещё играет, цель читается после её конца.
            const float sinceBrief = globalTime - briefSpokenAt;
            const bool heardRecently = briefSpokenMission == selectedStoryMission &&
                                       sinceBrief >= 0.0f &&
                                       sinceBrief < std::max(clipLength + 2.0f, 45.0f);
            const bool stillPlaying = heardRecently && sinceBrief < clipLength;
            if (!heardRecently) speakCaseBriefing(selectedStoryMission, true);
            difficultyBanner = selectedCampaignMission().objective;
            difficultyBannerTimer = 7.0f;
            float narrationLength = 0.0f;
            if (stillPlaying) narrationLength = std::max(0.0f, clipLength - sinceBrief);
            else if (clipLength > 0.0f) narrationLength = clipLength;
            pendingObjectiveVoiceTimer = narrationLength > 0.6f ? narrationLength + 0.25f : 0.0f;
            if (pendingObjectiveVoiceTimer <= 0.0f) speakOnly(GMSG_MISSION_OBJECTIVE, difficultyBanner);
        } else {
            announce(GMSG_ENDLESS_START, "БЕСКОНЕЧНАЯ НОЧЬ: КУЗНИЦА ТРЕНИРУЕТСЯ НА БЕЗОПАСНЫХ ЭХО.", 4.0f);
        }
        memset(weaponTimers, 0, sizeof(weaponTimers));
        memset(tierWeaponTimers, 0, sizeof(tierWeaponTimers));
        itemLevels[HERO_DEFS[selectedHero].startWeaponId] = 1;

        monsters.clear();
        projectiles.clear();
        pickups.clear();
        breakthroughActive = false; breakthroughEarlyHit = false; breakthroughQueued = false;
        breakthroughEscalation = 0.0f;
        smartOneTapCount[0] = smartOneTapCount[1] = smartOneTapCount[2] = 0;
        smartSpawnChance[0] = smartSpawnChance[1] = smartSpawnChance[2] = 0.01f;
        smartStrengthMultiplier[0] = smartStrengthMultiplier[1] = smartStrengthMultiplier[2] = 1.0f;
        pendingResonatorBurst = 0; pendingMatryoshkaSplits.clear(); eventCooldown = 35.0f;
        squadTimer = 14.0f;
        ratTimer = 30.0f;
        huntActive = false; huntPackTotal = huntPackRemaining = 0; huntTimer = 0.0f; huntCoinsDropped = 0;
        measuredKillsPerSecond = 0.0f; lowActionTimer = 0.0f; killWindowTimer = 0.0f;
        particles.clear();
        floatTexts.clear();
        lightningArcs.clear();
        frostWaves.clear();
        shockwaves.clear();
        skillImpactMarks.clear();
        bossHazards.clear();

        spawnAccumulator = 0.0f;
        pendingSummons = 0;
        nextBossIndex = 0;
        bossWarningTimer = 0.0f;
        bossPhaseEventTimer = 0.0f; bossPhaseEventType = 0; bossPhaseEventNumber = 1;
        bossRushTimer = 0.0f;
        screenShake = 0.0f;

        // Three destructible resonators seed reachable long-range objectives beyond the starting camera.
        for (int i = 0; i < 3; ++i) spawnResonator(i);
        // Spawn initial wave so action starts immediately!
        for (int i = 0; i < 10; ++i) spawnMonster(0, false, false);
    }

    static int particleStyleFor(uint32_t color, bool additive, int size) {
        if (!additive) return 4; // non-additive bursts are soft ash/smoke, not glowing dots
        const int r = (color >> 16) & 255, g = (color >> 8) & 255, b = color & 255;
        if (r > 185 && g > 75 && r > b * 1.15) return 2; // warm embers / impact fragments
        if (r > 135 && b > 135 && abs(r - b) < 90) return 3; // violet rune motes
        if (b > 165 && b >= g * 0.96 && b > r * 1.08) return size >= 3 ? 1 : 5; // cool crystal shards
        if (g > 145 && g > r * 1.12 && g > b * 0.95) return 3; // green spores
        return size >= 4 ? 1 : 0; // bright general sparks
    }

    void spawnParticles(float x, float y, int count, uint32_t col, float speed = 110.0f, int size = 3, bool additive = true) {
        if (particles.size() > 700) return;
        for (int i = 0; i < count; ++i) {
            float a = ((rand() % 360) * 3.14159f) / 180.0f;
            float sp = speed * (0.3f + (rand() % 100) / 100.0f);
            Particle p;
            p.x = x;
            p.y = y;
            p.vx = cosf(a) * sp;
            p.vy = sinf(a) * sp;
            p.maxLife = 0.25f + (rand() % 35) / 100.0f;
            p.life = p.maxLife;
            p.color = col;
            p.size = size;
            p.additive = additive;
            p.style = particleStyleFor(col, additive, size);
            p.rotation = a + ((rand() % 101) - 50) * 0.01f;
            p.spin = ((rand() % 201) - 100) * 0.035f;
            p.stretch = 0.75f + (rand() % 51) * 0.01f;
            p.scale = 1.0f;
            particles.push_back(p);
        }
    }

    void addShockwave(float x, float y, float maxRadius, uint32_t color, float duration = 0.85f) {
        if (shockwaves.size() >= 40) shockwaves.erase(shockwaves.begin());
        Shockwave wave = {x, y, 18.0f, maxRadius, duration, duration, color};
        shockwaves.push_back(wave);
    }

    static bool hasSkillImpactStyle(int attackId) {
        return (attackId >= 0 && attackId <= 6) || (attackId >= 24 && attackId <= 29);
    }

    void preserveParticleRandomBudget(int particleCount) {
        // Keep cosmetic VFX from changing the seeded gameplay RNG sequence when particles become decals.
        if (particles.size() > 700) return;
        for (int i = 0; i < particleCount * 6; ++i) (void)rand();
    }

    void spawnHitEffect(float x, float y, uint32_t color, bool critical = false,
                        int attackId = -1, float groundOffset = 0.0f) {
        if (!options.hitEffectsEnabled) return;
        if (hitFxWindowTimer <= 0.0f) {
            hitFxWindowTimer = 0.05f;
            hitFxCountInWindow = 0;
            hitFxClusterEmitted = 0;
        }
        bool compacted = false;
        if (hitFxCountInWindow < 3) {
            hitFxCountInWindow++;
        } else if (!hitFxClusterEmitted) {
            // Collapse further impacts within the 50 ms window into one cheaper mark.
            hitFxClusterEmitted = 1;
            compacted = true;
        } else return;

        if (hasSkillImpactStyle(attackId)) {
            preserveParticleRandomBudget(compacted ? 5 : (critical ? 8 : 5));
            // Keep impact marks on the ground plane, beneath the sprite; suppress clutter at contact range.
            const float dx = x - playerX, dy = y - playerY;
            if (dx * dx + dy * dy < 58.0f * 58.0f || skillImpactMarks.size() >= 20) return;
            SkillImpactMark mark{};
            mark.x = x; mark.y = y + groundOffset * 0.68f;
            mark.maxLife = mark.life = compacted ? 0.18f : (critical ? 0.38f : 0.30f);
            mark.attackId = attackId;
            mark.critical = critical && !compacted;
            skillImpactMarks.push_back(mark);
            return;
        }

        if (!compacted) {
            const int count = critical ? 8 : 5;
            spawnParticles(x, y, count, color, critical ? 175.0f : 125.0f, critical ? 4 : 3, true);
            addShockwave(x, y, critical ? 36.0f : 23.0f, color, critical ? 0.28f : 0.20f);
        } else {
            spawnParticles(x, y, 5, color, 90.0f, 3, true);
            addShockwave(x, y, 30.0f, color, 0.24f);
        }
    }

    void renderSkillImpactMarks(Renderer& r, float camX, float camY) const {
        for (const SkillImpactMark& mark : skillImpactMarks) {
            const int sx = (int)(mark.x - camX), sy = (int)(mark.y - camY);
            if (sx < -48 || sx > SCREEN_W + 48 || sy < -48 || sy > SCREEN_H + 48) continue;
            const float progress = 1.0f - mark.life / std::max(0.001f, mark.maxLife);
            const int radius = 8 + (int)(progress * 12.0f);
            const int alpha = std::max(0, std::min(190, (int)(190.0f * (mark.life / mark.maxLife))));
            uint32_t color = 0xFFE7EFFF;
            switch (mark.attackId) {
                case 0: color = 0xFF91E9FF; break; // Silver Blade — crossing ground cuts.
                case 1: color = 0xFFFF8A35; break; // Fireball — charred orange crater.
                case 2: color = 0xFFC66BFF; break; // Rune Whirl — violet rune diamond.
                case 3: color = 0xFF63E6FF; break; // Chain Lightning — branching cyan fork.
                case 4: color = 0xFFFF496E; break; // Blood Aura — crimson pulse sigil.
                case 5: color = 0xFFFFC45A; break; // Battleaxe — heavy golden cleave.
                case 6: color = 0xFF73E9FF; break; // Frost Ring — six-point ice mark.
                case 24: color = 0xFF71DCFF; break; // Comet Shard — crystal splinter.
                case 25: color = 0xFF56D8FF; break; // Rune Turn — square pulse glyph.
                case 26: color = 0xFFBC83FF; break; // Rift Spear — forked arc.
                case 27: color = 0xFFFF9A42; break; // Ash Star — meteor scorch.
                case 28: color = 0xFFFFD96A; break; // Ultimate — sunburst seal.
                case 29: color = 0xFF58E4FF; break; // Dash — paired streaks.
                default: break;
            }
            r.fillCircle(sx, sy, radius + 3, 0xFF100A18, alpha / 5);
            switch (mark.attackId) {
                case 0:
                    r.drawLine(sx - radius, sy + radius / 2, sx + radius, sy - radius / 2, color, 3, alpha, true);
                    r.drawLine(sx - radius / 2, sy + radius, sx + radius / 2, sy - radius, 0xFFEFFFFF, 1, alpha * 3 / 4, true);
                    break;
                case 1:
                case 27:
                    r.fillCircle(sx, sy, radius / 2, color, alpha / 5);
                    r.drawCircleRing(sx, sy, radius, 2, color, alpha, true);
                    for (int i = 0; i < 4; ++i) {
                        const float a = i * 1.5707963f + 0.35f;
                        r.drawLine(sx + (int)(cosf(a) * radius), sy + (int)(sinf(a) * radius),
                                   sx + (int)(cosf(a) * (radius + 5)), sy + (int)(sinf(a) * (radius + 5)), color, 2, alpha, true);
                    }
                    break;
                case 2:
                case 24:
                    r.drawLine(sx, sy - radius, sx + radius, sy, color, 2, alpha, true);
                    r.drawLine(sx + radius, sy, sx, sy + radius, color, 2, alpha, true);
                    r.drawLine(sx, sy + radius, sx - radius, sy, color, 2, alpha, true);
                    r.drawLine(sx - radius, sy, sx, sy - radius, color, mark.attackId == 24 ? 3 : 2, alpha, true);
                    break;
                case 3:
                case 26:
                    r.drawLine(sx - 2, sy - radius, sx + 3, sy - 3, color, 3, alpha, true);
                    r.drawLine(sx + 3, sy - 3, sx - 5, sy + 2, color, 3, alpha, true);
                    r.drawLine(sx - 5, sy + 2, sx - radius, sy + radius, color, 2, alpha, true);
                    r.drawLine(sx + 3, sy - 3, sx + radius, sy + radius, color, 2, alpha, true);
                    break;
                case 4:
                case 25:
                    r.drawCircleRing(sx, sy, radius, 2, color, alpha, true);
                    for (int i = 0; i < 4; ++i) {
                        const float a = i * 1.5707963f;
                        r.drawLine(sx + (int)(cosf(a) * (radius - 3)), sy + (int)(sinf(a) * (radius - 3)),
                                   sx + (int)(cosf(a) * (radius + 3)), sy + (int)(sinf(a) * (radius + 3)), color, 2, alpha, true);
                    }
                    break;
                case 5:
                    r.drawLine(sx - radius, sy - radius / 2, sx - 2, sy + radius / 2, color, 4, alpha, true);
                    r.drawLine(sx - 2, sy + radius / 2, sx + radius, sy - radius / 2, 0xFFFFE7A5, 2, alpha, true);
                    break;
                case 6:
                    for (int i = 0; i < 3; ++i) {
                        const float a = i * 1.0471976f;
                        const int dx = (int)(cosf(a) * radius), dy = (int)(sinf(a) * radius);
                        r.drawLine(sx - dx, sy - dy, sx + dx, sy + dy, color, 2, alpha, true);
                    }
                    r.drawCircleRing(sx, sy, radius / 2, 1, 0xFFE4FFFF, alpha, true);
                    break;
                case 28:
                    r.drawCircleRing(sx, sy, radius, 2, color, alpha, true);
                    for (int i = 0; i < 8; ++i) {
                        const float a = i * 0.7853982f;
                        r.drawLine(sx + (int)(cosf(a) * 3), sy + (int)(sinf(a) * 3),
                                   sx + (int)(cosf(a) * radius), sy + (int)(sinf(a) * radius), color, 2, alpha, true);
                    }
                    break;
                case 29:
                    r.drawLine(sx - radius, sy - 4, sx + radius, sy - 4, color, 2, alpha, true);
                    r.drawLine(sx - radius + 3, sy, sx + radius - 3, sy, 0xFFDDFBFF, 2, alpha, true);
                    r.drawLine(sx - radius, sy + 4, sx + radius, sy + 4, color, 2, alpha, true);
                    break;
                default:
                    r.drawCircleRing(sx, sy, radius, 2, color, alpha, true);
                    break;
            }
            if (mark.critical) r.drawCircleRing(sx, sy, radius + 6, 1, 0xFFFFE8A0, alpha / 2, true);
        }
    }

    void addFloatingText(float x, float y, const char* txt, uint32_t col, bool big = false) {
        if (floatTexts.size() > 90) floatTexts.erase(floatTexts.begin());
        FloatingText ft;
        ft.x = x + (rand() % 16 - 8);
        ft.y = y;
        ft.vy = big ? -48.0f : -36.0f;
        ft.maxLife = big ? 0.95f : 0.65f;
        ft.life = ft.maxLife;
        ft.text = txt;
        ft.color = col;
        ft.big = big;
        floatTexts.push_back(ft);
    }

    void getVisiblePlayfieldBounds(float margin, float& minDx, float& maxDx, float& minDy, float& maxDy) const {
        minDx = -SCREEN_W * 0.5f + margin;
        maxDx =  SCREEN_W * 0.5f - margin;
        minDy = -SCREEN_H * 0.5f + 130.0f + margin; // Keep actors below the top HUD.
        maxDy =  SCREEN_H * 0.5f - 42.0f - margin;  // Keep actors above the bottom HUD.
    }

    bool isWorldPointVisible(float x, float y, float margin = 32.0f) const {
        float minDx, maxDx, minDy, maxDy;
        getVisiblePlayfieldBounds(margin, minDx, maxDx, minDy, maxDy);
        const float dx = x - playerX, dy = y - playerY;
        return dx >= minDx && dx <= maxDx && dy >= minDy && dy <= maxDy;
    }

    void clampWorldPointToPlayfield(float& x, float& y, float margin) const {
        float minDx, maxDx, minDy, maxDy;
        getVisiblePlayfieldBounds(margin, minDx, maxDx, minDy, maxDy);
        x = playerX + std::max(minDx, std::min(maxDx, x - playerX));
        y = playerY + std::max(minDy, std::min(maxDy, y - playerY));
    }

    // Rejection-sample in camera-relative playfield space. Unlike radial-then-clamp spawning,
    // this never takes an offscreen coordinate and squashes it onto a screen edge.
    bool chooseVisibleSpawnPoint(float minDistance, float margin, float& outX, float& outY) const {
        float minDx, maxDx, minDy, maxDy;
        getVisiblePlayfieldBounds(margin, minDx, maxDx, minDy, maxDy);
        if (maxDx <= minDx || maxDy <= minDy) return false;
        for (int attempt = 0; attempt < 96; ++attempt) {
            const float rx = (rand() % 10001) / 10000.0f;
            const float ry = (rand() % 10001) / 10000.0f;
            const float dx = minDx + rx * (maxDx - minDx);
            const float dy = minDy + ry * (maxDy - minDy);
            if (dx * dx + dy * dy < minDistance * minDistance) continue;
            outX = playerX + dx; outY = playerY + dy;
            if (isWorldPointVisible(outX, outY, margin)) return true;
        }
        // Deterministic safe fallback: the farthest safe-rectangle corner from the hero.
        const float xs[2] = {minDx, maxDx}, ys[2] = {minDy, maxDy};
        float bestD2 = -1.0f, bestX = 0.0f, bestY = 0.0f;
        for (float dx : xs) for (float dy : ys) {
            const float d2 = dx * dx + dy * dy;
            if (d2 > bestD2) { bestD2 = d2; bestX = dx; bestY = dy; }
        }
        outX = playerX + bestX; outY = playerY + bestY;
        return isWorldPointVisible(outX, outY, margin);
    }

    bool isWorldPointOffscreen(float x, float y, float margin = 0.0f) const {
        const float dx = x - playerX, dy = y - playerY;
        return dx < -SCREEN_W * 0.5f - margin || dx > SCREEN_W * 0.5f + margin ||
               dy < -SCREEN_H * 0.5f - margin || dy > SCREEN_H * 0.5f + margin;
    }

    // Ordinary foes enter from beyond the full camera rectangle and stay hidden until they walk in.
    // Squads arrive together from one edge, in a readable line: that is the difference
    // between "enemies trickle in" and "something is coming for you".
    bool chooseSideSpawnPoint(int side, float along, float margin, float& outX, float& outY) const {
        const float outerX = SCREEN_W * 0.5f + margin + 26.0f;
        const float outerY = SCREEN_H * 0.5f + margin + 26.0f;
        float dx = along, dy = 0.0f;
        if (side == 0) { dx = -outerX; dy = along; }
        else if (side == 1) { dx = outerX; dy = along; }
        else if (side == 2) { dx = along; dy = -outerY; }
        else { dx = along; dy = outerY; }
        outX = playerX + dx; outY = playerY + dy;
        return isWorldPointOffscreen(outX, outY, margin * 0.6f);
    }

    bool chooseOffscreenSpawnPoint(float minimumDistance, float margin, float& outX, float& outY) const {
        const float outerX = SCREEN_W * 0.5f + margin + 18.0f;
        const float outerY = SCREEN_H * 0.5f + margin + 18.0f;
        for (int attempt = 0; attempt < 96; ++attempt) {
            const int side = rand() % 4;
            const float alongX = ((rand() % 20001) / 10000.0f - 1.0f) * (SCREEN_W * 0.5f + 80.0f);
            const float alongY = ((rand() % 20001) / 10000.0f - 1.0f) * (SCREEN_H * 0.5f + 80.0f);
            const float extra = (float)(rand() % 91);
            float dx = alongX, dy = alongY;
            if (side == 0) dx = -outerX - extra;
            else if (side == 1) dx = outerX + extra;
            else if (side == 2) dy = -outerY - extra;
            else dy = outerY + extra;
            if (dx * dx + dy * dy < minimumDistance * minimumDistance) continue;
            outX = playerX + dx; outY = playerY + dy;
            if (isWorldPointOffscreen(outX, outY, margin)) return true;
        }
        // Deterministic fallback picks the farthest camera-exterior corner.
        float bestD2 = -1.0f;
        for (int sx : {-1, 1}) for (int sy : {-1, 1}) {
            const float dx = sx * (outerX + 48.0f), dy = sy * (outerY + 48.0f);
            const float d2 = dx * dx + dy * dy;
            if (d2 > bestD2) { bestD2 = d2; outX = playerX + dx; outY = playerY + dy; }
        }
        return bestD2 >= minimumDistance * minimumDistance && isWorldPointOffscreen(outX, outY, margin);
    }

    void teleportBossTowardMovement(Monster& boss) {
        float dx = playerVelocityX, dy = playerVelocityY;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 32.0f) {
            dx = playerFacing < 0 ? -1.0f : 1.0f;
            dy = 0.0f;
        } else { dx /= len; dy /= len; }
        float minDx, maxDx, minDy, maxDy;
        const float margin = 58.0f;
        getVisiblePlayfieldBounds(margin, minDx, maxDx, minDy, maxDy);
        const float tx = fabsf(dx) < 0.001f ? 1.0e6f : (dx > 0.0f ? maxDx : -minDx) / fabsf(dx);
        const float ty = fabsf(dy) < 0.001f ? 1.0e6f : (dy > 0.0f ? maxDy : -minDy) / fabsf(dy);
        const float reach = std::min(tx, ty) * 0.90f;
        static const uint32_t markerPalette[6] = {0xFFFFC848, 0xFF82F05A, 0xFFFF4C78, 0xFFFF7628, 0xFF49C8E8, 0xFF9EDBE8};
        const uint32_t color = markerPalette[boss.bossType % 6];
        spawnParticles(boss.x, boss.y, 24, color, 145.0f, 4, true);
        boss.x = playerX + dx * reach;
        boss.y = playerY + dy * reach;
        clampWorldPointToPlayfield(boss.x, boss.y, margin);
        addShockwave(boss.x, boss.y, 74.0f, color, 0.42f);
        spawnParticles(boss.x, boss.y, 30, color, 160.0f, 4, true);
        boss.facing = boss.x < playerX ? 1 : -1;
    }

    // A boss entrance resets the arena: every mob on screen dies at once, and the boss
    // rush window that follows repopulates it five times as fast.
    void clearArenaForBoss() {
        int cleared = 0;
        for (Monster& mob : monsters) {
            if (mob.isBoss || mob.isResonator || mob.hp <= 0.0f) continue;
            spawnParticles(mob.x, mob.y, 10, 0xFFE8DCFF, 150.0f, 3, false);
            mob.hp = 0.0f;
            ++cleared;
        }
        if (cleared > 0) {
            addShockwave(playerX, playerY, 420.0f, 0xFFFFE9BC, 0.75f);
            sound.play(SFX_TIER_UP, 0.10f);
        }
        bossRushTimer = BOSS_RUSH_SECONDS;
    }

    // A squad is a small authored group: it enters from one edge in a line, so the player
    // reads "shield wall coming from the left" instead of an endless random trickle.
    void spawnSquad() {
        const int theme = currentArenaTheme();
        const int maxType = std::min(9, 1 + (int)(runTime / 20.0f) + theme * 2);
        const int side = rand() % 4;
        int type = 0, count = 4;
        bool elites = false;
        if (theme == 0)      { type = 0; count = 4; }                       // imps, loose line
        else if (theme == 1) { type = 1; count = 5; }                       // skeleton patrol
        else if (theme == 2) { type = maxType >= 8 ? 8 : 3; count = 4; }    // firing line
        else if (theme == 3) { type = 4; count = 3; elites = true; }        // armoured push
        else                 { type = 6; count = 4; }                       // archive wardens
        type = std::min(type, maxType);
        const float spacing = 62.0f;
        for (int i = 0; i < count; ++i) {
            const float along = (i - (count - 1) * 0.5f) * spacing;
            spawnMonster(type, elites && i % 2 == 0, false, 0, side, along);
        }
        // A second half-squad from the opposite edge turns the push into a pincer in the
        // later acts, and stays a single readable line in the first one.
        if (theme >= 2 && (rand() % 100) < (theme == 2 ? 45 : 70)) {
            const int otherType = std::min(theme == 4 ? 3 : 2, maxType);
            const int opposite = (side + 2) % 4;
            for (int i = 0; i < 3; ++i)
                spawnMonster(otherType, false, false, 0, opposite, (i - 1) * spacing);
        }
    }

    // A rat swarm enters from one edge and crosses the whole arena in a straight line.
    // It never turns toward the player: it is a moving hazard you can also farm.
    void spawnRatSwarm() {
        const int side = rand() % 4;
        const float margin = 96.0f;
        const float outerX = SCREEN_W * 0.5f + margin;
        const float outerY = SCREEN_H * 0.5f + margin;
        float startX = playerX, startY = playerY;
        float dirX = 0.0f, dirY = 0.0f;
        // Travel direction is perpendicular to the entry edge, with a slight diagonal so
        // the line cuts across the screen instead of hugging one edge.
        const float skew = ((rand() % 101) / 100.0f - 0.5f) * 0.55f;
        if (side == 0)      { startX = playerX - outerX; startY = playerY + ((rand() % 201) - 100) / 100.0f * outerY * 0.7f; dirX = 1.0f; dirY = skew; }
        else if (side == 1) { startX = playerX + outerX; startY = playerY + ((rand() % 201) - 100) / 100.0f * outerY * 0.7f; dirX = -1.0f; dirY = skew; }
        else if (side == 2) { startY = playerY - outerY; startX = playerX + ((rand() % 201) - 100) / 100.0f * outerX * 0.7f; dirX = skew; dirY = 1.0f; }
        else                { startY = playerY + outerY; startX = playerX + ((rand() % 201) - 100) / 100.0f * outerX * 0.7f; dirX = skew; dirY = -1.0f; }
        const float len = std::max(0.001f, sqrtf(dirX * dirX + dirY * dirY));
        dirX /= len; dirY /= len;
        // Крыс вдвое больше: стая должна быть плотной массой, которую видно и
        // слышно, а не редкой цепочкой из восьми штук.
        const int count = 16 + rand() % 9;
        // Стая — это кучка, а не шеренга: раньше крысы вставали на перпендикулярную
        // линию с шагом 34 px и читались как строй. Теперь точки берутся в круге
        // радиусом ~54 px вокруг точки входа, с плотным ядром и редкими отставшими.
        const float packR = 54.0f;
        for (int i = 0; i < count; ++i) {
            const size_t before = monsters.size();
            spawnMonster(RAT_ENEMY_TYPE, false, false);
            if (monsters.size() == before) continue;
            Monster& rat = monsters.back();
            // Золотой угол даёт равномерное заполнение, не слипаясь в одну точку.
            const float ang = i * 2.39996323f + ((rand() % 100) / 100.0f) * 0.4f;
            const float rad = packR * sqrtf((i + 0.6f) / count) * (0.75f + ((rand() % 100) / 100.0f) * 0.5f);
            rat.x = startX + cosf(ang) * rad;
            rat.y = startY + sinf(ang) * rad * 0.8f;
            const float scatter = ((rand() % 100) / 100.0f - 0.5f) * 0.16f;
            const float cs = cosf(scatter), sn = sinf(scatter);
            rat.ratDirX = dirX * cs - dirY * sn;
            rat.ratDirY = dirX * sn + dirY * cs;
            rat.ratLife = 6.5f;
            rat.ratPhase = (rand() % 200) * 0.1f;
            rat.facing = rat.ratDirX >= 0.0f ? 1 : -1;
            rat.ratHeading = atan2f(rat.ratDirY, rat.ratDirX);
        }
    }

    void spawnMonster(int type, bool isElite, bool isBoss, int bossType = 0, int squadSide = -1, float squadAlong = 0.0f) {
        // Keep the hard world cap inclusive of bosses and summoned units. A boss may
        // displace one regular enemy at the ceiling rather than pushing beyond 520.
        if (monsters.size() >= 520) {
            if (!isBoss) return;
            auto regular = std::find_if(monsters.begin(), monsters.end(), [](const Monster& m) { return !m.isBoss; });
            if (regular == monsters.end()) return;
            monsters.erase(regular);
        }
        Monster m;
        m.uniqueId = nextMonsterId++;
        // Reserve room for the full sprite, its ground shadow and the HUD; reject unsafe points rather than clamping them.
        const float spawnMargin = isBoss ? 78.0f : 64.0f;
        const float minimumDistance = isBoss ? 360.0f : 440.0f;
        bool placed = false;
        if (squadSide >= 0) placed = chooseSideSpawnPoint(squadSide, squadAlong, spawnMargin, m.x, m.y);
        if (!placed) {
            placed = isBoss
                ? chooseVisibleSpawnPoint(minimumDistance, spawnMargin, m.x, m.y)
                : chooseOffscreenSpawnPoint(minimumDistance, spawnMargin, m.x, m.y);
        }
        if (!placed) return;
        m.facing = m.x < playerX ? 1 : -1;
        m.vx = 0.0f;
        m.vy = 0.0f;
        m.type = type;
        m.isBoss = isBoss;
        m.bossPhase = 1;
        m.isElite = isElite;
        m.isMatryoshka = !isBoss && type == MATRYOSHKA_ENEMY_TYPE;
        m.isRat = !isBoss && type == RAT_ENEMY_TYPE;
        m.bossType = bossType;
        m.isShieldBearer = false; m.shieldHits = 0;
        float shieldDx = playerX - m.x, shieldDy = playerY - m.y;
        float shieldLen = std::max(1.0f, sqrtf(shieldDx * shieldDx + shieldDy * shieldDy));
        m.shieldFacingX = shieldDx / shieldLen; m.shieldFacingY = shieldDy / shieldLen;
        // Only the gate warden blocks. The others defend by moving: plague pools, a charge,
        // a ring the player has to slip through.
        if (isBoss && bossType == 0) m.shieldHits = 1;
        m.smartTactic = (!isBoss && !isElite && !m.isRat) ? rand() % 3 : 0;
        m.isSmart = !isBoss && !isElite && !m.isMatryoshka && !m.isRat && ((rand() % 10000) < (int)(smartSpawnChance[m.smartTactic] * 10000.0f));
        m.smartDodgeTimer = 0.0f;
        m.smartThinkTimer = 0.0f;
        m.smartDodgeX = m.smartDodgeY = 0.0f;
        m.hitFlashTimer = 0.0f;
        m.freezeTimer = 0.0f;
        m.shootTimer = m.isSmart && m.smartTactic == 0 ? 0.75f : (1.3f + (rand() % 150) / 100.0f);
        m.specialTimer = 3.0f + (rand() % 200) / 100.0f;
        m.abilityTimer = isBoss ? (5.5f + bossType * 0.5f + (rand() % 160) / 100.0f) : 0.0f;
        m.abilityWindup = m.abilityActive = 0.0f;
        m.abilityDirX = m.abilityDirY = 0.0f;
        m.abilityTargetX = m.abilityTargetY = 0.0f;
        m.animOffset = (rand() % 100) * 0.1f;

        float timeScale = getLongTermEnemyTier();
        float phaseHpScale = getSublevelEnemyHpMultiplier();
        if (isBoss) {
            static const float bossHp[6] = {1800.0f, 3300.0f, 6100.0f, 10500.0f, 4300.0f, 8200.0f};
            m.maxHp = bossHp[bossType % 6] * (1.0f + std::min(1.0f, runTime / 480.0f) * 0.32f) * timeScale * balance.enemyHpMultiplier * phaseHpScale;
            m.maxHp *= getEnemyOathHpMultiplier();
            m.hp = m.maxHp;
            m.speed = 70.0f + bossType * 7.0f;
            m.speed *= getEnemyOathSpeedMultiplier();
            m.damage = (22.0f + bossType * 9.0f) * balance.enemyDamageMultiplier;
            m.damage *= getEnemyOathDamageMultiplier();
            m.radius = 30.0f;
        } else {
            static const float baseHps[MONSTER_TYPE_COUNT]   = {28.0f, 48.0f, 42.0f, 52.0f, 115.0f, 68.0f, 84.0f, 135.0f, 72.0f, 190.0f, 112.0f, 18.0f};
            static const float baseSpds[MONSTER_TYPE_COUNT]  = {102.0f, 72.0f, 94.0f, 66.0f, 62.0f, 86.0f, 88.0f, 58.0f, 70.0f, 76.0f, 82.0f, 215.0f};
            static const float baseDmgs[MONSTER_TYPE_COUNT]  = {9.0f,  12.0f, 12.0f, 14.0f, 19.0f, 16.0f, 16.0f, 21.0f, 15.0f, 24.0f, 11.0f, 5.0f};
            static const float baseRads[MONSTER_TYPE_COUNT]  = {13.0f, 15.0f, 16.0f, 15.0f, 20.0f, 18.0f, 17.0f, 21.0f, 16.0f, 23.0f, 18.0f, 7.0f};
            const int safeType = std::max(0, std::min(MONSTER_TYPE_COUNT - 1, type));
            m.maxHp = baseHps[safeType] * timeScale * balance.enemyHpMultiplier * phaseHpScale * (isElite ? 3.2f : 1.0f);
            m.maxHp *= getEnemyOathHpMultiplier();
            m.hp = m.maxHp;
            m.speed = baseSpds[safeType] * (isElite ? 1.12f : 1.0f);
            m.speed *= getEnemyOathSpeedMultiplier();
            m.damage = baseDmgs[safeType] * balance.enemyDamageMultiplier * (isElite ? 1.4f : 1.0f);
            m.damage *= getEnemyOathDamageMultiplier();
            m.radius = baseRads[safeType] * (isElite ? 1.25f : 1.0f);
            int shieldChance = options.hardModeEnabled ? 105 : 48;
            m.isShieldBearer = !isElite && !m.isMatryoshka && !m.isRat && (rand() % 1000) < shieldChance;
            if (m.isShieldBearer) { m.shieldHits = 3; m.maxHp *= 1.18f; m.hp = m.maxHp; }
        }
        if (m.isSmart) {
            const float learnedStrength = smartStrengthMultiplier[m.smartTactic];
            m.maxHp *= 1.12f * learnedStrength;
            m.hp = m.maxHp;
            m.speed *= 1.06f * learnedStrength;
            m.damage *= learnedStrength;
            m.shootTimer /= std::min(1.4f, learnedStrength);
        }
        m.speed *= getPopulationAggression();
        if (!options.hardModeEnabled && !isBoss && getThreatPressure() > 0.85f)
            m.speed *= 0.94f;
        monsters.push_back(m);
    }

    float getResonatorOffscreenRadius() const {
        // Target the nearest safe ring outside the camera; the diagonal angle needs a little extra margin.
        const float halfW = SCREEN_W * 0.5f, halfH = SCREEN_H * 0.5f;
        return std::max(950.0f, std::max(halfW + 140.0f, (halfH + 140.0f) / 0.8660254f));
    }

    void spawnResonator(int index) {
        if (monsters.size() >= 520 || countResonators() >= MAX_ACTIVE_RESONATORS) return;
        float bx = playerX, by = playerY;
        bool found = false;
        const float nearRing = getResonatorOffscreenRadius();
        for (int attempt = 0; attempt < 96 && !found; ++attempt) {
            if (index < 3) {
                // A reachable triangle just beyond the visible field, not several thousand units away.
                static const float angles[3] = {0.0f, 2.0943951f, 4.1887902f};
                const float distance = nearRing + index * 300.0f;
                const float a = angles[index];
                bx = playerX + cosf(a) * distance;
                by = playerY + sinf(a) * distance;
                if (!isWorldPointOffscreen(bx, by, 58.0f)) continue;
            } else {
                const float angle = (float)(rand() % 100000) * (6.2831853f / 100000.0f);
                const float distance = nearRing + 220.0f + (float)(rand() % 1001);
                bx = playerX + cosf(angle) * distance;
                by = playerY + sinf(angle) * distance;
                if (!isWorldPointOffscreen(bx, by, 58.0f)) continue;
            }
            found = true;
            for (const Monster& other : monsters) {
                if (!other.isResonator || other.hp <= 0.0f) continue;
                const float dx = bx - other.x, dy = by - other.y;
                if (dx * dx + dy * dy < 950.0f * 950.0f) { found = false; break; }
            }
        }
        if (!found) return;
        Monster b{};
        b.uniqueId = nextMonsterId++;
        b.x = bx; b.y = by;
        b.facing = 1; b.vx = b.vy = 0.0f; b.type = 0; b.isBoss = false; b.isElite = false; b.isResonator = true;
        b.bossType = 0; b.hp = b.maxHp = std::max(1.0f, getAverageDpsLastMinute() * 5.0f); b.speed = 0.0f; b.damage = 0.0f; b.radius = 38.0f;
        b.hitFlashTimer = b.freezeTimer = 0.0f; b.shootTimer = 999.0f; b.specialTimer = b.abilityTimer = 999.0f;
        b.abilityWindup = b.abilityActive = b.animOffset = 0.0f;
        monsters.push_back(b);
    }

    static const char* resonatorForm(int count) {
        const int n100 = count % 100;
        if (n100 >= 11 && n100 <= 14) return "РЕЗОНАТОРОВ";
        switch (count % 10) {
            case 1: return "РЕЗОНАТОР";
            case 2: case 3: case 4: return "РЕЗОНАТОРА";
            default: return "РЕЗОНАТОРОВ";
        }
    }
    static const char* enemyForm(int count) {
        const int n100 = count % 100;
        if (n100 >= 11 && n100 <= 14) return "ВРАГОВ";
        switch (count % 10) {
            case 1: return "ВРАГ";
            case 2: case 3: case 4: return "ВРАГА";
            default: return "ВРАГОВ";
        }
    }
    int countResonators() const {
        int count = 0;
        for (const Monster& m : monsters) if (m.isResonator && m.hp > 0.0f) ++count;
        return count;
    }
    float getResonatorSpawnRateBonus() const {
        return countResonators() * RESONATOR_SPAWN_RATE_PER_SECOND;
    }

    void releaseMatryoshkaChildren() {
        if (pendingMatryoshkaSplits.empty()) return;
        std::vector<PendingMatryoshkaSplit> pending;
        pending.swap(pendingMatryoshkaSplits);
        for (const PendingMatryoshkaSplit& split : pending) {
            for (int child = 0; child < MATRYOSHKA_CHILDREN; ++child) {
                if (monsters.size() >= 520) {
                    // Preserve the hard world cap but still honor the split: evict one ordinary non-boss.
                    auto expendable = std::find_if(monsters.begin(), monsters.end(), [](const Monster& m) {
                        return !m.isBoss && !m.isResonator && !m.isMatryoshka && m.hp > 0.0f;
                    });
                    if (expendable == monsters.end()) break;
                    monsters.erase(expendable);
                }
                const size_t before = monsters.size();
                spawnMonster((child == 0) ? 0 : 1, false, false); // bats and skeletons; never chain into another matryoshka
                if (monsters.size() == before) continue;
                Monster& spawned = monsters.back();
                const float angle = (child == 0 ? 0.0f : 3.1415926f) + (spawned.uniqueId % 7) * 0.11f;
                const float distance = 28.0f + spawned.radius;
                const float childX = split.x + cosf(angle) * distance;
                const float childY = split.y + sinf(angle) * distance;
                // Keep the local split only if the full child fits in view; otherwise retain spawnMonster's safe visible point.
                if (isWorldPointVisible(childX, childY, 54.0f)) { spawned.x = childX; spawned.y = childY; }
                spawned.facing = spawned.x < playerX ? 1 : -1;
                spawnParticles(spawned.x, spawned.y, 8, 0xFFFFD66E, 95.0f, 3, true);
            }
        }
    }

    void releaseResonatorBurst() {
        const int requested = pendingResonatorBurst;
        pendingResonatorBurst = 0;
        if (requested <= 0) return;
        // Release the stored pack around the active camera/player so none appear in an unseen area.
        int spawned = 0;
        for (int i = 0; i < requested; ++i) {
            const bool elite = (i % 19 == 0);
            const int type = (i % 11 == 0) ? 8 : ((i % 4 == 0) ? 1 : 0);
            const size_t before = monsters.size();
            spawnMonster(type, elite, false);
            if (monsters.size() > before) ++spawned;
        }
        char burst[160]; // UTF-8 text must not be byte-truncated before the HUD can wrap it.
        snprintf(burst, sizeof(burst), "РЕЗОНАТОР РАЗРУШЕН: +%d %s", spawned, enemyForm(spawned));
        announce(GMSG_RESONATOR_DESTROYED, burst, 3.2f);
        if (options.screenShakeEnabled) screenShake = std::max(screenShake, 0.32f);
    }

    void triggerHuntEvent() {
        huntActive = true;
        huntPackTotal = 20;
        huntPackRemaining = 20;
        huntTimer = 30.0f;
        huntCoinsDropped = 0;
        for (int i = 0; i < huntPackTotal; ++i) {
            const size_t before = monsters.size();
            spawnMonster(i % 6 == 0 ? 8 : (i % 3 == 0 ? 1 : 0), i % 7 == 0, false);
            if (monsters.size() > before) monsters.back().fromHunt = true;
        }
        announce(GMSG_WORLD_EVENT, "ТИХАЯ ОХОТА: СТАЯ ИДЁТ ПО СЛЕДУ", 3.4f);
        difficultyBanner = "СТАЯ НА СЛЕДУ: ДОБЕЙ 20 — ЗАБЕРЁШЬ НАГРАДУ";
        difficultyBannerTimer = 4.0f;
        eventCooldown = 72.0f;
        lowActionTimer = 0.0f;
    }

    // Called from the kill path for pack members only.
    void onHuntPackKill(float x, float y) {
        if (!huntActive) return;
        --huntPackRemaining;
        if (huntPackRemaining % 3 == 0 || huntPackRemaining <= 2) {
            Pickup cache{};
            cache.x = x; cache.y = y;
            cache.type = 3; cache.value = 18; cache.bobTime = (rand() % 100) * 0.05f;
            pickups.push_back(cache);
            ++huntCoinsDropped;
        }
        if (huntPackRemaining <= 0) {
            huntActive = false;
            const int bounty = 40 + (int)runTime / 10;
            awardGold(bounty);
            addFloatingText(playerX, playerY - 60.0f, "СТАЯ УНИЧТОЖЕНА", 0xFFFFD874, true);
            spawnParticles(playerX, playerY, 30, 0xFFFFD874, 180.0f, 4, true);
            announce(GMSG_HUNT_CLEARED, "СТАЯ УНИЧТОЖЕНА: НАГРАДА ЗАБРАНА", 3.0f);
        }
    }

    void failHuntEvent() {
        huntActive = false;
        for (Monster& mob : monsters) if (mob.fromHunt && mob.hp > 0.0f) mob.fromHunt = false;
        announce(GMSG_HUNT_ESCAPED, "СТАЯ УШЛА ПО СЛЕДУ: НАГРАДЫ НЕТ", 3.0f);
    }

    void generateUpgradeChoices() {
        auto collectCandidates = [this]() {
            std::vector<int> candidates;
            const int equipped = countEquippedItems();
            const int buildLimit = getBuildLimit();
            for (int i = 0; i < ITEM_COUNT; ++i) {
                if (!isItemUnlocked(i) || itemLevels[i] >= getItemMaxLevel(i)) continue;
                if (itemLevels[i] == 0 && equipped >= buildLimit) continue;
                candidates.push_back(i);
            }
            return candidates;
        };
        std::vector<int> pool = collectCandidates();
        // If every acquired item is maxed, unlock one more slot rather than presenting dead cards.
        if (pool.empty() && countEquippedItems() >= getBuildLimit() && countEquippedItems() < ITEM_COUNT) {
            bool hasUnlockedNewItem = false;
            for (int i = 0; i < ITEM_COUNT; ++i)
                if (itemLevels[i] == 0 && isItemUnlocked(i) && getItemMaxLevel(i) > 0) hasUnlockedNewItem = true;
            if (hasUnlockedNewItem) { ++buildLimitBonus; pool = collectCandidates(); }
        }
        if (pool.empty()) {
            upgradeChoices[0] = 0; upgradeChoices[1] = 1; upgradeChoices[2] = 2; upgradeChoices[3] = 3;
            upgradeRerollUsed = false;
            return;
        }
        for (int k = 0; k < 4; ++k) {
            if (pool.empty()) {
                upgradeChoices[k] = upgradeChoices[rand() % k];
                continue;
            }
            const int pick = chooseWeightedItem(pool);
            upgradeChoices[k] = pick;
            pool.erase(std::remove(pool.begin(), pool.end(), pick), pool.end());
        }
        hoveredCard = -1;
        upgradeRerollUsed = false;
    }

    void applyUpgrade(int itemId) {
        if (itemId < 0 || itemId >= ITEM_COUNT || !isItemUnlocked(itemId)) return;
        if (itemLevels[itemId] == 0 && countEquippedItems() >= getBuildLimit()) return;
        if (itemLevels[itemId] >= getItemMaxLevel(itemId)) return;
        itemLevels[itemId]++;
        checkSymbiosis();
        if (itemId == 10) {
            // Сердце титана gives a meaningful immediate sustain bump on every rank.
            playerMaxHp += 40.0f;
            restorePlayerHp(40.0f);
        }
    }

    void openTreasureChest(int forcedCount = 0) {
        sound.play(SFX_CHEST, 0.05f);
        chestRewards.clear();
        int count = forcedCount > 0 ? forcedCount : 1 + (rand() % 100 < 45 ? 1 : 0) + (rand() % 100 < 20 ? 1 : 0);
        for (int c = 0; c < count; ++c) {
            std::vector<int> upgradable;
            for (int i = 0; i < ITEM_COUNT; ++i) {
                if (isItemUnlocked(i) && itemLevels[i] > 0 && itemLevels[i] < getItemMaxLevel(i)) upgradable.push_back(i);
            }
            if (upgradable.empty()) {
                const int equipped = countEquippedItems();
                for (int i = 0; i < ITEM_COUNT; ++i)
                    if (isItemUnlocked(i) && itemLevels[i] < getItemMaxLevel(i) && (itemLevels[i] > 0 || equipped < getBuildLimit())) upgradable.push_back(i);
            }
            if (!upgradable.empty()) {
                int pick = chooseWeightedItem(upgradable);
                const int oldLevel = itemLevels[pick];
                applyUpgrade(pick);
                chestRewards.push_back({pick, oldLevel, itemLevels[pick]});
            }
        }
        chestBonusGold = (int)roundf((45 + (rand() % 45)) * getGoldMultiplier());
        awardGold(chestBonusGold);
        writeSave();
        state = STATE_CHEST;
    }

    // One recorded line per case that needs a spoken cue beyond the scheduled briefing.
    void speakMissionFlavour(int mission) {
        const CampaignMissionDef& def = CAMPAIGN_MISSIONS[std::max(0, std::min(CAMPAIGN_MISSION_COUNT - 1, mission))];
        if (def.kind == 3) speakOnly(GMSG_STORY_BRANCH, "Выбор ветки. Один след останется непройденным.");
        else if (mission == 0) speakOnly(GMSG_STORY_NO_NAME, "Свидетель без имени. В реестре такой строки нет.");
        else if (def.kind == 2) speakOnly(GMSG_STORY_COPY_WARNING, "Это копия. Она отвечает лучше оригинала.");
    }

    // Closing a case is what opens the next one; the mask is the campaign's whole memory.
    void recordStoryMissionComplete() {
        if (gameMode != MODE_STORY) return;
        markCampaignMissionDone(selectedStoryMission);
        save.storyChapter = std::max(save.storyChapter, CAMPAIGN_MISSIONS[selectedStoryMission].act);
        if (selectedStoryMission == CAMPAIGN_MISSION_COUNT - 1) save.seenEndingCutscene = 1;
        writeSave();
    }

    void finishTreasureChest() {
        sound.play(SFX_CLICK, 0.02f);
        if (missionCompletePending) {
            missionCompletePending = false;
            recordRunEnd();
            recordStoryMissionComplete();
            speakOnly(GMSG_STORY_CASE_CLOSED, "Дело закрыто. Следующее откроется само.");
            sound.musicMode = 4;
            state = STATE_MISSION_COMPLETE;
        } else state = STATE_PLAYING;
    }

    float getOverkillShardMinimumExcess(int rank) const {
        static const float threshold[4] = {0.0f, 10.0f, 8.0f, 6.0f};
        return threshold[std::max(0, std::min(3, rank))];
    }
    float getOverkillShardRatio(int rank) const {
        static const float ratio[4] = {0.0f, 0.16f, 0.20f, 0.24f};
        return ratio[std::max(0, std::min(3, rank))];
    }
    float getOverkillShardDamageCap(int rank) const {
        static const float cap[4] = {0.0f, 12.0f, 17.0f, 22.0f};
        return cap[std::max(0, std::min(3, rank))];
    }
    float getOverkillShardRadius(int rank) const {
        static const float radius[4] = {0.0f, 92.0f, 108.0f, 124.0f};
        return radius[std::max(0, std::min(3, rank))];
    }
    int getOverkillShardTargetCount(int rank) const {
        static const int count[4] = {0, 2, 3, 3};
        return count[std::max(0, std::min(3, rank))];
    }
    float getOverkillShardCooldown(int rank) const {
        static const float cooldown[4] = {0.0f, 1.25f, 1.05f, 0.85f};
        return cooldown[std::max(0, std::min(3, rank))];
    }

    float getEmberDuration(int rank) const {
        static const float duration[4] = {0.0f, 2.46f, 3.06f, 3.66f};
        return duration[std::max(0, std::min(3, rank))];
    }
    float getEmberTickDamage(int rank) const {
        static const float damage[4] = {0.0f, 3.0f, 5.0f, 7.0f};
        return damage[std::max(0, std::min(3, rank))];
    }
    int getEmberTickCount(int rank) const {
        static const int count[4] = {0, 4, 5, 6};
        return count[std::max(0, std::min(3, rank))];
    }
    float getEmberApplyCooldown(int rank) const {
        static const float cooldown[4] = {0.0f, 0.32f, 0.26f, 0.20f};
        return cooldown[std::max(0, std::min(3, rank))];
    }
    float getExecutionThresholdPercent(int rank) const {
        static const float threshold[4] = {0.0f, 8.0f, 12.0f, 16.0f};
        return threshold[std::max(0, std::min(3, rank))];
    }
    bool isExecutionAttack(int attackId) const { return isActiveWeaponAttack(attackId); }
    int getProjectileParryDamage(int rank) const {
        static const int damage[4] = {0, 8, 12, 16};
        return damage[std::max(0, std::min(3, rank))];
    }
    int getResonatorSuperChargeBonus(int rank) const {
        static const int bonus[4] = {0, 8, 12, 16};
        return bonus[std::max(0, std::min(3, rank))];
    }
    float getRetaliationBonusPercent(int rank) const {
        static const float bonus[4] = {0.0f, 0.35f, 0.50f, 0.65f};
        return bonus[std::max(0, std::min(3, rank))];
    }
    void registerRetaliationCharge(float healthDamage) {
        if (healthDamage <= 0.0f || itemLevels[38] <= 0 || retaliationChargeTimer > 0.0f) return;
        retaliationChargeTimer = 4.5f;
        const int rank = std::max(1, std::min(3, itemLevels[38]));
        char text[64];
        snprintf(text, sizeof(text), "ОТВЕТ ГОТОВ: +%.0f%%", getRetaliationBonusPercent(rank) * 100.0f);
        addFloatingText(playerX, playerY - 48.0f, text, 0xFFFFA36F, true);
        if (options.hitEffectsEnabled) {
            preserveParticleRandomBudget(8);
            spawnParticles(playerX, playerY, 8, 0xFFFF8058, 90.0f, 0, true);
        }
        sound.play(SFX_DASH, 0.035f);
    }
    float getProjectileParryCooldown(int rank) const {
        static const float cooldown[4] = {0.0f, 2.2f, 1.8f, 1.4f};
        return cooldown[std::max(0, std::min(3, rank))];
    }
    bool tryReflectEnemyProjectile(Projectile& p) {
        const int rank = std::max(0, std::min(3, itemLevels[36]));
        if (rank <= 0 || projectileParryCooldown > 0.0f || !p.isEnemy || p.isBossProjectile || p.projectileKind == 4) return false;
        projectileParryCooldown = getProjectileParryCooldown(rank);
        size_t nearest = monsters.size();
        float nearestD2 = 620.0f * 620.0f;
        for (size_t i = 0; i < monsters.size(); ++i) {
            const Monster& target = monsters[i];
            if (target.hp <= 0.0f || target.isBoss || target.isResonator) continue;
            const float dx = target.x - playerX, dy = target.y - playerY;
            const float d2 = dx * dx + dy * dy;
            if (d2 < nearestD2) { nearestD2 = d2; nearest = i; }
        }
        if (nearest == monsters.size()) {
            p.life = 0.0f; // A successful parry still blocks the hit if no ordinary target is in range.
            addFloatingText(playerX, playerY - 34.0f, "СНАРЯД ПОГЛОЩЁН", 0xFF9CEBFF, false);
        } else {
            const Monster& target = monsters[nearest];
            float dx = target.x - playerX, dy = target.y - playerY;
            const float dist = std::max(1.0f, sqrtf(dx * dx + dy * dy));
            const float speed = std::max(320.0f, sqrtf(p.vx * p.vx + p.vy * p.vy));
            p.x = playerX; p.y = playerY;
            p.vx = dx / dist * speed; p.vy = dy / dist * speed;
            p.weaponId = REFLECTED_PROJECTILE_ATTACK_ID;
            p.damage = (float)getProjectileParryDamage(rank);
            p.radius = std::max(5.0f, std::min(8.0f, p.radius));
            p.life = dist / speed + 0.45f; p.pierce = 1; p.isEnemy = false; p.isCrit = false;
            p.isBossProjectile = false; p.projectileKind = 0; p.hitIds.clear(); p.trailTimer = 0.0f;
            addFloatingText(playerX, playerY - 34.0f, "ОБРАТНЫЙ ТОК", 0xFFB5F4FF, false);
        }
        if (options.hitEffectsEnabled) {
            preserveParticleRandomBudget(10);
            spawnParticles(playerX, playerY, 10, 0xFF72DFFF, 82.0f, 3, true);
            addShockwave(playerX, playerY, 56.0f, 0xFF72DFFF, 0.28f);
        }
        sound.play(SFX_LIGHTNING, 0.055f);
        return true;
    }
    float getFrostShatterDamage(int rank) const {
        static const float damage[4] = {0.0f, 6.0f, 8.0f, 10.0f};
        return damage[std::max(0, std::min(3, rank))];
    }
    float getFrostShatterRadius(int rank) const {
        static const float radius[4] = {0.0f, 76.0f, 92.0f, 108.0f};
        return radius[std::max(0, std::min(3, rank))];
    }
    int getFrostShatterTargetCount(int rank) const {
        static const int count[4] = {0, 2, 3, 4};
        return count[std::max(0, std::min(3, rank))];
    }
    float getFrostShatterCooldown(int rank) const {
        static const float cooldown[4] = {0.0f, 0.80f, 0.68f, 0.56f};
        return cooldown[std::max(0, std::min(3, rank))];
    }
    bool triggerFrostShatter(size_t sourceIndex) {
        const int rank = std::max(0, std::min(3, itemLevels[35]));
        if (rank <= 0 || frostShatterCooldown > 0.0f || sourceIndex >= monsters.size()) return false;
        const Monster& source = monsters[sourceIndex];
        if (source.hp <= 0.0f || source.isBoss || source.isResonator) return false;
        const float x = source.x, y = source.y;
        const float radius = getFrostShatterRadius(rank), radius2 = radius * radius;
        const int maxTargets = getFrostShatterTargetCount(rank);
        size_t selected[4] = {};
        int selectedCount = 0;
        for (int pick = 0; pick < maxTargets; ++pick) {
            size_t best = monsters.size();
            float bestDist2 = radius2;
            for (size_t i = 0; i < monsters.size(); ++i) {
                const Monster& target = monsters[i];
                if (i == sourceIndex || target.hp <= 0.0f || target.isBoss || target.isResonator) continue;
                bool used = false;
                for (int k = 0; k < selectedCount; ++k) if (selected[k] == i) { used = true; break; }
                if (used) continue;
                const float dx = target.x - x, dy = target.y - y;
                const float d2 = dx * dx + dy * dy;
                if (d2 <= bestDist2) { bestDist2 = d2; best = i; }
            }
            if (best == monsters.size()) break;
            selected[selectedCount++] = best;
        }
        if (selectedCount == 0) return false; // Do not spend the global gate when there is nothing to hit.
        frostShatterCooldown = getFrostShatterCooldown(rank);
        if (options.hitEffectsEnabled) {
            preserveParticleRandomBudget(10);
            spawnParticles(x, y, 10, 0xFF7DEBFF, 88.0f, 3, true);
            addShockwave(x, y, 48.0f + radius * 0.32f, 0xFF72DFFF, 0.32f);
        }
        addFloatingText(x, y - 34.0f, "ЛЕДЯНОЙ ОТКОЛ", 0xFFB8F5FF, false);
        sound.play(SFX_FREEZE, 0.055f);
        const float baseDamage = getFrostShatterDamage(rank);
        for (int k = 0; k < selectedCount; ++k) {
            const size_t targetIndex = selected[k];
            if (targetIndex >= monsters.size() || monsters[targetIndex].hp <= 0.0f) continue;
            const Monster& target = monsters[targetIndex];
            const float dx = target.x - x, dy = target.y - y;
            const float dist = std::max(1.0f, sqrtf(dx * dx + dy * dy));
            const float typeScale = target.isElite ? getEliteDamageMultiplier() : 1.0f;
            const float scale = std::max(0.01f, getDamageMultiplier() * balance.playerDamageMultiplier * typeScale);
            damageMonster(targetIndex, baseDamage / scale, dx / dist * 2.0f, dy / dist * 2.0f, FROST_SHATTER_ATTACK_ID);
        }
        return true;
    }
    bool applyEmberOnCriticalHit(size_t targetIndex) {
        const int rank = std::max(0, std::min(3, itemLevels[34]));
        if (rank <= 0 || emberGlobalCooldown > 0.0f || targetIndex >= monsters.size()) return false;
        Monster& target = monsters[targetIndex];
        if (target.hp <= 0.0f || target.isBoss || target.isResonator || target.emberTicksRemaining > 0) return false;
        target.emberTicksRemaining = getEmberTickCount(rank);
        target.emberTimer = getEmberDuration(rank);
        target.emberPulseTimer = 0.45f; // four/six ticks fit safely before the status expires.
        target.emberDamagePerTick = getEmberTickDamage(rank);
        emberGlobalCooldown = getEmberApplyCooldown(rank);
        addFloatingText(target.x, target.y - target.radius - 12.0f, "УГОЛЬНЫЙ ОЖОГ", 0xFFFFA85A, false);
        if (options.hitEffectsEnabled) {
            preserveParticleRandomBudget(6);
            spawnParticles(target.x, target.y, 6, 0xFFFF9347, 70.0f, 2, true);
        }
        sound.play(SFX_GEM, 0.035f);
        return true;
    }
    void updateEmberStatus(size_t targetIndex, float dt) {
        if (targetIndex >= monsters.size()) return;
        Monster& target = monsters[targetIndex];
        if (target.emberTicksRemaining <= 0 || target.hp <= 0.0f) {
            target.emberTicksRemaining = 0; target.emberTimer = target.emberPulseTimer = target.emberDamagePerTick = 0.0f;
            return;
        }
        const float elapsed = std::max(0.0f, dt);
        target.emberTimer = std::max(0.0f, target.emberTimer - elapsed);
        target.emberPulseTimer -= elapsed;
        int safety = 0;
        while (target.emberPulseTimer <= 0.0f && target.emberTicksRemaining > 0 && target.hp > 0.0f && safety++ < 6) {
            target.emberPulseTimer += 0.60f;
            const float typeScale = target.isElite ? getEliteDamageMultiplier() : 1.0f;
            const float scale = std::max(0.01f, getDamageMultiplier() * balance.playerDamageMultiplier * typeScale);
            damageMonster(targetIndex, target.emberDamagePerTick / scale, 0.0f, 0.0f, EMBER_TICK_ATTACK_ID);
            --target.emberTicksRemaining;
        }
        if (target.emberTicksRemaining <= 0 || target.hp <= 0.0f) {
            target.emberTicksRemaining = 0; target.emberTimer = target.emberPulseTimer = target.emberDamagePerTick = 0.0f;
        }
    }

    bool isActiveWeaponAttack(int attackId) const {
        return (attackId >= 0 && attackId <= 6) || (attackId >= 24 && attackId <= 27);
    }
    int getCrossfireDistinctWeaponThreshold(int rank) const {
        static const int count[4] = {0, 4, 3, 3};
        return count[std::max(0, std::min(3, rank))];
    }
    float getCrossfireWindowSeconds(int rank) const {
        static const float seconds[4] = {0.0f, 2.2f, 2.5f, 2.8f};
        return seconds[std::max(0, std::min(3, rank))];
    }
    float getCrossfireProcCooldown(int rank) const {
        static const float seconds[4] = {0.0f, 2.8f, 2.4f, 2.0f};
        return seconds[std::max(0, std::min(3, rank))];
    }
    float getCrossfireGlobalCooldown(int rank) const {
        static const float seconds[4] = {0.0f, 0.90f, 0.75f, 0.60f};
        return seconds[std::max(0, std::min(3, rank))];
    }
    float getCrossfireRadius(int rank) const {
        static const float radius[4] = {0.0f, 104.0f, 122.0f, 140.0f};
        return radius[std::max(0, std::min(3, rank))];
    }
    float getCrossfireDamage(int rank) const {
        static const float damage[4] = {0.0f, 16.0f, 20.0f, 24.0f};
        return damage[std::max(0, std::min(3, rank))];
    }
    int getCrossfireTargetCount(int rank) const {
        static const int count[4] = {0, 2, 3, 4};
        return count[std::max(0, std::min(3, rank))];
    }
    int getCrossfireMaskCount(uint16_t mask) const {
        int count = 0;
        while (mask) { count += mask & 1u; mask >>= 1; }
        return count;
    }
    bool triggerCrossfirePulse(size_t sourceIndex, int rank) {
        if (sourceIndex >= monsters.size()) return false;
        Monster& source = monsters[sourceIndex];
        if (source.hp <= 0.0f || source.isBoss || source.isResonator) return false;
        const float radius = getCrossfireRadius(rank);
        const float radius2 = radius * radius;
        const int maxTargets = getCrossfireTargetCount(rank);
        size_t selected[4] = {};
        int selectedCount = 0;
        for (int pick = 0; pick < maxTargets; ++pick) {
            size_t best = monsters.size();
            float bestDist2 = radius2;
            for (size_t i = 0; i < monsters.size(); ++i) {
                const Monster& candidate = monsters[i];
                if (i == sourceIndex || candidate.hp <= 0.0f || candidate.isBoss || candidate.isResonator) continue;
                bool alreadySelected = false;
                for (int k = 0; k < selectedCount; ++k) if (selected[k] == i) { alreadySelected = true; break; }
                if (alreadySelected) continue;
                const float dx = candidate.x - source.x, dy = candidate.y - source.y;
                const float d2 = dx * dx + dy * dy;
                if (d2 <= bestDist2) { bestDist2 = d2; best = i; }
            }
            if (best == monsters.size()) break;
            selected[selectedCount++] = best;
        }
        if (selectedCount == 0) return false;
        const float originX = source.x, originY = source.y;
        source.crossfireWeaponMask = 0;
        source.crossfireWindowTimer = 0.0f;
        source.crossfireCooldown = getCrossfireProcCooldown(rank);
        crossfireGlobalCooldown = getCrossfireGlobalCooldown(rank);
        if (options.hitEffectsEnabled) {
            preserveParticleRandomBudget(12);
            spawnParticles(originX, originY, 12, 0xFF68E6D8, 110.0f, 3, true);
            addShockwave(originX, originY, 64.0f + radius * 0.36f, 0xFF68E6D8, 0.34f);
        }
        addFloatingText(originX, originY - 42.0f, "ПЕРЕКРЁСТНЫЙ ОГОНЬ!", 0xFF9CFFF0, true);
        sound.play(SFX_LIGHTNING, 0.055f);
        const float baseDamage = getCrossfireDamage(rank);
        for (int k = 0; k < selectedCount; ++k) {
            const size_t targetIndex = selected[k];
            if (targetIndex >= monsters.size() || monsters[targetIndex].hp <= 0.0f) continue;
            Monster& target = monsters[targetIndex];
            const float dx = target.x - originX, dy = target.y - originY;
            const float dist = std::max(1.0f, sqrtf(dx * dx + dy * dy));
            const float typeScale = target.isElite ? getEliteDamageMultiplier() : 1.0f;
            const float scale = std::max(0.01f, getDamageMultiplier() * balance.playerDamageMultiplier * typeScale);
            damageMonster(targetIndex, baseDamage / scale, dx / dist * 3.0f, dy / dist * 3.0f, CROSSFIRE_ATTACK_ID);
        }
        return true;
    }
    void registerCrossfireWeaponHit(size_t sourceIndex, int attackId) {
        const int rank = std::max(0, std::min(3, itemLevels[33]));
        if (rank <= 0 || !isActiveWeaponAttack(attackId) || crossfireGlobalCooldown > 0.0f || sourceIndex >= monsters.size()) return;
        Monster& source = monsters[sourceIndex];
        if (source.hp <= 0.0f || source.isBoss || source.isResonator || source.crossfireCooldown > 0.0f) return;
        if (source.crossfireWindowTimer <= 0.0f) {
            source.crossfireWeaponMask = 0;
            source.crossfireWindowTimer = getCrossfireWindowSeconds(rank);
        }
        const int bit = attackId <= 6 ? attackId : attackId - 17;
        const uint16_t weaponBit = (uint16_t)(1u << bit);
        if (source.crossfireWeaponMask & weaponBit) {
            if (getCrossfireMaskCount(source.crossfireWeaponMask) >= getCrossfireDistinctWeaponThreshold(rank))
                triggerCrossfirePulse(sourceIndex, rank); // Retry a full charge if no eligible survivor was nearby before.
            return; // Repeated hits from one weapon never advance the combo.
        }
        source.crossfireWeaponMask |= weaponBit;
        if (getCrossfireMaskCount(source.crossfireWeaponMask) < getCrossfireDistinctWeaponThreshold(rank)) return;
        // If no nearby survivor can receive the pulse, preserve the charge until the fixed window expires.
        triggerCrossfirePulse(sourceIndex, rank);
    }
    void updateCrossfireTimers(Monster& m, float dt) {
        const float elapsed = std::max(0.0f, dt);
        m.crossfireCooldown = std::max(0.0f, m.crossfireCooldown - elapsed);
        if (m.crossfireWindowTimer > 0.0f) {
            m.crossfireWindowTimer = std::max(0.0f, m.crossfireWindowTimer - elapsed);
            if (m.crossfireWindowTimer <= 0.0f) m.crossfireWeaponMask = 0;
        }
    }

    int getAshRhythmSparkThreshold(int rank) const {
        static const int thresholds[4] = {0, 18, 14, 10};
        return thresholds[std::max(0, std::min(3, rank))];
    }
    float getAshRhythmCooldownAdvance(int rank) const {
        static const float advance[4] = {0.0f, 0.15f, 0.22f, 0.30f};
        return advance[std::max(0, std::min(3, rank))];
    }
    void collectAshRhythmSpark() {
        const int rank = std::max(0, std::min(3, itemLevels[32]));
        if (rank <= 0) return;
        const int threshold = getAshRhythmSparkThreshold(rank);
        if (++ashRhythmSparkCount < threshold) return;
        ashRhythmSparkCount = 0;
        const float advance = getAshRhythmCooldownAdvance(rank);
        int accelerated = 0;
        for (int weapon = 0; weapon < 7; ++weapon) {
            if (itemLevels[weapon] <= 0) continue;
            weaponTimers[weapon] = std::max(0.0f, weaponTimers[weapon] - advance);
            ++accelerated;
        }
        for (int weapon = 24; weapon <= 27; ++weapon) {
            if (itemLevels[weapon] <= 0) continue;
            tierWeaponTimers[weapon - 24] = std::max(0.0f, tierWeaponTimers[weapon - 24] - advance);
            ++accelerated;
        }
        ashRhythmFlashTimer = 0.45f;
        addFloatingText(playerX, playerY - 58.0f, "РИТМ ПЕПЛА • АТАКИ УСКОРЕНЫ", 0xFF8FFFE0, true);
        if (options.hitEffectsEnabled) {
            preserveParticleRandomBudget(10);
            spawnParticles(playerX, playerY, 10, 0xFF67E7D4, 72.0f, 3, true);
            addShockwave(playerX, playerY, 54.0f + accelerated * 3.0f, 0xFF67E7D4, 0.30f);
        }
        sound.play(SFX_GEM, 0.08f);
    }

    void triggerOverkillShard(size_t sourceIndex, float x, float y, float excessDamage) {
        const int rank = std::max(0, std::min(3, itemLevels[31]));
        if (rank <= 0 || overkillShardCooldown > 0.0f || excessDamage < getOverkillShardMinimumExcess(rank)) return;
        const float radius = getOverkillShardRadius(rank);
        const float radius2 = radius * radius;
        const int maxTargets = getOverkillShardTargetCount(rank);
        size_t selected[3] = {};
        int selectedCount = 0;
        // Choose the nearest eligible survivors first; the small fixed cap avoids per-kill heap work.
        for (int pick = 0; pick < maxTargets; ++pick) {
            size_t best = monsters.size();
            float bestDist2 = radius2;
            for (size_t i = 0; i < monsters.size(); ++i) {
                if (i == sourceIndex || monsters[i].hp <= 0.0f || monsters[i].isBoss || monsters[i].isResonator) continue;
                bool alreadySelected = false;
                for (int k = 0; k < selectedCount; ++k) if (selected[k] == i) { alreadySelected = true; break; }
                if (alreadySelected) continue;
                const float dx = monsters[i].x - x, dy = monsters[i].y - y;
                const float d2 = dx * dx + dy * dy;
                if (d2 <= bestDist2) { bestDist2 = d2; best = i; }
            }
            if (best == monsters.size()) break;
            selected[selectedCount++] = best;
        }
        if (selectedCount == 0) return; // no target: preserve the next available proc
        overkillShardCooldown = getOverkillShardCooldown(rank);
        const float pulseDamage = std::min(getOverkillShardDamageCap(rank), excessDamage * getOverkillShardRatio(rank));
        if (pulseDamage <= 0.0f) return;
        if (options.hitEffectsEnabled) {
            preserveParticleRandomBudget(12);
            spawnParticles(x, y, 12, 0xFFFF9B58, 112.0f, 3, true);
            addShockwave(x, y, 54.0f + radius * 0.35f, 0xFFFFA64D, 0.34f);
        }
        addFloatingText(x, y - 38.0f, "ОСКОЛКИ ПОСЛЕДНЕГО УДАРА", 0xFFFFC176, true);
        sound.play(SFX_DASH, 0.055f);
        for (int k = 0; k < selectedCount; ++k) {
            const size_t targetIndex = selected[k];
            if (targetIndex >= monsters.size() || monsters[targetIndex].hp <= 0.0f) continue;
            Monster& target = monsters[targetIndex];
            const float dx = target.x - x, dy = target.y - y;
            const float dist = std::max(1.0f, sqrtf(dx * dx + dy * dy));
            const float typeScale = target.isElite ? getEliteDamageMultiplier() : 1.0f;
            const float scaling = std::max(0.01f, getDamageMultiplier() * balance.playerDamageMultiplier * typeScale);
            damageMonster(targetIndex, pulseDamage / scaling, dx / dist * 4.0f, dy / dist * 4.0f, OVERKILL_SHARD_ATTACK_ID);
        }
    }

    void damageMonster(size_t idx, float rawDmg, float knockX, float knockY, int attackId = -1) {
        if (idx >= monsters.size()) return;
        Monster& m = monsters[idx];
        if (m.hp <= 0.0f) return;
        if (m.isResonator) {
            m.hp -= rawDmg * getDamageMultiplier();
            m.hitFlashTimer = 0.12f;
            spawnHitEffect(m.x, m.y, 0xFFBD69FF, true);
            if (m.hp <= 0.0f) {
                const int burstCount = getResonatorBurstCount();
                pendingResonatorBurst += burstCount;
                char burstLabel[64];
                snprintf(burstLabel, sizeof(burstLabel), "РЕЗОНАТОР РАЗРУШЕН! +%d %s", burstCount, enemyForm(burstCount));
                addFloatingText(m.x, m.y - 50.0f, burstLabel, 0xFFFF9868, true);
                spawnParticles(m.x, m.y, 60, 0xFFBD69FF, 225.0f, 5, true);
                addShockwave(m.x, m.y, 180.0f, 0xFFBD69FF, 0.75f);
                const int witnessRank = std::max(0, std::min(3, itemLevels[39]));
                if (witnessRank > 0) {
                    const int charge = getResonatorSuperChargeBonus(witnessRank);
                    superCharge = std::min(100.0f, superCharge + charge * getSuperGainMultiplier());
                    char witnessText[64];
                    snprintf(witnessText, sizeof(witnessText), "РУНА СВИДЕТЕЛЯ: +%d%% СУПЕРА", charge);
                    addFloatingText(m.x, m.y - 72.0f, witnessText, 0xFFE3BEFF, true);
                    addShockwave(m.x, m.y, 112.0f, 0xFFCE8CFF, 0.52f);
                    sound.play(SFX_LEVELUP, 0.06f);
                }
            }
            return;
        }
        if (m.isBoss && m.phaseInvulnTimer > 0.0f) return;
        const float roll = (rand() % 10000) / 10000.0f;
        const bool isPassiveProc = attackId == OVERKILL_SHARD_ATTACK_ID || attackId == CROSSFIRE_ATTACK_ID || attackId == EMBER_TICK_ATTACK_ID || attackId == FROST_SHATTER_ATTACK_ID || attackId == REFLECTED_PROJECTILE_ATTACK_ID;
        const int criticalTier = isPassiveProc ? 0 : getCriticalTierForRoll(roll);
        const bool isSuperCrit = criticalTier == 2;
        const bool isCrit = criticalTier >= 1;
        float heroDx = playerX - m.x, heroDy = playerY - m.y;
        float heroLen = std::max(1.0f, sqrtf(heroDx * heroDx + heroDy * heroDy));
        const float frontDot = (heroDx / heroLen) * m.shieldFacingX + (heroDy / heroLen) * m.shieldFacingY;
        if (m.shieldHits > 0 && frontDot > 0.35f) {
            if (isCrit) {
                m.shieldHits = 0; m.isShieldBearer = false;
                spawnParticles(m.x, m.y, 12, 0xFF71DFFF, 95.0f, 3, true);
                addFloatingText(m.x, m.y - m.radius - 12.0f, "КРИТ ПРОБИВАЕТ ЩИТ!", 0xFFFFD848, true);
            } else {
                --m.shieldHits;
                m.hitFlashTimer = 0.10f;
                spawnParticles(m.x, m.y, 5, 0xFF71DFFF, 55.0f, 2, true);
                addFloatingText(m.x, m.y - m.radius, "БЛОК ЩИТОМ", 0xFF79DFFF, false);
                if (m.shieldHits <= 0) { m.isShieldBearer = false; addFloatingText(m.x, m.y - m.radius - 15.0f, "ЩИТ РАЗБИТ", 0xFFFFD878, true); }
                return;
            }
        }
        const int exposureRank = std::max(0, std::min(3, itemLevels[29]));
        const bool targetExposed = !isPassiveProc && exposureRank > 0 && m.exposeTimer > 0.0f;
        const float exposureMultiplier = targetExposed ? 1.0f + getExposureDamageBonus(exposureRank) : 1.0f;
        const int retaliationRank = std::max(0, std::min(3, itemLevels[38]));
        const bool retaliationHit = retaliationRank > 0 && retaliationChargeTimer > 0.0f && isActiveWeaponAttack(attackId);
        const float retaliationMultiplier = retaliationHit ? 1.0f + getRetaliationBonusPercent(retaliationRank) : 1.0f;
        const float excessDamage = std::max(0.0f, rawDmg * getDamageMultiplier() * balance.playerDamageMultiplier *
                    (m.isBoss ? getBossDamageMultiplier() : 1.0f) *
                    (m.isElite ? getEliteDamageMultiplier() : 1.0f) *
                    getCriticalDamageMultiplier(criticalTier) * exposureMultiplier * retaliationMultiplier - m.hp);
        float dmg = rawDmg * getDamageMultiplier() * balance.playerDamageMultiplier *
                    (m.isBoss ? getBossDamageMultiplier() : 1.0f) *
                    (m.isElite ? getEliteDamageMultiplier() : 1.0f) *
                    getCriticalDamageMultiplier(criticalTier) * exposureMultiplier * retaliationMultiplier;
        const int executionRank = std::max(0, std::min(3, itemLevels[37]));
        const bool executionHit = executionRank > 0 && isExecutionAttack(attackId) && !m.isBoss && !m.isResonator &&
            m.maxHp > 0.0f && m.hp / m.maxHp <= getExecutionThresholdPercent(executionRank) * 0.01f && dmg < m.hp;
        const bool smartOneTap = m.isSmart && dmg >= m.hp && !executionHit;
        if (executionHit) dmg = m.hp;
        const float appliedDamage = std::min(dmg, m.hp);
        if (retaliationHit) {
            retaliationChargeTimer = 0.0f; // One charge is consumed only by a valid active-weapon hit.
            addFloatingText(m.x, m.y - m.radius - 28.0f, "ВОЗМЕЗДИЕ!", 0xFFFFC08A, true);
            if (options.hitEffectsEnabled) {
                preserveParticleRandomBudget(8);
                spawnParticles(m.x, m.y, 8, 0xFFFF8B60, 90.0f, 0, true);
            }
        }
        m.hp -= dmg;
        if (executionHit) {
            addFloatingText(m.x, m.y - m.radius - 13.0f, "ЗНАК ПАЛАЧА", 0xFFFF7C64, true);
            if (options.hitEffectsEnabled) {
                preserveParticleRandomBudget(8);
                spawnParticles(m.x, m.y, 8, 0xFFFF705E, 78.0f, 3, true);
            }
            sound.play(SFX_DASH, 0.035f);
        }
        if (m.isBoss) handleBossPhaseTransition(m);
        if (m.hp > 0.0f && dmg > 0.0f && !isPassiveProc && exposureRank > 0 && m.exposeTimer <= 0.0f) {
            if (m.exposeBuildTimer <= 0.0f) m.exposeHitStacks = 0;
            ++m.exposeHitStacks;
            m.exposeBuildTimer = EXPOSURE_BUILD_WINDOW;
            if (m.exposeHitStacks >= getExposureHitsRequired(exposureRank)) {
                m.exposeHitStacks = 0;
                m.exposeBuildTimer = 0.0f;
                m.exposeTimer = getExposureDuration(exposureRank);
                addFloatingText(m.x, m.y - m.radius - 18.0f, "СЛАБОЕ МЕСТО РАСКРЫТО!", 0xFFD6A5FF, true);
                if (options.hitEffectsEnabled) {
                    preserveParticleRandomBudget(12);
                    spawnParticles(m.x, m.y, 12, 0xFFBD83FF, 105.0f, 3, true);
                    addShockwave(m.x, m.y, 58.0f + exposureRank * 8.0f, 0xFFBD83FF, 0.34f);
                }
            }
        }
        m.hitFlashTimer = 0.11f;
        const uint32_t hitColor = executionHit ? 0xFFFF705E : (attackId == EMBER_TICK_ATTACK_ID ? 0xFFFF9347 : (attackId == CROSSFIRE_ATTACK_ID ? 0xFF68E6D8 : (isSuperCrit ? 0xFF8A65FF : (isCrit ? 0xFFFFD848 : (m.isBoss ? 0xFFFF8648 : (m.isSmart ? 0xFF68E8FF : 0xFFFF5870))))));
        spawnHitEffect(m.x, m.y, hitColor, isCrit, attackId, m.radius);
        totalDamageDealt += appliedDamage;
        dpsWindowDamage += appliedDamage;

        float kbScale = m.isBoss ? 0.15f : (m.isElite ? 0.45f : 1.0f);
        m.x += knockX * kbScale;
        m.y += knockY * kbScale;

        char buf[32];
        snprintf(buf, sizeof(buf), isSuperCrit ? "%d!!" : (isCrit ? "%d!" : "%d"), (int)roundf(dmg));
        addFloatingText(m.x, m.y - m.radius, buf, isSuperCrit ? 0xFFB69BFF : (isCrit ? 0xFFFFD830 : 0xFFFFFFFF), isCrit);
        if (isSuperCrit) addFloatingText(m.x, m.y - m.radius - 18.0f, "СУПЕРКРИТ ×4 УРОН!", 0xFFA88AFF, true);
        if (smartOneTap) {
            const int tactic = std::max(0, std::min(2, m.smartTactic));
            smartOneTapCount[tactic]++;
            smartSpawnChance[tactic] = std::max(0.002f, smartSpawnChance[tactic] / 1.1f);
            smartStrengthMultiplier[tactic] = std::min(1.85f, smartStrengthMultiplier[tactic] * 1.1f);
        }

        if (m.hp > 0.0f && isCrit && isActiveWeaponAttack(attackId)) applyEmberOnCriticalHit(idx);
        if (m.hp > 0.0f && !isPassiveProc) registerCrossfireWeaponHit(idx, attackId);
        if (m.hp <= 0.0f) {
            // Monster death!
            if (m.isMatryoshka) {
                pendingMatryoshkaSplits.push_back({m.x, m.y});
                addFloatingText(m.x, m.y - 32.0f, "МАТРЁШКА РАСКРЫЛАСЬ! +2 ВРАГА", 0xFFFFD77A, true);
                spawnParticles(m.x, m.y, 22, 0xFFFFC65E, 145.0f, 4, true);
                addShockwave(m.x, m.y, 64.0f, 0xFFFFD77A, 0.42f);
            }
            kills++;
            if (m.fromHunt) onHuntPackKill(m.x, m.y);
            if (itemLevels[21] > 0) {
                if (++dashRuneKillCounter >= getDashRefreshKillsRequired()) {
                    dashRuneKillCounter = 0;
                    dashCooldown = 0.0f;
                    addFloatingText(playerX, playerY - 42.0f, "РУНА ВЕРНУЛА РЫВОК", 0xFF8FFFE0, true);
                    spawnParticles(playerX, playerY, 12, 0xFF67E7D4, 100.0f, 1, true);
                }
            }
            if (itemLevels[22] > 0 && (m.isElite || m.isBoss)) {
                const float echoHeal = getEchoHealAmount();
                restorePlayerHp(echoHeal);
                if (echoHeal > 0.0f) addFloatingText(playerX, playerY - 56.0f, "+ЭХО HP", 0xFF88F5C8, false);
            }
            if (!superResolvingUltimate)
                superCharge = std::min(100.0f, superCharge + (m.isBoss ? 12.0f : (m.isElite ? 2.0f : 0.50f)) * getSuperGainMultiplier());
            if (m.isBoss) spawnParticles(m.x, m.y, 40, 0xFFFF9020, 190.0f, 4);
            else {
                // Every family dies in its own material: bone shards, slime, sparks, ash.
                const DeathBurst burst = monsterDeathBurst(m.type, m.isElite);
                spawnParticles(m.x, m.y, burst.count, burst.color, m.isElite ? 150.0f : 108.0f, 4, true);
                if (burst.count > 9) spawnParticles(m.x, m.y, 4, burst.accent, 62.0f, 3, false);
            }
            if (m.isSmart) addFloatingText(m.x, m.y - 28.0f, "РАЗУМНЫЙ ПОВЕРЖЕН", 0xFF7CE8FF, true);

            // Blood Aura's soul-steal scales into a real sustain build at high ranks.
            if (itemLevels[4] > 0 && (rand() % 100) < std::min(38, 8 + itemLevels[4] * 4)) {
                restorePlayerHp(1.0f + itemLevels[4] * 0.55f);
            }

            if (m.isBoss) {
                bossWarningTimer = 0.0f;
                save.bossesDefeated++;
                const int previousBuildTier = getBuildTier();
                runBossKills++;
                const int currentBuildTier = getBuildTier();
                if (currentBuildTier > previousBuildTier) {
                    buildTierUpValue = currentBuildTier;
                    buildTierUpTimer = 3.4f;
                    speakOnly(GMSG_BUILD_TIER_UP, "Тир сборки повышен: тир " + std::to_string(currentBuildTier));
                    sound.play(SFX_TIER_UP, 0.25f);
                    addFloatingText(playerX, playerY - 86.0f, "ТИР СБОРКИ ПОВЫШЕН!", 0xFFFFD878, true);
                    spawnParticles(playerX, playerY, 42, 0xFFFFD05C, 175.0f, 5, true);
                    spawnParticles(playerX, playerY, 20, 0xFF77E8FF, 125.0f, 3, true);
                    addShockwave(playerX, playerY, 320.0f, 0xFFFFD878, 0.82f);
                }
                int bossGold = (int)roundf((180 + m.bossType * 120) * getGoldMultiplier());
                awardGold(bossGold);
                if (gameMode == MODE_STORY && !m.isTrioBoss && CAMPAIGN_MISSIONS[selectedStoryMission].kind != 1 && m.bossType < 4) {
                    static const char* sealLines[4] = {
                        "МОРГАР: «Я ОХРАНЯЛ ПРИКАЗ, А НЕ ЛЮДЕЙ». ПЕЧАТЬ I — ДОЛГ.",
                        "МАЛАКОР: «ЭТО ГОЛОС СЕСТРЫ. НО ЭТО НЕ ОНА». ПЕЧАТЬ II — ГОЛОС.",
                        "КАРДИНАЛ: «Я СТЁР ИМЕНА ИЗ КНИГ. ЛЮДИ ВСЁ РАВНО ПОМНИЛИ». ПЕЧАТЬ III — ИМЯ.",
                        "АЗАРОТ: «АРХИВ ХРАНИТ КОПИИ, НЕ ЛЮДЕЙ». ПЕЧАТЬ IV — СВИДЕТЕЛЬСТВО."
                    };
                    const uint32_t storyBit = 1u << (m.bossType % 4);
                    const bool firstSeal = (save.storySealMask & storyBit) == 0;
                    save.storySealMask |= storyBit;
                    save.storyEntryMask |= storyBit;
                    save.storyChapter = std::max(save.storyChapter, CAMPAIGN_MISSIONS[selectedStoryMission].act);
                    if (firstSeal) announce(GMSG_SEAL_LORE, sealLines[m.bossType % 4], 7.0f);
                }
                if (gameMode == MODE_STORY && m.isMissionFinalBoss) {
                    missionCompletePending = true;
                    for (Monster& survivor : monsters) if (&survivor != &m && !survivor.isResonator) survivor.hp = 0.0f;
                }
                if (m.isTrioBoss) {
                    ++trioBossesDefeated;
                    if (bossTrioSpawned > 0 && trioBossesDefeated >= bossTrioSpawned &&
                        (save.storySealMask & 0x0Fu) == 0x0Fu) {
                        if (gameMode == MODE_STORY && selectedStoryMission == 5)
                            save.storyEntryMask |= STORY_HEART_MISSION_COMPLETE_BIT;
                        if (!save.seenEndingCutscene) endingCutscenePending = true;
                        else campaignEndPending = true;
                        announce(GMSG_CAMPAIGN_END, "ТРИ ЭХО-ПРИКАЗА РАЗРУШЕНЫ. КАМПАНИЯ ЗАВЕРШЕНА — ПОРТАЛ НЕ НУЖЕН.", 6.0f);
                    }
                }
                writeSave();
                screenShake = 1.05f;
                sound.play(SFX_BOSS_DEATH, 0.01f);
                victoryMusicTimer = 8.0f;
                spawnParticles(m.x, m.y, 130, m.bossType == 3 ? 0xFFFF8A20 : 0xFFFFD348, 285.0f, 5, true);
                spawnParticles(m.x, m.y, 55, 0xFFECE8FF, 190.0f, 3, true);
                addShockwave(m.x, m.y, 180.0f, 0xFFFFD348, 0.65f);
                addShockwave(m.x, m.y, 290.0f, 0xFFFF7138, 1.05f);
                addShockwave(m.x, m.y, 360.0f, 0xFF9B5DFF, 1.30f);
                addFloatingText(m.x, m.y - 58, "БОСС ПОВЕРЖЕН!", 0xFFFFE878, true);
                Pickup chest;
                chest.x = m.isMissionFinalBoss ? playerX : m.x;
                chest.y = m.isMissionFinalBoss ? playerY : m.y;
                chest.type = 6; // Boss Chest
                chest.value = 1;
                chest.bobTime = 0.0f;
                chest.magnetized = false;
                pickups.push_back(chest);
                addFloatingText(m.x, m.y - 30, "СУНДУК БОССА!", 0xFFFFD838, true);
            } else {
                Pickup p;
                p.x = m.x;
                p.y = m.y;
                p.bobTime = (rand() % 100) * 0.05f;
                p.magnetized = false;
                int roll = rand() % 1000;
                const int healingChance = getHealingDropChancePermille();
                if (m.isElite) {
                    p.type = 2; // Purple big XP gem
                    p.value = 18;
                    pickups.push_back(p);
                    Pickup g = p;
                    g.x += 12;
                    g.type = 3;
                    g.value = 12;
                    pickups.push_back(g);
                    if (healingChance > 0 && roll < healingChance) {
                        Pickup heal = p; heal.x -= 12; heal.type = 4; heal.value = 35; pickups.push_back(heal);
                    }
                } else if (healingChance > 0 && roll < healingChance) {
                    p.type = 4; // A real tactical recovery drop, available from any wounded-run kill.
                    p.value = 35;
                    pickups.push_back(p);
                } else if (m.isSmart && roll < 360) {
                    p.type = 5; // Smart mobs are the reliable source of vacuum magnets.
                    p.value = 1;
                    pickups.push_back(p);
                } else if (!options.hardModeEnabled && !m.isSmart && roll < 6) {
                    p.type = 5; // In standard mode, an exceptionally rare magnet may drop from a common mob.
                    p.value = 1;
                    pickups.push_back(p);
                } else if (roll < 95) {
                    p.type = 3; // Gold coin
                    p.value = 3;
                    pickups.push_back(p);
                } else {
                    p.type = (m.type >= 3) ? 1 : 0; // Green or Blue XP gem
                    p.value = (p.type == 1) ? 6 : 3;
                    pickups.push_back(p);
                }
            }
            if (!m.isBoss && attackId != OVERKILL_SHARD_ATTACK_ID && attackId != CROSSFIRE_ATTACK_ID && attackId != EMBER_TICK_ATTACK_ID && attackId != FROST_SHATTER_ATTACK_ID && attackId != REFLECTED_PROJECTILE_ATTACK_ID && excessDamage > 0.0f)
                triggerOverkillShard(idx, m.x, m.y, excessDamage);
        }
    }

    int findNearestMonster(float x, float y, float maxRange) const {
        int best = -1;
        float bestD2 = maxRange * maxRange;
        for (size_t i = 0; i < monsters.size(); ++i) {
            if (monsters[i].hp <= 0.0f) continue;
            float dx = monsters[i].x - x;
            float dy = monsters[i].y - y;
            float d2 = dx * dx + dy * dy;
            if (d2 < bestD2) {
                bestD2 = d2;
                best = (int)i;
            }
        }
        return best;
    }

    void triggerDash() {
        if (dashCooldown > 0.0f || dashTimer > 0.0f) return;
        dashTimer = 0.32f;
        dashEvadeTriggered = false;
        dashCooldown = std::max(0.85f, 2.25f - itemLevels[8] * 0.18f);
        float dx = 0.0f, dy = 0.0f;
        if (keyW || keyUp) dy -= 1.0f;
        if (keyS || keyDown) dy += 1.0f;
        if (keyA || keyLeft) dx -= 1.0f;
        if (keyD || keyRight) dx += 1.0f;
        if (dx == 0.0f && dy == 0.0f) {
            dx = (float)playerFacing;
        } else {
            float len = sqrtf(dx * dx + dy * dy);
            dx /= len; dy /= len;
        }
        dashDirX = dx;
        dashDirY = dy;
        dashHitIds.clear();
        sound.play(SFX_DASH, 0.05f);
        spawnParticles(playerX, playerY, 16, 0xFF50E8FF, 160.0f, 3, true);
    }

    void updateWeapons(float dt) {
        const size_t projectileCountBefore = projectiles.size();
        float cdMult = getCooldownMultiplier();
        float areaMult = getAreaMultiplier();

        // 0: Серебряные клинки (Homing rapid blades)
        if (itemLevels[0] > 0) {
            weaponTimers[0] -= dt;
            if (weaponTimers[0] <= 0.0f) {
                int lvl = itemLevels[0];
                weaponTimers[0] = std::max(0.22f, (0.78f - lvl * 0.05f) * cdMult * getWeaponRankCooldownMultiplier(0));
                int count = 1 + lvl / 2 + (evolvedWeapons[0] ? 2 : 0);
                int target = findNearestMonster(playerX, playerY, 550.0f);
                float baseAngle = (playerFacing > 0) ? 0.0f : 3.14159f;
                if (target >= 0) {
                    baseAngle = atan2f(monsters[target].y - playerY, monsters[target].x - playerX);
                }
                for (int i = 0; i < count; ++i) {
                    float spread = (i - (count - 1) * 0.5f) * 0.16f;
                    float a = baseAngle + spread;
                    Projectile p;
                    p.x = playerX;
                    p.y = playerY;
                    p.vx = cosf(a) * 520.0f;
                    p.vy = sinf(a) * 520.0f;
                    p.weaponId = 0;
                    p.damage = (18.0f + lvl * 7.5f) * getEvoMultiplier(0) * getWeaponRankDamageMultiplier(0);
                    p.radius = 9.0f * areaMult * getWeaponRankAreaMultiplier(0);
                    p.life = 1.35f;
                    p.pierce = 1 + (lvl >= 4 ? 1 : 0) + (lvl >= 7 ? 1 : 0);
                    p.isEnemy = false;
                    p.isCrit = false;
                    projectiles.push_back(p);
                }
                sound.play(SFX_BLADE, 0.14f);
            }
        }

        // 1: Огненный шар (Explosive Fireball)
        if (itemLevels[1] > 0) {
            weaponTimers[1] -= dt;
            if (weaponTimers[1] <= 0.0f) {
                int lvl = itemLevels[1];
                weaponTimers[1] = std::max(0.55f, (1.65f - lvl * 0.10f) * cdMult * getWeaponRankCooldownMultiplier(1));
                int count = 1 + (lvl >= 4 ? 1 : 0) + (lvl >= 7 ? 1 : 0) + (evolvedWeapons[1] ? 1 : 0);
                int target = findNearestMonster(playerX, playerY, 600.0f);
                float baseAngle = (target >= 0)
                    ? atan2f(monsters[target].y - playerY, monsters[target].x - playerX)
                    : ((rand() % 360) * 3.14159f / 180.0f);
                for (int i = 0; i < count; ++i) {
                    float a = baseAngle + (i - (count - 1) * 0.5f) * 0.28f;
                    Projectile p;
                    p.x = playerX;
                    p.y = playerY;
                    p.vx = cosf(a) * 340.0f;
                    p.vy = sinf(a) * 340.0f;
                    p.weaponId = 1;
                    p.damage = (34.0f + lvl * 14.0f) * getEvoMultiplier(1) * getWeaponRankDamageMultiplier(1);
                    p.radius = 14.0f * areaMult * getWeaponRankAreaMultiplier(1);
                    p.life = 1.8f;
                    p.pierce = 1;
                    p.isEnemy = false;
                    p.isCrit = false;
                    projectiles.push_back(p);
                }
                sound.play(SFX_FIREBALL, 0.20f);
            }
        }

        // 2: Рунический вихрь (Orbiting Arcane Blades)
        if (itemLevels[2] > 0) {
            int lvl = itemLevels[2];
            orbitalAngle += dt * (3.2f + lvl * 0.25f);
            weaponTimers[2] -= dt;
            if (weaponTimers[2] <= 0.0f) {
                weaponTimers[2] = 0.22f * cdMult * getWeaponRankCooldownMultiplier(2); // Hit tick interval improves with each rank
                int orbCount = 3 + lvl / 2 + (evolvedWeapons[2] ? 2 : 0);
                float orbitRad = (78.0f + lvl * 6.0f) * areaMult * getWeaponRankAreaMultiplier(2);
                float hitRad = 23.0f * areaMult * getWeaponRankAreaMultiplier(2);
                float dmg = (15.0f + lvl * 6.5f) * getEvoMultiplier(2) * getWeaponRankDamageMultiplier(2);
                for (int c = 0; c < orbCount; ++c) {
                    float a = orbitalAngle + c * (2.0f * 3.14159f / orbCount);
                    float ox = playerX + cosf(a) * orbitRad;
                    float oy = playerY + sinf(a) * orbitRad;
                    for (size_t m = 0; m < monsters.size(); ++m) {
                        if (monsters[m].hp <= 0.0f) continue;
                        float dx = monsters[m].x - ox;
                        float dy = monsters[m].y - oy;
                        float r = monsters[m].radius + hitRad;
                        if (dx * dx + dy * dy <= r * r) {
                            damageMonster(m, dmg, cosf(a) * 10.0f, sinf(a) * 10.0f, 2);
                        }
                    }
                }
            }
        }

        // 3: Цепная молния (Chain Lightning strikes)
        if (itemLevels[3] > 0) {
            weaponTimers[3] -= dt;
            if (weaponTimers[3] <= 0.0f) {
                int lvl = itemLevels[3];
                weaponTimers[3] = std::max(0.65f, (2.1f - lvl * 0.14f) * cdMult * getWeaponRankCooldownMultiplier(3));
                int strikes = 3 + lvl / 2 + (evolvedWeapons[3] ? 3 : 0);
                float dmg = (30.0f + lvl * 12.0f) * getEvoMultiplier(3) * getWeaponRankDamageMultiplier(3);
                float prevX = playerX, prevY = playerY;
                bool hitAny = false;
                for (int s = 0; s < strikes; ++s) {
                    int target = findNearestMonster(prevX, prevY, 380.0f * getWeaponRankAreaMultiplier(3));
                    if (target < 0) break;
                    Monster& m = monsters[target];
                    LightningArc arc = {prevX, prevY, m.x, m.y, 0.24f};
                    lightningArcs.push_back(arc);
                    prevX = m.x;
                    prevY = m.y;
                    preserveParticleRandomBudget(8);
                    damageMonster((size_t)target, dmg, 0.0f, -6.0f, 3);
                    hitAny = true;
                }
                if (hitAny) sound.play(SFX_LIGHTNING, 0.20f);
            }
        }

        // 4: Кровавая аура (Soul / Garlic Aura)
        if (itemLevels[4] > 0) {
            auraPulse += dt * 4.5f;
            weaponTimers[4] -= dt;
            if (weaponTimers[4] <= 0.0f) {
                int lvl = itemLevels[4];
                weaponTimers[4] = 0.38f * cdMult * getWeaponRankCooldownMultiplier(4);
                float auraRad = (88.0f + lvl * 11.0f) * areaMult * getWeaponRankAreaMultiplier(4);
                float dmg = (10.0f + lvl * 4.8f) * getEvoMultiplier(4) * getWeaponRankDamageMultiplier(4);
                for (size_t m = 0; m < monsters.size(); ++m) {
                    if (monsters[m].hp <= 0.0f) continue;
                    float dx = monsters[m].x - playerX;
                    float dy = monsters[m].y - playerY;
                    float d2 = dx * dx + dy * dy;
                    if (d2 <= auraRad * auraRad) {
                        float dist = sqrtf(std::max(1.0f, d2));
                        damageMonster(m, dmg, (dx / dist) * 8.0f, (dy / dist) * 8.0f, 4);
                    }
                }
            }
        }

        // 5: Боевой топор (High-damage piercing arc axes)
        if (itemLevels[5] > 0) {
            weaponTimers[5] -= dt;
            if (weaponTimers[5] <= 0.0f) {
                int lvl = itemLevels[5];
                weaponTimers[5] = std::max(0.55f, (1.45f - lvl * 0.09f) * cdMult * getWeaponRankCooldownMultiplier(5));
                int count = 1 + lvl / 3 + (lvl >= 7 ? 1 : 0) + (evolvedWeapons[5] ? 2 : 0);
                for (int i = 0; i < count; ++i) {
                    Projectile p;
                    p.x = playerX;
                    p.y = playerY - 8.0f;
                    float dir = (i % 2 == 0) ? (float)playerFacing : -(float)playerFacing;
                    p.vx = dir * (110.0f + i * 45.0f) + (rand() % 40 - 20);
                    p.vy = -370.0f - i * 35.0f;
                    p.weaponId = 5;
                    p.damage = (36.0f + lvl * 15.0f) * getEvoMultiplier(5) * getWeaponRankDamageMultiplier(5);
                    p.radius = 18.0f * areaMult * getWeaponRankAreaMultiplier(5);
                    p.life = 2.0f;
                    p.pierce = 999; // Pierces all enemies!
                    p.isEnemy = false;
                    p.isCrit = false;
                    projectiles.push_back(p);
                }
                sound.play(SFX_BLADE, 0.18f);
            }
        }

        // 6: Ледяное кольцо (Frost Nova)
        if (itemLevels[6] > 0) {
            weaponTimers[6] -= dt;
            if (weaponTimers[6] <= 0.0f) {
                int lvl = itemLevels[6];
                weaponTimers[6] = std::max(1.2f, (3.2f - lvl * 0.22f) * cdMult * getWeaponRankCooldownMultiplier(6));
                float maxRad = (155.0f + lvl * 18.0f) * areaMult * getWeaponRankAreaMultiplier(6);
                float dmg = (24.0f + lvl * 10.0f) * getEvoMultiplier(6) * getWeaponRankDamageMultiplier(6);
                FrostWave fw = {playerX, playerY, 20.0f, maxRad, 0.40f, 0.40f};
                frostWaves.push_back(fw);
                bool frozeAnyTarget = false;
                for (size_t m = 0; m < monsters.size(); ++m) {
                    if (monsters[m].hp <= 0.0f) continue;
                    float dx = monsters[m].x - playerX;
                    float dy = monsters[m].y - playerY;
                    if (dx * dx + dy * dy <= maxRad * maxRad) {
                        monsters[m].freezeTimer = 1.6f + lvl * 0.2f + (evolvedWeapons[6] ? 2.5f : 0.0f);
                        monsters[m].iceFractureArmed = itemLevels[35] > 0 && !monsters[m].isBoss && !monsters[m].isResonator;
                        frozeAnyTarget = true;
                        damageMonster(m, dmg, 0.0f, 0.0f, 6);
                    }
                }
                if (frozeAnyTarget) sound.play(SFX_FREEZE, 0.30f);
            }
        }
        updateTierWeapons(dt);
        if (projectiles.size() > projectileCountBefore) sound.play(SFX_PROJECTILE_FLIGHT, 0.14f);
    }

    void updateTierWeapons(float dt) {
        const float cdMult = getCooldownMultiplier();
        const float areaMult = getAreaMultiplier();
        // 24: Tier II Comet Shard — fast homing-style volleys with piercing crystal bolts.
        if (itemLevels[24] > 0) {
            const int level = itemLevels[24];
            tierWeaponTimers[0] -= dt;
            if (tierWeaponTimers[0] <= 0.0f) {
                tierWeaponTimers[0] = std::max(0.38f, (1.28f - level * 0.085f) * cdMult);
                const int count = 1 + (level >= 4 ? 1 : 0) + (level >= 7 ? 1 : 0);
                const int target = findNearestMonster(playerX, playerY, 900.0f);
                const float baseAngle = target >= 0 ? atan2f(monsters[target].y-playerY, monsters[target].x-playerX)
                                                     : (playerFacing > 0 ? 0.0f : 3.14159f);
                for (int i = 0; i < count; ++i) {
                    const float angle = baseAngle + (i - (count - 1) * 0.5f) * 0.12f;
                    Projectile p{}; p.x = playerX; p.y = playerY;
                    p.vx = cosf(angle) * 590.0f; p.vy = sinf(angle) * 590.0f;
                    p.weaponId = 24; p.damage = 30.0f + level * 13.0f;
                    p.radius = (7.0f + level * 0.7f) * areaMult; p.life = 1.75f;
                    p.pierce = 1 + (level >= 5 ? 1 : 0); p.isEnemy = false;
                    projectiles.push_back(p);
                }
                sound.play(SFX_BLADE, 0.16f);
            }
        }
        // 25: Tier III Rune Turn — a sweeping, player-centered pulse that clears space.
        if (itemLevels[25] > 0) {
            const int level = itemLevels[25];
            tierWeaponTimers[1] -= dt;
            if (tierWeaponTimers[1] <= 0.0f) {
                tierWeaponTimers[1] = std::max(1.65f, (4.25f - level * 0.24f) * cdMult);
                const float radius = (190.0f + level * 22.0f) * areaMult;
                const float damage = 28.0f + level * 14.0f;
                addShockwave(playerX, playerY, radius, 0xFF58D9FF, 0.55f);
                bool hit = false;
                for (size_t i = 0; i < monsters.size(); ++i) {
                    const float dx = monsters[i].x-playerX, dy = monsters[i].y-playerY;
                    if (monsters[i].hp > 0.0f && dx*dx+dy*dy <= radius*radius) {
                        damageMonster(i, damage, dx*0.10f, dy*0.10f, 25); hit = true;
                    }
                }
                spawnParticles(playerX, playerY, 18, 0xFF6EDCFF, 145.0f, 1, true);
                if (hit) sound.play(SFX_LIGHTNING, 0.15f);
            }
        }
        // 26: Tier IV Rift Spear — crackling chain jumps between distinct nearby targets.
        if (itemLevels[26] > 0) {
            const int level = itemLevels[26];
            tierWeaponTimers[2] -= dt;
            if (tierWeaponTimers[2] <= 0.0f) {
                tierWeaponTimers[2] = std::max(1.35f, (3.35f - level * 0.20f) * cdMult);
                const int maxJumps = 3 + level / 2;
                float fromX = playerX, fromY = playerY;
                bool hit = false;
                std::vector<int> struck;
                for (int jump = 0; jump < maxJumps; ++jump) {
                    int best = -1; float bestD2 = 560.0f * 560.0f;
                    for (size_t i = 0; i < monsters.size(); ++i) {
                        if (monsters[i].hp <= 0.0f || std::find(struck.begin(), struck.end(), (int)i) != struck.end()) continue;
                        const float dx = monsters[i].x-fromX, dy = monsters[i].y-fromY, d2 = dx*dx+dy*dy;
                        if (d2 < bestD2) { bestD2 = d2; best = (int)i; }
                    }
                    if (best < 0) break;
                    const float targetX = monsters[best].x, targetY = monsters[best].y;
                    lightningArcs.push_back({fromX, fromY, targetX, targetY, 0.28f});
                    preserveParticleRandomBudget(7);
                    damageMonster((size_t)best, 31.0f + level * 15.0f, 0.0f, -4.0f, 26);
                    struck.push_back(best); fromX = targetX; fromY = targetY; hit = true;
                }
                if (hit) sound.play(SFX_LIGHTNING, 0.24f);
            }
        }
        // 27: Tier V Ash Star — slow, high-impact meteor aimed at the densest threat area.
        if (itemLevels[27] > 0) {
            const int level = itemLevels[27];
            tierWeaponTimers[3] -= dt;
            if (tierWeaponTimers[3] <= 0.0f) {
                tierWeaponTimers[3] = std::max(2.0f, (6.4f - level * 0.42f) * cdMult);
                const int target = findNearestMonster(playerX, playerY, 1050.0f);
                if (target >= 0) {
                    const float tx = monsters[target].x, ty = monsters[target].y;
                    const float radius = (150.0f + level * 22.0f) * areaMult;
                    const float damage = 82.0f + level * 34.0f;
                    addShockwave(tx, ty, radius, 0xFFFF9A42, 0.72f);
                    preserveParticleRandomBudget(34); // the scorched impact mark now renders beneath actors.
                    for (size_t i = 0; i < monsters.size(); ++i) {
                        const float dx = monsters[i].x-tx, dy = monsters[i].y-ty;
                        if (monsters[i].hp > 0.0f && dx*dx+dy*dy <= radius*radius)
                            damageMonster(i, damage, dx*0.08f, dy*0.08f, 27);
                    }
                    sound.play(SFX_FIREBALL, 0.28f);
                }
            }
        }
    }

    void recordRunEnd() {
        if (runRecorded) return;
        runRecorded = true;
        save.totalRuns++;
        if ((int)runTime > save.bestTimeSec) save.bestTimeSec = (int)runTime;
        if (kills > save.bestKills) save.bestKills = kills;
        writeSave();
    }

    void spawnExtractionPortal() {
        Pickup portal{};
        float angle = (rand() % 6283) / 1000.0f;
        float dist = 520.0f + (rand() % 340);
        portal.x = playerX + cosf(angle) * dist;
        portal.y = playerY + sinf(angle) * dist;
        portal.type = 7; portal.value = 1; portal.bobTime = 0.0f; portal.magnetized = false;
        pickups.push_back(portal);
        portalSpawned = true;
        announce(GMSG_PORTAL_FOUND, gameMode == MODE_STORY ? "ЭХО-ПОРТАЛ К КУЗНИЦЕ: СОХРАНИТ ДОБЫЧУ, НО НЕ СЛОМАЕТ ПЕЧАТИ." : "РЕДКИЙ ПОРТАЛ ОБНАРУЖЕН — ВОЙДИ В НЕГО, ЧТОБЫ СОХРАНИТЬ ЗАБЕГ", 7.0f);
        addShockwave(portal.x, portal.y, 120.0f, 0xFF69E8FF, 1.2f);
        sound.play(SFX_EVOLVE, 0.1f);
    }

    void extractThroughPortal() {
        if (state != STATE_PLAYING) return;
        const int bonus = std::max(0, (int)roundf(runGold * 0.50f));
        awardGold(bonus);
        recordRunEnd();
        writeSave();
        // announce() only speaks when the banner has a duration; the old 0.0f here meant the
        // "портал закрыт" line was shown but never read aloud.
        announce(GMSG_PORTAL_USED, gameMode == MODE_STORY ? "КУЗНИЦА СОХРАНИЛА ДОБЫЧУ И УЛИКИ. АРХИВ ВСЁ ЕЩЁ ЗАПЕЧАТАН." : "ПОРТАЛ ЗАКРЫТ — ЗОЛОТО СОХРАНЕНО", 4.0f);
        sound.musicMode = 4;
        state = STATE_EXTRACTION;
    }

    void updatePlaying(float dt) {
        if (cheatGodMode) playerHp = playerMaxHp;   // дебаг-чит: бессмертие
        const float framePlayerStartX = playerX;
        const float framePlayerStartY = playerY;
        runTime += dt;
        for (Monster& m : monsters) if (m.isBoss && m.phaseInvulnTimer > 0.0f)
            m.phaseInvulnTimer = std::max(0.0f, m.phaseInvulnTimer - dt);
        phoenixReviveTimer = std::max(0.0f, phoenixReviveTimer - dt);
        wardFlashTimer = std::max(0.0f, wardFlashTimer - dt);
        overkillShardCooldown = std::max(0.0f, overkillShardCooldown - dt);
        crossfireGlobalCooldown = std::max(0.0f, crossfireGlobalCooldown - dt);
        emberGlobalCooldown = std::max(0.0f, emberGlobalCooldown - dt);
        frostShatterCooldown = std::max(0.0f, frostShatterCooldown - dt);
        projectileParryCooldown = std::max(0.0f, projectileParryCooldown - dt);
        retaliationChargeTimer = std::max(0.0f, retaliationChargeTimer - dt);
        ashRhythmFlashTimer = std::max(0.0f, ashRhythmFlashTimer - dt);
        veilCooldown = std::max(0.0f, veilCooldown - dt);
        resonatorSpawnTimer = std::max(0.0f, resonatorSpawnTimer - dt);
        if (resonatorSpawnTimer <= 0.0f) {
            const int beforeResonators = countResonators();
            if (beforeResonators < MAX_ACTIVE_RESONATORS) spawnResonator(resonatorSpawnIndex++);
            const int afterResonators = countResonators();
            if (afterResonators > beforeResonators) {
                const Monster& newResonator = monsters.back();
                spawnParticles(newResonator.x, newResonator.y, 24, 0xFFB46CFF, 78.0f, 3);
                addShockwave(newResonator.x, newResonator.y, 82.0f, 0xFFD59BFF, 0.62f);
                announce(GMSG_RESONATOR_SPAWN, gameMode == MODE_STORY ? "ЭХО-ЛИТЕЙНЯ: НАЙДИ РЕЗОНАТОР И РАЗРУШЬ — ОН ПЕЧАТАЕТ КОПИИ ПРИКАЗА." : "НОВЫЙ РЕЗОНАТОР НА ПОЛЕ — НАЙДИ И РАЗРУШЬ ЕГО", 3.4f);
            }
            // Let the player reach the current distant objective before another is introduced.
            resonatorSpawnTimer = afterResonators < MAX_ACTIVE_RESONATORS
                ? 40.0f + (rand() % 1001) / 100.0f
                : 16.0f;
        }
        waveNumber = 1 + (int)(runTime / 30.0f);
        if (gameMode != MODE_STORY && !portalSpawned) {
            portalSpawnTimer -= dt;
            if (portalSpawnTimer <= 0.0f) spawnExtractionPortal();
        }
        if (victoryMusicTimer > 0.0f) victoryMusicTimer = std::max(0.0f, victoryMusicTimer - dt);
        if (magnetAudioTimer > 0.0f) magnetAudioTimer = std::max(0.0f, magnetAudioTimer - dt);
        if (storyTimer > 0.0f) storyTimer = std::max(0.0f, storyTimer - dt);
        if (buildTierUpTimer > 0.0f) buildTierUpTimer = std::max(0.0f, buildTierUpTimer - dt);
        if (difficultyBannerTimer > 0.0f) difficultyBannerTimer = std::max(0.0f, difficultyBannerTimer - dt);
        if (eventCooldown > 0.0f) eventCooldown = std::max(0.0f, eventCooldown - dt);
        killWindowTimer += dt;
        if (killWindowTimer >= 5.0f) {
            measuredKillsPerSecond = (kills - killWindowStartCount) / killWindowTimer;
            killWindowStartCount = kills; killWindowTimer = 0.0f;
            if (measuredKillsPerSecond < 10.0f) lowActionTimer += 5.0f;
            else lowActionTimer = 0.0f;
        }
        if (huntActive) {
            huntTimer = std::max(0.0f, huntTimer - dt);
            if (huntTimer <= 0.0f) failHuntEvent();
        }
        // The hunt never overlaps the breakthrough: one is an opportunity, the other is a wall.
        if (!huntActive && !breakthroughActive && lowActionTimer >= 8.0f && eventCooldown <= 0.0f &&
            countActiveEnemies() < 95 &&
            (options.hardModeEnabled || playerHp / std::max(1.0f, playerMaxHp) > 0.34f)) triggerHuntEvent();

        if (breakthroughActive) {
            if (breakthroughTimer > 0.0f) {
                breakthroughTimer = std::max(0.0f, breakthroughTimer - dt);
                if (breakthroughTimer <= 0.0f) {
                    if (breakthroughEarlyHit) finishBreakthrough();
                    else { difficultyBanner = "ОРДА НЕ ОСТАНОВИТСЯ, ПОКА ТЫ НЕ ПОЛУЧИШЬ УДАР"; difficultyBannerTimer = 4.0f; speakOnly(GMSG_BREAKTHROUGH_PRESSURE, difficultyBanner); }
                }
            } else {
                breakthroughEscalation += dt;
            }
        } else if (recoveryTimer > 0.0f) {
            recoveryTimer = std::max(0.0f, recoveryTimer - dt);
        }
        if (!breakthroughActive && playerLevel >= nextBreakthroughLevel) breakthroughQueued = true;
        if (breakthroughQueued && (options.hardModeEnabled || (getThreatPressure() < 0.78f && countActiveEnemies() < 118))) {
            breakthroughQueued = false;
            startBreakthrough();
        }
        if (!breakthroughActive && !breakthroughQueued && playerLevel >= nextBreakthroughLevel - 1 &&
            lastBreakthroughWarningLevel != nextBreakthroughLevel) {
            lastBreakthroughWarningLevel = nextBreakthroughLevel;
            difficultyBanner = "ПРЕДУПРЕЖДЕНИЕ: НА СЛЕДУЮЩЕМ УРОВНЕ БУДЕТ ПРОРЫВ";
            speakOnly(GMSG_BREAKTHROUGH_WARNING, difficultyBanner);
            difficultyBannerTimer = 3.5f;
            sound.play(SFX_BOSS, 0.06f);
        }

        if (screenShake > 0.0f) screenShake = std::max(0.0f, screenShake - dt);
        if (bossWarningTimer > 0.0f) bossWarningTimer = std::max(0.0f, bossWarningTimer - dt);
        if (bossPhaseEventTimer > 0.0f) bossPhaseEventTimer = std::max(0.0f, bossPhaseEventTimer - dt);
        if (bossRushTimer > 0.0f) bossRushTimer = std::max(0.0f, bossRushTimer - dt);
        if (playerHurtTimer > 0.0f) playerHurtTimer = std::max(0.0f, playerHurtTimer - dt);
        if (playerSlowTimer > 0.0f) playerSlowTimer = std::max(0.0f, playerSlowTimer - dt);
        if (dashCooldown > 0.0f) dashCooldown = std::max(0.0f, dashCooldown - dt);
        if (superTimer > 0.0f) superTimer = std::max(0.0f, superTimer - dt);

        for (BossHazard& hazard : bossHazards) {
            if (hazard.warningTimer > 0.0f) {
                hazard.warningTimer = std::max(0.0f, hazard.warningTimer - dt);
                if (hazard.warningTimer <= 0.0f) spawnParticles(hazard.x, hazard.y, 20, 0xFF72D969, 105.0f, 4, true);
                continue;
            }
            hazard.life -= dt;
            hazard.tickTimer -= dt;
            float hx = playerX - hazard.x, hy = playerY - hazard.y;
            if (hazard.tickTimer <= 0.0f && hx * hx + hy * hy <= hazard.radius * hazard.radius) {
                hazard.tickTimer = 0.68f;
                applyBossAbilityHit(hazard.damage, hazard.hazardType != 1);
            }
        }
        bossHazards.erase(std::remove_if(bossHazards.begin(), bossHazards.end(),
            [](const BossHazard& h) { return h.life <= 0.0f; }), bossHazards.end());

        // DPS rolling calculation
        dpsTimer += dt;
        if (dpsTimer >= 1.0f) {
            const float dpsSample = dpsWindowDamage / std::max(0.001f, dpsTimer);
            recordRecentDpsSample(dpsSample);
            currentDps = currentDps * 0.4f + dpsSample * 0.6f;
            dpsWindowDamage = 0.0f;
            dpsTimer = 0.0f;
        }

        // Passive HP regeneration (Сердце титана + base)
        float regen = 0.35f + itemLevels[10] * 1.5f;
        restorePlayerHp(regen * dt);

        // Player Movement
        if (dashTimer > 0.0f) {
            dashTimer -= dt;
            float dashSpd = getMoveSpeed() * 4.8f;
            playerX += dashDirX * dashSpd * dt;
            playerY += dashDirY * dashSpd * dt;
            // Dash is now a deliberate offensive escape: each enemy can be clipped once per dash.
            for (size_t mi = 0; mi < monsters.size(); ++mi) {
                Monster& target = monsters[mi];
                if (target.hp <= 0.0f || std::find(dashHitIds.begin(), dashHitIds.end(), target.uniqueId) != dashHitIds.end()) continue;
                float hx = target.x - playerX, hy = target.y - playerY;
                float hitR = target.radius + 22.0f;
                if (hx * hx + hy * hy <= hitR * hitR) {
                    dashHitIds.push_back(target.uniqueId);
                    damageMonster(mi, 42.0f + playerLevel * 2.5f, dashDirX * 12.0f, dashDirY * 12.0f, 29);
                    preserveParticleRandomBudget(5);
                }
            }
            if (((int)(dashTimer * 60.0f)) % 2 == 0) {
                spawnParticles(playerX, playerY, 2, 0xFF40D8FF, 40.0f, 3, true);
            }
        } else {
            float dx = 0.0f, dy = 0.0f;
            if (keyW || keyUp) dy -= 1.0f;
            if (keyS || keyDown) dy += 1.0f;
            if (keyA || keyLeft) dx -= 1.0f;
            if (keyD || keyRight) dx += 1.0f;

            // Optional Right-Mouse-Button follow cursor
            if (mouseRightDown && dx == 0.0f && dy == 0.0f) {
                float mdx = (mouseX - SCREEN_W * 0.5f);
                float mdy = (mouseY - SCREEN_H * 0.5f);
                float mlen = sqrtf(mdx * mdx + mdy * mdy);
                if (mlen > 14.0f) {
                    dx = mdx / mlen;
                    dy = mdy / mlen;
                }
            }

            if (dx != 0.0f || dy != 0.0f) {
                float len = sqrtf(dx * dx + dy * dy);
                dx /= len; dy /= len;
                if (dx > 0.1f) playerFacing = 1;
                else if (dx < -0.1f) playerFacing = -1;
                float spd = getMoveSpeed() * (playerSlowTimer > 0.0f ? 0.68f : 1.0f);
                playerX += dx * spd * dt;
                playerY += dy * spd * dt;
            }
        }
        if (dt > 0.0001f) {
            playerVelocityX = (playerX - framePlayerStartX) / dt;
            playerVelocityY = (playerY - framePlayerStartY) / dt;
        }

        // Spawning Waves & Bosses
        float baseSpawnRate = 3.2f + std::min(8.5f, runTime / 35.0f) * 0.75f + std::max(0.0f, runTime - 210.0f) / 120.0f;
        const bool bossRush = bossRushTimer > 0.0f;
        const float rushMultiplier = bossRush ? BOSS_RUSH_MULTIPLIER : 1.0f;
        float spawnRate = std::min((baseSpawnRate + getResonatorSpawnRateBonus()) * balance.spawnMultiplier * getSublevelSpawnMultiplier() * getEnemyOathSpawnMultiplier() * campaignDifficultyMultiplier() * rushMultiplier, 16.0f * rushMultiplier);
        spawnAccumulator += spawnRate * dt;
        while (spawnAccumulator >= 1.0f) {
            spawnAccumulator -= 1.0f;
            // Which enemies show up is a property of the place, not a global dice roll:
            // the crypt fields shields and brutes, the cathedral its shooters, the ash
            // throne heavy elites. Strength still unlocks with the run clock.
            // The place also decides how advanced its garrison is: the ruins open with
            // imps and skeletons, the archive heart opens with wardens and shooters.
            const int maxType = std::min(9, 1 + (int)(runTime / 20.0f) + currentArenaTheme() * 2);
            int t = pickActMonsterType(currentArenaTheme(), maxType);
            const bool matryoshkaSpawn = runTime >= 150.0f && (rand() % 10000) < 250;
            if (matryoshkaSpawn) t = MATRYOSHKA_ENEMY_TYPE;
            const float eliteChance = (runTime < 90.0f ? 6.0f : (runTime < 240.0f ? 12.0f : 18.0f)) + getEnemyOathEliteChancePercent();
            bool elite = !matryoshkaSpawn && (runTime > 25.0f) && ((rand() % 10000) < (int)roundf(eliteChance * 100.0f));
            spawnMonster(t, elite, false);
        }
        squadTimer -= dt;
        if (squadTimer <= 0.0f && state == STATE_PLAYING) {
            squadTimer = 17.0f + (float)(rand() % 90) / 10.0f;
            spawnSquad();
        }
        if (pendingObjectiveVoiceTimer > 0.0f) {
            pendingObjectiveVoiceTimer -= dt;
            if (pendingObjectiveVoiceTimer <= 0.0f && !difficultyBanner.empty())
                speakOnly(GMSG_MISSION_OBJECTIVE, difficultyBanner);
        }
        ratTimer -= dt;
        if (ratTimer <= 0.0f && state == STATE_PLAYING) {
            ratTimer = 28.0f + (float)(rand() % 160) / 10.0f;
            spawnRatSwarm();
            announce(GMSG_RAT_SWARM, "КРЫСИНАЯ СТАЯ ПРОХОДИТ ЧЕРЕЗ ПОЛЕ", 2.4f);
        }

        // Story campaigns are standalone missions: each has one clearly scheduled final encounter.
        // Endless keeps the original repeating 90-second guardian cadence.
        int livingBosses = 0;
        for (const Monster& m : monsters) if (m.isBoss && m.hp > 0.0f) ++livingBosses;
        const bool bossSlotAvailable = livingBosses < 3;
        if (gameMode == MODE_STORY) {
            const CampaignMissionDef& mission = selectedCampaignMission();
            if (!storyMissionBossSpawned && runTime >= mission.bossTimeSeconds && bossSlotAvailable && isCampaignMissionUnlocked(selectedStoryMission)) {
                storyMissionBossSpawned = true;
                if (mission.finalTrio) {
                    clearArenaForBoss();
                    bossTrioStarted = true;
                    bossTrioSpawned = 0;
                    trioBossesDefeated = 0;
                    const int roster[3] = {3, 4, 5}; // три последних стража, не те, что уже пали
                    for (int j = 0; j < 3; ++j) {
                        spawnMonster(roster[j], false, true, roster[j]);
                        if (monsters.back().isBoss) {
                            Monster& echo = monsters.back();
                            echo.hp *= 0.58f; echo.maxHp *= 0.58f; echo.isTrioBoss = true;
                            ++bossTrioSpawned;
                            static const uint32_t echoPalette[3] = {0xFFFF7620, 0xFF49C8E8, 0xFF9EDBE8};
                            const uint32_t color = echoPalette[j];
                            spawnParticles(echo.x, echo.y, 58, color, 205.0f, 4, true);
                            addShockwave(echo.x, echo.y, 145.0f, color, 0.86f);
                        }
                    }
                    bossWarningName = save.seenEndingCutscene
                        ? "ТРЕНИРОВОЧНЫЙ ПРОТОКОЛ: ТРИ ЭХО-ПРИКАЗА"
                        : "АРХИВНЫЙ ПРОТОКОЛ: ТРИ ЭХО-ПРИКАЗА";
                    bossWarningTimer = 6.0f;
                    announce(GMSG_TRIO_INTRO, "ТРИ ЗАПИСИ ЗАЩИЩАЮТ СЕРДЦЕ АРХИВА. ЭТО НЕ ВОСКРЕСШИЕ СТРАЖИ.", 7.0f);
                } else {
                    clearArenaForBoss();
                    spawnMonster(mission.bossType, false, true, mission.bossType);
                    if (monsters.back().isBoss) {
                        Monster& boss = monsters.back();
                        boss.isMissionFinalBoss = true;
                        if (mission.bossReinforced) {
                            // Возвращение: тот же страж, но Архив поднял его злее.
                            // Силуэт и приёмы те же — меняются цифры и темп, чтобы
                            // знакомый бой не превращался в копию первого.
                            boss.bossReinforced = true;
                            boss.maxHp *= 1.45f;
                            boss.hp = boss.maxHp;
                            boss.damage *= 1.15f;
                            boss.speed *= 1.08f;
                            boss.bossPhase = 2;
                            boss.abilityTimer = std::min(boss.abilityTimer, 3.2f);
                            boss.specialTimer = std::min(boss.specialTimer, 4.0f);
                        }
                        static const uint32_t missionPalette[6] = {0xFFFFC848, 0xFF80FF50, 0xFFFF4068, 0xFFFF7620, 0xFF49C8E8, 0xFF9EDBE8};
                        const uint32_t color = missionPalette[mission.bossType % 6];
                        spawnParticles(boss.x, boss.y, 85, color, 240.0f, 5, true);
                        spawnParticles(boss.x, boss.y, 45, 0xFFE8DCFF, 165.0f, 3, true);
                        addShockwave(boss.x, boss.y, 160.0f, color, 0.95f);
                    }
                    bossWarningName = mission.finalBoss;
                    if (mission.bossReinforced) bossWarningName += "  •  ВОЗВРАЩЕНИЕ";
                    bossWarningTimer = 5.0f;
                    announce(GMSG_MISSION_BRIEFING, mission.briefing, 5.0f);
                }
                if (options.screenShakeEnabled) screenShake = std::max(screenShake, 0.48f);
                sound.play(SFX_BOSS, 0.12f);
            }
        } else {
            const float nextBossTime = 90.0f + nextBossIndex * 90.0f;
            if (runTime >= nextBossTime && bossSlotAvailable) {
                const int bType = nextBossIndex % 6;
                ++nextBossIndex;
                bossDeferredTimer = 0.0f;
                clearArenaForBoss();
                spawnMonster(bType, false, true, bType);
                const Monster& arrivingBoss = monsters.back();
                static const uint32_t arrivalPalette[6] = {0xFFFFC848, 0xFF80FF50, 0xFFFF4068, 0xFFFF7620, 0xFF49C8E8, 0xFF9EDBE8};
                const uint32_t arrivalColor = arrivalPalette[bType % 6];
                spawnParticles(arrivingBoss.x, arrivingBoss.y, 85, arrivalColor, 240.0f, 5, true);
                spawnParticles(arrivingBoss.x, arrivingBoss.y, 45, 0xFFE8DCFF, 165.0f, 3, true);
                addShockwave(arrivingBoss.x, arrivingBoss.y, 160.0f, arrivalColor, 0.95f);
                static const char* bNames[6] = {
                    "МОРГАР — ВОРОТА НЕ ДОЛЖНЫ ОТКРЫТЬСЯ",
                    "МАЛАКОР — ПОСЛЕДНИЙ ГОЛОС СЕСТРЫ",
                    "КАРДИНАЛ — ИМЕНА ДОЛЖНЫ ЗАМОЛЧАТЬ",
                    "АЗАРОТ — ПЕРВЫЙ АРХИВАРИУС",
                    "ПРОТОКОЛИСТ — ВСЁ, ЧТО ЗАПИСАНО, ПОВТОРИТСЯ",
                    "ДВОЙНИК — ТЫ УЖЕ БЫЛ ЗДЕСЬ"
                };
                bossWarningName = bNames[bType];
                bossWarningTimer = 4.0f;
                speakOnly(GMSG_BOSS_WARNING, std::string("Страж приближается: ") + bNames[bType]);
                sound.play(SFX_BOSS, 0.1f);
            }
        }

        // Update Weapons
        updateWeapons(dt);

        // Update Projectiles
        for (size_t i = 0; i < projectiles.size(); ++i) {
            Projectile& p = projectiles[i];
            p.life -= dt;
            if (p.weaponId == 5) {
                // Gravity on thrown battleaxe
                p.vy += 680.0f * dt;
            }
            if (p.isEnemy && p.projectileKind == 2 && p.life > 0.0f) {
                const float speed = sqrtf(p.vx * p.vx + p.vy * p.vy);
                const float desired = atan2f(playerY - p.y, playerX - p.x);
                float current = atan2f(p.vy, p.vx);
                float delta = desired - current;
                while (delta > 3.1415926f) delta -= 6.2831853f;
                while (delta < -3.1415926f) delta += 6.2831853f;
                current += std::max(-2.2f * dt, std::min(2.2f * dt, delta));
                p.vx = cosf(current) * speed; p.vy = sinf(current) * speed;
            }
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            if (options.projectileTrailsEnabled && !p.isEnemy && particles.size() < 680) {
                p.trailTimer -= dt;
                if (p.trailTimer <= 0.0f) {
                    Particle trail{};
                    trail.x = p.x; trail.y = p.y; trail.vx = -p.vx * 0.025f; trail.vy = -p.vy * 0.025f;
                    trail.life = trail.maxLife = 0.18f;
                    trail.color = p.weaponId == 1 ? 0xFFFF8B38 : (p.weaponId == 5 ? 0xFFFFD16E :
                        (p.weaponId == 24 ? 0xFF70DEFF : 0xFF79E8FF));
                    trail.size = 3; trail.additive = true; trail.style = 5;
                    trail.rotation = atan2f(p.vy, p.vx); trail.stretch = 1.8f; trail.scale = 1.0f;
                    particles.push_back(trail);
                    p.trailTimer = 0.045f;
                }
            }
            if (p.isEnemy && p.projectileKind == 3 && p.life <= 0.0f) {
                spawnEnemyHazard(p.x, p.y, p.damage * 0.70f, 1);
                p.projectileKind = 4;
                continue;
            }

            if (p.isEnemy) {
                // A well-timed dash through an enemy projectile counts as one precise evade.
                float dx = p.x - playerX;
                float dy = p.y - playerY;
                const bool overlapsPlayer = dx * dx + dy * dy <= (p.radius + 14.0f) * (p.radius + 14.0f);
                if (overlapsPlayer && dashTimer > 0.0f) registerDashEvade();
                if (overlapsPlayer && dashTimer <= 0.0f && playerHurtTimer <= 0.0f) {
                        if (tryReflectEnemyProjectile(p)) continue;
                        if (p.projectileKind == 3) {
                            spawnEnemyHazard(p.x, p.y, p.damage * 0.70f, 1);
                            p.projectileKind = 4; p.life = 0.0f;
                            continue;
                        }
                        float dmg = applyVeilMitigation(std::max(2.0f, p.damage - getArmor()));
                        playerHp -= dmg;
                        registerRetaliationCharge(dmg);
                        notePlayerDamaged();
                        playerHurtTimer = 0.35f;
                        if (options.screenShakeEnabled) screenShake = 0.20f;
                        sound.play(p.isBossProjectile ? SFX_BOSS_IMPACT : SFX_HIT, p.isBossProjectile ? 0.14f : 0.1f);
                        p.life = 0.0f;
                    }
            } else {
                // Check hit against monsters
                for (size_t m = 0; m < monsters.size(); ++m) {
                    if (monsters[m].hp <= 0.0f) continue;
                    float dx = monsters[m].x - p.x;
                    float dy = monsters[m].y - p.y;
                    float r = monsters[m].radius + p.radius;
                    if (dx * dx + dy * dy <= r * r) {
                        int mId = (int)m;
                        if (std::find(p.hitIds.begin(), p.hitIds.end(), mId) != p.hitIds.end()) continue;
                        p.hitIds.push_back(mId);

                        if (p.weaponId == 1) {
                            // Fireball AoE explosion; ground marks replace sprite-covering particle bursts.
                            float blastRad = 85.0f * getAreaMultiplier() * getWeaponRankAreaMultiplier(p.weaponId);
                            preserveParticleRandomBudget(24);
                            sound.play(SFX_PROJECTILE_IMPACT, 0.05f);
                            for (size_t k = 0; k < monsters.size(); ++k) {
                                if (monsters[k].hp <= 0.0f) continue;
                                float ex = monsters[k].x - p.x;
                                float ey = monsters[k].y - p.y;
                                if (ex * ex + ey * ey <= blastRad * blastRad) {
                                    damageMonster(k, p.damage, ex * 0.15f, ey * 0.15f, p.weaponId);
                                }
                            }
                            p.life = 0.0f;
                            break;
                        } else {
                            float spd = sqrtf(p.vx * p.vx + p.vy * p.vy);
                            float kx = (spd > 1.0f) ? (p.vx / spd) * 9.0f : 0.0f;
                            float ky = (spd > 1.0f) ? (p.vy / spd) * 9.0f : 0.0f;
                            damageMonster(m, p.damage, kx, ky, p.weaponId);
                            sound.play(SFX_PROJECTILE_IMPACT, 0.045f);
                            p.pierce--;
                            if (p.pierce <= 0) {
                                p.life = 0.0f;
                                break;
                            }
                        }
                    }
                }
            }
        }
        projectiles.erase(
            std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p) { return p.life <= 0.0f; }),
            projectiles.end()
        );

        // Update Monsters
        for (size_t i = 0; i < monsters.size(); ++i) {
            Monster& m = monsters[i];
            if (m.hp <= 0.0f) continue;
            if (m.isResonator) { if (m.hitFlashTimer > 0.0f) m.hitFlashTimer -= dt; continue; }
            if (m.hitFlashTimer > 0.0f) m.hitFlashTimer -= dt;
            if (m.freezeTimer > 0.0f) {
                m.freezeTimer = std::max(0.0f, m.freezeTimer - dt);
                if (m.freezeTimer <= 0.0f && m.iceFractureArmed) {
                    m.iceFractureArmed = false;
                    triggerFrostShatter(i);
                }
            }
            updateExposureTimers(m, dt);
            updateEmberStatus(i, dt);
            if (m.hp <= 0.0f) continue;
            if (m.isBoss) {
                handleBossPhaseTransition(m);
                m.specialTimer -= dt;
                if (m.specialTimer <= 0.0f) {
                    // Each guardian calls a different kind of help, on its own cadence:
                    // Morgar walls himself in with shield bearers, Malakor floods the floor
                    // with plagued slimes, the Cardinal posts archers, Azaroth raises two
                    // heavy archive guards.
                    int summonType = 1, summonCount = 3;
                    float summonInterval = 6.2f;
                    switch (m.bossType % 6) {
                        case 0: summonType = 4; summonCount = 2; summonInterval = 7.4f; break;
                        case 1: summonType = 2; summonCount = 4; summonInterval = 5.2f; break;
                        case 2: summonType = 8; summonCount = 3; summonInterval = 6.4f; break;
                        case 4: summonType = 3; summonCount = 4; summonInterval = 5.8f; break;
                        case 5: summonType = 6; summonCount = 3; summonInterval = 6.1f; break;
                        default: summonType = 7; summonCount = 2; summonInterval = 6.9f; break;
                    }
                    m.specialTimer = std::max(3.6f, summonInterval - 0.9f * (m.bossPhase - 1));
                    pendingSummonType = summonType;
                    pendingSummons = std::max(pendingSummons, summonCount + (m.hp < m.maxHp * 0.5f ? 1 : 0) +
                                                                       (m.bossReinforced ? 2 : 0));
                    static const char* summonLines[6] = {
                        "МОРГАР: ВЫЗОВ СТРАЖИ ВОРОТ!",
                        "МАЛАКОР: ПРИЗЫВ ЧУМНОЙ СТАИ!",
                        "КАРДИНАЛ: ПИСАРИ ЗАКРЫВАЮТ КРУГ!",
                        "АЗАРОТ: ПЕПЕЛЬНЫЙ ЛЕГИОН, КО МНЕ!",
                        "ПРОТОКОЛИСТ: СВИДЕТЕЛИ, ЗАЙМИТЕ СТРОКИ!",
                        "ДВОЙНИК: СЛЕПКИ, ВСТАНЬТЕ РЯДОМ!"
                    };
                    announce(GMSG_BOSS_SUMMON, summonLines[m.bossType % 6], 2.8f);
                    sound.play(SFX_BOSS_TELEGRAPH, 0.28f);
                }

                if (m.abilityActive > 0.0f) m.abilityActive = std::max(0.0f, m.abilityActive - dt);
                if (m.abilityWindup > 0.0f) {
                    m.abilityWindup = std::max(0.0f, m.abilityWindup - dt);
                    if (m.abilityWindup <= 0.0f) {
                        if (m.bossType == 1) {
                            if (bossHazards.size() >= 8) bossHazards.erase(bossHazards.begin());
                            bossHazards.push_back({m.abilityTargetX, m.abilityTargetY, 112.0f, 0.0f, 4.2f, 0.15f, 10.0f + m.bossType * 2.0f, m.bossType});
                            addShockwave(m.abilityTargetX, m.abilityTargetY, 118.0f, 0xFF68D85E, 0.55f);
                            spawnParticles(m.abilityTargetX, m.abilityTargetY, 24, 0xFF72D969, 120.0f, 4, true);
                        } else if (m.bossType == 2) {
                            float tx = m.abilityTargetX - m.x, ty = m.abilityTargetY - m.y;
                            float len = sqrtf(tx * tx + ty * ty);
                            if (len > 1.0f) { m.abilityDirX = tx / len; m.abilityDirY = ty / len; }
                            m.abilityActive = 0.78f;
                            spawnParticles(m.x, m.y, 20, 0xFFFF4B78, 190.0f, 4, true);
                        } else if (m.bossType == 4) {
                            // Протоколист пишет строку поперёк арены: вал снарядов идёт на
                            // игрока, а в валу оставлен пробел там, где он стоял. Правило
                            // «не куда отойти, а куда встать» — обратное кольцу Азарота.
                            float tx = m.abilityTargetX - m.x, ty = m.abilityTargetY - m.y;
                            float len = sqrtf(tx * tx + ty * ty);
                            if (len < 1.0f) { tx = 1.0f; ty = 0.0f; len = 1.0f; }
                            const float nx = tx / len, ny = ty / len;      // направление строки
                            const float px = -ny, py = nx;                 // вдоль строки
                            const float lateral = (playerY - m.y) * py + (playerX - m.x) * px;
                            const float speed = 200.0f + m.bossPhase * 22.0f;
                            const int span = 15;
                            for (int i = 0; i < span; ++i) {
                                const float off = lateral + (i - span / 2) * 26.0f;
                                if (i == span / 2) continue;               // пробел в строке
                                Projectile ep;
                                ep.x = m.x + px * off - nx * 26.0f;
                                ep.y = m.y + py * off - ny * 26.0f;
                                ep.vx = nx * speed; ep.vy = ny * speed;
                                ep.weaponId = 99; ep.projectileKind = 1; ep.isBossProjectile = true;
                                ep.damage = m.damage * 0.45f;
                                ep.radius = 5.0f; ep.life = 3.2f; ep.pierce = 1;
                                ep.isEnemy = true; ep.isCrit = false;
                                projectiles.push_back(ep);
                            }
                            addShockwave(m.x, m.y, 96.0f, 0xFF49C8E8, 0.40f);
                            spawnParticles(m.x, m.y, 30, 0xFF49C8E8, 180.0f, 4, true);
                        } else if (m.bossType == 5) {
                            // Двойник меняется местами со своим отражением вокруг героя.
                            // Раньше он вставал ровно в зеркальную точку: если он подходил
                            // вплотную, зеркало тоже ложилось на героя, и сам обмен местами
                            // убивал касанием. Теперь зеркало держит дистанцию, а крест
                            // разворачивается так, чтобы один луч шёл прямо в героя, а не мимо.
                            const float oldX = m.x, oldY = m.y;
                            float awayX = m.x - playerX, awayY = m.y - playerY;
                            float awayLen = sqrtf(awayX * awayX + awayY * awayY);
                            if (awayLen < 1.0f) { awayX = 1.0f; awayY = 0.0f; awayLen = 1.0f; }
                            const float keepDist = 190.0f + 26.0f * (float)(m.bossPhase - 1);
                            float targetX = playerX - (awayX / awayLen) * keepDist;
                            float targetY = playerY - (awayY / awayLen) * keepDist;
                            targetX = std::max(80.0f, std::min((float)SCREEN_W - 80.0f, targetX));
                            targetY = std::max(90.0f, std::min((float)SCREEN_H - 90.0f, targetY));
                            // У края арены зеркало может подойти слишком близко: отодвигаем.
                            float fx = targetX - playerX, fy = targetY - playerY;
                            float flen = sqrtf(fx * fx + fy * fy);
                            if (flen < 150.0f) {
                                if (flen < 1.0f) { fx = -awayX / awayLen; fy = -awayY / awayLen; flen = 1.0f; }
                                targetX = playerX + (fx / flen) * 150.0f;
                                targetY = playerY + (fy / flen) * 150.0f;
                            }
                            m.x = targetX;
                            m.y = targetY;
                            spawnParticles(oldX, oldY, 34, 0xFF9EDBE8, 200.0f, 4, true);
                            addShockwave(oldX, oldY, 84.0f, 0xFF9EDBE8, 0.45f);
                            spawnParticles(m.x, m.y, 34, 0xFFB6E8F4, 200.0f, 4, true);
                            addShockwave(m.x, m.y, 84.0f, 0xFFB6E8F4, 0.45f);
                            const int cross = m.bossPhase >= 3 ? 10 : 8;
                            const float crossSpeed = 235.0f + m.bossPhase * 18.0f;
                            for (int side = 0; side < 2; ++side) {
                                const float bx = side == 0 ? m.x : oldX;
                                const float by = side == 0 ? m.y : oldY;
                                // Ось креста смотрит в героя: залп больше не уходит в сторону.
                                // У старой точки он сдвинут на полшага — это эхо, а не выстрел в упор.
                                const float aim = atan2f(playerY - by, playerX - bx) +
                                                  (side == 0 ? 0.0f : 3.1415927f / (float)cross);
                                for (int shot = 0; shot < cross; ++shot) {
                                    const float a = aim + shot * 6.2831853f / (float)cross;
                                    Projectile ep;
                                    ep.x = bx; ep.y = by;
                                    ep.vx = cosf(a) * crossSpeed; ep.vy = sinf(a) * crossSpeed;
                                    ep.weaponId = 99; ep.projectileKind = 5; ep.isBossProjectile = true;
                                    ep.damage = m.damage * 0.42f;
                                    ep.radius = 6.0f; ep.life = 2.2f; ep.pierce = 1;
                                    ep.isEnemy = true; ep.isCrit = false;
                                    projectiles.push_back(ep);
                                }
                            }
                            m.abilityActive = 0.55f;
                        } else if (m.bossType == 0) {
                            // Моргар — ударная волна вокруг себя: правило «отойди из круга».
                            const float radius = m.bossPhase >= 3 ? 190.0f : 150.0f;
                            float ax = playerX - m.x, ay = playerY - m.y;
                            if (ax * ax + ay * ay <= radius * radius) applyBossAbilityHit(30.0f + m.bossPhase * 4.0f);
                            addShockwave(m.x, m.y, radius + 22.0f, 0xFFFFC64A, 0.55f);
                            spawnParticles(m.x, m.y, 26, 0xFFFFC64A, 190.0f, 4, true);
                        } else {
                            // Азарот — кольцо искр с проходом: правило «найди разрыв и беги»,
                            // принципиально другое, чем «выйди из радиуса».
                            const int count = 18;
                            const float gapCenter = atan2f(playerY - m.y, playerX - m.x);
                            const float gapHalf = 0.52f;
                            const float speed = 250.0f + m.bossPhase * 22.0f;
                            for (int shot = 0; shot < count; ++shot) {
                                const float a = shot * 6.2831853f / count;
                                float delta = a - gapCenter;
                                while (delta > 3.1415927f) delta -= 6.2831853f;
                                while (delta < -3.1415927f) delta += 6.2831853f;
                                if (fabsf(delta) < gapHalf) continue; // the safe wedge
                                Projectile ep;
                                ep.x = m.x; ep.y = m.y;
                                ep.vx = cosf(a) * speed; ep.vy = sinf(a) * speed;
                                ep.weaponId = 99; ep.projectileKind = 5; ep.isBossProjectile = true;
                                ep.damage = m.damage * 0.55f;
                                ep.radius = 7.0f; ep.life = 2.6f; ep.pierce = 1;
                                ep.isEnemy = true; ep.isCrit = false;
                                projectiles.push_back(ep);
                            }
                            addShockwave(m.x, m.y, 120.0f, 0xFFFF6B32, 0.45f);
                            spawnParticles(m.x, m.y, 42, 0xFFFF6B32, 210.0f, 4, true);
                        }
                    }
                } else if (m.abilityActive <= 0.0f) {
                    m.abilityTimer -= dt;
                    if (m.abilityTimer <= 0.0f) {
                        m.abilityTimer = std::max(4.2f, 8.5f + (rand() % 180) / 100.0f - 1.15f * (m.bossPhase - 1) -
                                                            (m.bossReinforced ? 1.1f : 0.0f));
                        m.abilityWindup = 0.95f;
                        m.abilityTargetX = playerX;
                        m.abilityTargetY = playerY;
                        static const char* abilityLines[6] = {
                            "МОРГАР: ЗАМАХ — УДАРНАЯ ВОЛНА! УЙДИ ИЗ КРУГА!",
                            "МАЛАКОР: МЕТКА ЧУМЫ! ВЫЙДИ ИЗ ЗЕЛЁНОЙ ЗОНЫ!",
                            "КАРДИНАЛ: РЫВОК ПО ЛИНИИ! СМЕНИ ПОЗИЦИЮ!",
                            "АЗАРОТ: КОЛЬЦО ИСКР! НАЙДИ РАЗРЫВ!",
                            "ПРОТОКОЛИСТ: СТРОКА ПРОТОКОЛА! ВСТАНЬ В ПРОБЕЛ!",
                            "ДВОЙНИК: ОБМЕН МЕСТАМИ! НЕ СТОЙ НА ОСИ!"
                        };
                        announce(GMSG_BOSS_ABILITY, abilityLines[m.bossType % 6], 2.1f);
                        sound.play(SFX_BOSS_TELEGRAPH, 0.22f);
                    }
                }
            }

            float dx = playerX - m.x;
            float dy = playerY - m.y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (m.isRat) {
                // Rats hold their heading. They are not pursuers: they run across the field,
                // scurry around each other and leave. Contact still hurts.
                m.ratLife -= dt;
                // A rat runs straight with a small weave. The head swings, the path does not.
                const float weave = sinf(globalTime * 2.1f + m.ratPhase) * 0.055f;
                const float cosW = cosf(weave), sinW = sinf(weave);
                const float rx = m.ratDirX * cosW - m.ratDirY * sinW;
                const float ry = m.ratDirX * sinW + m.ratDirY * cosW;
                float speed = m.speed * (m.freezeTimer > 0.0f ? 0.4f : 1.0f);
                m.x += rx * speed * dt;
                m.y += ry * speed * dt;
                m.ratHeading = atan2f(m.ratDirY, m.ratDirX) + weave; // the body turns where it goes
                m.facing = rx >= 0.0f ? 1 : -1;
                // Крыса, убежавшая далеко от героя, больше не участвует в бою: иначе она
                // живёт до конца таймера за краем экрана и занимает место в лимите.
                const float awayX = fabsf(m.x - playerX), awayY = fabsf(m.y - playerY);
                const bool farAway = awayX > SCREEN_W * 0.5f + 180.0f || awayY > SCREEN_H * 0.5f + 180.0f;
                const bool tooFar = sqrtf(awayX * awayX + awayY * awayY) > std::max(SCREEN_W, SCREEN_H) * 0.9f;
                if (farAway && (m.ratLife <= 0.0f || tooFar)) { m.hp = 0.0f; }
                if (dist <= m.radius + 14.0f && dashTimer > 0.0f) registerDashEvade();
                if (dist <= m.radius + 14.0f && dashTimer <= 0.0f && playerHurtTimer <= 0.0f) {
                    float dmg = applyVeilMitigation(std::max(2.0f, m.damage * getSublevelEnemyDamageMultiplier() - getArmor()));
                    playerHp -= dmg;
                    registerRetaliationCharge(dmg);
                    notePlayerDamaged();
                    playerHurtTimer = 0.42f;
                    sound.play(SFX_HIT, 0.06f);
                }
                continue; // no chase, no shooting, no separation push
            }

            if (m.isBoss && !isWorldPointVisible(m.x, m.y, 58.0f)) {
                teleportBossTowardMovement(m);
                dx = playerX - m.x; dy = playerY - m.y;
                dist = sqrtf(dx * dx + dy * dy);
            }
            // Direction uses a dead zone so skeletons don't flip when nearly aligned with the player.
            if (dx < -12.0f) m.facing = -1;
            else if (dx > 12.0f) m.facing = 1;

            // Recycle distant regular enemies beyond the camera; never pop them onto the screen.
            if (!m.isBoss && dist > 1100.0f) {
                float offscreenX, offscreenY;
                if (chooseOffscreenSpawnPoint(650.0f, 64.0f, offscreenX, offscreenY)) { m.x = offscreenX; m.y = offscreenY; }
                continue;
            }

            if (m.isSmart && m.smartTactic == 2) {
                m.smartDodgeTimer = std::max(0.0f, m.smartDodgeTimer - dt);
                m.smartThinkTimer -= dt;
                if (m.smartThinkTimer <= 0.0f) {
                    m.smartThinkTimer = 0.11f; // Think at ~9 Hz, not once per projectile.
                    float bestMiss = m.radius + 26.0f;
                    for (const Projectile& p : projectiles) {
                        if (p.isEnemy || p.life <= 0.0f) continue;
                        float rx = p.x - m.x, ry = p.y - m.y;
                        float vv = p.vx*p.vx + p.vy*p.vy;
                        if (vv < 100.0f) continue;
                        float t = -(rx*p.vx + ry*p.vy) / vv;
                        if (t < 0.0f || t > 0.48f) continue;
                        float cx = rx + p.vx*t, cy = ry + p.vy*t;
                        float miss = sqrtf(cx*cx + cy*cy);
                        if (miss >= bestMiss) continue;
                        bestMiss = miss;
                        float invSpeed = 1.0f / sqrtf(vv);
                        float leftX = -p.vy * invSpeed, leftY = p.vx * invSpeed;
                        float awayX = m.x - playerX, awayY = m.y - playerY;
                        float side = leftX*awayX + leftY*awayY;
                        float sign = side >= 0.0f ? 1.0f : -1.0f;
                        m.smartDodgeX = leftX * sign;
                        m.smartDodgeY = leftY * sign;
                        m.smartDodgeTimer = 0.22f;
                    }
                }
            }

            float spdMult = ((m.freezeTimer > 0.0f) ? 0.35f : 1.0f) * getSublevelEnemySpeedMultiplier();
            // Blood Aura slows nearby monsters
            if (itemLevels[4] > 0 && dist < 110.0f * getAreaMultiplier()) {
                spdMult *= 0.72f;
            }

            float moveX = dx, moveY = dy;
            if (m.isSmart && m.smartTactic == 1) {
                // Intercept where the player is moving, with a bounded lead so dashes do not create huge jumps.
                float interceptTime = std::min(0.72f, dist / std::max(110.0f, m.speed + 80.0f));
                float leadX = playerVelocityX * interceptTime;
                float leadY = playerVelocityY * interceptTime;
                float leadLen = sqrtf(leadX*leadX + leadY*leadY);
                if (leadLen > 190.0f) { leadX *= 190.0f / leadLen; leadY *= 190.0f / leadLen; }
                moveX += leadX; moveY += leadY;
            } else if (m.isSmart && m.smartTactic == 2 && m.smartDodgeTimer > 0.0f) {
                moveX = m.smartDodgeX; moveY = m.smartDodgeY;
            }
            const float bossPhaseSpeed = m.isBoss ? (m.bossPhase == 1 ? 1.0f : (m.bossPhase == 2 ? 1.10f : 1.20f)) : 1.0f;
            float currentSpeed = m.speed * spdMult * bossPhaseSpeed;
            if (m.isBoss && m.bossType == 2 && m.abilityActive > 0.0f) {
                moveX = m.abilityDirX; moveY = m.abilityDirY;
                currentSpeed *= 3.2f;
            }
            float moveLen = sqrtf(moveX*moveX + moveY*moveY);
            if (moveLen > 1.0f) {
                m.x += (moveX / moveLen) * currentSpeed * dt;
                m.y += (moveY / moveLen) * currentSpeed * dt;
            }

            // Ranged attacks for Cultist/Bosses, plus rare predictive sharpshooters.
            bool smartShooter = m.isSmart && m.smartTactic == 0;
            if (((m.type == 3 || m.type == 8 || m.type == 9 || m.isBoss) || smartShooter) && dist < (smartShooter ? 700.0f : 500.0f)) {
                m.shootTimer -= dt;
                if (m.shootTimer <= 0.0f) {
                    m.shootTimer = smartShooter ? 1.85f : (m.isBoss ? std::max(0.72f, 1.35f - m.bossType * 0.12f - 0.18f * (m.bossPhase - 1)) : (m.type == 9 ? 2.7f : 3.4f));
                    // Fire patterns are per guardian, not one shared ring shot: a heavy
                    // bolt volley, a slow curse fan, a tight crescent, and a rotating spiral.
                    int shots = 1;
                    float spread = 0.12f;
                    float spiral = 0.0f;
                    if (m.isBoss) {
                        switch (m.bossType % 6) {
                            case 0: shots = 3; spread = 0.26f; break;  // Morgar: bolt volley
                            case 1: shots = 3; spread = 0.62f; break;  // Malakor: curse fan
                            case 2: shots = m.bossPhase >= 2 ? 2 : 1; spread = 0.13f; break; // Cardinal: single crescent
                            case 4: shots = 2; spread = 0.0f; break;   // Protocolist: parallel ruled lines
                            case 5: shots = m.bossPhase >= 2 ? 4 : 2; spread = 1.25f; break; // Double: mirrored pairs
                            default: shots = 2; spiral = 0.85f; break; // Azaroth: spiral
                        }
                        if (m.bossPhase >= 3 && (m.bossType % 6) != 2) ++shots;
                    }
                    float aimX = playerX, aimY = playerY;
                    if (smartShooter) {
                        float shotSpeedForLead = 220.0f;
                        float leadTime = std::min(0.65f, dist / shotSpeedForLead);
                        float leadX = playerVelocityX * leadTime, leadY = playerVelocityY * leadTime;
                        float leadLen = sqrtf(leadX*leadX + leadY*leadY);
                        if (leadLen > 190.0f) { leadX *= 190.0f / leadLen; leadY *= 190.0f / leadLen; }
                        aimX += leadX; aimY += leadY;
                    }
                    float baseA = atan2f(aimY - m.y, aimX - m.x);
                    for (int s = 0; s < shots; ++s) {
                        float a;
                        if (m.isBoss && spiral > 0.0f)
                            a = baseA + s * 0.62f + globalTime * spiral;   // leaves the spiral readable
                        else
                            a = baseA + (s - (shots - 1) * 0.5f) * (m.isBoss ? spread : 0.12f);
                        Projectile ep;
                        ep.x = m.x;
                        ep.y = m.y;
                        int kind = m.isBoss ? bossProjectileKind(m.bossType) :
                                   (smartShooter ? (m.smartTactic == 2 ? 2 : (m.smartTactic == 1 ? 1 : 0)) :
                                    (m.type == 8 ? 2 : (m.type == 9 ? 3 : (m.type == 3 ? 1 : 0))));
                        float shotSpeed = m.isBoss ? (230.0f + m.bossType * 18.0f) : (smartShooter ? 220.0f : 185.0f);
                        if (kind == 1) shotSpeed *= 1.48f; // bone needle: fast, narrow
                        if (kind == 2) shotSpeed *= 0.82f; // homing curse: slow but steering
                        if (kind == 3) shotSpeed *= 0.72f; // ash seed leaves a delayed ground hazard
                        if (m.isBoss && m.bossPhase >= 3) shotSpeed *= 1.20f;
                        ep.vx = cosf(a) * shotSpeed;
                        ep.vy = sinf(a) * shotSpeed;
                        ep.weaponId = 99; ep.projectileKind = kind; ep.isBossProjectile = m.isBoss;
                        ep.damage = m.damage * getSublevelEnemyDamageMultiplier() * (m.isBoss ? 0.76f : 0.72f);
                        if (kind == 3) ep.damage *= 0.72f;
                        ep.radius = kind == 1 ? 5.0f : (kind == 3 ? 9.0f : 7.0f);
                        ep.life = kind == 3 ? 2.1f : 2.8f;
                        ep.pierce = 1;
                        ep.isEnemy = true;
                        ep.isCrit = false;
                        projectiles.push_back(ep);
                    }
                    sound.play(SFX_PROJECTILE_FLIGHT, 0.16f);
                }
            }

            // Keep sparse groups readable; disable pairwise separation in dense endgame swarms.
            if (monsters.size() < 180) for (size_t j = i + 1; j < monsters.size(); ++j) {
                Monster& m2 = monsters[j];
                if (m2.hp <= 0.0f || m2.isResonator || m2.isRat || m.isRat) continue;
                float sx = m2.x - m.x;
                float sy = m2.y - m.y;
                float minD = (m.radius + m2.radius) * 0.78f;
                float d2 = sx * sx + sy * sy;
                if (d2 < minD * minD && d2 > 0.01f) {
                    float d = sqrtf(d2);
                    float push = (minD - d) * 0.35f;
                    float nx = sx / d, ny = sy / d;
                    if (!m.isBoss)  { m.x -= nx * push;  m.y -= ny * push; }
                    if (!m2.isBoss) { m2.x += nx * push; m2.y += ny * push; }
                }
            }

            // Contact-range collision is either a timed evade or ordinary damage.
            if (dist <= m.radius + 14.0f && dashTimer > 0.0f) registerDashEvade();
            if (dist <= m.radius + 14.0f && dashTimer <= 0.0f && playerHurtTimer <= 0.0f) {
                float dmg = applyVeilMitigation(std::max(2.0f, m.damage * getSublevelEnemyDamageMultiplier() - getArmor()));
                playerHp -= dmg;
                registerRetaliationCharge(dmg);
                notePlayerDamaged();
                playerHurtTimer = 0.42f;
                if (options.screenShakeEnabled) screenShake = 0.22f;
                sound.play(m.isBoss ? SFX_BOSS_IMPACT : SFX_HIT, m.isBoss ? 0.14f : 0.08f);
                char buf[32];
                snprintf(buf, sizeof(buf), "-%d HP", (int)roundf(dmg));
                addFloatingText(playerX, playerY - 24.0f, buf, 0xFFFF3850, true);
            }
        }
        monsters.erase(
            std::remove_if(monsters.begin(), monsters.end(), [](const Monster& m) { return m.hp <= 0.0f; }),
            monsters.end()
        );
        if (endingCutscenePending && !save.seenEndingCutscene) {
            endingCutscenePending = false;
            beginCutscene(5, STATE_PLAYING);
            return;
        }
        if (campaignEndPending && save.seenEndingCutscene) {
            campaignEndPending = false;
            recordRunEnd();
            sound.musicMode = 4;
            state = STATE_CAMPAIGN_END;
            return;
        }
        releaseMatryoshkaChildren();
        if (pendingSummons > 0) {
            int count = pendingSummons;
            pendingSummons = 0;
            for (int n = 0; n < count; ++n) spawnMonster(pendingSummonType, n == 0, false);
        }
        bool bossMusic = bossWarningTimer > 0.0f;
        for (const Monster& m : monsters) if (m.isBoss && m.hp > 0.0f) { bossMusic = true; break; }
        const int liveMusicCrowd = countActiveEnemies();
        // Crowd-driven tempo ladder with hysteresis: slow <40, medium 40–89,
        // fast 90–159, very fast 160+. Wider release thresholds prevent flicker.
        musicCrowdTempoTier=SoundEngine::nextMusicTempoTier(musicCrowdTempoTier,liveMusicCrowd);
        sound.musicTempoTier.store(musicCrowdTempoTier,std::memory_order_relaxed);
        if (breakthroughActive || liveMusicCrowd >= 90) hordeMusicActive = true;
        else if (liveMusicCrowd <= 55) hordeMusicActive = false; // hysteresis avoids a musical flicker near the threshold
        const int musicMood = bossMusic ? 2 : (victoryMusicTimer > 0.0f ? 4 : (hordeMusicActive ? 1 : (recoveryTimer > 0.0f ? 3 : 0)));
        sound.musicBossType.store(currentGameplayBossType(), std::memory_order_relaxed);
        sound.musicMode.store(musicMood, std::memory_order_relaxed);

        // Update Pickups (XP gems, gold, potions, magnets, boss chests)
        bool magnetPullingThisFrame = false;
        float pickRad = getPickupRadius();
        for (size_t i = 0; i < pickups.size(); ++i) {
            Pickup& p = pickups[i];
            p.bobTime += dt * 4.0f;
            p.age += dt;
            if ((p.type == 4 || p.type == 5) && p.age >= 60.0f) {
                p.value = 0;
                addFloatingText(p.x, p.y - 16.0f, "ЭССЕНЦИЯ РАССЕЯЛАСЬ", 0xFFB0A0C8, false);
                continue;
            }
            float dx = playerX - p.x;
            float dy = playerY - p.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (p.type == 7) {
                if (dist <= 36.0f) { p.value = 0; extractThroughPortal(); break; }
                continue;
            }

            if (p.type != 6 && dist <= pickRad) {
                p.magnetized = true;
            }
            if (p.magnetized && dist > 1.0f) {
                if (p.type <= 3) magnetPullingThisFrame = true;
                float pullSpd = 460.0f;
                p.x += (dx / dist) * pullSpd * dt;
                p.y += (dy / dist) * pullSpd * dt;
            }

            if (dist <= 24.0f) {
                if (p.type <= 2) {
                    // XP Gem
                    int gained = (int)roundf(p.value * getXpMultiplier());
                    playerXp += gained;
                    collectAshRhythmSpark();
                    sound.play(SFX_GEM, 0.22f);
                } else if (p.type == 3) {
                    // Gold Coin
                    const int goldValue = (int)roundf(p.value * getGoldMultiplier());
                    awardGold(goldValue);
                    sound.play(SFX_GEM, 0.22f);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "+%d ЗОЛ", p.value);
                    addFloatingText(playerX, playerY - 18.0f, buf, 0xFFFFD838, false);
                } else if (p.type == 4) {
                    // Healing Potion
                    restorePlayerHp(p.value);
                    sound.play(SFX_LEVELUP, 0.1f);
                    addFloatingText(playerX, playerY - 22.0f, "+35 ЗДОРОВЬЯ", 0xFF40FF70, true);
                } else if (p.type == 5) {
                    // Vacuum Magnet: pull ALL gems on map!
                    for (auto& other : pickups) {
                        if (other.type <= 3) other.magnetized = true;
                    }
                    sound.play(SFX_MAGNET, 0.75f);
                    magnetAudioTimer = 0.90f;
                    addFloatingText(playerX, playerY - 24.0f, "МАГНИТ ДУШ!",  0xFF50E8FF, true);
                } else if (p.type == 6) {
                    // Boss Chest!
                    p.value = 0;
                    openTreasureChest();
                    break;
                }
                p.value = 0; // Mark collected
            }
        }
        if (magnetPullingThisFrame && magnetAudioTimer <= 0.0f) {
            sound.play(SFX_MAGNET, 0.75f);
            magnetAudioTimer = 0.90f;
        }
        pickups.erase(
            std::remove_if(pickups.begin(), pickups.end(), [](const Pickup& p) { return p.value <= 0; }),
            pickups.end()
        );
        if (missionCompletePending && state == STATE_PLAYING &&
            std::none_of(pickups.begin(), pickups.end(), [](const Pickup& p) { return p.type == 6; })) {
            missionCompletePending = false;
            recordRunEnd();
            recordStoryMissionComplete();
            speakOnly(GMSG_STORY_CASE_CLOSED, "Дело закрыто. Следующее откроется само.");
            sound.musicMode = 4;
            state = STATE_MISSION_COMPLETE;
            return;
        }

        // Check Level Up
        if (state == STATE_PLAYING && playerXp >= xpToNext) {
            playerXp -= xpToNext;
            playerLevel++;
            if (playerLevel >= nextBreakthroughLevel) startBreakthrough();
            xpToNext = getXpRequirementForLevel(playerLevel);
            restorePlayerHp(12.0f);
            sound.play(SFX_LEVELUP, 0.05f);
            sound.playLevelUpCue(); // ss_levelup_ding.mp3 when the player ships one
            generateUpgradeChoices();
            state = STATE_LEVELUP;
        }

        // Phoenix relic offers one costly, single-use revival per run.
        if (playerHp <= 0.0f && itemLevels[14] > 0) {
            const int phoenixRank = itemLevels[14];
            itemLevels[14] = 0;
            playerHp = std::max(1.0f, playerMaxHp * std::min(0.70f, 0.30f + phoenixRank * 0.07f));
            playerHurtTimer = 2.1f + 0.12f * (phoenixRank - 1); dashCooldown = 0.0f; screenShake = 0.72f;
            phoenixReviveTimer = PHOENIX_REVIVE_DURATION;
            sound.play(SFX_REVIVE, 0.45f);
            spawnParticles(playerX, playerY, 110, 0xFFFFD66B, 245.0f, 4, true);
            spawnParticles(playerX, playerY, 48, 0xFFFFF3BF, 150.0f, 2, true);
            addShockwave(playerX, playerY, 230.0f, 0xFFFFD76A, 0.78f);
            addShockwave(playerX, playerY, 355.0f, 0xFFEF8CFF, 1.18f);
            addFloatingText(playerX, playerY - 52.0f, "ВОСКРЕШЕНИЕ ФЕНИКСА", 0xFFFFE48A, true);
            announce(GMSG_PHOENIX_REVIVE, "ПЕПЕЛ ФЕНИКСА: ВОСКРЕШЕНИЕ УСИЛЕНО РАНГОМ АРТЕФАКТА.", 5.0f);
        }
        // Dramatic defeat beat; stats are recorded immediately, then the game-over screen follows.
        if (playerHp <= 0.0f) {
            playerHp = 0.0f;
            recordRunEnd();
            playerDeathTimer = 1.6f;
            screenShake = 0.95f;
            sound.musicMode = 3;
            sound.play(SFX_PLAYER_DEATH, 0.01f);
            spawnParticles(playerX, playerY, 95, 0xFFFF4058, 210.0f, 5, true);
            spawnParticles(playerX, playerY, 48, 0xFFB060FF, 125.0f, 3, true);
            addShockwave(playerX, playerY, 190.0f, 0xFFFF4058, 0.9f);
            addShockwave(playerX, playerY, 285.0f, 0xFF6A40A8, 1.35f);
            announce(GMSG_PLAYER_DEATH, gameMode == MODE_STORY ? "ТЕЛО ПАЛО. КУЗНИЦА СОХРАНИЛА ПАМЯТЬ И УЖЕ НАЙДЕННЫЕ УЛИКИ." : "ТВОЯ ИСКРА ПОГАСЛА...", playerDeathTimer);
            state = STATE_DEATH;
        }
    }

    void updateEffects(float dt) {
        for (auto& p : particles) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.vx *= p.style == 4 ? 0.975f : 0.94f;
            p.vy *= p.style == 4 ? 0.975f : 0.94f;
            p.rotation += p.spin * dt;
            if (p.style == 4) p.scale = std::min(1.65f, p.scale + dt * 0.95f);
            else p.scale = std::max(0.40f, p.scale - dt * (p.style == 2 ? 0.9f : 0.48f));
            p.life -= dt;
        }
        particles.erase(
            std::remove_if(particles.begin(), particles.end(), [](const Particle& p) { return p.life <= 0.0f; }),
            particles.end()
        );

        for (auto& ft : floatTexts) {
            ft.y += ft.vy * dt;
            ft.life -= dt;
        }
        floatTexts.erase(
            std::remove_if(floatTexts.begin(), floatTexts.end(), [](const FloatingText& f) { return f.life <= 0.0f; }),
            floatTexts.end()
        );

        for (auto& l : lightningArcs) l.life -= dt;
        lightningArcs.erase(
            std::remove_if(lightningArcs.begin(), lightningArcs.end(), [](const LightningArc& l) { return l.life <= 0.0f; }),
            lightningArcs.end()
        );

        for (auto& fw : frostWaves) {
            fw.life -= dt;
            float t = 1.0f - (fw.life / fw.maxLife);
            fw.radius = 20.0f + (fw.maxRadius - 20.0f) * t;
        }
        frostWaves.erase(
            std::remove_if(frostWaves.begin(), frostWaves.end(), [](const FrostWave& f) { return f.life <= 0.0f; }),
            frostWaves.end()
        );
        for (auto& w : shockwaves) {
            w.life -= dt;
            float progress = 1.0f - w.life / w.maxLife;
            w.radius = 18.0f + (w.maxRadius - 18.0f) * progress;
        }
        shockwaves.erase(
            std::remove_if(shockwaves.begin(), shockwaves.end(), [](const Shockwave& w) { return w.life <= 0.0f; }),
            shockwaves.end()
        );
        for (auto& mark : skillImpactMarks) mark.life -= dt;
        skillImpactMarks.erase(
            std::remove_if(skillImpactMarks.begin(), skillImpactMarks.end(), [](const SkillImpactMark& mark) { return mark.life <= 0.0f; }),
            skillImpactMarks.end()
        );
    }

    int currentGameplayBossType() const {
        for (const Monster& m : monsters) if (m.isBoss && m.hp > 0.0f && m.isTrioBoss) return 3; // the finale uses Azarot's archive motif
        for (const Monster& m : monsters) if (m.isBoss && m.hp > 0.0f) return m.bossType % 6;
        return 0;
    }

    int currentGameplayMusicMode() const {
        bool bossMusic = bossWarningTimer > 0.0f;
        for (const Monster& m : monsters) if (m.isBoss && m.hp > 0.0f) { bossMusic = true; break; }
        return bossMusic ? 2 : (victoryMusicTimer > 0.0f ? 4 : (hordeMusicActive ? 1 : (recoveryTimer > 0.0f ? 3 : 0)));
    }

    void restoreGameplayMusicImmediately() {
        const int mode = currentGameplayMusicMode();
        sound.musicBossType.store(currentGameplayBossType(), std::memory_order_relaxed);
        sound.musicMode.store(mode, std::memory_order_relaxed);
        sound.requestImmediateMusicChange(mode, 0);
    }

    void update(float dt) {
        if (dt > 0.0f) {
            const float ms = dt * 1000.0f;
            frameMsSmooth += (ms - frameMsSmooth) * 0.10f;
            const float fps = 1000.0f / std::max(1.0f, frameMsSmooth);
            fpsSmooth += (fps - fpsSmooth) * 0.10f;
            debugPeakEntities = std::max(debugPeakEntities, (int)monsters.size());
        }
        if (dt > 0.05f) dt = 0.05f;
        if (hitFxWindowTimer > 0.0f) {
            hitFxWindowTimer = std::max(0.0f, hitFxWindowTimer - dt);
            if (hitFxWindowTimer <= 0.0f) { hitFxCountInWindow = 0; hitFxClusterEmitted = 0; }
        }
        globalTime += dt;
        sound.update(dt);
        // Отладка замораживает бой: пока выбираешь пункт, тебя не должны убить.
        // Клик мышью здесь не сбрасываем — его читает render() в этом же кадре.
        if (cheatMenuOpen) return;
        const bool menuMusicState = state == STATE_MENU || state == STATE_HERO_SELECT || state == STATE_MISSION_SELECT || state == STATE_SHOP || state == STATE_HELP ||
                                    state == STATE_SETTINGS || state == STATE_HALL || state == STATE_PATCH_NOTES;
        // The Soul Forge reuses menu mode; the hint lets audio.h pick ss_forge_shop.mp3 there.
        sound.musicSceneHint.store(state == STATE_SHOP ? 1 : 0, std::memory_order_relaxed);
        // A level-up card must not compete with the battle bed: 80 % quieter while it is open.
        sound.musicSceneGain.store(state == STATE_LEVELUP ? 0.20f : 1.0f, std::memory_order_relaxed);
        if (state == STATE_LEVELUP) sound.musicMode = 6;
        else if (state == STATE_GAMEOVER) sound.musicMode = 7;
        else if (state == STATE_MISSION_COMPLETE) sound.musicMode = 4;
        else if (state == STATE_BOSS_MUSIC_TEST) {
            sound.musicBossType.store(currentGameplayBossType(), std::memory_order_relaxed);
            sound.musicMode.store(bossTestMusicMode, std::memory_order_relaxed);
        }
        else if (menuMusicState) sound.musicMode = 5;

        if (state == STATE_CUTSCENE) {
            cutsceneTimer += dt;
            updateEffects(dt);
            if (cutsceneTimer >= cutsceneDuration(cutsceneId)) finishCutscene();
        } else if (state == STATE_PATCH_NOTES) {
            updateEffects(dt);
        } else if (state == STATE_PLAYING) {
            updatePlaying(dt);
            releaseResonatorBurst();
            updateEffects(dt);
        } else if (state == STATE_DEATH) {
            playerDeathTimer = std::max(0.0f, playerDeathTimer - dt);
            screenShake = std::max(0.0f, screenShake - dt);
            updateEffects(dt);
            if (playerDeathTimer <= 0.0f) state = STATE_GAMEOVER;
        } else if (state == STATE_MENU || state == STATE_HERO_SELECT || state == STATE_MISSION_SELECT || state == STATE_MISSION_COMPLETE) {
            // Keep campaign menus and completion screens atmospheric without advancing a run.
            updateEffects(dt);
        } else if (state == STATE_BOSS_MUSIC_TEST) {
            // Actors are frozen so this is a safe music audition, not a progression run.
            updateEffects(dt);
        }
    }

    // =========================================================================
    // WORLD & HUD RENDERING
    // =========================================================================

    void renderWorld(Renderer& r, float camX, float camY) {
        const int themeIndex = currentArenaTheme();
        // 1. Tiled Dark Fantasy Cobblestone Floor + Glowing Rune Circles & Torches
        int tileSize = 64;
        int startTileX = (int)floorf(camX / tileSize) - 1;
        int startTileY = (int)floorf(camY / tileSize) - 1;
        int endTileX = startTileX + (SCREEN_W / tileSize) + 3;
        int endTileY = startTileY + (SCREEN_H / tileSize) + 3;

        for (int ty = startTileY; ty <= endTileY; ++ty) {
            for (int tx = startTileX; tx <= endTileX; ++tx) {
                int sx = tx * tileSize - (int)camX;
                int sy = ty * tileSize - (int)camY;
                uint32_t h = (uint32_t)(tx * 73856093 ^ ty * 19349663);
                const ArenaTheme& theme = ARENA_THEMES[themeIndex];
                uint32_t baseCol = ((tx + ty) & 1) ? theme.floorA : theme.floorB;
                if ((h & 7) == 0) baseCol = theme.floorDetail;
                r.fillRect(sx, sy, tileSize, tileSize, baseCol);
                // Mortar follows the place: cold violet grout in the ruins, soot in the ash.
                r.drawRect(sx, sy, tileSize, tileSize, theme.mortar, 1);
                // Cut-stone bevel on a third of the tiles: the floor reads as masonry, not noise.
                if ((h & 3) == 1) {
                    r.drawLine(sx + 2, sy + 2, sx + tileSize - 3, sy + 2, theme.floorDetail, 1, 130);
                    r.drawLine(sx + 2, sy + 2, sx + 2, sy + tileSize - 3, theme.floorDetail, 1, 110);
                }
                // Each place paves its floor differently — this is the first thing that tells
                // the player "new case, new place" before a single enemy appears.
                switch (theme.propKind) {
                    case 0: // Руины: hairline cracks running through the slabs.
                        if ((h & 7) == 2) {
                            r.drawLine(sx + 12, sy + 8, sx + 30, sy + 26, theme.mortar, 1, 150);
                            r.drawLine(sx + 30, sy + 26, sx + 44, sy + 22, theme.mortar, 1, 120);
                        }
                        break;
                    case 1: // Крипта: sunken tomb slabs with a shallow groove.
                        if ((h & 7) == 5) {
                            r.fillRect(sx + 8, sy + 12, tileSize - 16, tileSize - 24, theme.floorDetail, 190);
                            r.drawRect(sx + 8, sy + 12, tileSize - 16, tileSize - 24, theme.mortar, 1, 160);
                            r.drawLine(sx + tileSize / 2, sy + 18, sx + tileSize / 2, sy + tileSize - 18, theme.mortar, 1, 120);
                        }
                        break;
                    case 2: // Собор: polished marble with a pale inlay line.
                        if ((h & 3) == 0) r.drawLine(sx + 4, sy + tileSize - 6, sx + tileSize - 6, sy + 4, theme.floorDetail, 2, 150);
                        break;
                    case 3: // Пепельный трон: scorched slabs and a glowing seam.
                        if ((h & 7) == 3) {
                            r.fillRoundedRect(sx + 10, sy + 14, 30, 18, 6, 0xFF160D09, 210);
                            if ((h & 32) == 0) r.drawLine(sx + 16, sy + 22, sx + 38, sy + 28, theme.flame, 1, 130, true);
                        }
                        break;
                    default: // Сердце Архива: ruled catalogue squares.
                        if ((h & 7) == 6) {
                            r.drawRect(sx + 10, sy + 10, tileSize - 20, tileSize - 20, theme.runeOuter, 1, 90);
                            r.drawLine(sx + 16, sy + 22, sx + tileSize - 16, sy + 22, theme.runeInner, 1, 60);
                        }
                        break;
                }
                // Ground clutter: rubble, bones, candle stubs, ash, loose folios.
                if ((h & 15) == 9) {
                    const int cx0 = sx + 12 + (int)((h >> 5) % 34);
                    const int cy0 = sy + 14 + (int)((h >> 9) % 30);
                    switch (theme.propKind) {
                        case 0:
                            r.fillCircle(cx0, cy0, 3, theme.floorDetail);
                            r.fillCircle(cx0 + 7, cy0 + 3, 2, theme.mortar);
                            break;
                        case 1:
                            r.fillRect(cx0 - 5, cy0 - 1, 11, 2, 0xFFB6AC98, 220);
                            r.fillRect(cx0 - 2, cy0 - 4, 2, 6, 0xFFB6AC98, 200);
                            break;
                        case 2:
                            r.fillRect(cx0 - 3, cy0 - 6, 6, 10, 0xFF8FE8FF, 120);
                            break;
                        case 3:
                            r.fillRoundedRect(cx0 - 6, cy0 - 3, 13, 6, 3, 0xFF3A2318, 220);
                            break;
                        default:
                            r.fillRect(cx0 - 4, cy0 - 5, 9, 10, 0xFFDCD8F0, 190);
                            r.drawRect(cx0 - 4, cy0 - 5, 9, 10, theme.runeOuter, 1, 120);
                            break;
                    }
                }

                // World props must never be stacked on each other. A seal keeps its tile only
                // when it wins the hash against its right/bottom neighbours, so seals always
                // sit at least two tiles apart instead of clumping into mush.
                auto sealWins = [](int cx, int cy) {
                    const uint32_t own = (uint32_t)(cx * 73856093 ^ cy * 19349663);
                    if ((own & 31u) != 7u) return false;
                    for (int oy = 0; oy <= 1; ++oy)
                        for (int ox = 0; ox <= 1; ++ox) {
                            if (!ox && !oy) continue;
                            const uint32_t other = (uint32_t)((cx + ox) * 73856093 ^ (cy + oy) * 19349663);
                            if ((other & 31u) == 7u) return false;
                        }
                    return true;
                };
                // Moss / stone detail patches
                if ((h & 15) == 3) {
                    static const uint32_t groundMarks[5] = {0xFF263326, 0xFF51202A, 0xFF24505A, 0xFF59301A, 0xFF2C2A66};
                    r.fillRoundedRect(sx + 14, sy + 18, 22, 14, 4, groundMarks[themeIndex]);
                } else if (sealWins(tx, ty)) {
                    // Ancient floor seal: independently phased, breathing rings with radial cuts and a rotating glyph.
                    const int rcx = sx + 32, rcy = sy + 32;
                    const uint32_t runeOuter = theme.runeOuter;
                    const uint32_t runeInner = theme.runeInner;
                    const float seedPhase = (float)(h & 1023u) * 0.0061359f;
                    const float phase = globalTime * 0.72f + seedPhase;
                    const float counterPhase = -globalTime * 0.46f + seedPhase * 0.7f;
                    const int breath = (int)(sinf(globalTime * 1.65f + seedPhase) * 2.0f);
                    r.addGlow(rcx, rcy, 24 + breath, runeOuter, 22);
                    r.drawCircleRing(rcx, rcy, 25 + breath, 2, runeOuter, 92, true);
                    r.drawCircleRing(rcx, rcy, 17 - breath / 2, 1, runeInner, 125, true);
                    r.drawCircleRing(rcx, rcy, 8 + (int)(sinf(globalTime * 2.0f + seedPhase) * 1.0f), 1, runeOuter, 145, true);
                    for (int tick = 0; tick < 12; ++tick) {
                        const float a = phase + tick * 0.5235988f;
                        const float r1 = (tick % 3 == 0) ? 18.0f : 20.0f;
                        const float r2 = (tick % 3 == 0) ? 27.0f : 24.0f;
                        r.drawLine(rcx + (int)(cosf(a) * r1), rcy + (int)(sinf(a) * r1),
                                   rcx + (int)(cosf(a) * r2), rcy + (int)(sinf(a) * r2),
                                   runeInner, tick % 3 == 0 ? 2 : 1, tick % 3 == 0 ? 135 : 88, true);
                    }
                    int gx[4], gy[4];
                    for (int point = 0; point < 4; ++point) {
                        const float a = counterPhase + point * 1.5707963f;
                        gx[point] = rcx + (int)(cosf(a) * (point % 2 == 0 ? 12.0f : 6.0f));
                        gy[point] = rcy + (int)(sinf(a) * (point % 2 == 0 ? 12.0f : 6.0f));
                    }
                    for (int point = 0; point < 4; ++point)
                        r.drawLine(gx[point], gy[point], gx[(point + 1) % 4], gy[(point + 1) % 4], runeInner, 1, 105, true);
                }
            }
        }

        // Landmarks and braziers live on two independent grids, so they used to land on top of
        // each other. Both loops ask the same helper and skip when the other prop is too close.
        auto landmarkAt = [](int gx, int gy, int& wx, int& wy) {
            const uint32_t seed = (uint32_t)(gx * 1664525u) ^ (uint32_t)(gy * 1013904223u);
            if ((seed & 3u) != 0u) return false;
            wx = gx * 320 + 70 + (int)((seed >> 8) % 180);
            wy = gy * 320 + 70 + (int)((seed >> 16) % 180);
            return true;
        };
        auto landmarkNear = [&](int wx, int wy, int minDist) {
            const int cx = wx / 320, cy = wy / 320;
            for (int oy = -1; oy <= 1; ++oy)
                for (int ox = -1; ox <= 1; ++ox) {
                    int lx = 0, ly = 0;
                    if (!landmarkAt(cx + ox, cy + oy, lx, ly)) continue;
                    if ((lx - wx) * (lx - wx) + (ly - wy) * (ly - wy) < minDist * minDist) return true;
                }
            return false;
        };
        auto torchNear = [&](int wx, int wy, int minDist) {
            const int cx = (wx - 192) / 384, cy = (wy - 192) / 384;
            for (int oy = -1; oy <= 1; ++oy)
                for (int ox = -1; ox <= 1; ++ox) {
                    const int tx = (cx + ox) * 384 + 192, ty = (cy + oy) * 384 + 192;
                    if ((tx - wx) * (tx - wx) + (ty - wy) * (ty - wy) < minDist * minDist) return true;
                }
            return false;
        };

        // Flickering Braziers / Torches on a larger grid (every 384 px)
        int torchGrid = 384;
        int btx0 = (int)floorf(camX / torchGrid) - 1;
        int bty0 = (int)floorf(camY / torchGrid) - 1;
        for (int bty = bty0; bty <= bty0 + 4; ++bty) {
            for (int btx = btx0; btx <= btx0 + 5; ++btx) {
                int wx = btx * torchGrid + 192;
                int wy = bty * torchGrid + 192;
                int sx = wx - (int)camX;
                int sy = wy - (int)camY;
                if (sx < -120 || sx > SCREEN_W + 120 || sy < -120 || sy > SCREEN_H + 120) continue;
                if (landmarkNear(wx, wy, 132)) continue; // a landmark already owns this spot
                int flicker = (int)(sinf(globalTime * 11.0f + btx * 3.1f + bty * 1.7f) * 10.0f);
                const ArenaTheme& theme = ARENA_THEMES[themeIndex];
                const uint32_t flame = theme.flame;
                r.addGlow(sx, sy - 10, 95 + flicker, flame, 110);
                r.drawShadow(sx, sy + 12, 12, 5);
                r.fillRoundedRect(sx - 7, sy - 4, 14, 16, 3, 0xFF3E384A);
                r.fillCircle(sx, sy - 9, 7, flame);
                r.fillCircle(sx, sy - 11, 4, theme.flameCore);
            }
        }

        // Biome landmarks on a seeded world grid: charred trees, bone cairns, ice monoliths and ruined pillars.
        int landmarkCell = 320;
        int lx0 = (int)floorf(camX / landmarkCell) - 1;
        int ly0 = (int)floorf(camY / landmarkCell) - 1;
        // Each place has its own landmark: a broken arch, a bone cairn, a glass pillar in a
        // shaft of light, a charred obelisk, or a stack of archive folios.
        for (int gy = ly0; gy <= ly0 + SCREEN_H / landmarkCell + 2; ++gy) {
            for (int gx = lx0; gx <= lx0 + SCREEN_W / landmarkCell + 2; ++gx) {
                int wx = 0, wy = 0;
                if (!landmarkAt(gx, gy, wx, wy)) continue;
                if (torchNear(wx, wy, 132)) continue; // a brazier already owns this spot
                int x = wx - (int)camX, y = wy - (int)camY;
                if (x < -60 || x > SCREEN_W + 60 || y < -90 || y > SCREEN_H + 60) continue;
                const ArenaTheme& theme = ARENA_THEMES[themeIndex];
                if (theme.propKind == 0) {
                    // Руины: a collapsed arch, its keystone still wedged in place.
                    r.fillRect(x - 26, y - 26, 12, 44, 0xFF4B4058);
                    r.fillRect(x + 14, y - 26, 12, 44, 0xFF4B4058);
                    r.fillRect(x - 30, y - 34, 60, 9, 0xFF5F5270);
                    r.drawRect(x - 26, y - 26, 12, 44, theme.mortar, 1);
                    r.drawRect(x + 14, y - 26, 12, 44, theme.mortar, 1);
                    r.fillRoundedRect(x - 8, y + 8, 16, 8, 2, 0xFF2A3A2A); // moss at the foot
                    r.drawCircleRing(x, y - 6, 15, 1, theme.runeOuter, 70);
                } else if (theme.propKind == 1) {
                    // Крипта: a cairn of skulls and ribs.
                    r.fillCircle(x - 7, y + 2, 8, 0xFFD9CFB8);
                    r.fillCircle(x + 8, y + 4, 7, 0xFFC8BCA4);
                    r.fillCircle(x + 1, y - 12, 8, 0xFFE4DBC6);
                    r.fillRect(x - 2, y - 10, 3, 3, 0xFF231A1A);
                    r.fillRect(x + 4, y - 10, 3, 3, 0xFF231A1A);
                    r.fillRect(x - 18, y + 10, 36, 3, 0xFF9C9280);
                    r.fillRect(x - 14, y + 15, 28, 3, 0xFF887E6E);
                    r.addGlow(x, y - 4, 26, theme.runeOuter, 40);
                } else if (theme.propKind == 2) {
                    // Собор: a stained-glass pillar with a cold shaft of light above it.
                    r.fillRect(x - 12, y - 46, 24, 66, 0xFF1B3A48);
                    r.drawRect(x - 12, y - 46, 24, 66, 0xFF2F6C84, 2);
                    for (int pane = 0; pane < 4; ++pane) {
                        const uint32_t glass[4] = {0xFF3FA8C8, 0xFF68D8F0, 0xFF3F7AC8, 0xFF7FE8FF};
                        r.fillRect(x - 8, y - 42 + pane * 16, 16, 11, glass[pane], 190);
                    }
                    r.addGlow(x, y - 30, 46, theme.flame, 58);
                    r.fillRect(x - 16, y + 20, 32, 6, 0xFF20424F);
                    for (int streak = 0; streak < 3; ++streak)
                        r.drawLine(x - 22 + streak * 8, y - 54, x - 34 + streak * 12, y - 96,
                                   theme.runeInner, 2, 26, true);
                } else if (theme.propKind == 3) {
                    // Пепельный трон: a charred obelisk with a burning seam.
                    r.fillRect(x - 11, y - 40, 22, 60, 0xFF3A271C);
                    r.fillRect(x - 15, y - 46, 30, 9, 0xFF543725);
                    r.drawRect(x - 11, y - 40, 22, 60, 0xFF120C08, 2);
                    r.drawLine(x, y - 34, x, y + 12, theme.flame, 2, 150, true);
                    r.addGlow(x, y + 16, 32, theme.flame, 52);
                    for (int crack = 0; crack < 3; ++crack)
                        r.drawLine(x - 7 + crack * 7, y - 20 + crack * 9, x - 2 + crack * 7, y - 8 + crack * 9,
                                   0xFF8A4A20, 1, 140);
                } else {
                    // Сердце Архива: stacked folios and a floating index tablet.
                    for (int shelf = 0; shelf < 3; ++shelf)
                        r.fillRect(x - 20, y + 16 - shelf * 9, 40, 8, 0xFF2A2560, 245);
                    for (int book = 0; book < 6; ++book) {
                        const uint32_t spine[3] = {0xFF6E7BE8, 0xFF8C6EE8, 0xFF4E6EC8};
                        r.fillRect(x - 18 + book * 6, y - 12, 5, 12, spine[book % 3]);
                    }
                    r.addGlow(x, y - 26, 34, theme.runeOuter, 58);
                    r.drawCircleRing(x, y - 26, 13, 1, theme.runeInner, 150, true);
                    r.drawLine(x - 9, y - 26, x + 9, y - 26, theme.runeInner, 1, 130, true);
                }
            }
        }

        // Ambient air: dust in the ruins, sifting ash in the crypt, rising motes in the
        // cathedral, embers in the ash throne, drifting pages in the archive heart.
        {
            const ArenaTheme& theme = ARENA_THEMES[themeIndex];
            const float spanX = (float)SCREEN_W + 120.0f, spanY = (float)SCREEN_H + 120.0f;
            for (int i = 0; i < theme.moteCount; ++i) {
                const uint32_t seed = (uint32_t)(i * 2654435761u);
                const float phase = (float)(seed & 1023u) / 1024.0f;
                const float drift = theme.moteSpeed * (0.6f + phase * 0.8f);
                float my = fmodf(phase * spanY + globalTime * drift, spanY);
                if (my < 0.0f) my += spanY;
                const float mx = fmodf(phase * spanX * 1.7f + sinf(globalTime * 0.4f + phase * 6.28f) * 26.0f
                                       + globalTime * 9.0f * (phase - 0.5f), spanX);
                const int px = (int)mx - 60, py = (int)my - 60;
                const int twinkle = 40 + (int)(38.0f * (0.5f + 0.5f * sinf(globalTime * 1.6f + phase * 9.0f)));
                if (theme.propKind == 4) {
                    // Pages: small pale rectangles that tumble as they drift.
                    const int tilt = (int)(sinf(globalTime * 1.2f + phase * 7.0f) * 3.0f);
                    r.fillRoundedRect(px, py + tilt, 7, 9, 2, theme.mote, twinkle / 2);
                } else if (theme.propKind == 3) {
                    r.fillRect(px, py, 2, 3, theme.mote, twinkle);
                    r.fillCircle(px, py, 1, theme.flame, twinkle);
                } else {
                    r.fillCircle(px, py, theme.propKind == 2 ? 2 : 1, theme.mote, twinkle);
                }
            }
            // A wide, very soft wash of the place's own light ties the palette together.
            r.fillRect(0, 0, SCREEN_W, SCREEN_H, theme.lightTint, 16);
        }

        int pScreenX = (int)(playerX - camX);
        int pScreenY = (int)(playerY - camY);

        // 2. Blood Aura (Weapon 4) under player
        if (itemLevels[4] > 0) {
            float auraRad = (88.0f + itemLevels[4] * 11.0f) * getAreaMultiplier();
            int pulse = (int)(sinf(auraPulse) * 4.0f);
            r.addGlow(pScreenX, pScreenY, (int)auraRad, 0xFF901834, 75);
            r.drawCircleRing(pScreenX, pScreenY, (int)auraRad + pulse, 3, 0xFFFF2858, 190, true);
            r.drawCircleRing(pScreenX, pScreenY, (int)(auraRad * 0.68f) - pulse, 2, 0xFFFF6888, 130, true);
        }

        // 3. Frost Nova ground seal: layered ice rings, a rotating 12-notch dial and four moving shards.
        for (const auto& fw : frostWaves) {
            const int fx = (int)(fw.x - camX);
            const int fy = (int)(fw.y - camY);
            const float progress = std::max(0.0f, std::min(1.0f, 1.0f - fw.life / std::max(0.001f, fw.maxLife)));
            const int alpha = (int)((fw.life / std::max(0.001f, fw.maxLife)) * 220.0f);
            const int radius = std::max(6, (int)fw.radius);
            const int innerRadius = std::max(4, (int)(radius * (0.72f - progress * 0.08f)));
            const float rotation = globalTime * 2.8f;
            r.fillCircle(fx, fy, radius, 0xFF36B9E8, alpha / 15);
            r.addGlow(fx, fy, radius, 0xFF50DFFF, alpha / 3);
            r.drawCircleRing(fx, fy, radius, 4, 0xFF60F0FF, alpha, true);
            r.drawCircleRing(fx, fy, std::max(4, radius - 7), 1, 0xFFB8FAFF, alpha * 3 / 4, true);
            r.drawCircleRing(fx, fy, innerRadius, 2, 0xFF45B9FF, alpha * 2 / 3, true);
            for (int mark = 0; mark < 12; ++mark) {
                const float a = rotation + mark * 0.5235988f;
                const float r0 = (mark % 3 == 0) ? innerRadius * 0.84f : innerRadius * 0.92f;
                const float r1 = (mark % 3 == 0) ? radius * 0.90f : radius * 0.80f;
                r.drawLine(fx + (int)(cosf(a) * r0), fy + (int)(sinf(a) * r0),
                           fx + (int)(cosf(a) * r1), fy + (int)(sinf(a) * r1),
                           mark % 3 == 0 ? 0xFFD6FFFF : 0xFF65DFFF,
                           mark % 3 == 0 ? 2 : 1, mark % 3 == 0 ? alpha : alpha * 2 / 3, true);
            }
            for (int shard = 0; shard < 4; ++shard) {
                const float a = -rotation * 0.68f + shard * 1.5707963f;
                const int sx = fx + (int)(cosf(a) * innerRadius * 0.52f);
                const int sy = fy + (int)(sinf(a) * innerRadius * 0.52f);
                const int tx = (int)(cosf(a + 0.72f) * 5.0f);
                const int ty = (int)(sinf(a + 0.72f) * 5.0f);
                r.drawLine(sx - tx, sy - ty, sx + tx, sy + ty, 0xFFE5FFFF, 2, alpha, true);
            }
        }

        // Boss ability zones: warning circles remain visible before they become dangerous.
        for (const auto& hazard : bossHazards) {
            const int hx = (int)(hazard.x - camX), hy = (int)(hazard.y - camY);
            const uint32_t color = hazard.hazardType == 0 ? 0xFF6DDB58 : (hazard.hazardType == 1 ? 0xFFFF6A38 : 0xFF5CE8FF);
            if (hazard.warningTimer > 0.0f) {
                const int pulse = (int)(sinf(globalTime * 18.0f) * 5.0f);
                r.fillCircle(hx, hy, (int)hazard.radius, color, 38);
                r.drawCircleRing(hx, hy, (int)hazard.radius + pulse, 4, color, 220, true);
            } else {
                const int alpha = (int)(90.0f + 45.0f * sinf(globalTime * 8.0f));
                r.fillCircle(hx, hy, (int)hazard.radius, color, alpha);
                r.drawCircleRing(hx, hy, (int)hazard.radius, 3, color, 220, true);
            }
        }

        // Expanding dramatic rings for boss arrival/death and hero defeat.
        for (const auto& wave : shockwaves) {
            int sx = (int)(wave.x - camX), sy = (int)(wave.y - camY);
            int alpha = std::min(255, (int)(255.0f * wave.life / wave.maxLife));
            r.addGlow(sx, sy, std::max(8, (int)wave.radius), wave.color, alpha / 2);
            r.drawCircleRing(sx, sy, (int)wave.radius, 5, wave.color, alpha, true);
        }

        // 4. Pickups (Gems, Gold, Potions, Magnets, Boss Chests)
        for (const auto& p : pickups) {
            if ((p.type == 4 || p.type == 5) && p.age > 50.0f && ((int)(p.age * 8.0f) % 2 == 0)) continue;
            int sx = (int)(p.x - camX);
            int sy = (int)(p.y - camY + sinf(p.bobTime) * 3.0f);
            if (sx < -40 || sx > SCREEN_W + 40 || sy < -40 || sy > SCREEN_H + 40) continue;

            static const uint32_t pickupGlow[8] = {
                0xFF30A8FF,0xFF30FF80,0xFFD040FF,0xFFFFC820,
                0xFFFF3050,0xFF40D8FF,0xFFFFC830,0xFF45DFFF
            };
            static const int pickupGlowRadius[8] = {12,14,18,14,18,20,36,50};
            const int pickupId = std::max(0, std::min(7, p.type));
            r.addGlow(sx, sy, pickupGlowRadius[pickupId], pickupGlow[pickupId], pickupId == 7 ? 150 : 105);
            if (pickupId == 6) r.drawShadow(sx, sy + 14, 16, 6);
            if (pickupId == 7) {
                const int pulse = 27 + (int)(4.0f * sinf(globalTime * 3.2f));
                r.drawCircleRing(sx, sy, pulse, 3, 0xFF64E9FF, 215, true);
                r.drawCircleRing(sx, sy, pulse - 8, 2, 0xFFBC80FF, 205, true);
            }
            r.drawPickupSprite(pickupId, sx, sy, globalTime + p.bobTime);
            if (pickupId == 7)
                r.drawTextCentered(FONT_SMALL, sx, sy - 38, "ПОРТАЛ • ВОЙТИ", 0xFFB6F7FF, true);
        }

        // Ground-plane skill impacts are intentionally beneath actors and culled off-screen.
        renderSkillImpactMarks(r, camX, camY);

        // 5. Monsters & Bosses
        for (const auto& m : monsters) {
            int sx = (int)(m.x - camX);
            int sy = (int)(m.y - camY);
            if (sx < -60 || sx > SCREEN_W + 60 || sy < -60 || sy > SCREEN_H + 60) continue;

            if (m.isResonator) {
                // Animated aura stays dynamic; the crystal/bronze/stone silhouette is a baked pixel sprite.
                r.drawShadow(sx, sy + 28, 30, 9);
                const int pulse = 2 + (int)(2.0f * (1.0f + sinf(globalTime * 4.2f)));
                const uint32_t core = m.hitFlashTimer > 0.0f ? 0xFFFFFFFF : 0xFFD878FF;
                r.addGlow(sx, sy - 4, 43 + pulse, 0xFFB94CFF, 105);
                r.drawCircleRing(sx, sy - 5, 21 + pulse, 2, 0xFFE3B8FF, 185, true);
                r.drawCircleRing(sx, sy - 5, 30 - pulse, 1, 0xFF9D6BE8, 145, true);
                r.drawResonatorSprite(sx, sy, globalTime, m.hitFlashTimer > 0.0f);
                const int hpw = 64; r.fillRect(sx - hpw/2, sy - 42, hpw, 5, 0xFF130D1D);
                r.fillRect(sx - hpw/2, sy - 42, (int)(hpw * std::max(0.0f, m.hp / m.maxHp)), 5, 0xFFCF75FF);
                r.drawTextCentered(FONT_SMALL, sx, sy - 58, "РЕЗОНАТОР • РАЗРУШЬ", 0xFFE7C2FF);
                continue;
            }
            if (m.isBoss && m.abilityWindup > 0.0f) {
                static const uint32_t tellPalette[6] = {0xFFFFC64A, 0xFF70E85F, 0xFFFF4E78, 0xFFFF7948, 0xFF62D8F0, 0xFFB6E8F4};
                const uint32_t tellColor = tellPalette[m.bossType % 6];
                if (m.bossType == 2) {
                    const int tx = (int)(m.abilityTargetX - camX), ty = (int)(m.abilityTargetY - camY);
                    r.drawLine(sx, sy, tx, ty, tellColor, 5, 150, true);
                    r.drawCircleRing(tx, ty, 42, 4, tellColor, 230, true);
                } else if (m.bossType == 0) {
                    const int tx = (int)(m.x - camX), ty = (int)(m.y - camY);
                    const int rad = m.bossPhase >= 3 ? 190 : 150;
                    r.fillCircle(tx, ty, rad, tellColor, 35);
                    r.drawCircleRing(tx, ty, rad, 4, tellColor, 230, true);
                } else {
                    // Azaroth: the telegraph shows the gap as well, so the safe wedge is
                    // readable before the burst instead of being a surprise.
                    const int tx = (int)(m.x - camX), ty = (int)(m.y - camY);
                    const float progress = std::max(0.0f, std::min(1.0f, 1.0f - m.abilityWindup / 0.95f));
                    const int rad = 96 + (int)(progress * 34.0f);
                    const float gapCenter = atan2f(m.abilityTargetY - m.y, m.abilityTargetX - m.x);
                    for (int arc = 0; arc < 48; ++arc) {
                        const float a = arc * 6.2831853f / 48.0f;
                        float delta = a - gapCenter;
                        while (delta > 3.1415927f) delta -= 6.2831853f;
                        while (delta < -3.1415927f) delta += 6.2831853f;
                        if (fabsf(delta) < 0.52f) continue;
                        const int px = tx + (int)(cosf(a) * rad);
                        const int py = ty + (int)(sinf(a) * rad);
                        r.fillCircle(px, py, 3, tellColor, 190);
                        r.addGlow(px, py, 7, tellColor, 90);
                    }
                    r.drawCircleRing(tx, ty, rad, 1, tellColor, 110, true);
                }
            }
            if (m.isBoss) {
                if (m.phaseInvulnTimer > 0.0f) {
                    const float lockPulse = 0.5f + 0.5f * sinf(globalTime * 36.0f + m.animOffset);
                    const int shell = 55 + (int)(lockPulse * 7.0f);
                    r.addGlow(sx, sy, shell, 0xFFC8F8FF, 190);
                    r.drawCircleRing(sx, sy, shell - 4, 3, 0xFFE8FCFF, 245, true);
                    r.drawCircleRing(sx, sy, shell + 5, 1, 0xFF77E8FF, 200, true);
                }
                if (m.bossPhase >= 2) {
                    const uint32_t phaseColor = m.bossPhase == 2 ? 0xFFFFA544 : 0xFFFF4F75;
                    const int phasePulse = 3 + (int)(3.0f * (0.5f + 0.5f * sinf(globalTime * (m.bossPhase == 3 ? 7.0f : 4.5f) + m.animOffset)));
                    r.addGlow(sx, sy, 46 + phasePulse, phaseColor, m.bossPhase == 3 ? 145 : 95);
                    r.drawCircleRing(sx, sy, 38 + phasePulse, m.bossPhase == 3 ? 3 : 2, phaseColor, m.bossPhase == 3 ? 235 : 175, true);
                }
                r.drawBossSprite(sx, sy, m.bossType, globalTime + m.animOffset, m.hitFlashTimer > 0.0f, m.bossPhase >= 3,
                                 m.bossReinforced);
            } else {
                r.drawMonsterSprite(sx, sy, m.type, globalTime + m.animOffset, m.hitFlashTimer > 0.0f, m.isElite, m.freezeTimer > 0.0f, m.facing < 0,
                                    m.isRat ? m.ratHeading : 0.0f);
                if (m.isMatryoshka) {
                    const int pulse = 2 + (int)(2.0f * (1.0f + sinf(globalTime * 6.0f + m.animOffset)));
                    r.addGlow(sx, sy, (int)m.radius + 12, 0xFFFFC65E, 110);
                    r.drawCircleRing(sx, sy, (int)m.radius + 7 + pulse, 2, 0xFFFFD77A, 220, true);
                    r.drawTextCentered(FONT_SMALL, sx, sy - (int)m.radius - 17, "×2", 0xFFFFE5A0, true);
                }
                if (m.isSmart) {
                    static const uint32_t smartColors[3] = {0xFFFF8058, 0xFF58DFFF, 0xFFD080FF};
                    uint32_t smartColor = smartColors[m.smartTactic % 3];
                    int pulse = 2 + (int)(2.0f * (1.0f + sinf(globalTime * 7.0f + m.animOffset)));
                    r.addGlow(sx, sy, (int)m.radius + 12, smartColor, 125);
                    r.drawCircleRing(sx, sy, (int)m.radius + 7 + pulse, 2, smartColor, 220, true);
                    r.drawTextCentered(FONT_SMALL, sx, sy - (int)m.radius - 19, "!", smartColor);
                }
                if (m.freezeTimer > 0.0f) {
                    r.fillRoundedRect(sx - 11, sy - 14, 22, 26, 4, 0xFF40E8FF, 95);
                }
                // Show mini HP bar above damaged elites or high-HP monsters
                if (m.hp < m.maxHp) {
                    int bw = m.isSmart ? 38 : (m.isElite ? 34 : 24);
                    int bx = sx - bw / 2;
                    int by = sy - (int)m.radius - 12;
                    r.fillRect(bx, by, bw, 4, 0xFF140C1C);
                    int fillW = (int)((m.hp / m.maxHp) * bw);
                    uint32_t hpColor = m.isSmart ? (m.smartTactic == 0 ? 0xFFFF8058 : (m.smartTactic == 1 ? 0xFF58DFFF : 0xFFD080FF)) : (m.isElite ? 0xFFFFB820 : 0xFFFF3850);
                    r.fillRect(bx, by, fillW, 4, hpColor);
                }
            }
            if (itemLevels[33] > 0 && !m.isBoss && !m.isResonator && m.crossfireWeaponMask != 0) {
                const int rank = std::max(1, std::min(3, itemLevels[33]));
                const int required = getCrossfireDistinctWeaponThreshold(rank);
                const int charge = std::min(required, getCrossfireMaskCount(m.crossfireWeaponMask));
                const int ringRadius = (int)m.radius + 14;
                // Charge is shown by glowing dots outside the body, never by ticks across it.
                for (int spark = 0; spark < required; ++spark) {
                    const float angle = -1.5707963f + spark * 6.2831853f / required;
                    const bool lit = spark < charge;
                    r.fillCircle(sx + (int)lroundf(cosf(angle) * ringRadius), sy + (int)lroundf(sinf(angle) * ringRadius),
                                 lit ? 3 : 2, lit ? 0xFFB8FFF1 : 0xFF316F70, lit ? 225 : 110);
                }
            }
            if (m.emberTicksRemaining > 0) {
                const int emberPulse = 1 + (int)(2.0f * (0.5f + 0.5f * sinf(globalTime * 9.0f + m.animOffset)));
                const int emberRadius = (int)m.radius + 8 + emberPulse;
                r.addGlow(sx, sy, emberRadius + 4, 0xFFFF9347, 66);
                r.drawCircleRing(sx, sy, emberRadius, 1, 0xFFFFA85A, 150, true);
                r.fillCircle(sx + (int)(cosf(globalTime * 6.0f + m.animOffset) * emberRadius),
                             sy + (int)(sinf(globalTime * 6.0f + m.animOffset) * emberRadius), 2, 0xFFFFD28A, 215);
            }
            if (m.exposeTimer > 0.0f) {
                const int pulse = 1 + (int)(2.0f * (0.5f + 0.5f * sinf(globalTime * 12.0f + m.animOffset)));
                const int markRadius = (int)m.radius + 10 + pulse;
                r.addGlow(sx, sy, markRadius + 5, 0xFFBD83FF, 78);
                // Broken ring of dots: reads as "exposed" without drawing a line across the mob.
                drawDottedRing(r, sx, sy, markRadius, 10, 0xFFE3C2FF, 210, globalTime * 1.6f + m.animOffset);
            }
            if (m.shieldHits > 0) {
                // The shield used to be a short bar crossed by a ring, offset from the mob by its
                // facing vector — on screen that read as "someone drew a diameter next to the mob".
                // Now it is a bright arc hugging the mob's own circle, on the side it is guarding.
                const float facing = atan2f(m.shieldFacingY, m.shieldFacingX);
                drawShieldArc(r, sx, sy, (int)m.radius + 6, facing, 0xFF79E9FF, 205, m.isShieldBearer ? 3 : 2);
                drawShieldArc(r, sx, sy, (int)m.radius + 10, facing, 0xFFB3F5FF, 130, 1);
                for (int charge = 0; charge < std::min(4, m.shieldHits); ++charge) {
                    const float a = facing + (charge - (std::min(4, m.shieldHits) - 1) * 0.5f) * 0.34f;
                    r.fillCircle(sx + (int)lroundf(cosf(a) * (m.radius + 14)), sy + (int)lroundf(sinf(a) * (m.radius + 14)), 2, 0xFFD9FBFF, 215);
                }
            }
        }

        // 6. Player Hero Sprite + HP Bar + Dash Bar
        r.addGlow(pScreenX, pScreenY, 48, 0xFF6848A8, 95);
        if (superTimer > 0.0f) {
            int pulse = 34 + (int)(sinf(globalTime * 26.0f) * 8.0f);
            r.addGlow(pScreenX, pScreenY, pulse + 30, 0xFFFFC840, 220);
            r.drawCircleRing(pScreenX, pScreenY, pulse, 5, 0xFFFFE878, 245, true);
        }
        if (itemLevels[30] > 0 && playerWard > 0.0f) {
            const int pulse = (int)(sinf(globalTime * 5.0f) * 2.0f);
            r.addGlow(pScreenX, pScreenY, 28 + pulse, 0xFFFFD878, wardFlashTimer > 0.0f ? 150 : 48);
            r.drawCircleRing(pScreenX, pScreenY, 25 + pulse, 2, 0xFFFFD878, wardFlashTimer > 0.0f ? 235 : 112, true);
        }
        if (itemLevels[32] > 0) {
            const int threshold = getAshRhythmSparkThreshold(itemLevels[32]);
            const int radius = 40;
            const bool bursting = ashRhythmFlashTimer > 0.0f;
            r.drawCircleRing(pScreenX, pScreenY, radius, 1, 0xFF67E7D4, bursting ? 190 : 48, true);
            if (bursting) r.addGlow(pScreenX, pScreenY, radius + 5, 0xFF67E7D4, (int)(ashRhythmFlashTimer * 220.0f));
            for (int spark = 0; spark < threshold; ++spark) {
                const float angle = -1.5707963f + spark * 6.2831853f / threshold;
                const int inner = radius - 3, outer = radius + 3;
                const int x0 = pScreenX + (int)(cosf(angle) * inner);
                const int y0 = pScreenY + (int)(sinf(angle) * inner);
                const int x1 = pScreenX + (int)(cosf(angle) * outer);
                const int y1 = pScreenY + (int)(sinf(angle) * outer);
                const bool charged = spark < ashRhythmSparkCount;
                r.drawLine(x0, y0, x1, y1, charged ? 0xFFB9FFF1 : 0xFF327A76,
                           charged ? 2 : 1, charged ? 235 : 90, true);
            }
        }
        r.drawHeroSprite(pScreenX, pScreenY, selectedHero, globalTime, playerFacing, playerHurtTimer > 0.15f, dashTimer > 0.0f);
        if (phoenixReviveTimer > 0.0f) {
            const float remain = std::min(1.0f, phoenixReviveTimer / PHOENIX_REVIVE_DURATION);
            const float progress = 1.0f - remain;
            const int alpha = std::max(0, std::min(255, (int)(255.0f * (remain * 0.68f + 0.32f))));
            const int pulse = 35 + (int)(progress * 150.0f + sinf(globalTime * 18.0f) * 7.0f);
            r.addGlow(pScreenX, pScreenY, 42 + (int)(remain * 28.0f), 0xFFFFC94F, alpha);
            r.drawCircleRing(pScreenX, pScreenY, pulse, 4, 0xFFFFD86B, alpha, true);
            r.drawCircleRing(pScreenX, pScreenY, std::max(18, pulse - 23), 2, 0xFFFFF0BC, alpha * 3 / 4, true);
            // Hand-drawn feather fan: broad wing silhouettes and fine quill highlights, not a flash sprite.
            for (int side : {-1, 1}) {
                for (int feather = 0; feather < 7; ++feather) {
                    const float t = feather / 6.0f;
                    const int rootX = pScreenX + side * 10;
                    const int rootY = pScreenY - 2;
                    const int tipX = pScreenX + side * (30 + feather * 12);
                    const int tipY = pScreenY - (26 + (int)(sinf((t + 0.08f) * 3.14159f) * 58.0f));
                    const int featherAlpha = std::max(0, alpha - feather * 8);
                    r.drawLine(rootX, rootY, tipX, tipY, 0xFFFFC84A, 4, featherAlpha, false);
                    r.drawLine(tipX, tipY, pScreenX + side * (22 + feather * 10), pScreenY + 14 - feather * 3,
                               0xFFFFE5A1, 2, featherAlpha * 3 / 4, false);
                }
            }
            r.drawLine(pScreenX - 9, pScreenY - 21, pScreenX, pScreenY - 39, 0xFFFFE5A1, 3, alpha, false);
            r.drawLine(pScreenX + 9, pScreenY - 21, pScreenX, pScreenY - 39, 0xFFFFC84A, 3, alpha, false);
        }

        // Orbiting Runic Blades (Weapon 2)
        if (itemLevels[2] > 0) {
            int lvl = itemLevels[2];
            int orbCount = 2 + lvl / 2 + (evolvedWeapons[2] ? 2 : 0);
            float orbitRad = (78.0f + lvl * 6.0f) * getAreaMultiplier();
            for (int c = 0; c < orbCount; ++c) {
                float a = orbitalAngle + c * (2.0f * 3.14159f / orbCount);
                int ox = pScreenX + (int)(cosf(a) * orbitRad);
                int oy = pScreenY + (int)(sinf(a) * orbitRad);
                r.addGlow(ox, oy, 20, 0xFFC040FF, 180);
                r.fillCircle(ox, oy, 8, 0xFFD868FF);
                r.fillCircle(ox, oy, 4, 0xFFFFFFFF);
            }
        }

        // 7. Projectiles
        for (const auto& p : projectiles) {
            int sx = (int)(p.x - camX);
            int sy = (int)(p.y - camY);
            if (sx < -40 || sx > SCREEN_W + 40 || sy < -40 || sy > SCREEN_H + 40) continue;

            if (p.weaponId == REFLECTED_PROJECTILE_ATTACK_ID) {
                // Reversed hostile bolt: unmistakable cyan return streak.
                const float len = std::max(1.0f, sqrtf(p.vx * p.vx + p.vy * p.vy));
                const int tx = (int)(p.vx / len * 12.0f), ty = (int)(p.vy / len * 12.0f);
                r.addGlow(sx, sy, 16, 0xFF72DFFF, 185);
                r.drawLine(sx - tx, sy - ty, sx + tx, sy + ty, 0xFFBDF8FF, 5);
                r.drawLine(sx - tx / 2, sy - ty / 2, sx + tx / 2, sy + ty / 2, 0xFFFFFFFF, 2);
            } else if (p.weaponId == 24) {
                // Comet Shard: a bright crystal head with a clean blue tail.
                const float len = std::max(1.0f, sqrtf(p.vx*p.vx + p.vy*p.vy));
                const int tx = (int)(p.vx / len * 13.0f), ty = (int)(p.vy / len * 13.0f);
                r.addGlow(sx, sy, 17, 0xFF62D9FF, 180);
                r.drawLine(sx - tx, sy - ty, sx + tx, sy + ty, 0xFF8BEAFF, 5);
                r.drawLine(sx - tx / 2, sy - ty / 2, sx + tx / 2, sy + ty / 2, 0xFFFFFFFF, 2);
            } else if (p.weaponId == 0) {
                // Silver Blade
                r.addGlow(sx, sy, 16, 0xFF40D8FF, 165);
                float len = sqrtf(p.vx * p.vx + p.vy * p.vy);
                float nx = (len > 1.0f) ? p.vx / len : 1.0f;
                float ny = (len > 1.0f) ? p.vy / len : 0.0f;
                r.drawLine(sx - (int)(nx * 10), sy - (int)(ny * 10), sx + (int)(nx * 10), sy + (int)(ny * 10), 0xFF80F0FF, 5);
                r.drawLine(sx - (int)(nx * 7), sy - (int)(ny * 7), sx + (int)(nx * 9), sy + (int)(ny * 9), 0xFFFFFFFF, 2);
            } else if (p.weaponId == 1) {
                // Fireball
                r.addGlow(sx, sy, 26, 0xFFFF6010, 195);
                r.fillCircle(sx, sy, 9, 0xFFFF5010);
                r.fillCircle(sx, sy, 5, 0xFFFFF060);
            } else if (p.weaponId == 2 && !options.hardModeEnabled) {
                // Rune Whirl uses a cool player-only palette outside Abyss mode.
                r.addGlow(sx, sy, 16, 0xFF43D9F4, 180);
                r.drawCircleRing(sx, sy, 7, 2, 0xFFB0F7FF, 230);
                r.fillCircle(sx, sy, 4, 0xFFFFFFFF);
            } else if (p.weaponId == 5) {
                // Spinning Battleaxe
                r.addGlow(sx, sy, 20, 0xFFFF9830, 145);
                float rot = globalTime * 16.0f;
                int ax = (int)(cosf(rot) * 11.0f);
                int ay = (int)(sinf(rot) * 11.0f);
                r.drawLine(sx - ax, sy - ay, sx + ax, sy + ay, 0xFFD8E4F0, 4);
                r.fillCircle(sx + ax, sy + ay, 6, 0xFFFFB840);
            } else if (p.isEnemy && p.projectileKind == 1) {
                // Bone splinters: ivory core with a restrained warm-gold edge, fitting the skeletal king.
                r.addGlow(sx, sy, 14, 0xFFFFD878, 145);
                r.drawLine(sx - (int)(p.vx * 0.045f), sy - (int)(p.vy * 0.045f), sx + (int)(p.vx * 0.025f), sy + (int)(p.vy * 0.025f), 0xFFFFF2D8, 4);
                r.fillCircle(sx, sy, 3, 0xFFFFFFFF);
            } else if (p.isEnemy && p.projectileKind == 2) {
                // Malakor's plague orbs home in with a toxic-green halo.
                r.addGlow(sx, sy, 18, 0xFF72F05F, 175);
                r.drawCircleRing(sx, sy, 8, 2, 0xFFB9FF72, 220);
                r.fillCircle(sx, sy, 4, 0xFFF4FFD9);
            } else if (p.isEnemy && p.projectileKind == 5) {
                // Cardinal's blood crescents are visually distinct from bone and poison shots.
                r.addGlow(sx, sy, 17, 0xFFFF285B, 185);
                const float len = std::max(1.0f, sqrtf(p.vx * p.vx + p.vy * p.vy));
                const int nx = (int)(p.vx / len * 10.0f), ny = (int)(p.vy / len * 10.0f);
                r.drawCircleRing(sx, sy, 7, 2, 0xFFFFA0B5, 235, true);
                r.drawLine(sx - nx, sy - ny, sx + nx, sy + ny, 0xFFFFD4DF, 3, 240, true);
                r.fillCircle(sx, sy, 3, 0xFFFFFFFF);
            } else if (p.isEnemy && p.projectileKind == 3) {
                r.addGlow(sx, sy, 22, 0xFFFF7438, 185);
                r.fillCircle(sx, sy, 8, 0xFFB84EFF);
                r.drawCircleRing(sx, sy, 10, 2, 0xFFFFC273, 220);
            } else {
                // Enemy dark orb (also retained on the high-difficulty rune palette).
                r.addGlow(sx, sy, 16, 0xFFE030FF, 170);
                r.fillCircle(sx, sy, 6, 0xFFB820E8);
                r.fillCircle(sx, sy, 3, 0xFFFFB0FF);
            }
        }

        // 8. Chain Lightning Arcs
        for (const auto& l : lightningArcs) {
            int x0 = (int)(l.x0 - camX), y0 = (int)(l.y0 - camY);
            int x1 = (int)(l.x1 - camX), y1 = (int)(l.y1 - camY);
            int mx = (x0 + x1) / 2 + (rand() % 24 - 12);
            int my = (y0 + y1) / 2 + (rand() % 24 - 12);
            r.drawLine(x0, y0, mx, my, 0xFF60E0FF, 4, 230, true);
            r.drawLine(mx, my, x1, y1, 0xFF60E0FF, 4, 230, true);
            r.drawLine(x0, y0, mx, my, 0xFFFFFFFF, 2, 255, true);
            r.drawLine(mx, my, x1, y1, 0xFFFFFFFF, 2, 255, true);
        }

        // 9. Particles: small material-coded silhouettes with a smooth fade, not one repeated glow dot.
        for (const auto& p : particles) {
            int sx = (int)(p.x - camX);
            int sy = (int)(p.y - camY);
            const float t = std::max(0.0f, std::min(1.0f, p.life / p.maxLife));
            const int alpha = (int)(255.0f * t * t * (3.0f - 2.0f * t));
            r.drawParticle(sx, sy, p.size, p.color, alpha, p.style, p.rotation, p.stretch, p.scale, p.additive);
        }

        // 10. Floating Damage Numbers & Alerts
        for (const auto& ft : floatTexts) {
            int sx = (int)(ft.x - camX);
            int sy = (int)(ft.y - camY);
            int alpha = std::min(255, (int)((ft.life / ft.maxLife) * 320.0f));
            r.drawTextCentered(ft.big ? FONT_MED : FONT_SMALL, sx, sy, ft.text.c_str(), ft.color, true, alpha);
        }
    }

    void drawUIFrame(Renderer& r, int x, int y, int w, int h, uint32_t accent, bool selected = false, int alpha = 238) const {
        r.fillRoundedRect(x, y, w, h, 8, selected ? 0xFF211934 : 0xFF100D1B, alpha);
        r.drawRoundedRect(x, y, w, h, 8, selected ? accent : 0xFF4B3D62, selected ? 2 : 1, alpha);
        r.fillRoundedRect(x + 12, y + 1, std::max(1, w - 24), 2, 1, accent, alpha);
    }

    // F3: the technical read-out the player can flip on and forget about.
    void drawDebugOverlay(Renderer& r) {
        char line[256];
        const int pad = 8, lineH = 17;
        const bool hasMessage = storyTimer > 0.0f && !storyLine.empty();
        const int rows = 15 + (hasMessage ? 2 : 0);
        const int labelW = 176;
        const int boxW = std::min(SCREEN_W - 16, 486), boxH = rows * lineH + pad * 2;
        const int bx = 8, by = 8;
        r.fillRoundedRect(bx, by, boxW, boxH, 6, 0xFF05030A, 232);
        r.drawRoundedRect(bx, by, boxW, boxH, 6, 0xFF62D8F0, 1, 200);
        auto put = [&](int row, const char* label, const char* value, uint32_t color = 0xFFE6E0F4) {
            const int y = by + pad + row * lineH;
            r.drawText(FONT_SMALL, bx + pad, y, label, 0xFF8E86AC, true, 235);
            r.drawText(FONT_SMALL, bx + pad + labelW, y, value, color, true, 245);
        };
        snprintf(line, sizeof(line), "v%s  BUILD %d", GAME_VERSION, GAME_VERSION_NUM);
        put(0, "ВЕРСИЯ", line, 0xFF7EF0C0);
        put(1, "СОСТОЯНИЕ", stateName(state));
        snprintf(line, sizeof(line), "%.0f  (%.1f MS)", fpsSmooth, frameMsSmooth);
        put(2, "FPS", line, fpsSmooth < 45.0f ? 0xFFFF9B7E : 0xFF9BF0B0);
        snprintf(line, sizeof(line), "%dx%d", SCREEN_W, SCREEN_H);
        put(3, "ЭКРАН", line);
        snprintf(line, sizeof(line), "%.0f  %.0f  (%.0f С)", playerX, playerY, runTime);
        put(4, "ГЕРОЙ", line);
        snprintf(line, sizeof(line), "%d  (ПИК %d)", (int)monsters.size(), debugPeakEntities);
        put(5, "ВРАГИ", line);
        snprintf(line, sizeof(line), "%d / %d", (int)projectiles.size(), (int)pickups.size());
        put(6, "СНАРЯДЫ/ПОДБОРЫ", line);
        snprintf(line, sizeof(line), "%d / %d", (int)particles.size(), (int)floatTexts.size());
        put(7, "ЧАСТИЦЫ/ТЕКСТ", line);
        snprintf(line, sizeof(line), "%d / %d", (int)skillImpactMarks.size(), (int)shockwaves.size());
        put(8, "МЕТКИ/ВОЛНЫ", line);
        snprintf(line, sizeof(line), "L%d  HP %.0f  XP %d/%d", playerLevel, playerHp, playerXp, xpToNext);
        put(9, "ПРОКАЧКА", line);
        int ownedItems = 0;
        for (int i = 0; i < ITEM_COUNT; ++i) if (itemLevels[i] > 0) ++ownedItems;
        snprintf(line, sizeof(line), "ТИР %d  %d/%d  ВОЛНА %d", getBuildTier(), ownedItems, getBuildLimit(), waveNumber);
        put(10, "СБОРКА", line);
        snprintf(line, sizeof(line), "MODE %d  SFX %d%%  MUS %d%%", sound.musicMode.load(), (int)roundf(sound.sfxVolume.load() * 100.0f), (int)roundf(sound.musicVolume.load() * 100.0f));
        put(11, "ЗВУК", line);
        snprintf(line, sizeof(line), "%d/%d КЛИПОВ  %s", voiceClips, MessageRegistry::voicedDefs(), options.messageSpeechEnabled != 0 ? "ВКЛ" : "ВЫКЛ");
        put(12, "ОЗВУЧКА", line, (options.messageSpeechEnabled != 0 && voiceClips > 0) ? 0xFF9BF0B0 : 0xFFFF9B7E);
        put(13, "СООБЩЕНИЯ", messages.summary().c_str(), MessageRegistry::invalidRows() == 0 ? 0xFFE6E0F4 : 0xFFFF6A7A);
        // The announcer is its own bus: the gain and the live effects duck explain a quiet
        // line better than the clip count does (a file can be in the corridor and still drown).
        snprintf(line, sizeof(line), "x%.2f  SFX %+.1f ДБ", sound.announcerBusGain(), sound.announcerDuckDb());
        put(14, "ГОЛОС ШИНА", line, sound.voiceSpeaking() ? 0xFF9BF0B0 : 0xFFE6E0F4);
        if (hasMessage) {
            snprintf(line, sizeof(line), "%s  %.1f С", storySpeaker.c_str(), storyTimer);
            put(15, "НА ЭКРАНЕ", line, 0xFFC9A6FF);
            snprintf(line, sizeof(line), "%s  ПРОЗВУЧАЛО %d", sound.voiceEnabled() ? "ГОЛОС ON" : "ГОЛОС OFF", sound.voiceSpokenCount());
            put(16, "ОЗВУЧЕНО", line, sound.voiceEnabled() ? 0xFF9BF0B0 : 0xFFFF9B7E);
        }
    }

    // Single entry point for every player-facing combat/story message.
    // It shows the line, counts it in the registry and (optionally) reads it aloud,
    // so a new message can never silently miss the announcer.
    void announce(GameMsgId id, const std::string& text, float seconds) {
        storyLine = text;
        storyTimer = seconds;
        storyDuration = seconds;
        storySpeaker = MessageRegistry::key(id);
        messages.note(id);
        if (seconds > 0.0f && !storyLine.empty()) speakMessage(id, text);
    }

    // Visual-only variant: the line appears and is counted, but the voice is handled by
    // the caller (used when a case has its own longer narration).
    void announceVisual(GameMsgId id, const std::string& text, float seconds) {
        storyLine = text;
        storyTimer = seconds;
        storyDuration = seconds;
        storySpeaker = MessageRegistry::key(id);
        messages.note(id);
    }

    // Voice-only path for banners that already have their own visual channel
    // (difficulty strip, boss warning, build tier). Keeps one switch for every sound bite.
    void speakOnly(GameMsgId id, const std::string& text) {
        messages.note(id);
        speakMessage(id, text);
    }

    // Speak an arbitrary recorded key (used by the per-case narrations, which live
    // outside the message registry because each case has its own file).
    bool speakKey(const std::string& key, const std::string& text, bool allowDuplicate = false) {
        if (text.empty()) return false;
        if (options.messageSpeechEnabled == 0) { ++messages.voicedSuppressed; return false; }
        const bool duplicate = !allowDuplicate && text == announceLastText && (globalTime - announceLastAt) < 4.0f;
        if (duplicate) return false;
        if (!sound.speakMessage(key)) return false;
        announceLastText = text;
        announceLastAt = globalTime;
        ++messages.voicedSent;
        return true;
    }

    // The narration of one case: its own clip when it has been recorded, otherwise the
    // generic "миссия начинается" line, so an older build never goes silent.
    bool speakCaseBriefing(int mission, bool allowDuplicate = false) {
        const CampaignMissionDef& def = CAMPAIGN_MISSIONS[std::max(0, std::min(CAMPAIGN_MISSION_COUNT - 1, mission))];
        if (options.messageSpeechEnabled == 0) { ++messages.voicedSuppressed; return false; }
        const bool own = speakKey(missionBriefKey(mission), def.briefing, allowDuplicate);
        if (!own) speakMessage(GMSG_MISSION_BRIEFING, def.briefing);
        briefSpokenMission = mission;
        briefSpokenAt = globalTime;
        return true;
    }

    // Called from the select screen: reading the case you just highlighted is the whole
    // point of having a narration per case.
    void noteMissionPreview() {
        if (storyMissionCursor == briefPreviewMission) return;
        briefPreviewMission = storyMissionCursor;
        speakCaseBriefing(storyMissionCursor, true);
    }

    void speakMessage(GameMsgId id, const std::string& text) {
        if (!MessageRegistry::voiced(id) || text.empty()) return;
        if (options.messageSpeechEnabled == 0) { ++messages.voicedSuppressed; return; }
        // Repeated identical lines every few seconds would turn into garble.
        const bool duplicate = text == announceLastText && (globalTime - announceLastAt) < 4.0f;
        if (duplicate) return;
        // The clip is chosen by registry key, not by the on-screen text: numbers and
        // boss names change every run, the recorded phrase does not.
        if (sound.speakMessage(MessageRegistry::key(id))) {
            announceLastText = text;
            announceLastAt = globalTime;
            ++messages.voicedSent;
        }
    }


    // A ring made of separate dots instead of a solid circle with radial ticks: the old
    // ticks read as a "diameter line" drawn next to the mob, which looked broken.
    static void drawDottedRing(Renderer& r, int cx, int cy, int radius, int dots, uint32_t color, int alpha, float phase, int dotSize = 2) {
        for (int i = 0; i < dots; ++i) {
            const float a = phase + i * (6.2831853f / dots);
            r.fillCircle(cx + (int)lroundf(cosf(a) * radius), cy + (int)lroundf(sinf(a) * radius), dotSize, color, alpha);
        }
    }

    // A convex shield wall: an arc centred on the mob, opening toward the side it blocks.
    static void drawShieldArc(Renderer& r, int cx, int cy, int radius, float facing, uint32_t color, int alpha, int thickness) {
        const float half = 0.72f; // ~82 degrees of visible wall
        const int steps = 10;
        int prevX = 0, prevY = 0;
        for (int i = 0; i <= steps; ++i) {
            const float a = facing - half + (2.0f * half) * (float)i / (float)steps;
            const int x = cx + (int)lroundf(cosf(a) * radius);
            const int y = cy + (int)lroundf(sinf(a) * radius);
            if (i > 0) r.drawLine(prevX, prevY, x, y, color, thickness, alpha, true);
            prevX = x; prevY = y;
        }
    }

    // A translucent window knocked out of the screen centre, with a soft fade in/out.
    int textLineCount(Renderer& r, const std::string& text, int maxWidth) const {
        // Mirror drawTextWrapped's greedy wrap so a panel is exactly as tall as its text.
        int lines = 0;
        std::string word, line;
        auto flush = [&](bool force) {
            if (!line.empty()) { ++lines; line.clear(); }
            else if (force) ++lines;
        };
        for (size_t i = 0; i <= text.size(); ++i) {
            const char c = (i < text.size()) ? text[i] : ' ';
            if (c == ' ' || c == '\n') {
                if (!word.empty()) {
                    const std::string cand = line.empty() ? word : (line + " " + word);
                    if (r.textWidth(FONT_SMALL, cand.c_str()) > maxWidth && !line.empty()) {
                        flush(false);
                        line = word;
                    } else line = cand;
                    word.clear();
                }
                if (c == '\n') flush(true);
            } else word.push_back(c);
        }
        if (!line.empty()) flush(false);
        return std::max(1, lines);
    }

    int storyToastLineCount(Renderer& r, int maxWidth) const {
        return textLineCount(r, storyLine, maxWidth);
    }

    // Every combat message lives in one bottom-left stack: nothing is drawn in the middle of
    // the battlefield any more. The voice line still carries the words; the panel is just the
    // receipt, so the player never has to look away from the fight.
    void drawCombatNotices(Renderer& r) {
        struct Row { std::string text; uint32_t edge; uint32_t ink; int alpha; };
        std::vector<Row> rows;
        char buf[220];

        // Bottom-most first: the story toast stays closest to the controls.
        if (storyTimer > 0.0f && !storyLine.empty()) {
            const float fadeIn = storyDuration > 0.0f ? std::min(1.0f, (storyDuration - storyTimer) * 6.0f) : 1.0f;
            const float fadeOut = std::min(1.0f, storyTimer * 2.6f);
            const int alpha = std::max(0, std::min(212, (int)(212.0f * std::min(fadeIn, fadeOut))));
            rows.push_back({storyLine, 0xFFB58BE8, 0xFFE9D7FF, alpha});
        }
        if (difficultyBannerTimer > 0.0f && !difficultyBanner.empty()) {
            const int alpha = std::min(235, (int)(difficultyBannerTimer * 100.0f));
            const uint32_t color = getDifficultySegment() == 1 ? 0xFFFF6A48 : 0xFF82E8FF;
            rows.push_back({difficultyBanner, color, color, alpha});
        }
        if (buildTierUpTimer > 0.0f) {
            const int alpha = std::min(235, 145 + (int)((0.5f + 0.5f * sinf(globalTime * 13.0f)) * 90.0f));
            snprintf(buf, sizeof(buf), "ТИР СБОРКИ ПОВЫШЕН  •  ТИР %d", buildTierUpValue);
            rows.push_back({buf, 0xFFFFD75E, 0xFFFFF0B0, alpha});
        }
        if (bossWarningTimer > 0.0f) {
            // The border pulses; the words stay readable for the whole warning.
            const int alpha = std::min(235, 196 + (int)(39.0f * (0.5f + 0.5f * sinf(globalTime * 12.0f))));
            snprintf(buf, sizeof(buf), "СТРАЖ ПРИБЛИЖАЕТСЯ  •  %s", bossWarningName.c_str());
            rows.push_back({buf, 0xFFFF5268, 0xFFFFE7D0, alpha});
        }
        if (bossPhaseEventTimer > 0.0f) {
            static const char* bossShort[4] = {"МОРГАР • ВОРОТА", "МАЛАКОР • ГОЛОС", "КАРДИНАЛ • РЕЕСТР", "АЗАРОТ • АРХИВ"};
            static const char* roman[3] = {"I", "II", "III"};
            static const char* phaseNames[3] = {"ОХОТА", "НАТИСК", "ЯРОСТЬ"};
            const int phase = std::max(1, std::min(3, bossPhaseEventNumber));
            const uint32_t color = phase == 2 ? 0xFFFFA843 : 0xFFFF456D;
            const float progress = 1.0f - bossPhaseEventTimer / 3.2f;
            const int alpha = std::max(0, std::min(235, (int)(235.0f * std::min(1.0f, progress * 7.0f) * std::min(1.0f, bossPhaseEventTimer * 3.0f))));
            snprintf(buf, sizeof(buf), "ФАЗА %s  •  %s  •  %s", roman[phase - 1], phaseNames[phase - 1],
                     bossShort[bossPhaseEventType % 6]);
            rows.push_back({buf, color, 0xFFFFFFFF, alpha});
        }
        if (huntActive) {
            snprintf(buf, sizeof(buf), "ТИХАЯ ОХОТА: ОСТАЛОСЬ %d ИЗ %d  •  %.0f с",
                     std::max(0, huntPackRemaining), huntPackTotal, ceilf(huntTimer));
            rows.push_back({buf, 0xFFFFC848, 0xFFFFE6A8, 220});
        }
        if (rows.empty()) return;

        const bool portrait = SCREEN_H > SCREEN_W;
        const int panelW = std::min(portrait ? SCREEN_W - 24 : 560, SCREEN_W - 24);
        const int panelX = portrait ? 12 : 18;
        const bool twoLineHints = portrait || SCREEN_W < 1100;
        int cursorY = SCREEN_H - (twoLineHints ? 54 : 36) - 10;
        for (const Row& row : rows) {
            if (row.alpha <= 2 || row.text.empty()) continue;
            const int lines = textLineCount(r, row.text, panelW - 34);
            const int h = 20 + lines * FONT_SMALL.lineHeight;
            const int y = cursorY - h;
            if (y < 4) break; // never spill over the top of the screen
            r.fillRoundedRect(panelX, y, panelW, h, 10, 0xFF0B0714, (int)(row.alpha * 0.72f));
            r.drawRoundedRect(panelX, y, panelW, h, 10, row.edge, 1, row.alpha);
            r.fillRoundedRect(panelX + 1, y + 8, 3, h - 16, 2, row.edge, row.alpha);
            r.drawTextWrapped(FONT_SMALL, panelX + 16, y + 10, panelW - 32,
                              row.text.c_str(), row.ink, false, row.alpha);
            cursorY = y - 6;
        }
    }

    void renderHUDPortrait(Renderer& r) {
        char buf[160];
        r.fillRect(0, 0, SCREEN_W, 23, 0xFF100D1B, 238);
        const float xpRatio = std::max(0.0f, std::min(1.0f, (float)playerXp / (float)std::max(1, xpToNext)));
        r.fillRect(0, 0, (int)(SCREEN_W * xpRatio), 3, 0xFF43D9F4);
        r.fillRect(0, 3, (int)(SCREEN_W * xpRatio), 2, 0xFF2379BD);
        snprintf(buf, sizeof(buf), "УРОВЕНЬ %d  •  ОПЫТ %d / %d", playerLevel, playerXp, xpToNext);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 6, buf, 0xFFF5F0FF, true);

        const int pad = 12, fullW = SCREEN_W - pad * 2;
        const float hpRatio = std::max(0.0f, std::min(1.0f, playerHp / std::max(1.0f, playerMaxHp)));
        drawUIFrame(r, pad, 30, fullW, 48, 0xFFFF647E, true, 242);
        r.fillRoundedRect(pad + 9, 37, fullW - 18, 17, 5, 0xFF28101C);
        const uint32_t hpColor = hpRatio < 0.25f ? 0xFFFF3F53 : (hpRatio < 0.55f ? 0xFFFF8A45 : 0xFFDD4260);
        r.fillRoundedRect(pad + 9, 37, (int)((fullW - 18) * hpRatio), 17, 5, hpColor);
        if (retaliationChargeTimer > 0.0f) snprintf(buf, sizeof(buf), "HP %d/%d • ОТВЕТ +%.0f%%", (int)ceilf(playerHp), (int)roundf(playerMaxHp), getRetaliationBonusPercent(itemLevels[38]) * 100.0f);
        else if (playerWard > 0.0f) snprintf(buf, sizeof(buf), "ЗДОРОВЬЕ %d/%d • ЩИТ %d/%d", (int)ceilf(playerHp), (int)roundf(playerMaxHp), (int)ceilf(playerWard), (int)getOverhealWardCapacity(itemLevels[30]));
        else snprintf(buf, sizeof(buf), "ЗДОРОВЬЕ  %d / %d", (int)ceilf(playerHp), (int)roundf(playerMaxHp));
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 38, buf, 0xFFFFFFFF);
        if (itemLevels[30] > 0) {
            const float wardRatio = playerWard / std::max(1.0f, getOverhealWardCapacity(itemLevels[30]));
            r.fillRoundedRect(pad + 9, 55, (int)((fullW - 18) * std::max(0.0f, std::min(1.0f, wardRatio))), 3, 1, 0xFFFFD878);
        }
        const float dash = dashCooldown <= 0.0f ? 1.0f : std::max(0.0f, 1.0f - dashCooldown / 3.0f);
        r.fillRoundedRect(pad + 9, 60, fullW - 18, 11, 4, 0xFF151522);
        r.fillRoundedRect(pad + 9, 60, (int)((fullW - 18) * dash), 11, 4, dash >= 1.0f ? 0xFF22AFC8 : 0xFF285B83);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 58, dash >= 1.0f ? "РЫВОК ГОТОВ • SPACE" : "РЫВОК ПЕРЕЗАРЯЖАЕТСЯ", 0xFFF0FCFF);

        const int mins = (int)(runTime / 60.0f), secs = (int)runTime % 60;
        r.fillRect(pad, 84, fullW, 46, 0xE9100D1B);
        r.drawRect(pad, 84, fullW, 46, 0xFF514366, 1);
        snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
        r.drawText(FONT_MED, pad + 12, 88, buf, 0xFFFFE38A);
        snprintf(buf, sizeof(buf), "УБИЙСТВ %d  •  ЗОЛОТО %d", (int)kills, save.gold);
        r.drawTextRight(FONT_SMALL, SCREEN_W - pad - 12, 90, buf, 0xFFFFD878);
        snprintf(buf, sizeof(buf), "УРОН/СЕК %d  •  СУПЕР %.0f%%", (int)roundf(currentDps), std::min(100.0f, superCharge));
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 110, buf, 0xFF9CEBFF);

        static const char* chapterNames[4] = {"РУИНЫ", "КРИПТА", "СОБОР", "ПЕПЕЛЬНЫЙ ТРОН"};
        static const char* storyStageNames[6] = {"0/5 • РУИНЫ", "1/5 • КРИПТА", "2/5 • СОБОР", "3/5 • ПЕП. ТРОН", "4/5 • АРХИВ", "5/5 • СЕРДЦЕ"};
        static const char* phaseNames[3] = {"ТИШИНА", "ПРОРЫВ", "ОТДЫХ"};
        const char* stage = gameMode == MODE_STORY
            ? storyStageNames[std::max(0, std::min(5, currentRunChapter))]
            : chapterNames[std::min(3, (int)(runTime / 90.0f))];
        snprintf(buf, sizeof(buf), "%s  •  %s  •  %d/%d РЕЗОНАТОРОВ", stage, phaseNames[getDifficultySegment()], countResonators(), MAX_ACTIVE_RESONATORS);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 137, buf, 0xFFE4D7B5);
        if (gameMode == MODE_STORY) {
            const CampaignMissionDef& mission = selectedCampaignMission();
            if (!storyMissionBossSpawned) {
                const int untilBoss = std::max(0, (int)ceilf(mission.bossTimeSeconds - runTime));
                snprintf(buf, sizeof(buf), "БОСС ЧЕРЕЗ %02d:%02d  •  МИССИЯ ~%d МИН", untilBoss / 60, untilBoss % 60, campaignMissionMinutes(selectedStoryMission));
            } else {
                const int remaining = std::max(0, (int)ceilf(mission.durationSeconds - runTime));
                snprintf(buf, sizeof(buf), "БОСС В БОЮ  •  ДО ЛИМИТА %02d:%02d", remaining / 60, remaining % 60);
            }
        } else {
            const int untilBoss = std::max(0, 90 + nextBossIndex * 90 - (int)runTime);
            snprintf(buf, sizeof(buf), "ENDLESS  •  СЛЕДУЮЩИЙ БОСС ЧЕРЕЗ %02d:%02d", untilBoss / 60, untilBoss % 60);
        }
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 155, buf, storyMissionBossSpawned ? 0xFFFFA982 : 0xFFFFD878);

        int activeItems = 0;
        for (int i = 0; i < ITEM_COUNT; ++i) activeItems += itemLevels[i] > 0;
        drawUIFrame(r, pad, 177, fullW, 70, 0xFF9C7BCA, false, 225);
        snprintf(buf, sizeof(buf), "СБОРКА %d/%d • ТИР %d", activeItems, getBuildLimit(), getBuildTier());
        r.drawText(FONT_SMALL, pad + 9, 180, buf, 0xFFCEC5DE);
        int slot = 0;
        for (int i = 0; i < ITEM_COUNT; ++i) {
            if (itemLevels[i] <= 0) continue;
            const int col = slot % 12, row = slot / 12;
            const int cx = pad + 21 + col * ((fullW - 36) / 12), cy = 207 + row * 24;
            r.fillRoundedRect(cx - 10, cy - 9, 21, 19, 4, 0xFF171221, 248);
            r.drawRoundedRect(cx - 10, cy - 9, 21, 19, 4, (i < 7 && evolvedWeapons[i]) ? 0xFFFFC84F : 0xFF514461, 1);
            r.drawWeaponIcon(cx, cy, i, 13, i < 7 && evolvedWeapons[i]);
            snprintf(buf, sizeof(buf), "%d", itemLevels[i]);
            r.drawTextCentered(FONT_SMALL, cx + 8, cy + 1, buf, 0xFFFFDE76);
            ++slot;
        }
        int bossBars = 0;
        for (const Monster& m : monsters) {
            if (!m.isBoss || m.hp <= 0.0f || bossBars >= 3) continue;
            const int by = 258 + bossBars * 25, bx = SCREEN_W / 10, bw = SCREEN_W * 8 / 10;
            const float ratio = std::max(0.0f, std::min(1.0f, m.hp / std::max(1.0f, m.maxHp)));
            drawUIFrame(r, bx - 4, by - 2, bw + 8, 22, 0xFFFF8A68, false, 235);
            r.fillRoundedRect(bx, by, bw, 18, 4, 0xFF21111C);
            r.fillRoundedRect(bx, by, (int)(bw * ratio), 18, 4, 0xFFD95C28);
            r.drawRoundedRect(bx, by, bw, 18, 4, 0xFFFFA544, 1);
            snprintf(buf, sizeof(buf), "БОСС • ФАЗА %d", std::max(1, std::min(3, m.bossPhase)));
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, by + 1, buf, 0xFFFFFFFF);
            ++bossBars;
        }
        drawCombatNotices(r);
        r.fillRect(0, SCREEN_H - 32, SCREEN_W, 32, 0xFF0E0A18, 218);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, SCREEN_H - 27, "WASD • ДВИЖЕНИЕ   SPACE • РЫВОК   Q • СУПЕР", 0xFFDDD5EF);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, SCREEN_H - 13, "ПКМ • КУРСОР   M/N • ЗВУК   F11 • ЭКРАН   ESC • ПАУЗА", 0xFFB8B0D0);
    }

    void renderHUD(Renderer& r, float camX, float camY) {
        if (SCREEN_W < 1100) { renderHUDPortrait(r); return; }
        char buf[128];

        // Slim progression rail keeps the playfield open while making the next level legible.
        r.fillRect(0, 0, SCREEN_W, 25, 0xFF100D1B, 238);
        float xpRatio = std::max(0.0f, std::min(1.0f, (float)playerXp / (float)std::max(1, xpToNext)));
        int xpFillW = (int)(SCREEN_W * xpRatio);
        r.fillRect(0, 0, xpFillW, 3, 0xFF43D9F4);
        r.fillRect(0, 3, xpFillW, 2, 0xFF2379BD);
        r.drawRect(0, 0, SCREEN_W, 25, 0xFF403451, 1);
        snprintf(buf, sizeof(buf), "УРОВЕНЬ %d     •     ОПЫТ %d / %d", playerLevel, playerXp, xpToNext);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 6, buf, 0xFFF5F0FF, true);

        // Vitality and dash are grouped; short bars make their status readable at a glance.
        drawUIFrame(r, 14, 33, 296, 58, 0xFFFF647E, true, 242);
        const int hpBarX = 24, hpBarY = 41, hpBarW = 276, hpBarH = 20;
        const float hpRatio = std::max(0.0f, std::min(1.0f, playerHp / std::max(1.0f, playerMaxHp)));
        r.fillRoundedRect(hpBarX, hpBarY, hpBarW, hpBarH, 5, 0xFF28101C);
        const uint32_t hpColor = hpRatio < 0.25f ? 0xFFFF3F53 : (hpRatio < 0.55f ? 0xFFFF8A45 : 0xFFDD4260);
        r.fillRoundedRect(hpBarX, hpBarY, (int)(hpBarW * hpRatio), hpBarH, 5, hpColor);
        r.drawRoundedRect(hpBarX, hpBarY, hpBarW, hpBarH, 5, 0xFFFF9CAC, 1);
        if (retaliationChargeTimer > 0.0f) snprintf(buf, sizeof(buf), "ЗДОРОВЬЕ %d/%d • ОТВЕТ +%.0f%%", (int)ceilf(playerHp), (int)roundf(playerMaxHp), getRetaliationBonusPercent(itemLevels[38]) * 100.0f);
        else if (playerWard > 0.0f) snprintf(buf, sizeof(buf), "ЗДОРОВЬЕ %d/%d • ЩИТ %d/%d", (int)ceilf(playerHp), (int)roundf(playerMaxHp), (int)ceilf(playerWard), (int)getOverhealWardCapacity(itemLevels[30]));
        else snprintf(buf, sizeof(buf), "ЗДОРОВЬЕ   %d / %d", (int)ceilf(playerHp), (int)roundf(playerMaxHp));
        r.drawTextCentered(FONT_SMALL, hpBarX + hpBarW / 2, hpBarY + 2, buf, 0xFFFFFFFF);
        if (itemLevels[30] > 0) {
            const float wardRatio = playerWard / std::max(1.0f, getOverhealWardCapacity(itemLevels[30]));
            r.fillRoundedRect(hpBarX, 62, (int)(hpBarW * std::max(0.0f, std::min(1.0f, wardRatio))), 3, 1, 0xFFFFD878);
        }
        const float dashReady = dashCooldown <= 0.0f ? 1.0f : std::max(0.0f, 1.0f - dashCooldown / 3.0f);
        const int dashBarY = 66, dashBarH = 17;
        r.fillRoundedRect(hpBarX, dashBarY, hpBarW, dashBarH, 4, 0xFF151522);
        r.fillRoundedRect(hpBarX, dashBarY, (int)(hpBarW * dashReady), dashBarH, 4, dashReady >= 1.0f ? 0xFF22AFC8 : 0xFF285B83);
        r.drawRoundedRect(hpBarX, dashBarY, hpBarW, dashBarH, 4, 0xFF55D9E9, 1);
        r.drawTextCentered(FONT_SMALL, hpBarX + hpBarW / 2, dashBarY + 1,
            dashReady >= 1.0f ? "РЫВОК ГОТОВ  •  ПРОБЕЛ" : "РЫВОК ПЕРЕЗАРЯЖАЕТСЯ", 0xFFF0FCFF);

        // A two-row loadout tray caps width even when all twenty item slots are active.
        int activeItems = 0;
        for (int i = 0; i < ITEM_COUNT; ++i) activeItems += itemLevels[i] > 0;
        drawUIFrame(r, 14, 98, 296, 108, 0xFF9C7BCA, false, 225);
        snprintf(buf, sizeof(buf), "СБОРКА  %d / %d  •  ТИР %d", activeItems, getBuildLimit(), getBuildTier());
        r.drawText(FONT_SMALL, 23, 101, buf, 0xFFCEC5DE);
        int slotIndex = 0;
        for (int i = 0; i < ITEM_COUNT; ++i) {
            if (itemLevels[i] <= 0) continue;
            const int col = slotIndex % 8, row = slotIndex / 8;
            const int cx = 29 + col * 34, cy = 130 + row * 24;
            r.fillRoundedRect(cx - 11, cy - 10, 23, 21, 4, 0xFF171221, 248);
            r.drawRoundedRect(cx - 11, cy - 10, 23, 21, 4, (i < 7 && evolvedWeapons[i]) ? 0xFFFFC84F : 0xFF514461, 1);
            r.drawWeaponIcon(cx, cy, i, 14, i < 7 && evolvedWeapons[i]);
            snprintf(buf, sizeof(buf), "%d", itemLevels[i]);
            r.fillRoundedRect(cx + 3, cy + 2, 11, 10, 2, 0xFF0B0910, 245);
            r.drawTextCentered(FONT_SMALL, cx + 8, cy + 1, buf, 0xFFFFDE76);
            ++slotIndex;
        }

        // The central readout separates the run clock from its chapter and current wave state.
        const int mins = (int)(runTime / 60.0f), secs = ((int)runTime) % 60;
        const int timerPanelW = 280;
        const int timerPanelX = (SCREEN_W - timerPanelW) / 2;
        drawUIFrame(r, timerPanelX, 33, timerPanelW, 49, 0xFFFFD064, true, 242);
        snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, 35, buf, 0xFFFFE38A);
        static const char* chapterNames[4] = {"РУИНЫ", "КРИПТА", "СОБОР", "ПЕПЕЛЬНЫЙ ТРОН"};
        static const char* storyStageNames[6] = {"0/5 • РУИНЫ", "1/5 • КРИПТА", "2/5 • СОБОР", "3/5 • ПЕП. ТРОН", "4/5 • АРХИВ", "5/5 • СЕРДЦЕ"};
        static const char* phaseNames[3] = {"ТИШИНА", "ПРОРЫВ", "ОТДЫХ"};
        if (gameMode == MODE_STORY)
            snprintf(buf, sizeof(buf), "%s • %s", storyStageNames[std::max(0, std::min(5, currentRunChapter))], phaseNames[getDifficultySegment()]);
        else
            snprintf(buf, sizeof(buf), "%s • %s", chapterNames[std::min(3, (int)(runTime / 90.0f))], phaseNames[getDifficultySegment()]);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 61, buf, 0xFFE4D7B5);
        const int resonatorCount = countResonators();
        if (gameMode == MODE_STORY)
            snprintf(buf, sizeof(buf), "%d ИЗ %d РЕЗОНАТОРОВ   •   +4 ЭХА/МИН   •   ВЗРЫВ: %d", resonatorCount, MAX_ACTIVE_RESONATORS, getResonatorBurstCount());
        else
            snprintf(buf, sizeof(buf), "%d ИЗ %d РЕЗОНАТОРОВ   •   +4 ВРАГА/МИН   •   ВЗРЫВ: %d", resonatorCount, MAX_ACTIVE_RESONATORS, getResonatorBurstCount());
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 87, buf, resonatorCount ? 0xFFE2B5FF : 0xFF85E8A5);
        if (gameMode == MODE_STORY) {
            const CampaignMissionDef& mission = selectedCampaignMission();
            if (!storyMissionBossSpawned) {
                const int untilBoss = std::max(0, (int)ceilf(mission.bossTimeSeconds - runTime));
                snprintf(buf, sizeof(buf), "ФИНАЛЬНЫЙ БОСС ЧЕРЕЗ %02d:%02d   •   МИССИЯ ~%d МИН",
                         untilBoss / 60, untilBoss % 60, campaignMissionMinutes(selectedStoryMission));
            } else {
                const int remaining = std::max(0, (int)ceilf(mission.durationSeconds - runTime));
                snprintf(buf, sizeof(buf), "ФИНАЛЬНЫЙ БОСС В БОЮ   •   ДО ЛИМИТА МИССИИ %02d:%02d",
                         remaining / 60, remaining % 60);
            }
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 107, buf, storyMissionBossSpawned ? 0xFFFFA982 : 0xFFFFD878);
        } else {
            int livingBossCount = 0, currentBossPhase = 0;
            for (const Monster& m : monsters) if (m.isBoss && m.hp > 0.0f) { ++livingBossCount; currentBossPhase = std::max(currentBossPhase, m.bossPhase); }
            if (livingBossCount >= 3) {
                static const char* phaseNames[3] = {"ОХОТА", "НАТИСК", "ЯРОСТЬ"};
                const int phase = std::max(1, std::min(3, currentBossPhase));
                snprintf(buf, sizeof(buf), "ENDLESS • %d БОССА В БОЮ • ФАЗА %s • НОВЫЙ СТРАЖ ЖДЁТ СЛОТ",
                         livingBossCount, phaseNames[phase - 1]);
            } else if (livingBossCount > 0) {
                static const char* phaseNames[3] = {"ОХОТА", "НАТИСК", "ЯРОСТЬ"};
                const int phase = std::max(1, std::min(3, currentBossPhase));
                snprintf(buf, sizeof(buf), "ENDLESS • ПОБЕЖДЕНО БОССОВ: %d   •   СТРАЖЕЙ В БОЮ: %d   •   ФАЗА %d: %s",
                         runBossKills, livingBossCount, phase, phaseNames[phase - 1]);
            } else {
                const int nextBossTime = 90 + nextBossIndex * 90;
                const int untilBoss = std::max(0, (int)ceilf(nextBossTime - runTime));
                snprintf(buf, sizeof(buf), "ENDLESS • БОССОВ ПОБЕЖДЕНО: %d   •   СЛЕДУЮЩИЙ СТРАЖ ЧЕРЕЗ %02d:%02d",
                         runBossKills, untilBoss / 60, untilBoss % 60);
            }
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 107, buf, livingBossCount ? 0xFFFFA982 : 0xFF75E7FF);
        }

        // Show simultaneous boss health bars as a readable stack.
        int bossBars = 0;
        for (const auto& m : monsters) {
            if (!m.isBoss || m.hp <= 0.0f || bossBars >= 3) continue;
            static const char* bNames[6] = {"МОРГАР • ВОРОТА", "МАЛАКОР • ГОЛОС", "КАРДИНАЛ • РЕЕСТР", "АЗАРОТ • АРХИВАРИУС",
                                            "ПРОТОКОЛИСТ • СТРОКИ", "ДВОЙНИК • СЛЕПОК"};
            static const char* echoNames[6] = {"ЭХО: ВОРОТА", "ЭХО: ГОЛОС", "ЭХО: ИМЕНА", "ЭХО: СВИДЕТЕЛЬСТВО",
                                               "ЭХО: СТРОКИ", "ЭХО: СЛЕПОК"};
            static const char* phaseRoman[3] = {"I", "II", "III"};
            static const char* phaseLabels[3] = {"ОХОТА", "НАТИСК", "ЯРОСТЬ"};
            const int phase = std::max(1, std::min(3, m.bossPhase));
            const uint32_t phaseColor = phase == 1 ? 0xFFFF8A68 : (phase == 2 ? 0xFFFFA544 : 0xFFFF4F75);
            const int bw = 540, bh = 19;
            const int bx = (SCREEN_W - bw) / 2, by = 122 + bossBars * 24;
            drawUIFrame(r, bx - 5, by - 2, bw + 10, bh + 4, phaseColor, false, 238);
            const float br = std::max(0.0f, std::min(1.0f, m.hp / std::max(1.0f, m.maxHp)));
            r.fillRoundedRect(bx, by, bw, bh, 4, 0xFF21111C);
            const uint32_t hpColor = phase == 1 ? 0xFFB62B43 : (phase == 2 ? 0xFFD95C28 : 0xFF9237C9);
            r.fillRoundedRect(bx, by, (int)(bw * br), bh, 4, hpColor);
            r.drawRoundedRect(bx, by, bw, bh, 4, phaseColor, 1);
            char bossLabel[128];
            const char* bossName = m.isTrioBoss ? echoNames[m.bossType % 6] : bNames[m.bossType % 6];
            const char* returnTag = m.bossReinforced ? " • ВОЗВРАЩЕНИЕ" : "";
            snprintf(bossLabel, sizeof(bossLabel), "%s%s  •  ФАЗА %s — %s",
                     bossName, returnTag, phaseRoman[phase - 1], phaseLabels[phase - 1]);
            if (m.bossReinforced && r.textWidth(FONT_SMALL, bossLabel) > bw - 12) {
                // Bar is 540 px wide: a long guardian name plus the whole word «возвращение»
                // would run past the frame. The short tag keeps the marker and fits.
                snprintf(bossLabel, sizeof(bossLabel), "%s • ВОЗВР.  •  ФАЗА %s — %s",
                         bossName, phaseRoman[phase - 1], phaseLabels[phase - 1]);
            }
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, by + 1, bossLabel, 0xFFFFFFFF);
            ++bossBars;
        }


        // 4. Top-right run ledger: fixed-width columns and compact large counters prevent collisions.
        const int ledgerW = 310, ledgerX = SCREEN_W - 324;
        drawUIFrame(r, ledgerX, 33, ledgerW, 102, 0xFF66D5E8, true, 242);
        auto compactCounter = [](long long value) {
            char out[32];
            if (value >= 1000000000LL) snprintf(out, sizeof(out), "%.1fB", value / 1000000000.0);
            else if (value >= 1000000LL) snprintf(out, sizeof(out), "%.1fM", value / 1000000.0);
            else if (value >= 10000LL) snprintf(out, sizeof(out), "%.1fK", value / 1000.0);
            else snprintf(out, sizeof(out), "%lld", value);
            return std::string(out);
        };
        snprintf(buf, sizeof(buf), "УБИЙСТВ: %s", compactCounter(kills).c_str());
        r.drawText(FONT_SMALL, ledgerX + 14, 39, buf, 0xFFFF8898);
        snprintf(buf, sizeof(buf), "ЗОЛ.: %s", compactCounter(save.gold).c_str());
        r.drawTextRight(FONT_SMALL, ledgerX + ledgerW - 12, 39, buf, 0xFFFFD840);
        snprintf(buf, sizeof(buf), "УРОН/СЕК: %d", (int)roundf(currentDps));
        r.drawText(FONT_SMALL, ledgerX + 14, 62, buf, 0xFF70E8FF);
        // Dedicated third row keeps long DPS values (e.g. 1111) from touching audio status.
        snprintf(buf, sizeof(buf), "SFX %s   /   MUSIC %s", sound.enabled ? "ON" : "OFF", sound.musicEnabled ? "ON" : "OFF");
        r.drawTextRight(FONT_SMALL, ledgerX + ledgerW - 12, 85, buf, (sound.enabled && sound.musicEnabled) ? 0xFF70FF98 : 0xFF9890A8);
        static const char* tempoLabels[4]={"МЕДЛЕННО","СРЕДНЕ","БЫСТРО","ОЧЕНЬ БЫСТРО"};
        static const uint32_t tempoColors[4]={0xFF8C91C7,0xFF65D9E8,0xFFFFB957,0xFFFF6374};
        const int displayedTempo=std::max(0,std::min(3,musicCrowdTempoTier));
        snprintf(buf,sizeof(buf),"ТЕМП: %s",tempoLabels[displayedTempo]);
        r.drawTextRight(FONT_SMALL, ledgerX + ledgerW - 12, 108, buf, tempoColors[displayedTempo]);

        // 5. Off-screen Boss Chest indicator arrow
        for (const auto& p : pickups) {
            if (p.type == 6) {
                snprintf(buf, sizeof(buf), "« СУНДУК ДРЕВНИХ ПОБЛИЗОСТИ! »");
                r.drawTextCentered(FONT_MED, SCREEN_W / 2, SCREEN_H - 58, buf, 0xFFFFD838);
                break;
            }
            if (p.type == 7) {
                float dx = p.x - playerX, dy = p.y - playerY;
                float ax = fabsf(dx), ay = fabsf(dy);
                if (ax > SCREEN_W * 0.42f || ay > SCREEN_H * 0.40f) {
                    float scale = std::min((SCREEN_W * 0.43f) / std::max(1.0f, ax), (SCREEN_H * 0.40f) / std::max(1.0f, ay));
                    int sx = SCREEN_W / 2 + (int)(dx * scale), sy = SCREEN_H / 2 + (int)(dy * scale);
                    float len = std::max(1.0f, sqrtf(dx * dx + dy * dy));
                    int tx = (int)(dx / len * 18.0f), ty = (int)(dy / len * 18.0f);
                    r.addGlow(sx, sy, 22, 0xFF45DFFF, 190);
                    r.drawLine(sx - tx, sy - ty, sx + tx, sy + ty, 0xFF73EEFF, 4, 235, true);
                    r.drawTextCentered(FONT_SMALL, sx, sy - 20, "ПОРТАЛ", 0xFFB8F7FF, true);
                }
                break;
            }
        }

        // Off-screen compass for the nearest intact resonator keeps the objective readable when the player moves away.
        const Monster* nearestResonator = nullptr;
        float nearestResonatorD2 = 1.0e30f;
        for (const Monster& structure : monsters) {
            if (!structure.isResonator || structure.hp <= 0.0f) continue;
            const float dx = structure.x - playerX, dy = structure.y - playerY;
            const float d2 = dx * dx + dy * dy;
            if (d2 < nearestResonatorD2) { nearestResonatorD2 = d2; nearestResonator = &structure; }
        }
        if (nearestResonator) {
            const int playerScreenX = (int)(playerX - camX);
            const int playerScreenY = (int)(playerY - camY);
            const float dx = nearestResonator->x - playerX, dy = nearestResonator->y - playerY;
            const int bxOnScreen = (int)(nearestResonator->x - camX);
            const int byOnScreen = (int)(nearestResonator->y - camY);
            if (bxOnScreen < 38 || bxOnScreen > SCREEN_W - 38 || byOnScreen < 100 || byOnScreen > SCREEN_H - 38) {
                const float ax = fabsf(dx), ay = fabsf(dy);
                const float scale = std::min((SCREEN_W * 0.43f) / std::max(1.0f, ax), (SCREEN_H * 0.39f) / std::max(1.0f, ay));
                const int sx = std::max(48, std::min(SCREEN_W - 48, playerScreenX + (int)(dx * scale)));
                const int sy = std::max(96, std::min(SCREEN_H - 48, playerScreenY + (int)(dy * scale)));
                const float len = std::max(1.0f, sqrtf(dx * dx + dy * dy));
                const int tx = (int)(dx / len * 17.0f), ty = (int)(dy / len * 17.0f);
                r.addGlow(sx, sy, 21, 0xFFBB75FF, 150);
                r.drawCircleRing(sx, sy, 12 + (int)(sinf(globalTime * 7.0f) * 2.0f), 2, 0xFFD7B1FF, 225, true);
                r.drawLine(sx - tx, sy - ty, sx + tx, sy + ty, 0xFFD7B1FF, 4, 225, true);
                char marker[48];
                snprintf(marker, sizeof(marker), "РЕЗОНАТОР • %dм", (int)roundf(len));
                r.drawTextCentered(FONT_SMALL, sx, sy - 22, marker, 0xFFEAD6FF, true);
            }
        }

        // Compact ultimate meter in the gap between the left HUD and run timer.
        const int uw = 180, uh = 13, uy = 55;
        const int ux = std::max(320, timerPanelX - uw - 10);
        char superLabel[64];
        if (superCharge >= 100.0f && playerHp > getSuperSacrificeCost() + 1.0f)
            snprintf(superLabel, sizeof(superLabel), "ГОТОВ [Q] • -%.0f HP", getSuperSacrificeCost());
        else if (superCharge >= 100.0f)
            snprintf(superLabel, sizeof(superLabel), "НУЖНО HP ДЛЯ СУПЕРА");
        else snprintf(superLabel, sizeof(superLabel), "СУПЕР [Q] • ЗАРЯД");
        r.drawTextCentered(FONT_SMALL, ux + uw / 2, 36, superLabel, 0xFFE8D8FF);
        r.fillRoundedRect(ux, uy, uw, uh, 5, 0xFF171025, 245);
        r.fillRoundedRect(ux, uy, (int)(uw * std::min(1.0f, superCharge / 100.0f)), uh, 5, superCharge >= 100.0f ? 0xFFFFB820 : 0xFF8D45C8, 245);
        r.drawRoundedRect(ux, uy, uw, uh, 5, superCharge >= 100.0f ? 0xFFFFE068 : 0xFF786090, 1);
        drawCombatNotices(r);

        // 6. Bottom Controls Hint Strip
        r.fillRect(0, SCREEN_H - 26, SCREEN_W, 26, 0xFF0E0A18, 210);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, SCREEN_H - 22,
            "[WASD / Стрелки] Движение • [ПКМ] Курсор • [Пробел] Рывок • [Q] Супер • [M] SFX [N] Музыка • [F11] Экран • [ESC] Пауза",
            0xFFB8B0D0);
    }

    // =========================================================================
    // MODALS & MENUS (Level-Up, Chest, Pause, Game Over, Main Menu, Soul Forge)
    // =========================================================================

    float advisorScoreForItem(int id) const {
        if (id < 0 || id >= ITEM_COUNT) return -1.0e9f;
        if (id >= 20) return 0.0f; // The learned model covers IDs 0..19 only.
        float f[27] = {};
        f[0] = 1.0f;
        f[1 + id] = 1.0f;
        f[21] = itemLevels[id] / (float)std::max(1, getItemMaxLevel(id));
        f[22] = id == HERO_DEFS[selectedHero].startWeaponId ? 1.0f : 0.0f;
        if (id < 7) f[23] = itemLevels[id] >= 5 ? itemLevels[id] / 8.0f : 0.0f;
        else if (id <= 13) f[23] = itemLevels[id - 7] / 8.0f;
        f[24] = playerMaxHp > 0.0f ? std::max(0.0f, std::min(1.0f, playerHp / playerMaxHp)) : 0.0f;
        f[25] = std::max(0.0f, std::min(1.0f, runTime / 600.0f));
        int weapons = 0;
        for (int w = 0; w < 7; ++w) weapons += itemLevels[w] > 0;
        f[26] = weapons / 7.0f;
        if (id >= 20) return 0.0f; // New relics have no trained row yet; tier/context signals rank them safely.
        float score = 0.0f;
        for (int k = 0; k < 27; ++k) score += ADVISOR_UPGRADE_WEIGHTS[id][k] * f[k];
        return score;
    }

    struct AdvisorSnapshot {
        float hpRatio = 1.0f;
        float criticalChance = 0.0f;
        float nearestThreat = 9999.0f;
        int weaponCount = 0; // Base-weapon count retained for existing recommendation logic.
        int activeWeaponCount = 0; // Includes the four late-run weapon IDs used by crossfire.
        int nearbyThreats = 0;
        int midrangeThreats = 0;
        int resonatorCount = 0;
        bool bossAlive = false;
        bool bossNear = false;
    };

    AdvisorSnapshot captureAdvisorSnapshot() const {
        AdvisorSnapshot c;
        c.hpRatio = playerMaxHp > 0.0f ? std::max(0.0f, std::min(1.0f, playerHp / playerMaxHp)) : 1.0f;
        c.criticalChance = getCritChance();
        for (int w = 0; w < 7; ++w) c.weaponCount += itemLevels[w] > 0;
        c.activeWeaponCount = c.weaponCount;
        for (int w = 24; w <= 27; ++w) c.activeWeaponCount += itemLevels[w] > 0;
        // One pass over the current world feeds all four candidate evaluations.
        for (const Monster& m : monsters) {
            if (m.hp <= 0.0f) continue;
            if (m.isResonator) { ++c.resonatorCount; continue; }
            const float dx = m.x - playerX, dy = m.y - playerY;
            const float d2 = dx * dx + dy * dy;
            const float distance = sqrtf(d2);
            c.nearestThreat = std::min(c.nearestThreat, distance);
            if (distance <= 135.0f) ++c.nearbyThreats;
            if (distance <= 250.0f) ++c.midrangeThreats;
            if (m.isBoss) {
                c.bossAlive = true;
                if (distance <= 520.0f) c.bossNear = true;
            }
        }
        return c;
    }

    float advisorTierBonus(int id) const {
        static const float bonuses[6] = {0.0f, 0.0f, 0.16f, 0.34f, 0.52f, 0.60f};
        const int tier = getItemTier(id);
        return tier >= 1 && tier <= 5 ? bonuses[tier] : 0.0f;
    }

    float advisorContextBonus(int id, const AdvisorSnapshot& c) const {
        if (id < 0 || id >= ITEM_COUNT) return -1.0e6f;
        const float danger = 1.0f - c.hpRatio;
        float bonus = 0.0f;
        if (id < 7) {
            if (itemLevels[id] == 0) bonus += c.weaponCount <= 2 ? 0.34f : (c.weaponCount >= 5 ? -0.10f : 0.14f);
            else bonus += itemLevels[id] < 5 ? 0.08f : 0.13f;
            if (id == HERO_DEFS[selectedHero].startWeaponId) bonus += 0.06f;
        }
        if (id == 10) bonus += 0.14f + danger * 0.95f;
        if (id == 14) bonus += 0.10f + danger * 0.40f;
        if (id == 4 || id == 6) {
            if (c.nearbyThreats >= 4) bonus += 0.24f;
            else if (c.midrangeThreats >= 6) bonus += 0.13f;
        }
        if (id == 8 && (c.nearbyThreats >= 3 || c.nearestThreat < 115.0f)) bonus += 0.18f;
        if (id == 7 && runTime < 360.0f) bonus += 0.11f;
        if (id == 11 || id == 12) bonus += c.bossNear ? 0.18f : 0.10f;
        if (id == 19) bonus += c.bossAlive ? 0.36f : (runTime > 360.0f ? 0.20f : 0.0f);
        if (id >= 16 && id < ITEM_COUNT && runTime > 300.0f) bonus += 0.05f;
        if (id == 20 && itemLevels[id] == 0) bonus += 0.10f;
        if (id == 21 && dashCooldown > 1.2f) bonus += 0.12f;
        if (id == 22 && (c.bossNear || c.hpRatio < 0.55f)) bonus += 0.10f;
        if (id == 23 && c.hpRatio < 0.65f) bonus += 0.16f;
        if (id == 28) {
            if (itemLevels[id] == 0) bonus += c.nearbyThreats > 0 ? 0.18f : 0.06f;
            else bonus += echoStepCharges >= std::max(1, getEchoStepThreshold() - 1) ? 0.20f : 0.08f;
        }
        if (id == 29) {
            if (c.bossAlive) bonus += 0.24f;
            else if (c.nearbyThreats > 0 || c.midrangeThreats >= 3) bonus += 0.12f;
        }
        if (id == 31 && c.nearbyThreats >= 2) bonus += std::min(0.20f, c.nearbyThreats * 0.025f);
        if (id == 32 && c.weaponCount >= 3) bonus += std::min(0.20f, 0.06f + c.weaponCount * 0.02f);
        if (id == 33 && c.activeWeaponCount >= 3) bonus += std::min(0.24f, 0.08f + c.activeWeaponCount * 0.025f);
        if (id == 34) bonus += 0.03f + std::min(0.16f, c.criticalChance * 0.16f);
        if (id == 35 && itemLevels[6] > 0) bonus += 0.18f + std::min(0.10f, itemLevels[6] * 0.02f);
        if (id == 36 && c.bossAlive) bonus += 0.08f;
        if (id == 37 && c.activeWeaponCount >= 2) bonus += std::min(0.18f, 0.06f + c.activeWeaponCount * 0.025f);
        if (id == 38 && (c.hpRatio < 0.80f || c.nearbyThreats >= 2)) bonus += c.hpRatio < 0.55f ? 0.20f : 0.12f;
        if (id == 39) bonus += c.resonatorCount > 0 ? 0.20f : 0.04f;
        if (id == 30) {
            if (c.hpRatio < 0.80f) bonus += 0.12f;
            if (playerHp < playerMaxHp && (itemLevels[4] > 0 || itemLevels[10] > 0 || itemLevels[22] > 0)) bonus += 0.22f;
            else if (playerHp >= playerMaxHp) bonus += 0.06f;
        }

        // Counterfactual rank count: how many upgrades remain if this card is taken now?
        if (id <= 13) {
            const int weapon = id < 7 ? id : id - 7;
            const int relic = weapon + 7;
            const int weaponAfter = itemLevels[weapon] + (id == weapon ? 1 : 0);
            const int relicAfter = itemLevels[relic] + (id == relic ? 1 : 0);
            const int ranksLeft = std::max(0, 9 - weaponAfter) + std::max(0, 4 - relicAfter);
            if (!evolvedWeapons[weapon]) {
                if (ranksLeft == 0) bonus += 0.38f;
                else if (ranksLeft <= 2) bonus += 0.22f;
                else if (ranksLeft <= 4) bonus += 0.09f;
            }
        }
        return bonus;
    }

    void getAdvisorRanking(int rankedCards[4]) const {
        if (!options.advisorEnabled) {
            for (int i = 0; i < 4; ++i) rankedCards[i] = -1;
            return;
        }
        getAdvisorRanking(rankedCards, captureAdvisorSnapshot());
    }

    void getAdvisorRanking(int rankedCards[4], const AdvisorSnapshot& context) const {
        for (int i = 0; i < 4; ++i) rankedCards[i] = -1;
        if (!options.advisorEnabled) return;
        float scores[4] = {-1.0e30f, -1.0e30f, -1.0e30f, -1.0e30f};
        bool used[4] = {false, false, false, false};
        for (int rank = 0; rank < 4; ++rank) {
            int best = -1;
            for (int card = 0; card < 4; ++card) {
                if (used[card]) continue;
                const int id = upgradeChoices[card];
                // Learned weights predate the tier and item rebalance; keep them as a signal,
                // but do not let stale Tier I preferences bury a rare high-tier card.
                const float score = 0.35f * advisorScoreForItem(id) + advisorContextBonus(id, context) + advisorTierBonus(id);
                if (best < 0 || score > scores[best]) { best = card; scores[card] = score; }
            }
            if (best >= 0) { rankedCards[rank] = best; used[best] = true; }
        }
    }

    int getAdvisorChoiceIndex() const {
        int rankedCards[4];
        getAdvisorRanking(rankedCards);
        return rankedCards[0];
    }

    std::string getAdvisorRationale(int id) const {
        return getAdvisorRationale(id, captureAdvisorSnapshot());
    }

    std::string getAdvisorRationale(int id, const AdvisorSnapshot& c) const {
        if (id < 0 || id >= ITEM_COUNT) return "вариант недоступен";
        const int level = itemLevels[id];
        const int next = level + 1;
        if (id == 10) {
            char reason[112];
            snprintf(reason, sizeof(reason), "+40 HP к максимуму и сейчас, +1.5/с • HP %d%%", (int)roundf(c.hpRatio * 100.0f));
            return reason;
        }
        if (id == 14) return c.hpRatio < 0.4f ? "страховка от смерти особенно ценна при низком HP" : "одно воскрешение; каждый ранг даёт больше HP после него";
        if (id >= 0 && id <= 13) {
            const int weapon = id < 7 ? id : id - 7;
            const int relic = weapon + 7;
            const int weaponAfter = itemLevels[weapon] + (id == weapon ? 1 : 0);
            const int relicAfter = itemLevels[relic] + (id == relic ? 1 : 0);
            const int ranksLeft = std::max(0, 9 - weaponAfter) + std::max(0, 4 - relicAfter);
            if (!evolvedWeapons[weapon] && ranksLeft == 0) return "9-й ранг пробуждает супероружие: эволюция даёт +65% силы";
            if (!evolvedWeapons[weapon] && ranksLeft <= 2) {
                char reason[80];
                snprintf(reason, sizeof(reason), "до симбиоза останется %d ранга", ranksLeft);
                return reason;
            }
        }
        if ((id == 4 || id == 6) && c.nearbyThreats >= 3) {
            char reason[88];
            snprintf(reason, sizeof(reason), "%d рядом • %s даст окно для рывка", c.nearbyThreats, id == 6 ? "заморозка" : "аура");
            return reason;
        }
        if (id == 8 && c.nearestThreat < 150.0f) {
            char reason[88];
            snprintf(reason, sizeof(reason), "угроза в %.0f ед. • скорость и рывок помогут выйти", c.nearestThreat);
            return reason;
        }
        if (id == 19) return c.bossAlive ? "босс уже здесь: прямо сейчас +20% урона ему и +5 брони за ранг" : "+20% урона боссам и +5 брони за ранг";
        if (id == 11 && c.bossNear) return "босс рядом: +12% общего урона и +8% крит. шанса";
        if (id < 7) {
            char reason[88];
            if (level == 0) snprintf(reason, sizeof(reason), "новая атака • после выбора будет %d из 7", c.weaponCount + 1);
            else snprintf(reason, sizeof(reason), "уровень %d → %d • усиливает уже работающее оружие", level, next);
            return reason;
        }
        if (id == 7) return runTime < 360.0f ? "+10% опыта сейчас — поможет быстрее открыть сборки" : "+10% опыта за ранг; забег уже поздний";
        if (id == 8) return "+14% скорости и быстрее перезаряжается рывок";
        if (id == 9) return "+45% радиуса сбора • меньше риска за самоцветами";
        if (id == 11) return "+12% общего урона и +8% крита за ранг";
        if (id == 12) return "+9% частоты атак и +10% радиуса за ранг";
        if (id == 13) return "+15% золота, но элитные враги станут опаснее";
        if (id == 15) return "+6% заряда суперумения и +4% критического шанса";
        if (id == 16) return "Тир II: +11% урона и +5% заряда суперумения за ранг";
        if (id == 17) return "Тир III: +30% крита за ранг; шанс выше 100% даёт суперкрит ×4";
        if (id == 18) return "Тир IV: +14% радиуса и +10% частоты атак за ранг";
        if (id == 20) return "+12% урона элитам за ранг";
        if (id == 21) return std::string("рывок возвращается каждые ") + std::to_string(getDashRefreshKillsRequired()) + " убийств";
        if (id == 22) return "+2% макс. HP при убийстве элиты или босса за ранг";
        if (id == 23) return "поглощает половину следующего удара, пока покров заряжается";
        if (id == 39) return c.resonatorCount > 0 ? "разрушение резонатора сейчас даст +8/12/16% заряда суперумения" : "каждый разрушенный резонатор даст +8/12/16% суперзаряда";
        if (id == 38) return "потеря HP зарядит следующий активный удар на +35/50/65% на 4,5 с";
        if (id == 37) return "добьёт обычную цель и элиту при пороге здоровья; боссы исключены";
        if (id == 36) return "перехватит обычный снаряд и направит ответный удар в ближайшего врага";
        if (id == 35) return itemLevels[6] > 0 ? "Ледяное кольцо заставит переживших заморозку выпустить осколки" : "появится сильнее рядом с Ледяным кольцом; откол не цепляется";
        if (id == 34) return std::string("криты") + " " + std::to_string((int)roundf(c.criticalChance * 100.0f)) + "% • ожог без наложения по обычным целям и элитам";
        if (id == 33) return std::string("разнообразие оружия откроет перекрёстный рикошет; активно ") + std::to_string(c.activeWeaponCount) + " видов";
        if (id == 32) return std::string("опытные самоцветы разгоняют ") + std::to_string(c.weaponCount) + " активных атак";
        if (id == 31) return "сильный добивающий удар разлетится в ближайших врагов; осколки не цепляются";
        if (id == 30) {
            char reason[112];
            const int rank = std::max(1, std::min(3, next));
            snprintf(reason, sizeof(reason), "избыток лечения даёт щит %d%% (предел %d), первым принимает урон", (int)roundf(getOverhealWardRate(rank) * 100.0f), (int)getOverhealWardCapacity(rank));
            return reason;
        }
        if (id == 29) {
            char reason[160];
            const int rank = std::max(1, std::min(3, next));
            snprintf(reason, sizeof(reason), "фокус: %d попадания раскрывают цель на %.1f с • особенно полезно по боссу", getExposureHitsRequired(rank), getExposureDuration(rank));
            return reason;
        }
        return "взвешенный выбор по текущему билду и стадии забега";
    }

    void renderLevelUpAdaptive(Renderer& r) {
        r.fillRect(0, 0, SCREEN_W, SCREEN_H, 0xFF080510, 205);
        const bool portrait = SCREEN_W < SCREEN_H;
        r.drawTextCentered(FONT_LARGE, SCREEN_W / 2, 20, "НОВЫЙ УРОВЕНЬ!", 0xFFFFE058);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 58,
            upgradeRerollUsed ? "ВЫБЕРИ ВАРИАНТ [1–4]" : "ВЫБЕРИ УЛУЧШЕНИЕ • [1–4] • [R] ЗАМЕНИТЬ", 0xFFD8D0F0);
        int ranking[4];
        const AdvisorSnapshot context = options.advisorEnabled ? captureAdvisorSnapshot() : AdvisorSnapshot{};
        getAdvisorRanking(ranking, context);
        const int advisor = ranking[0];
        if (advisor >= 0) {
            const int aw = SCREEN_W - 32;
            r.fillRoundedRect(16, 82, aw, 42, 7, 0xFF100C1D, 235);
            r.drawRoundedRect(16, 82, aw, 42, 7, 0xFF3F3555, 1, 225);
            const int item = upgradeChoices[advisor];
            std::string line = "СОВЕТ [" + std::to_string(advisor + 1) + "] " + ITEM_DEFS[item].name + " — " + getAdvisorRationale(item, context);
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 96, line.c_str(), 0xFF75F0B0);
        }
        const int gap = portrait ? 8 : 12;
        const int startY = 134;
        const int cardW = portrait ? SCREEN_W - 32 : (SCREEN_W - 3 * 16) / 2;
        const int cardH = portrait ? (SCREEN_H - startY - 18 - 3 * gap) / 4 : (SCREEN_H - startY - 20 - gap) / 2;
        hoveredCard = -1;
        for (int i = 0; i < 4; ++i) {
            const int col = portrait ? 0 : i % 2, row = portrait ? i : i / 2;
            const int x = 16 + col * (cardW + gap), y = startY + row * (cardH + gap);
            const bool hovered = mouseX >= x && mouseX <= x + cardW && mouseY >= y && mouseY <= y + cardH;
            if (hovered) hoveredCard = i;
            const int id = upgradeChoices[i], level = itemLevels[id];
            const ItemDef& def = ITEM_DEFS[id];
            const uint32_t tier = getItemTierColor(id);
            r.fillRect(x, y, cardW, cardH, hovered ? 0xFF2E2052 : 0xFF1B1430, 248);
            r.drawRect(x, y, cardW, cardH, hovered ? 0xFFFFE058 : tier, hovered ? 3 : 2);
            r.drawText(FONT_SMALL, x + 12, y + 9, def.isWeapon ? "АКТИВНОЕ ОРУЖИЕ" : "ПАССИВНЫЙ АРТЕФАКТ", def.isWeapon ? 0xFF78E0FF : 0xFFE890FF, false);
            char tierLabel[24]; snprintf(tierLabel, sizeof(tierLabel), "ТИР %d", getItemTier(id));
            r.drawTextRight(FONT_SMALL, x + cardW - 12, y + 9, tierLabel, tier, false);
            r.drawWeaponIcon(x + 30, y + 55, id, 22);
            r.drawText(FONT_MED, x + 54, y + 39, def.name, 0xFFFFFFFF, false);
            char levelText[64];
            if (level == 0) snprintf(levelText, sizeof(levelText), "НОВЫЙ ПРЕДМЕТ • УРОВЕНЬ 1");
            else snprintf(levelText, sizeof(levelText), "УРОВЕНЬ %d → %d", level, level + 1);
            r.drawText(FONT_SMALL, x + 54, y + 60, levelText, 0xFFFFD848, false);
            const int textY = y + 88;
            const std::string preview = def.isWeapon ? getWeaponUpgradePreviewText(id) : getPassiveUpgradePreviewText(id);
            const int previewEnd = r.drawTextWrapped(FONT_SMALL, x + 12, textY, cardW - 24, preview.c_str(), def.isWeapon ? 0xFF9BEAFF : 0xFFF0AEFF, false);
            const int btnY = y + cardH - 36;
            if (cardH > 205 && previewEnd + FONT_SMALL.lineHeight + 5 < btnY)
                r.drawTextWrapped(FONT_SMALL, x + 12, previewEnd + 3, cardW - 24, def.desc, 0xFFD8D4EA, false);
            r.fillRect(x + 8, btnY, cardW - 16, 28, hovered ? 0xFF6A4A22 : 0xFF2E224C);
            r.drawRect(x + 8, btnY, cardW - 16, 28, hovered ? 0xFFFFE058 : tier, 1);
            snprintf(levelText, sizeof(levelText), "ВЫБРАТЬ [%d]", i + 1);
            r.drawTextCentered(FONT_SMALL, x + cardW / 2, btnY + 7, levelText, 0xFFFFFFFF, false);
        }
        if (mouseClicked && hoveredCard >= 0) {
            sound.play(SFX_CLICK, 0.02f);
            applyUpgrade(upgradeChoices[hoveredCard]);
            sound.playUpgradeCue(); // ss_upgrade_pick.mp3 when the player ships one
            state = STATE_PLAYING;
            restoreGameplayMusicImmediately();
        }
    }

    void renderLevelUpModal(Renderer& r) {
        if (SCREEN_W < 1200) { renderLevelUpAdaptive(r); return; }
        r.fillRect(0, 0, SCREEN_W, SCREEN_H, 0xFF080510, 195);

        r.fillRoundedRect(245, 42, 790, 78, 12, 0xFF1E1436, 245);
        r.drawRoundedRect(245, 42, 790, 78, 12, 0xFFFFD040, 3);
        r.drawTextCentered(FONT_LARGE, SCREEN_W / 2, 50, "НОВЫЙ УРОВЕНЬ!", 0xFFFFE058);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 88,
            upgradeRerollUsed ? "Выберите один из четырёх вариантов [1–4]" : "4 варианта • [1–4] выбор • [R] бесплатная замена",
            0xFFD8D0F0);
        int advisorRanking[4];
        const AdvisorSnapshot advisorContext = options.advisorEnabled ? captureAdvisorSnapshot() : AdvisorSnapshot{};
        getAdvisorRanking(advisorRanking, advisorContext);
        const int advisorCard = advisorRanking[0];
        if (advisorCard >= 0) {
            r.fillRoundedRect(75, 121, 1130, 45, 7, 0xFF100C1D, 235);
            r.drawRoundedRect(75, 121, 1130, 45, 7, 0xFF3F3555, 1, 225);
            const int advisedItem = upgradeChoices[advisorCard];
            std::string advisorLine = "СОВЕТ: [" + std::to_string(advisorCard + 1) + "] " + ITEM_DEFS[advisedItem].name + " — " + getAdvisorRationale(advisedItem, advisorContext);
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 123, advisorLine.c_str(), 0xFF75F0B0);
            if (advisorRanking[1] >= 0) {
                const int alternate = advisorRanking[1];
                const int alternateItem = upgradeChoices[alternate];
                std::string alternateLine = "ЕЩЁ ВАРИАНТ: [" + std::to_string(alternate + 1) + "] " + ITEM_DEFS[alternateItem].name + " — " + getAdvisorRationale(alternateItem, advisorContext);
                r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 148, alternateLine.c_str(), 0xFFD5C9EB);
            }
        }

        int cardW = 275, cardH = 440;
        int startX = 55, gap = 15, cardY = 171;
        hoveredCard = -1;

        for (int i = 0; i < 4; ++i) {
            int cx = startX + i * (cardW + gap);
            bool hovered = (mouseX >= cx && mouseX <= cx + cardW && mouseY >= cardY && mouseY <= cardY + cardH);
            if (hovered) hoveredCard = i;

            int itemId = upgradeChoices[i];
            const ItemDef& def = ITEM_DEFS[itemId];
            int curLvl = itemLevels[itemId];

            uint32_t bgTop = hovered ? 0xFF2E2052 : 0xFF1C1432;
            uint32_t bgBot = hovered ? 0xFF1A1230 : 0xFF120C20;
            const uint32_t tierColor = getItemTierColor(itemId);
            const uint32_t buttonBorderColor = hovered ? 0xFFFFE058 : tierColor;

            r.fillGradientV(cx, cardY, cardW, cardH, bgTop, bgBot, 248);
            r.drawRoundedRect(cx, cardY, cardW, cardH, 12, tierColor, 3);
            if (hovered) r.drawRoundedRect(cx + 5, cardY + 5, cardW - 10, cardH - 10, 10, 0xFFFFE058, 1);
            // A compact colored tier label makes the frame palette explicit, including on weapon cards.
            static const char* tierNames[6] = {"—", "I ТИР", "II ТИР", "III ТИР", "IV ТИР", "V ТИР"};
            const int tierTagX = cx + cardW / 2 + 42, tierTagY = cardY + 82;
            r.fillRoundedRect(tierTagX, tierTagY, 58, 24, 6, tierColor);
            r.drawRoundedRect(tierTagX, tierTagY, 58, 24, 6, 0xFFEFE8FF, 1);
            // The dark label already contrasts with every tier swatch; a default black text shadow made it look doubled.
            r.drawTextCentered(FONT_SMALL, tierTagX + 29, tierTagY + 6, tierNames[getItemTier(itemId)], 0xFF171020, false);

            // Type Badge
            r.fillRoundedRect(cx + 20, cardY + 18, cardW - 40, 26, 6, def.isWeapon ? 0xFF183858 : 0xFF381854);
            r.drawTextCentered(FONT_SMALL, cx + cardW / 2, cardY + 22,
                def.isWeapon ? "АКТИВНОЕ ОРУЖИЕ" : "ПАССИВНЫЙ АРТЕФАКТ",
                def.isWeapon ? 0xFF78E0FF : 0xFFE890FF);
            if (i == advisorCard) r.drawTextRight(FONT_SMALL, cx + cardW - 16, cardY + 51, "СОВЕТ ИИ", 0xFF75F0B0);

            // Weapon / Relic Icon
            r.drawWeaponIcon(cx + cardW / 2, cardY + 95, itemId, 34);

            // Level badge
            char lvlBuf[64];
            if (curLvl == 0) {
                snprintf(lvlBuf, sizeof(lvlBuf), "НОВЫЙ ПРЕДМЕТ • УР. 1");
            } else {
                snprintf(lvlBuf, sizeof(lvlBuf), "Уровень %d  →  Уровень %d", curLvl, curLvl + 1);
            }
            r.drawTextCentered(FONT_SMALL, cx + cardW / 2, cardY + 146, lvlBuf, 0xFFFFD848);

            // Item Name
            r.drawTextCentered(FONT_MED, cx + cardW / 2, cardY + 176, def.name, 0xFFFFFFFF);

            // Level Pips (8 pips)
            int pipStartX = cx + (cardW - (8 * 22 - 6)) / 2;
            for (int p = 0; p < 8; ++p) {
                uint32_t pCol = (p < curLvl) ? 0xFFFFD040 : (p == curLvl ? 0xFF40FF90 : 0xFF2A2240);
                r.fillRoundedRect(pipStartX + p * 22, cardY + 214, 16, 10, 3, pCol);
            }

            // Active weapon ranks preview their simultaneous damage, cadence, area and next projectile breakpoint.
            int descriptionY = cardY + 245;
            if (def.isWeapon) {
                const std::string weaponPreview = getWeaponUpgradePreviewText(itemId);
                const int previewEnd = r.drawTextWrapped(FONT_SMALL, cx + 22, cardY + 229, cardW - 44,
                    weaponPreview.c_str(), 0xFF9BEAFF, true);
                descriptionY = std::max(cardY + 267, previewEnd + 5);
            } else {
                const std::string passivePreview = getPassiveUpgradePreviewText(itemId);
                const int previewEnd = r.drawTextWrapped(FONT_SMALL, cx + 22, cardY + 229, cardW - 44,
                    passivePreview.c_str(), 0xFFF0AEFF, true);
                descriptionY = std::max(cardY + 267, previewEnd + 5);
            }
            r.drawTextWrapped(FONT_SMALL, cx + 22, descriptionY, cardW - 44, def.desc, 0xFFD8D4EA, true);

            // Bottom Action Button inside card
            int btnY = cardY + cardH - 58;
            r.fillRoundedRect(cx + 28, btnY, cardW - 56, 40, 8, hovered ? 0xFFD89820 : 0xFF2E224C);
            r.drawRoundedRect(cx + 28, btnY, cardW - 56, 40, 8, buttonBorderColor, 2);
            snprintf(lvlBuf, sizeof(lvlBuf), "ВЫБРАТЬ [%d]", i + 1);
            r.drawTextCentered(FONT_MED, cx + cardW / 2, btnY + 8, lvlBuf, 0xFFFFFFFF);
        }

        if (mouseClicked && hoveredCard >= 0) {
            sound.play(SFX_CLICK, 0.02f);
            applyUpgrade(upgradeChoices[hoveredCard]);
            sound.playUpgradeCue(); // ss_upgrade_pick.mp3 when the player ships one
            state = STATE_PLAYING;
            restoreGameplayMusicImmediately();
        }
    }

    void renderChestModal(Renderer& r) {
        r.fillRect(0, 0, SCREEN_W, SCREEN_H, 0xFF080510, 205);
        int mw = 680, mh = 450;
        int mx = (SCREEN_W - mw) / 2, my = (SCREEN_H - mh) / 2;

        r.fillGradientV(mx, my, mw, mh, 0xFF2C1B38, 0xFF140D22, 250);
        r.drawRoundedRect(mx, my, mw, mh, 14, 0xFFFFD040, 3);

        r.drawTextCentered(FONT_LARGE, SCREEN_W / 2, my + 24, "СОКРОВИЩЕ ДРЕВНИХ!", 0xFFFFE058);
        char buf[128];
        snprintf(buf, sizeof(buf), "Получено золота: +%d   •   Улучшений предметов: %d", chestBonusGold, (int)chestRewards.size());
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, my + 70, buf, 0xFF60F098);

        int n = (int)chestRewards.size();
        const int itemSpacing = 200, cardW = 180;
        const int startX = SCREEN_W / 2 - ((n - 1) * itemSpacing) / 2;
        for (int i = 0; i < n; ++i) {
            const ChestReward& reward = chestRewards[i];
            const int id = reward.itemId;
            const int ix = startX + i * itemSpacing, iy = my + 168;
            r.fillRoundedRect(ix - cardW / 2, iy - 22, cardW, 176, 10, 0xFF1A122A, 235);
            r.drawRoundedRect(ix - cardW / 2, iy - 22, cardW, 176, 10, getItemTierColor(id), 2);
            r.drawWeaponIcon(ix, iy, id, 40);
            r.drawTextWrapped(FONT_SMALL, ix - cardW / 2 + 8, iy + 44, cardW - 16, ITEM_DEFS[id].name, 0xFFFFFFFF, true);
            snprintf(buf, sizeof(buf), "%d → %d УРОВЕНЬ", reward.fromLevel, reward.toLevel);
            r.drawTextCentered(FONT_SMALL, ix, iy + 108, buf, 0xFFFFD848);
        }

        int bx = SCREEN_W / 2 - 150, by = my + mh - 74, bw = 300, bh = 48;
        bool hov = (mouseX >= bx && mouseX <= bx + bw && mouseY >= by && mouseY <= by + bh);
        r.fillRoundedRect(bx, by, bw, bh, 10, hov ? 0xFFE8A020 : 0xFFB87018);
        r.drawRoundedRect(bx, by, bw, bh, 10, 0xFFFFF090, 2);
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, by + 12, "ЗАБРАТЬ [ПРОБЕЛ / ЛКМ]", 0xFFFFFFFF);

        if (mouseClicked && hov) finishTreasureChest();
    }

    void renderPauseModal(Renderer& r) {
        r.fillRect(0, 0, SCREEN_W, SCREEN_H, 0xFF080512, 205);
        int mw = 720, mh = 500;
        int mx = (SCREEN_W - mw) / 2, my = (SCREEN_H - mh) / 2;
        r.fillGradientV(mx, my, mw, mh, 0xFF22183C, 0xFF120C22, 250);
        r.drawRoundedRect(mx, my, mw, mh, 14, 0xFF8870C8, 3);

        int shown = 0;
        for (int i = 0; i < ITEM_COUNT; ++i) shown += itemLevels[i] > 0;
        char loadoutTitle[96];
        snprintf(loadoutTitle, sizeof(loadoutTitle), "СБОРКА ГЕРОЯ  •  %d / %d ПРЕДМЕТОВ", shown, getBuildLimit());
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, my + 66, loadoutTitle, 0xFFC8C0E0);

        // Two wide columns with one compact line per item keep every possible slot visible.
        const int gx = mx + 36, gy = my + 102, itemW = 312, rowStep = 23;
        int slot = 0;
        for (int i = 0; i < ITEM_COUNT; ++i) {
            if (itemLevels[i] <= 0) continue;
            const int col = slot % 2, row = slot / 2;
            const int ix = gx + col * 328, iy = gy + row * rowStep;
            r.fillRoundedRect(ix, iy, itemW, 21, 4, 0xFF18122C);
            r.drawRoundedRect(ix, iy, itemW, 21, 4, getItemTierColor(i), 2);
            if (i < 7 && evolvedWeapons[i]) r.drawRoundedRect(ix + 3, iy + 3, itemW - 6, 15, 3, 0xFFFFC850, 1);
            r.drawWeaponIcon(ix + 14, iy + 10, i, 14, i < 7 && evolvedWeapons[i]);
            r.drawText(FONT_SMALL, ix + 30, iy + 1, ITEM_DEFS[i].name, 0xFFFFFFFF);
            char lbuf[16];
            snprintf(lbuf, sizeof(lbuf), "%d/%d", itemLevels[i], getItemMaxLevel(i));
            r.drawTextRight(FONT_SMALL, ix + itemW - 8, iy + 1, lbuf, 0xFFFFD848);
            ++slot;
        }

        UIButton btns[5] = {
            {SCREEN_W / 2 - 352, my + mh - 125, 220, 46, "ПРОДОЛЖИТЬ [ESC]", 1},
            {SCREEN_W / 2 - 116, my + mh - 125, 232, 46, sound.enabled ? "ЗВУК: ВКЛ [M]" : "ЗВУК: ВЫКЛ [M]", 2},
            {SCREEN_W / 2 + 132, my + mh - 125, 220, 46, "НАСТРОЙКИ [O]", 4},
            {SCREEN_W / 2 - 150, my + mh - 64,  300, 46, "ВЫЙТИ В ГЛАВНОЕ МЕНЮ", 3},
            {mx + mw - 214, my + 14, 200, 34, "ОТЛАДКА [F9]", 5}
        };

        for (int i = 0; i < 5; ++i) {
            const auto& b = btns[i];
            bool hov = (mouseX >= b.x && mouseX <= b.x + b.w && mouseY >= b.y && mouseY <= b.y + b.h);
            r.fillRoundedRect(b.x, b.y, b.w, b.h, 8, hov ? 0xFF4A3480 : 0xFF281C48);
            r.drawRoundedRect(b.x, b.y, b.w, b.h, 8, hov ? 0xFFFFE068 : 0xFF7058A8, 2);
            r.drawTextCentered(FONT_MED, b.x + b.w / 2, b.y + 11, b.label, 0xFFFFFFFF);
            if (mouseClicked && hov) {
                sound.play(SFX_CLICK, 0.02f);
                if (b.id == 1) state = STATE_PLAYING;
                else if (b.id == 2) { sound.enabled = !sound.enabled; saveOptions(); }
                else if (b.id == 3) { writeSave(); state = STATE_MENU; }
                else if (b.id == 4) { settingsReturnState = STATE_PAUSED; state = STATE_SETTINGS; }
                else if (b.id == 5) openCheatMenu();
            }
        }
    }

    void renderDeathModal(Renderer& r) {
        int alpha = 115 + (int)(80.0f * (1.0f - playerDeathTimer / 1.6f));
        r.fillRect(0, 0, SCREEN_W, SCREEN_H, 0xFF1B0614, alpha);
        int cx = SCREEN_W / 2, cy = SCREEN_H / 2;
        int pulse = 70 + (int)(16.0f * sinf(globalTime * 11.0f));
        r.addGlow(cx, cy, pulse + 72, 0xFFFF2648, 190);
        r.drawCircleRing(cx, cy, pulse, 6, 0xFFFF5678, 230, true);
        r.drawTextCentered(FONT_LARGE, cx, cy - 38, "ТВОЯ ИСКРА ПОГАСЛА", 0xFFFF7890);
        r.drawTextCentered(FONT_MED, cx, cy + 8, "Души павших остаются в Кузнице...", 0xFFE8D8F0);
    }

    // Финал: что именно игрок сделал с копией, и мораль дела.
    // Плюс «НОВАЯ НОЧЬ»: второй проход по всем актам с ×N золота и другой веткой.
    void beginNewCampaignCycle() {
        save.campaignCycle = std::min(3, save.campaignCycle + 1);
        save.campaignMissionsDone = 0;
        save.campaignTrack = 0; // в новую ночь можно пойти другим следом
        save.storySealMask = 0;
        save.storyEntryMask = 0;
        save.storyChapter = 0;
        save.bossesDefeated = 0;
        save.seenActIntros = 0;
        writeSave();
        sound.play(SFX_LEVELUP, 0.05f);
        openStoryMissionSelect();
    }

    void renderCampaignEnding(Renderer& r) {
        renderSubmenuFrame(r, "СЮЖЕТ ЗАВЕРШЁН", "ПАМЯТЬ ВОЗВРАЩЕНА", 0xFF9FE5A4);
        // Два финала вместо одного: выбор в акте IV меняет последнюю строку.
        const int keepCase = campaignMissionWithBrief(22);
        const int burnCase = campaignMissionWithBrief(23);
        const bool keptCopy = keepCase >= 0 && isCampaignMissionCompleted(keepCase);
        const bool burnedCopy = burnCase >= 0 && isCampaignMissionCompleted(burnCase);
        const char* epilogue[3];
        if (keptCopy) {
            epilogue[0] = "Копия осталась в архиве — как запись, а не как человек. Ты подписал её своим именем и запретил отвечать за тебя.";
            epilogue[1] = "Резервный приказ отменён. Архив больше не повторяет последние секунды умерших: он хранит их открыто и молчит.";
            epilogue[2] = "«Мы не вернули их. Мы вернули себе право помнить» — и право наконец перестать слушать запись.";
        } else if (burnedCopy) {
            epilogue[0] = "Копия сгорела вместе с приказом. В архиве стало на один голос меньше — и впервые за ночь стало тихо.";
            epilogue[1] = "Имена вернулись в общую книгу. Погибшие не вернулись, но их больше не заставляют говорить.";
            epilogue[2] = "«Забыть — не преступление. Преступление — решать за других, что они должны помнить».";
        } else {
            epilogue[0] = "Приказ отменён. Архив хранит копии, но больше не выдаёт их за людей.";
            epilogue[1] = "Умершие не вернулись. Живые снова могут помнить их без разрешения машины.";
            epilogue[2] = "«Мы не вернули их. Мы вернули себе право помнить».";
        }
        const bool portraitEnding = SCREEN_W < SCREEN_H;
        int y = 150;
        for (int i = 0; i < 3; ++i) {
            const uint32_t color = i == 2 ? 0xFFFFE07A : 0xFFE8F0E7;
            const BakedFont& font = i == 2 ? FONT_MED : FONT_SMALL;
            if (SCREEN_W < 1200) {
                r.drawTextWrapped(font, 40, y, SCREEN_W - 80, epilogue[i], color, false);
                y += i == 2 ? 46 : 40;
            } else {
                r.drawTextCentered(font, SCREEN_W / 2, y, epilogue[i], color);
                y += i == 2 ? 40 : 30;
            }
        }
        char buf[160];
        snprintf(buf, sizeof(buf), "ВРЕМЯ %02d:%02d   •   УБИЙСТВ %d   •   БОССОВ В ЭТОМ ЗАБЕГЕ %d   •   ЗОЛОТО В КАЗНЕ %d",
                 (int)runTime / 60, (int)runTime % 60, kills, runBossKills, save.gold);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, y + 18, buf, 0xFFFFD858, false);
        const int nextMultiplier = std::min(4, 1 + std::min(3, save.campaignCycle) + 1);
        snprintf(buf, sizeof(buf), "НОВАЯ НОЧЬ: все дела заново, другой след, золото в делах ×%d", nextMultiplier);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, y + 40, buf, 0xFFB9E7FF, false);

        const int bw = portraitEnding ? std::min(320, (SCREEN_W - 60) / 2) : 300, bh = 56;
        const int gap = 18, by = portraitEnding ? SCREEN_H - 96 : 470;
        const int totalW = 3 * bw + 2 * gap;
        const int bx = (SCREEN_W - totalW) / 2;
        struct { const char* label; int id; } buttons[3] = {
            {"НОВАЯ НОЧЬ", 0}, {"В МЕНЮ", 1}, {"БЕСКОНЕЧНЫЙ РЕЖИМ", 2}
        };
        for (int i = 0; i < 3; ++i) {
            const int x = bx + i * (bw + gap);
            const bool hov = mouseX >= x && mouseX <= x + bw && mouseY >= by && mouseY <= by + bh;
            const uint32_t fill = i == 0 ? (hov ? 0xFF4B3672 : 0xFF2E2350) : (i == 1 ? (hov ? 0xFF385B40 : 0xFF23402D) : (hov ? 0xFF286B82 : 0xFF1D455C));
            const uint32_t edge = i == 0 ? 0xFFFFD858 : (i == 1 ? 0xFF9FE5A4 : 0xFF8DEEFF);
            r.fillRoundedRect(x, by, bw, bh, 10, fill);
            r.drawRoundedRect(x, by, bw, bh, 10, edge, 2);
            r.drawTextCentered(FONT_MED, x + bw / 2, by + 16, buttons[i].label, 0xFFFFFFFF);
            if (mouseClicked && hov) {
                if (buttons[i].id == 0) beginNewCampaignCycle();
                else if (buttons[i].id == 1) state = STATE_MENU;
                else { gameMode = MODE_ENDLESS; openHeroSelectForMode(MODE_ENDLESS); }
            }
        }
    }

    void renderExtractionScreen(Renderer& r) {
        renderSubmenuFrame(r, "ПОРТАЛ СОХРАНИЛ ТВОЙ ЗАБЕГ", "ДОБЫЧА СОХРАНЕНА", 0xFF64D9F4);
        if (SCREEN_W < 1200) r.drawTextWrapped(FONT_MED, 40, 154, SCREEN_W - 80, gameMode == MODE_STORY ? "Кузница сохранила добычу и найденные улики; печати остались целы." : "Ты покинул уровень до гибели и забрал добычу.", 0xFFE3EAF5, false);
        else r.drawTextCentered(FONT_MED, SCREEN_W / 2, 154, gameMode == MODE_STORY ? "Кузница сохранила добычу и найденные улики; печати остались целы." : "Ты покинул уровень до гибели и забрал добычу.", 0xFFE3EAF5);
        char buf[128];
        snprintf(buf, sizeof(buf), "ВРЕМЯ %02d:%02d   •   УБИЙСТВ %d   •   БОССОВ %d", (int)runTime / 60, (int)runTime % 60, kills, runBossKills);
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, 300, buf, 0xFFFFE078);
        snprintf(buf, sizeof(buf), "СОБРАНО ЗОЛОТА: %d   •   ПОРТАЛ ДОБАВИЛ +50%%   •   В КАЗНЕ: %d", runGold, save.gold);
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, 344, buf, 0xFFFFD858);
        const int bw = std::min(380, SCREEN_W - 48), bh = 56;
        const int bx = (SCREEN_W - bw) / 2, by = SCREEN_W < SCREEN_H ? SCREEN_H - 96 : 460;
        bool hov = mouseX >= bx && mouseX <= bx + bw && mouseY >= by && mouseY <= by + bh;
        r.fillRoundedRect(bx, by, bw, bh, 10, hov ? 0xFF286B82 : 0xFF1D455C);
        r.drawRoundedRect(bx, by, bw, bh, 10, 0xFF8DEEFF, 2);
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, by + 16, "В МЕНЮ [ENTER]", 0xFFFFFFFF);
        if (mouseClicked && hov) state = STATE_MENU;
    }

    void renderGameOverModal(Renderer& r) {
        r.fillRect(0, 0, SCREEN_W, SCREEN_H, 0xFF0A040C, 215);
        int mw = 660, mh = 460;
        int mx = (SCREEN_W - mw) / 2, my = (SCREEN_H - mh) / 2;
        r.fillGradientV(mx, my, mw, mh, 0xFF2C1020, 0xFF140814, 250);
        r.drawRoundedRect(mx, my, mw, mh, 14, 0xFFFF3858, 3);

        r.drawTextCentered(FONT_LARGE, SCREEN_W / 2, my + 26, "ГЕРОЙ ПАЛ В БОЮ", 0xFFFF4868);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, my + 68, "Но собранное золото сохранено в Кузнице Душ!", 0xFFE8C8D8);

        char buf[128];
        auto compactCounter = [](long long value) {
            char out[32];
            if (value >= 1000000000LL) snprintf(out, sizeof(out), "%.1fB", value / 1000000000.0);
            else if (value >= 1000000LL) snprintf(out, sizeof(out), "%.1fM", value / 1000000.0);
            else if (value >= 10000LL) snprintf(out, sizeof(out), "%.1fK", value / 1000.0);
            else snprintf(out, sizeof(out), "%lld", value);
            return std::string(out);
        };
        int mins = (int)(runTime / 60.0f), secs = ((int)runTime) % 60;
        int sy = my + 112;
        auto drawStatRow = [&](const char* label, const char* val, uint32_t col) {
            r.fillRoundedRect(mx + 60, sy, mw - 120, 36, 6, 0xFF1A0D1C);
            const int labelW = r.textWidth(FONT_MED, label), valueW = r.textWidth(FONT_MED, val);
            const BakedFont& font = (labelW + valueW + 28 <= mw - 160) ? FONT_MED : FONT_SMALL;
            r.drawText(font, mx + 80, sy + 8, label, 0xFFD8C8E0);
            r.drawTextRight(font, mx + mw - 80, sy + 8, val, col);
            sy += 44;
        };

        snprintf(buf, sizeof(buf), "%02d:%02d (Волна %d)", mins, secs, waveNumber);
        drawStatRow("Время выживания:", buf, 0xFFFFE068);
        snprintf(buf, sizeof(buf), "%s (Уровень: %s)", compactCounter(kills).c_str(), compactCounter(playerLevel).c_str());
        drawStatRow("Уничтожено монстров:", buf, 0xFFFF8098);
        snprintf(buf, sizeof(buf), "%d / 4", runBossKills);
        drawStatRow("Повержено боссов:", buf, 0xFFFFB848);
        snprintf(buf, sizeof(buf), "%s", compactCounter((long long)roundf(totalDamageDealt)).c_str());
        drawStatRow("Всего нанесено урона:", buf, 0xFF70E8FF);
        snprintf(buf, sizeof(buf), "+%s (Казна: %s)", compactCounter(runGold).c_str(), compactCounter(save.gold).c_str());
        drawStatRow("Добыто золота за бой:", buf, 0xFFFFD838);

        UIButton btns[3] = {
            {mx + 50,  my + mh - 78, 175, 48, "ЗАНОВО", 1},
            {mx + 242, my + mh - 78, 175, 48, "КУЗНИЦА ДУШ", 2},
            {mx + 435, my + mh - 78, 175, 48, "В МЕНЮ", 3}
        };
        for (int i = 0; i < 3; ++i) {
            const auto& b = btns[i];
            bool hov = (mouseX >= b.x && mouseX <= b.x + b.w && mouseY >= b.y && mouseY <= b.y + b.h);
            r.fillRoundedRect(b.x, b.y, b.w, b.h, 8, hov ? 0xFFC82848 : 0xFF481830);
            r.drawRoundedRect(b.x, b.y, b.w, b.h, 8, hov ? 0xFFFFE068 : 0xFFFF5878, 2);
            r.drawTextCentered(FONT_MED, b.x + b.w / 2, b.y + 12, b.label, 0xFFFFFFFF);
            if (mouseClicked && hov) {
                sound.play(SFX_CLICK, 0.02f);
                if (b.id == 1) { if (gameMode == MODE_STORY) openStoryMissionSelect(); else startSelectedRun(); }
                else if (b.id == 2) state = STATE_SHOP;
                else if (b.id == 3) state = STATE_MENU;
            }
        }
    }

    void openHeroSelectForMode(int mode) {
        gameMode = mode == MODE_ENDLESS ? MODE_ENDLESS : MODE_STORY;
        state = STATE_HERO_SELECT;
    }

    void confirmHeroSelection() {
        if (gameMode == MODE_STORY) openStoryMissionSelect();
        else startSelectedRun();
    }

    // The newest case the player can actually take: the first open one, or — when every
    // reachable case is closed — the last case. Opening the campaign always lands here so
    // the player never has to scroll back from the very first mission of the act.
    int newestCampaignMission() const {
        int lastUnlocked = 0;
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i) {
            if (!isCampaignMissionUnlocked(i)) continue;
            if (!isCampaignMissionCompleted(i)) return i;
            lastUnlocked = i;
        }
        // Акт I закрыт, а след ещё не выбран: ставим курсор на первую дверь
        // следа А, чтобы кнопка предлагала выбор, а не «дело закрыто».
        if (!isCampaignTrackChosen()) {
            const int firstDoor = campaignActLaneMission(2, 1, 0);
            if (firstDoor >= 0 && isCampaignMissionCompleted(campaignActLaneMission(1, 0,
                    campaignActLaneCount(1, 0) - 1)))
                return firstDoor;
        }
        return lastUnlocked;
    }

    void openStoryMissionSelect() {
        gameMode = MODE_STORY;
        // Always land on the newest open case; the previous selection is not carried over,
        // because coming back from the menu usually means "continue the campaign".
        storyMissionCursor = newestCampaignMission();
        selectedStoryMission = storyMissionCursor;
        campaignActView = campaignActOf(storyMissionCursor);
        state = STATE_MISSION_SELECT;
        // Opening the campaign reads the case you are standing on, so the description is
        // not just text you have to read: it is spoken like a case file.
        speakCaseBriefing(storyMissionCursor, true);
        briefPreviewMission = storyMissionCursor;
    }

    void launchStoryMission(int mission) {
        if (mission < 0 || mission >= CAMPAIGN_MISSION_COUNT || !isCampaignMissionUnlocked(mission)) return;
        selectedStoryMission = mission;
        gameMode = MODE_STORY;
        startSelectedRun();
    }

    uint32_t getCampaignAccent(int mission) const {
        if (mission < 0 || mission >= CAMPAIGN_MISSION_COUNT) return 0xFFFFD878;
        const int act = campaignActOf(mission);
        const uint32_t actColor = CAMPAIGN_ACT_COLORS[act - 1];
        // Branch cases (kind 3) sit between two colours: keep them readable but distinct.
        if (CAMPAIGN_MISSIONS[mission].kind == 3) return 0xFFE9A0E0;
        if (CAMPAIGN_MISSIONS[mission].kind == 2) return 0xFFA8B8FF;
        return actColor;
    }

    void drawMissionIllustration(Renderer& r, int missionIndex, int x, int y, int w, int h) {
        missionIndex = std::max(0, std::min(CAMPAIGN_MISSION_COUNT - 1, missionIndex));
        static const uint32_t skyTop[CAMPAIGN_ACT_COUNT] = {0xFF2A1926,0xFF17251D,0xFF152936,0xFF321A19,0xFF24152F};
        static const uint32_t skyBottom[CAMPAIGN_ACT_COUNT] = {0xFF13101A,0xFF101812,0xFF101B25,0xFF190E16,0xFF110D1B};
        const int actIndex = campaignActOf(missionIndex) - 1;
        const uint32_t accent = getCampaignAccent(missionIndex);
        r.fillRoundedRect(x, y, w, h, 10, skyBottom[actIndex]);
        r.fillGradientV(x + 1, y + 1, w - 2, h - 2, skyTop[actIndex], skyBottom[actIndex], 255);

        // A compact, screen-clipped scene: horizon, landmark shapes, magical glyph and built-in character art.
        const int horizon = y + h - 48;
        const int moonX = x + w - 120;
        const int moonY = y + 48;
        r.addGlow(moonX, moonY, 74, accent, campaignActOf(missionIndex) == 5 ? 70 : 46);
        r.fillCircle(moonX, moonY, campaignActOf(missionIndex) == 5 ? 27 : 22, accent, 120);
        r.drawCircleRing(moonX, moonY, 37 + (int)(sinf(globalTime * 0.7f) * 2.0f), 2, accent, 100, true);

        for (int i = 0; i < 8; ++i) {
            const int bx = x + i * (w / 7) - 14;
            const int bh = 36 + (i * 31) % 76;
            const int actForStone = campaignActOf(missionIndex);
            const uint32_t stone = actForStone == 3 ? 0xFF183343 : (actForStone == 2 ? 0xFF202D24 : 0xFF211A28);
            r.fillRect(bx, horizon - bh, 48, bh + 42, stone, 220);
            r.fillRect(bx + 8, horizon - bh + 11, 32, 5, 0xFF070911, 190);
            if ((i + missionIndex) % 2 == 0) r.fillRect(bx + 18, horizon - bh + 24, 5, 19, accent, 100);
        }
        r.fillRect(x + 2, horizon, w - 4, h - (horizon - y) - 2, 0xFF090A12, 228);
        for (int i = 0; i < 12; ++i) {
            const int sx = x + 12 + i * (w - 24) / 12;
            r.drawLine(sx, horizon + 10 + (i % 3) * 5, sx + 44, horizon + 10 + (i % 3) * 5,
                       (i % 2) ? accent : 0xFF6B5D75, 2, 72, true);
        }

        const int glyphX = x + 300;
        const int glyphY = y + h / 2 + 4;
        r.addGlow(glyphX, glyphY, 92, accent, 38);
        r.drawCircleRing(glyphX, glyphY, 58 + (int)(sinf(globalTime * 1.1f) * 2.0f), 3, accent, 155, true);
        r.drawCircleRing(glyphX, glyphY, 36, 2, 0xFFFFE5A0, 135, true);
        for (int ray = 0; ray < 8; ++ray) {
            const float a = ray * 0.785398f + globalTime * 0.07f;
            r.drawLine(glyphX + (int)(cosf(a) * 40), glyphY + (int)(sinf(a) * 40),
                       glyphX + (int)(cosf(a) * 54), glyphY + (int)(sinf(a) * 54), accent, 2, 140, true);
        }

        // Act-specific foreground motif: the campaign moves from the gate to the heart
        // of the Archive, and the illustration has to move with it.
        const int act = campaignActOf(missionIndex);
        const int slot = campaignActProgressIndex(missionIndex);
        if (act == 1) { // burnt gate, ash over the post
            r.drawLine(x + 105, horizon, x + 123, y + 60, 0xFF9B7E58, 8, 210);
            r.drawLine(x + 182, horizon, x + 163, y + 60, 0xFF9B7E58, 8, 210);
            r.drawLine(x + 123, y + 60, x + 163, y + 60, 0xFFFFC848, 4, 150, true);
            for (int i = 0; i < 26; ++i) {
                const int ax = x + 40 + (i * 47 + slot * 31) % std::max(1, w - 80);
                const int ay = horizon - ((i * 29 + slot * 17) % 120);
                r.fillRect(ax, ay, 3, 3, 0xFFCBBFC0, 120);
            }
        } else if (act == 2) { // crypt flasks, voices in glass
            for (int i = 0; i < 4; ++i) {
                const int fx = x + 190 + i * 48;
                r.fillRoundedRect(fx, y + 92 + (i % 2) * 15, 23, 55, 7, 0xFF263A30, 225);
                r.drawRoundedRect(fx, y + 92 + (i % 2) * 15, 23, 55, 7, 0xFF8EE879, 1, 190);
                r.fillRect(fx + 7, y + 82 + (i % 2) * 15, 9, 12, 0xFFB5D78D, 210);
                r.addGlow(fx + 12, y + 124 + (i % 2) * 15, 19, 0xFF8EE879, 65);
            }
            if (slot % 2 == 1) // the copy of the sister: one flask is brighter than the others
                r.addGlow(x + 214, y + 150, 46, 0xFFB5FFD0, 90);
        } else if (act == 3) { // flooded registry: water lines over stone shelves
            for (int i = 0; i < 4; ++i)
                r.drawLine(x + 35, horizon + 8 + i * 8, x + w - 35, horizon + 8 + i * 8,
                           0xFF58BFD4, 2, 84 + i * 8, true);
            for (int i = 0; i < 5; ++i) {
                const int sx = x + 150 + i * 62;
                r.fillRect(sx, y + 74 + (i % 2) * 12, 34, 66, 0xFF1B3040, 230);
                r.drawLine(sx + 5, y + 88 + (i % 2) * 12, sx + 29, y + 88 + (i % 2) * 12, 0xFF8FD8E8, 1, 120, true);
                r.drawLine(sx + 5, y + 100 + (i % 2) * 12, sx + 29, y + 100 + (i % 2) * 12, 0xFF8FD8E8, 1, 90, true);
            }
        } else if (act == 4) { // archive throne and the faked witness
            r.addGlow(x + 330, horizon + 5, 70, 0xFFFF612F, 58);
            for (int i = 0; i < 6; ++i)
                r.drawLine(x + 202 + i * 34, y + 18, x + 209 + i * 34, horizon - 2, 0xFFB56A45, 2, 115);
            if (slot >= 3) // copy of the witness standing in the light
                r.fillRoundedRect(x + 118, y + 96, 46, 84, 10, 0xFFE8DCFF, 38);
        } else { // heart of the archive: three orders in a ring
            r.drawCircleRing(glyphX, glyphY, 74, 3, 0xFFFFE5A0, 175, true);
            r.drawLine(glyphX - 95, glyphY, glyphX + 95, glyphY, 0xFFFFE5A0, 2, 125, true);
            for (int i = 0; i < 3; ++i) {
                const float a = i * 2.0944f + globalTime * 0.35f;
                r.fillCircle(glyphX + (int)(cosf(a) * 74), glyphY + (int)(sinf(a) * 74), 6, accent, 210);
            }
        }

        const int frame = ((int)(globalTime * 3.0f)) & 1;
        r.drawBakedSpriteScaled(HERO_SPRITES[selectedHero % 5][frame], x + 56, y + h - 106, 66, 80, 245);
        // The dossier card shows the same guardians the fight does, drawn by the same
        // routine, so the briefing picture can never disagree with the arena.
        if (CAMPAIGN_MISSIONS[missionIndex].kind == 1) {
            const float scales[3] = {0.72f, 0.86f, 0.72f};
            const int centers[3] = {x + w - 250, x + w - 164, x + w - 76};
            const int roster[3] = {3, 4, 5};
            for (int i = 0; i < 3; ++i)
                r.drawBossFigure(centers[i], y + h - 14 - (int)(62.0f * scales[i]), roster[i],
                                 globalTime + i * 0.6f, false, false, true, scales[i]);
        } else {
            r.addGlow(x + w - 136, y + h - 104, 90, accent, 74);
            r.drawBossFigure(x + w - 136, y + h - 66,
                             CAMPAIGN_MISSIONS[missionIndex].bossType % 6, globalTime, false, false, false, 1.02f);
        }
        r.drawRoundedRect(x, y, w, h, 10, accent, 1, 210);
    }

    static const char* romanAct(int act) {
        static const char* numbers[CAMPAIGN_ACT_COUNT] = {"I", "II", "III", "IV", "V"};
        return numbers[std::max(0, std::min(CAMPAIGN_ACT_COUNT - 1, act - 1))];
    }

    int campaignActMissionAt(int act, int slot) const {
        int seen = 0;
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i) {
            if (CAMPAIGN_MISSIONS[i].act != act) continue;
            if (seen == slot) return i;
            ++seen;
        }
        return -1;
    }

    // Shortens a UTF-8 string so it fits maxWidth without splitting a character.
    std::string fitText(Renderer& r, const BakedFont& font, const char* text, int maxWidth) const {
        std::string out = text ? text : "";
        if (maxWidth <= 0) return "";
        if (r.textWidth(font, out.c_str()) <= maxWidth) return out;
        while (!out.empty()) {
            size_t cut = out.size() - 1;
            while (cut > 0 && ((unsigned char)out[cut] & 0xC0) == 0x80) --cut;
            out.resize(cut);
            const std::string withDots = out + "…";
            if (r.textWidth(font, withDots.c_str()) <= maxWidth) return withDots;
        }
        return "…";
    }

    int campaignActSlotOf(int act, int track, int mission) const {
        int slot = 0;
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i) {
            if (CAMPAIGN_MISSIONS[i].act != act || CAMPAIGN_MISSIONS[i].track != track) continue;
            if (i == mission) return slot;
            ++slot;
        }
        return -1;
    }
    // Ходьба по дереву: влево-вправо — дела одной полосы, вверх-вниз — акты.
    void moveCampaignCursor(int actDelta, int slotDelta) {
        const int act = std::max(1, std::min(CAMPAIGN_ACT_COUNT, campaignActOf(storyMissionCursor)));
        const int lane = campaignLaneTrack();
        int slot = campaignActSlotOf(act, lane, storyMissionCursor);
        if (slot < 0) slot = 0;
        if (slotDelta != 0) {
            const int count = campaignActLaneCount(act, lane);
            if (count > 0) {
                slot = (slot + slotDelta + count) % count;
                storyMissionCursor = campaignActLaneMission(act, lane, slot);
                selectedStoryMission = storyMissionCursor;
                campaignActView = act;
            }
            return;
        }
        int next = act;
        for (int step = 0; step < CAMPAIGN_ACT_COUNT; ++step) {
            next = std::max(1, std::min(CAMPAIGN_ACT_COUNT, next + actDelta));
            if (campaignActLaneCount(next, lane) > 0) break;
        }
        const int count = campaignActLaneCount(next, lane);
        if (count <= 0) return;
        storyMissionCursor = campaignActLaneMission(next, lane, std::min(slot, count - 1));
        selectedStoryMission = storyMissionCursor;
        campaignActView = next;
    }
    void setCampaignActView(int act) {
        campaignActView = std::max(1, std::min(CAMPAIGN_ACT_COUNT, act));
        const int lane = campaignLaneTrack();
        const int first = campaignActLaneCount(campaignActView, lane) > 0
                        ? campaignActLaneMission(campaignActView, lane, 0)
                        : campaignActFirstMission(campaignActView);
        if (first >= 0) storyMissionCursor = first;
        selectedStoryMission = storyMissionCursor;
    }

    // ── Дерево глав ─────────────────────────────────────────────────────────
    // Вместо плоского списка с вкладками актов — дерево: слева ствол с узлами
    // актов, справа от узла полосы следов. В актах II–IV две полосы: «А» и «Б».
    // Полоса выбранного следа светится, вторая перечёркнута до новой ночи.
    int campaignLaneTrack() const {
        if (save.campaignTrack == 1 || save.campaignTrack == 2) return save.campaignTrack;
        const int t = campaignTrackOf(storyMissionCursor);
        return t != 0 ? t : 1;
    }
    int campaignActLaneCount(int act, int track) const {
        int n = 0;
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i)
            if (CAMPAIGN_MISSIONS[i].act == act && CAMPAIGN_MISSIONS[i].track == track) ++n;
        return n;
    }
    int campaignActLaneMission(int act, int track, int slot) const {
        int seen = 0;
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i) {
            if (CAMPAIGN_MISSIONS[i].act != act || CAMPAIGN_MISSIONS[i].track != track) continue;
            if (seen == slot) return i;
            ++seen;
        }
        return -1;
    }
    // Сколько полос в акте: у актов с развилкой их две.
    int campaignActLanes(int act) const {
        return campaignActLaneCount(act, 2) > 0 ? 2 : 1;
    }
    // Высота дерева при заданной ширине и пределе по высоте.
    int campaignTreeHeight(int maxHeight) const {
        int lanes = 0;
        for (int a = 1; a <= CAMPAIGN_ACT_COUNT; ++a) lanes += campaignActLanes(a);
        int laneH = 30;
        const int headers = CAMPAIGN_ACT_COUNT * 24;
        if (headers + lanes * (laneH + 4) > maxHeight)
            laneH = std::max(20, std::min(30, (maxHeight - headers) / std::max(1, lanes) - 4));
        return headers + lanes * (laneH + 4);
    }

    void drawCampaignTree(Renderer& r, int x, int y, int w, int maxHeight) {
        const int laneH = std::max(20, std::min(30, (campaignTreeHeight(maxHeight) - CAMPAIGN_ACT_COUNT * 24) /
                                                   std::max(1, [&] {
                                                       int n = 0;
                                                       for (int a = 1; a <= CAMPAIGN_ACT_COUNT; ++a) n += campaignActLanes(a);
                                                       return n;
                                                   }()) - 4));
        const int headerH = 24, laneGap = 4;
        const int spineX = x + 15, laneX = x + 36;
        const int activeLane = campaignLaneTrack();
        int cy = y;
        // Ствол: от узла к узлу. Рисуем до чипов, чтобы линии были под ними.
        int firstNodeY = -1, lastNodeY = -1;
        for (int a = 1; a <= CAMPAIGN_ACT_COUNT; ++a) {
            const int nodeY = cy + headerH / 2;
            if (firstNodeY < 0) firstNodeY = nodeY;
            lastNodeY = nodeY;
            cy += headerH + campaignActLanes(a) * (laneH + laneGap);
        }
        r.drawLine(spineX, firstNodeY, spineX, lastNodeY, 0xFF5E4E86, 2, 210, false);

        cy = y;
        for (int a = 1; a <= CAMPAIGN_ACT_COUNT; ++a) {
            const uint32_t accent = CAMPAIGN_ACT_COLORS[a - 1];
            const int nodeY = cy + headerH / 2;
            const bool actOpen = isCampaignActUnlocked(a);
            const bool actCurrent = campaignActView == a;
            r.fillCircle(spineX, nodeY, actCurrent ? 12 : 10, actOpen ? 0xFF2A2038 : 0xFF1A1720, 255);
            r.drawCircleRing(spineX, nodeY, actCurrent ? 12 : 10, 2,
                             actCurrent ? 0xFFFFD858 : (actOpen ? accent : 0xFF4A4453), 235, false);
            char roman[8];
            snprintf(roman, sizeof(roman), "%s", romanAct(a));
            r.drawTextCentered(FONT_SMALL, spineX, nodeY - 7, roman,
                               actCurrent ? 0xFFFFE9A8 : (actOpen ? 0xFFE6DCF6 : 0xFF8B8492), false);
            char header[96];
            snprintf(header, sizeof(header), "АКТ %s · %s", romanAct(a), CAMPAIGN_ACT_TITLES[a - 1]);
            r.drawText(FONT_SMALL, laneX, cy + 4, fitText(r, FONT_SMALL, header, w - (laneX - x) - 96).c_str(),
                       actOpen ? (actCurrent ? 0xFFFFF0C8 : 0xFFCFC4E2) : 0xFF8B8492, false);
            // Счётчик показывает прогресс по СВОЕЙ полосе: иначе в акте с двумя
            // следами всегда было бы «3 из 8» и выглядело бы как недобор.
            const int headerLane = campaignActLanes(a) > 1 ? activeLane : 0;
            const int headerTotal = campaignActLaneCount(a, headerLane);
            int headerDone = 0;
            for (int slot = 0; slot < headerTotal; ++slot)
                if (isCampaignMissionCompleted(campaignActLaneMission(a, headerLane, slot))) ++headerDone;
            char counter[48];
            snprintf(counter, sizeof(counter), "%d / %d", headerDone, headerTotal);
            r.drawTextRight(FONT_SMALL, x + w - 6, cy + 4, counter,
                            headerTotal > 0 && headerDone == headerTotal ? 0xFF8AF0AE : 0xFF9C93AC, false);
            cy += headerH;

            const int lanes = campaignActLanes(a);
            for (int lane = 0; lane < lanes; ++lane) {
                const int laneTrack = lanes > 1 ? (lane == 0 ? 1 : 2) : 0;
                const int count = lanes > 1 ? campaignActLaneCount(a, laneTrack) : campaignActLaneCount(a, 0);
                const bool otherTrack = lanes > 1 && save.campaignTrack != 0 && save.campaignTrack != laneTrack;
                const bool unchosen = lanes > 1 && save.campaignTrack == 0;
                const int chipGap = 5;
                const int chipW = std::max(30, (w - (laneX - x) - (count - 1) * chipGap) / std::max(1, count));
                const int chipY = cy + 2, chipH = laneH - 4;
                // Подпись полосы слева от чипов.
                if (lanes > 1) {
                    const int tagX = x + 2, tagW = 20;
                    r.fillRoundedRect(tagX, chipY, tagW, chipH, 5,
                                      laneTrack == save.campaignTrack ? 0xFF3A2A56 : 0xFF221E2A);
                    r.drawRoundedRect(tagX, chipY, tagW, chipH, 5,
                                      laneTrack == save.campaignTrack ? 0xFFFFD858 : 0xFF4E4658, 1, 220);
                    r.drawTextCentered(FONT_SMALL, tagX + tagW / 2, chipY + (chipH - 14) / 2,
                                       campaignTrackLetter(laneTrack),
                                       laneTrack == save.campaignTrack ? 0xFFFFE9A8 : 0xFF9C93AC, false);
                }
                for (int slot = 0; slot < count; ++slot) {
                    const int i = lanes > 1 ? campaignActLaneMission(a, laneTrack, slot)
                                            : campaignActLaneMission(a, 0, slot);
                    if (i < 0) break;
                    const int chipX = laneX + slot * (chipW + chipGap);
                    const CampaignMissionDef& mission = CAMPAIGN_MISSIONS[i];
                    const bool completed = isCampaignMissionCompleted(i);
                    const bool unlocked = isCampaignMissionUnlocked(i);
                    const bool focused = storyMissionCursor == i;
                    const bool hovered = mouseX >= chipX && mouseX <= chipX + chipW &&
                                         mouseY >= chipY && mouseY <= chipY + chipH;
                    const uint32_t chipAccent = getCampaignAccent(i);
                    // Финальный выбор тоже показываем как «одна из двух дверей»:
                    // дверь, которую уже не открыть, гаснет.
                    const bool choiceLocked = isCampaignMissionLockedByChoice(i);
                    const bool dim = otherTrack || (unchosen && laneTrack != 0) || choiceLocked;
                    const uint32_t edge = focused ? 0xFFFFD858
                                        : dim ? 0xFF3E3949
                                        : completed ? 0xFF69E0A0 : (unlocked ? chipAccent : 0xFF554E60);
                    const uint32_t fill = focused ? 0xFF372549
                                        : dim ? 0xFF14111C
                                        : (hovered ? 0xFF282037 : (completed ? 0xFF16261F : 0xFF181421));
                    r.fillRoundedRect(chipX, chipY, chipW, chipH, 6, fill);
                    r.drawRoundedRect(chipX, chipY, chipW, chipH, 6, edge, focused ? 2 : 1);
                    char num[8];
                    snprintf(num, sizeof(num), "%02d", i + 1);
                    const uint32_t numColor = dim ? 0xFF6C6577
                                            : completed ? 0xFFB8FFD1
                                            : (unlocked ? 0xFFFFE9BC : 0xFF8A8490);
                    r.drawTextCentered(FONT_SMALL, chipX + chipW / 2, chipY + (chipH - 14) / 2, num, numColor, false);
                    if (completed) r.fillRoundedRect(chipX + 4, chipY + chipH - 4, chipW - 8, 2, 1, 0xFF69E0A0);
                    else if (unlocked && !dim) r.fillRoundedRect(chipX + 4, chipY + chipH - 4, chipW - 8, 2, 1, chipAccent);
                    if (mouseClicked && hovered) {
                        storyMissionCursor = i;
                        selectedStoryMission = i;
                        campaignActView = a;
                        sound.play(SFX_CLICK, 0.02f);
                    }
                }
                // Одиночная полоса с финальной развилкой: зачёркиваем только
                // проигранный вариант, а не всю полосу.
                if (lanes == 1) {
                    for (int slot = 0; slot < count; ++slot) {
                        const int i = campaignActLaneMission(a, 0, slot);
                        if (i < 0 || !isCampaignMissionLockedByChoice(i)) continue;
                        const int chipX = laneX + slot * (chipW + chipGap);
                        r.drawLine(chipX + 3, chipY + chipH - 3, chipX + chipW - 3, chipY + 3, 0xFF6A5A78, 1, 200, false);
                        r.drawLine(chipX + 3, chipY + 3, chipX + chipW - 3, chipY + chipH - 3, 0xFF6A5A78, 1, 200, false);
                    }
                }
                // Перечёркнутая полоса: крест по чипам и подпись.
                if (otherTrack || unchosen) {
                    const int laneW = count * (chipW + chipGap) - chipGap;
                    const int laneRight = laneX + laneW;
                    for (int slot = 0; slot < count; ++slot) {
                        const int chipX = laneX + slot * (chipW + chipGap);
                        r.drawLine(chipX + 3, chipY + chipH - 3, chipX + chipW - 3, chipY + 3, 0xFF6A5A78, 1, 200, false);
                        r.drawLine(chipX + 3, chipY + 3, chipX + chipW - 3, chipY + chipH - 3, 0xFF6A5A78, 1, 200, false);
                    }
                    const char* note = unchosen ? "ВЫБЕРИ СЛЕД" : "ЗАКРЫТ ДО НОВОЙ НОЧИ";
                    const int noteW = r.textWidth(FONT_SMALL, note) + 16;
                    if (noteW < laneW - 8) {
                        const int nx = laneX + (laneW - noteW) / 2;
                        const int ny = chipY + (chipH - 18) / 2;
                        r.fillRoundedRect(nx, ny, noteW, 18, 5, 0xF014101C);
                        r.drawRoundedRect(nx, ny, noteW, 18, 5, 0xFF6A5A78, 1, 220);
                        r.drawTextCentered(FONT_SMALL, nx + noteW / 2, ny + 2, note, 0xFFC9BEDA, false);
                    }
                }
                cy += laneH + laneGap;
            }
        }
    }

    void renderMissionSelectPortrait(Renderer& r) {
        renderMenuHeader(r, "КАМПАНИЯ • ДЕРЕВО ГЛАВ");
        const int x = 24, listW = SCREEN_W - 48;
        const int buttonY = SCREEN_H - 102;
        // Дереву отдаём всю высоту, кроме досье и кнопок: оно само ужимает полосы.
        const int treeY = 168;
        const int treeH = campaignTreeHeight(std::max(300, buttonY - treeY - 300));
        drawCampaignTree(r, x, treeY, listW, treeH);
        const int detailY = treeY + treeH + 8;
        const int detailX = 24, detailW = SCREEN_W - 48;
        const bool compactPortrait = SCREEN_H < 1100;
        const int selected = std::max(0, std::min(CAMPAIGN_MISSION_COUNT - 1, storyMissionCursor));
        const CampaignMissionDef& mission = CAMPAIGN_MISSIONS[selected];
        const bool unlocked = isCampaignMissionUnlocked(selected), completed = isCampaignMissionCompleted(selected);
        const uint32_t accent = getCampaignAccent(selected);
        const int detailH = std::max(200, std::min(440, buttonY - detailY - 14));
        const int imageH = compactPortrait ? 104 : 132;
        const int subtitleY = detailY + imageH + 12, titleY = subtitleY + 22, briefingY = titleY + 26;
        const int objectiveY = detailY + (compactPortrait ? 208 : 268);
        r.fillRect(detailX, detailY, detailW, detailH, 0xFF15111F, 250);
        r.drawRect(detailX, detailY, detailW, detailH, accent, 2, 235);
        drawMissionIllustration(r, selected, detailX + 8, detailY + 8, detailW - 16, imageH);
        r.drawText(FONT_SMALL, detailX + 16, subtitleY, mission.subtitle, accent, false);
        r.drawTextRight(FONT_SMALL, detailX + detailW - 14, subtitleY, campaignMissionStatus(selected),
            completed ? 0xFF8AF0AE : (unlocked ? 0xFFFFD17C : 0xFFB5AFBA), false);
        r.drawText(FONT_MED, detailX + 16, titleY, mission.title, 0xFFFFE4A8, false);
        r.drawTextWrapped(FONT_SMALL, detailX + 16, briefingY, detailW - 32, mission.briefing, 0xFFE1D9EC, false);
        r.fillRect(detailX + 10, objectiveY, detailW - 20, 46, 0xFF21192E);
        r.drawRect(detailX + 10, objectiveY, detailW - 20, 46, accent, 1, 170);
        r.drawText(FONT_SMALL, detailX + 20, objectiveY + 5, "ЦЕЛЬ", accent, false);
        r.drawTextWrapped(FONT_SMALL, detailX + 74, objectiveY + 4, detailW - 94, mission.objective, 0xFFF0E8F6, false);
        char timing[160];
        snprintf(timing, sizeof(timing), "ДЕЛО %02d/%d • ПАРТИЯ ~%d МИН • БОСС ~%d МИН • ТИР %s",
                 selected + 1, CAMPAIGN_MISSION_COUNT, campaignMissionMinutes(selected),
                 campaignBossMinutes(selected), romanAct(campaignStartingTier(selected)));
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, objectiveY + 52, timing, 0xFFFFD58A, false);
        char bossSummary[160];
        if (unlocked) snprintf(bossSummary, sizeof(bossSummary), "СТРАЖ: %s • СЛОЖНОСТЬ +%d%%",
                               mission.finalBoss, campaignDifficultyPercent(selected));
        else snprintf(bossSummary, sizeof(bossSummary), "СНАЧАЛА ЗАКРОЙ ДЕЛО %02d",
                      campaignMissingPrereq(selected) + 1);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, objectiveY + 70, bossSummary, 0xFFD8B8FF, false);

        const int buttonH = 54, gap = 16, buttonW = (SCREEN_W - 48 - gap) / 2;
        const int backX = 16, startX = backX + buttonW + gap;
        const bool backHover = mouseX >= backX && mouseX <= backX + buttonW && mouseY >= buttonY && mouseY <= buttonY + buttonH;
        const bool startHover = mouseX >= startX && mouseX <= startX + buttonW && mouseY >= buttonY && mouseY <= buttonY + buttonH;
        // Кнопка запускает дело или фиксирует выбор следа — одно действие.
        const bool canGo = unlocked || campaignCanChooseTrackHere(selected);
        r.fillRect(backX, buttonY, buttonW, buttonH, backHover ? 0xFF49336F : 0xFF2A2040);
        r.drawRect(backX, buttonY, buttonW, buttonH, 0xFF8F78BA, 2);
        r.drawTextCentered(FONT_SMALL, backX + buttonW / 2, buttonY + 17, "НАЗАД К ГЕРОЮ [ESC]", 0xFFFFFFFF, false);
        r.fillRect(startX, buttonY, buttonW, buttonH, !canGo ? 0xFF302A36 : (startHover ? 0xFFB84042 : 0xFF762A38));
        r.drawRect(startX, buttonY, buttonW, buttonH, canGo ? 0xFFFFD878 : 0xFF625B68, 2);
        char startLabel[64];
        if (campaignCanChooseTrackHere(selected))
            snprintf(startLabel, sizeof(startLabel), "ПОЙТИ СЛЕДОМ %s [ENTER]", campaignTrackLetter(campaignTrackOf(selected)));
        else if (!unlocked) snprintf(startLabel, sizeof(startLabel), "ДЕЛО ЕЩЁ ЗАКРЫТО");
        else if (completed) snprintf(startLabel, sizeof(startLabel), "ПЕРЕИГРАТЬ [ENTER]");
        else snprintf(startLabel, sizeof(startLabel), "НАЧАТЬ ДЕЛО [ENTER]");
        r.drawTextCentered(FONT_SMALL, startX + buttonW / 2, buttonY + 17, startLabel,
            canGo ? 0xFFFFFFFF : 0xFF9A939F, false);
        if (mouseClicked && backHover) state = STATE_HERO_SELECT;
        else if (mouseClicked && startHover && canGo) confirmCampaignSelection(selected);
        noteMissionPreview();
    }

    void renderMissionSelectMenu(Renderer& r) {
        if (SCREEN_W < SCREEN_H) { renderMissionSelectPortrait(r); return; }
        renderSubmenuFrame(r, "КАМПАНИЯ • ДЕРЕВО ГЛАВ",
                          "Акты II–IV идут двумя следами: А — свидетель, Б — копия. Второй след откроется в новую ночь.");

        int completedCount = campaignCompletedCount();
        const int seals = storySealCount();
        r.fillRoundedRect(40, 84, 205, 27, 7, 0xFF251B35);
        r.drawRoundedRect(40, 84, 205, 27, 7, 0xFFFFD878, 1);
        r.drawTextCentered(FONT_SMALL, 142, 90, "ПЕЧАТИ СЛОМАНЫ", 0xFFFFE5A0, false);
        for (int i = 0; i < 4; ++i) {
            const int sx = 259 + i * 22;
            r.fillRoundedRect(sx, 91, 16, 12, 4, i < seals ? 0xFF65D89A : 0xFF3B3345);
            r.drawRoundedRect(sx, 91, 16, 12, 4, i < seals ? 0xFFB6FFD0 : 0xFF6C6177, 1);
        }
        char progress[96];
        if (save.campaignCycle > 0)
            snprintf(progress, sizeof(progress), "ДЕЛ ЗАКРЫТО %d / %d • НОВАЯ НОЧЬ %d • ЗОЛОТО ×%d",
                     completedCount, CAMPAIGN_MISSION_COUNT, save.campaignCycle, campaignGoldMultiplier());
        else
            snprintf(progress, sizeof(progress), "ДЕЛ ЗАКРЫТО %d / %d", completedCount, CAMPAIGN_MISSION_COUNT);
        r.drawText(FONT_SMALL, 360, 90, progress, 0xFFE4D7B5);
        char trackLine[96];
        if (isCampaignTrackChosen())
            snprintf(trackLine, sizeof(trackLine), "СЛЕД %s • %s",
                     campaignTrackLetter(save.campaignTrack), campaignTrackName(save.campaignTrack));
        else snprintf(trackLine, sizeof(trackLine), "СЛЕД НЕ ВЫБРАН");
        r.drawTextRight(FONT_SMALL, SCREEN_W - 22, 90, trackLine,
                        isCampaignTrackChosen() ? 0xFFFFD858 : 0xFFBFB5D0);

        // ── Дерево глав ────────────────────────────────────────────────────
        // Никаких вкладок актов: слева ствол из пяти узлов, у каждого акта свои
        // полосы следов. Так видно, что акты II–IV идут двумя дорогами и
        // сходятся только в акте V, — а не «одно дело за другим».
        const bool compactLandscape = SCREEN_W < 1120;
        const int listX = compactLandscape ? 16 : 40, listY = 118;
        const int listW = compactLandscape ? 330 : 420;
        const int treeH = campaignTreeHeight(std::max(300, SCREEN_H - listY - 130));
        drawCampaignTree(r, listX, listY, listW, treeH);

        const int selected = std::max(0, std::min(CAMPAIGN_MISSION_COUNT - 1, storyMissionCursor));
        const CampaignMissionDef& mission = CAMPAIGN_MISSIONS[selected];
        const bool unlocked = isCampaignMissionUnlocked(selected);
        const bool completed = isCampaignMissionCompleted(selected);
        const uint32_t accent = getCampaignAccent(selected);
        const int detailX = listX + listW + 20;
        const int detailY = 162, detailW = compactLandscape ? SCREEN_W - detailX - 16 : SCREEN_W - detailX - 40;
        const int detailH = 440;
        r.fillRoundedRect(detailX, detailY, detailW, detailH, 12, 0xFF15111F, 250);
        r.drawRoundedRect(detailX, detailY, detailW, detailH, 12, accent, 2, 235);
        drawMissionIllustration(r, selected, detailX + 12, detailY + 12, detailW - 24, 200);
        if (mission.bossReinforced) {
            // Досье честно предупреждает: это не новый страж, а тот же, вернувшийся
            // сильнее. Игрок должен знать заранее, за что платит временем.
            const char* badge = "ВОЗВРАЩЕНИЕ • ТОТ ЖЕ СТРАЖ ЗЛЕЕ";
            const int bw = r.textWidth(FONT_SMALL, badge) + 22;
            r.fillRoundedRect(detailX + detailW - bw - 22, detailY + 22, bw, 24, 6, 0xFF4A1520, 240);
            r.drawRoundedRect(detailX + detailW - bw - 22, detailY + 22, bw, 24, 6, 0xFFFF5A72, 1, 230);
            r.drawText(FONT_SMALL, detailX + detailW - bw - 11, detailY + 28, badge, 0xFFFFB6C0, false);
        }

        r.drawText(FONT_SMALL, detailX + 22, detailY + 222, mission.subtitle, accent, false);
        char statusBuf[128];
        if (completed) snprintf(statusBuf, sizeof(statusBuf), "ПРОЙДЕНА • МОЖНО ПЕРЕИГРАТЬ");
        else if (unlocked) snprintf(statusBuf, sizeof(statusBuf), "ДОСТУПНА • ЕЩЁ НЕ ПРОЙДЕНА");
        else if (campaignCanChooseTrackHere(selected))
            snprintf(statusBuf, sizeof(statusBuf), "ВЫБЕРИ СЛЕД %s • ЭТО РЕШЕНИЕ ДО НОВОЙ НОЧИ",
                     campaignTrackLetter(campaignTrackOf(selected)));
        else if (isCampaignMissionLockedByChoice(selected))
            snprintf(statusBuf, sizeof(statusBuf), "ДРУГОЙ ВЫБОР СДЕЛАН • ЭТА ДВЕРЬ ЗАКРЫТА");
        else if (isCampaignMissionLockedByTrack(selected))
            snprintf(statusBuf, sizeof(statusBuf), "ЧУЖОЙ СЛЕД • ЗАКРЫТО ДО НОВОЙ НОЧИ");
        else snprintf(statusBuf, sizeof(statusBuf), "ЗАКРЫТА • НУЖНО ДЕЛО %02d", campaignMissingPrereq(selected) + 1);
        const char* status = statusBuf;
        const uint32_t statusColor = completed ? 0xFF8AF0AE : (unlocked ? 0xFFFFD17C : 0xFFB5AFBA);
        const int statusW = r.textWidth(FONT_SMALL, status);
        r.fillRoundedRect(detailX + detailW - statusW - 42, detailY + 217, statusW + 24, 24, 6,
                          completed ? 0xFF1D4A34 : (unlocked ? 0xFF49361F : 0xFF302D35));
        r.drawText(FONT_SMALL, detailX + detailW - statusW - 30, detailY + 222, status, statusColor, false);
        r.drawText(FONT_MED, detailX + 22, detailY + 242, mission.title, 0xFFFFE4A8, false);
        r.drawTextWrapped(FONT_SMALL, detailX + 22, detailY + 270, detailW - 44, mission.briefing, 0xFFE1D9EC, false);

        const int objectiveY = detailY + 315;
        r.fillRoundedRect(detailX + 14, objectiveY, detailW - 28, 48, 7, 0xFF21192E);
        r.drawRoundedRect(detailX + 14, objectiveY, detailW - 28, 48, 7, accent, 1, 170);
        r.drawText(FONT_SMALL, detailX + 27, objectiveY + 15, "ЦЕЛЬ", accent, false);
        r.drawTextWrapped(FONT_SMALL, detailX + 88, objectiveY + 8, detailW - 118, mission.objective, 0xFFF0E8F6, false);

        const int infoY = detailY + 374, infoX = detailX + 14, infoGap = 8, infoH = 49;
        const int infoW = (detailW - 28 - 3 * infoGap) / 4;
        const char* infoLabels[4] = {"ДЕЛО", "ФИНАЛЬНЫЙ СТРАЖ", "ПОЯВЛЕНИЕ БОССА", "ТИР • СЛОЖНОСТЬ"};
        char caseNumber[40], bossTime[40], tierAndDifficulty[40];
        snprintf(caseNumber, sizeof(caseNumber), "%02d • АКТ %s", selected + 1, romanAct(campaignActOf(selected)));
        snprintf(bossTime, sizeof(bossTime), "ЧЕРЕЗ ~%d МИН", campaignBossMinutes(selected));
        snprintf(tierAndDifficulty, sizeof(tierAndDifficulty), "%s • +%d%%", romanAct(campaignStartingTier(selected)), campaignDifficultyPercent(selected));
        const char* infoValues[4] = {caseNumber, mission.finalBoss, bossTime, tierAndDifficulty};
        for (int i = 0; i < 4; ++i) {
            const int ix = infoX + i * (infoW + infoGap);
            r.fillRoundedRect(ix, infoY, infoW, infoH, 7, 0xFF201A2B);
            r.drawRoundedRect(ix, infoY, infoW, infoH, 7, i == 0 ? 0xFF6A5A7B : accent, 1);
            r.drawTextCentered(FONT_SMALL, ix + infoW / 2, infoY + 6, infoLabels[i], 0xFFBEB5CD, false);
            r.drawTextCentered(FONT_SMALL, ix + infoW / 2, infoY + 27, infoValues[i], i == 0 ? 0xFFBFE8D0 : 0xFFFFD58A, false);
        }

        const int buttonY = SCREEN_H - 100, buttonH = 54;
        const int backX = compactLandscape ? 20 : 40;
        const int backW = compactLandscape ? 220 : 350;
        const int buttonGap = 18, startX = backX + backW + buttonGap;
        const int startW = SCREEN_W - startX - (compactLandscape ? 20 : 40);
        const bool backHover = mouseX >= backX && mouseX <= backX + backW && mouseY >= buttonY && mouseY <= buttonY + buttonH;
        const bool startHover = mouseX >= startX && mouseX <= startX + startW && mouseY >= buttonY && mouseY <= buttonY + buttonH;
        r.fillRect(backX, buttonY, backW, buttonH, backHover ? 0xFF49336F : 0xFF2A2040);
        r.drawRect(backX, buttonY, backW, buttonH, 0xFF8F78BA, 2);
        r.drawTextCentered(FONT_SMALL, backX + backW / 2, buttonY + 18, "НАЗАД К ГЕРОЮ [ESC]", 0xFFFFFFFF, false);
        const bool canGo = unlocked || campaignCanChooseTrackHere(selected);
        char startLabel[96];
        if (campaignCanChooseTrackHere(selected))
            snprintf(startLabel, sizeof(startLabel), "ПОЙТИ СЛЕДОМ %s [ENTER]",
                     campaignTrackLetter(campaignTrackOf(selected)));
        else if (!unlocked) snprintf(startLabel, sizeof(startLabel), "ДЕЛО ЕЩЁ ЗАКРЫТО");
        else if (completed) snprintf(startLabel, sizeof(startLabel), "ПЕРЕИГРАТЬ [ENTER]");
        else snprintf(startLabel, sizeof(startLabel), "НАЧАТЬ ДЕЛО [ENTER]");
        r.fillRect(startX, buttonY, startW, buttonH, !canGo ? 0xFF302A36 : (startHover ? 0xFFB84042 : 0xFF762A38));
        r.drawRect(startX, buttonY, startW, buttonH, canGo ? 0xFFFFD878 : 0xFF625B68, 2);
        r.drawTextCentered(FONT_SMALL, startX + startW / 2, buttonY + 18, startLabel,
            canGo ? 0xFFFFFFFF : 0xFF9A939F, false);
        if (mouseClicked && backHover) state = STATE_HERO_SELECT;
        else if (mouseClicked && startHover && canGo) confirmCampaignSelection(selected);
        noteMissionPreview();
    }

    void renderMissionComplete(Renderer& r) {
        const CampaignMissionDef& mission = selectedCampaignMission();
        char missionHeader[160];
        snprintf(missionHeader, sizeof(missionHeader), "%s • %s", mission.title, mission.subtitle);
        renderSubmenuFrame(r, "ДЕЛО ЗАКРЫТО", missionHeader, 0xFF9FE5A4);
        char line[200];
        snprintf(line, sizeof(line), "ВРЕМЯ %02d:%02d   •   УБИЙСТВ %d   •   БОССОВ %d",
                 (int)runTime / 60, (int)runTime % 60, kills, runBossKills);
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, 252, line, 0xFFFFE078);
        snprintf(line, sizeof(line), "ДЕЛО %02d ИЗ %d   •   АКТ %s: %s",
                 selectedStoryMission + 1, CAMPAIGN_MISSION_COUNT,
                 romanAct(campaignActOf(selectedStoryMission)),
                 CAMPAIGN_ACT_TITLES[campaignActOf(selectedStoryMission) - 1]);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 288, line, 0xFFB9D8FF);
        snprintf(line, sizeof(line), "ЗАКРЫТО ДЕЛ: %d / %d   •   ПЕЧАТИ: %d / 4   •   ЗОЛОТО: +%d",
                 campaignCompletedCount(), CAMPAIGN_MISSION_COUNT, storySealCount(), runGold);
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, 312, line, 0xFF9FE5A4);
        const bool compactComplete = SCREEN_W < 1200;
        const int btnY = SCREEN_W < SCREEN_H ? SCREEN_H - 96 : 460;
        const int btnW = compactComplete ? (SCREEN_W - 48 - 24) / 3 : 300, btnH = 54;
        const int gap = compactComplete ? 12 : 20;
        const int menuX = compactComplete ? 24 : 170;
        const int retryX = menuX + btnW + gap, nextX = retryX + btnW + gap;
        int nextMission = selectedStoryMission;
        for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i) {
            if (isCampaignMissionUnlocked(i) && !isCampaignMissionCompleted(i)) { nextMission = i; break; }
        }
        const bool hasNext = nextMission != selectedStoryMission && isCampaignMissionUnlocked(nextMission);
        const bool menuHover = mouseX >= menuX && mouseX <= menuX + btnW && mouseY >= btnY && mouseY <= btnY + btnH;
        const bool retryHover = mouseX >= retryX && mouseX <= retryX + btnW && mouseY >= btnY && mouseY <= btnY + btnH;
        const bool nextHover = mouseX >= nextX && mouseX <= nextX + btnW && mouseY >= btnY && mouseY <= btnY + btnH;
        auto button = [&](int x, const char* label, bool hover, uint32_t edge, bool enabled) {
            r.fillRoundedRect(x, btnY, btnW, btnH, 9, enabled ? (hover ? 0xFF3B5D43 : 0xFF263D2C) : 0xFF25252B);
            r.drawRoundedRect(x, btnY, btnW, btnH, 9, enabled ? edge : 0xFF5A565E, 2);
            r.drawTextCentered(FONT_SMALL, x + btnW / 2, btnY + 18, label, enabled ? 0xFFFFFFFF : 0xFF8F8B92);
        };
        button(menuX, "К СПИСКУ ДЕЛ", menuHover, 0xFF9FE5A4, true);
        button(retryX, "ПОВТОРИТЬ", retryHover, 0xFFFFD878, true);
        button(nextX, "СЛЕДУЮЩЕЕ ДЕЛО", nextHover, 0xFF9FE5A4, hasNext);
        if (mouseClicked && menuHover) { storyMissionCursor = selectedStoryMission; state = STATE_MISSION_SELECT; }
        else if (mouseClicked && retryHover) startSelectedRun();
        else if (mouseClicked && nextHover && hasNext) launchStoryMission(nextMission);
    }

    void drawMenuTitlePanel(Renderer& r, const char* title, const char* subtitle,
                            int x, int y, int w, int h, uint32_t accent = 0xFFFFD878) {
        r.fillGradientV(x, y, w, h, 0xFF261936, 0xFF110C1C, 244);
        r.drawRect(x, y, w, h, accent, 1, 245);
        r.fillRect(x + 12, y + 2, std::max(1, w - 24), 2, accent, 225);
        r.addGlow(x + w / 2, y + h / 2, std::min(90, w / 5), accent, 48);
        const BakedFont& titleFont = r.textWidth(FONT_LARGE, title) <= w - 36 ? FONT_LARGE : FONT_MED;
        r.drawTextCentered(titleFont, x + w / 2, y + 8, title, 0xFFFFE068);
        if (subtitle && *subtitle) r.drawTextCentered(FONT_SMALL, x + w / 2, y + h - 20, subtitle, 0xFFD8D0F0);
    }

    void renderSubmenuFrame(Renderer& r, const char* title, const char* subtitle,
                            uint32_t accent = 0xFFFFD878, int panelH = 68) {
        renderWorld(r, sinf(globalTime * 0.25f) * 140.0f - SCREEN_W / 2,
                    cosf(globalTime * 0.2f) * 90.0f - SCREEN_H / 2);
        r.fillRect(0, 0, SCREEN_W, SCREEN_H, 0xFF0A0714, 192);
        const int margin = std::min(16, std::max(8, SCREEN_W / 24));
        drawMenuTitlePanel(r, title, subtitle, margin, 8, SCREEN_W - margin * 2, panelH, accent);
    }

    void renderMenuHeader(Renderer& r, const char* subtitle) {
        renderWorld(r, sinf(globalTime * 0.25f) * 140.0f - SCREEN_W / 2, cosf(globalTime * 0.2f) * 90.0f - SCREEN_H / 2);
        r.fillRect(0, 0, SCREEN_W, SCREEN_H, 0xFF0A0714, 192);
        const bool compactHeader = SCREEN_W < 900;
        const int topBarH = compactHeader ? 68 : 44;
        r.fillRect(0, 0, SCREEN_W, topBarH, 0xFF120C22, 235);
        r.drawRect(0, 0, SCREEN_W, topBarH, 0xFF4C3C78, 1);
        char buf[128];
        snprintf(buf, sizeof(buf), "ЗОЛОТО: %d", save.gold);
        const int bMin = save.bestTimeSec / 60, bSec = save.bestTimeSec % 60;
        char record[128];
        snprintf(record, sizeof(record), "РЕКОРД %02d:%02d • УБИЙСТВ %d • БОССОВ %d", bMin, bSec, save.bestKills, save.bossesDefeated);
        char audio[96];
        snprintf(audio, sizeof(audio), "[M] SFX %s • [N] MUSIC %s • [F11]", sound.enabled ? "ON" : "OFF", sound.musicEnabled ? "ON" : "OFF");
        if (compactHeader) {
            r.drawText(FONT_SMALL, 12, 5, buf, 0xFFFFD838);
            r.drawTextRight(FONT_SMALL, SCREEN_W - 12, 5, "[F11]", 0xFF98E8FF);
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 25, record, 0xFFD0C8E8);
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 45, audio, 0xFF98E8FF);
        } else {
            snprintf(buf, sizeof(buf), "ЗОЛОТО В КАЗНЕ: %d", save.gold);
            r.drawText(FONT_MED, 24, 10, buf, 0xFFFFD838);
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 13, record, 0xFFD0C8E8);
            r.drawTextRight(FONT_SMALL, SCREEN_W - 24, 13, audio, 0xFF98E8FF);
        }

        const int headerY = topBarH + 4;
        const int titlePanelW = std::min(620, SCREEN_W - 24);
        const int titlePanelX = (SCREEN_W - titlePanelW) / 2;
        r.fillGradientV(titlePanelX, headerY, titlePanelW, 94, 0xFF261936, 0xFF110C1C, 242);
        r.drawRect(titlePanelX, headerY, titlePanelW, 94, 0xFF756044, 1, 240);
        r.fillRect(titlePanelX + 14, headerY + 2, titlePanelW - 28, 2, 0xFFFFD878, 225);
        r.addGlow(SCREEN_W / 2, headerY + 44, 128, 0xFF982048, 105);
        r.drawTextCentered(FONT_LARGE, SCREEN_W / 2, headerY + 12, "ПЕПЕЛ ТЬМЫ: ВЫЖИВШИЕ", 0xFFFFE068);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, headerY + 52, subtitle, 0xFFD8D0F0);
        snprintf(buf, sizeof(buf), "ПЕЧАТИ %d/4 • УЛИКИ %d/4 • [ЗАЛ СЛАВЫ → РЕЕСТР СВИДЕТЕЛЯ]", storySealCount(), storyEntryCount());
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, headerY + 70, buf, 0xFFFFD88A);
    }

    static constexpr int MAIN_MENU_ITEM_COUNT = 7;
    static constexpr float MENU_PI = 3.14159265358979323846f;
    static constexpr float MENU_TAU = 6.28318530717958647692f;
    static int menuCenterX() { return SCREEN_W / 2; }
    static int menuCenterY() {
        return std::min(SCREEN_H - 260, std::max(385, SCREEN_H / 2 + 25));
    }
    static constexpr int MENU_INNER_R = 82;
    static constexpr int MENU_OUTER_R = 225;
    static constexpr int MENU_SELECTED_INNER_R = 70;
    static constexpr int MENU_SELECTED_OUTER_R = 241;
    static constexpr float MENU_GAP_RADIANS = 0.10471975512f; // six degrees of visible space between sectors.

    static float wrapMenuAngle(float a) {
        while (a > MENU_PI) a -= MENU_TAU;
        while (a < -MENU_PI) a += MENU_TAU;
        return a;
    }

    static float menuSectorStep() { return MENU_TAU / MAIN_MENU_ITEM_COUNT; }

    static float menuSectorCenterAngle(int item) {
        return -MENU_PI * 0.5f + menuSectorStep() * item;
    }

    static float menuSectorHalfAngle() {
        return menuSectorStep() * 0.5f - MENU_GAP_RADIANS * 0.5f;
    }

    int radialMenuItemAt(int px, int py) const {
        const int dx = px - menuCenterX(), dy = py - menuCenterY();
        const int d2 = dx * dx + dy * dy;
        if (d2 < MENU_SELECTED_INNER_R * MENU_SELECTED_INNER_R ||
            d2 > MENU_SELECTED_OUTER_R * MENU_SELECTED_OUTER_R) return -1;
        const float angle = atan2f((float)dy, (float)dx);
        float relative = angle + MENU_PI * 0.5f;
        while (relative < 0.0f) relative += MENU_TAU;
        while (relative >= MENU_TAU) relative -= MENU_TAU;
        const int item = ((int)floorf((relative + menuSectorStep() * 0.5f) / menuSectorStep())) % MAIN_MENU_ITEM_COUNT;
        const float center = menuSectorCenterAngle(item);
        return fabsf(wrapMenuAngle(angle - center)) <= menuSectorHalfAngle() ? item : -1;
    }

    void drawRadialArc(Renderer& r, int radius, float start, float end, uint32_t color,
                       int thickness, int alpha) {
        const int segments = std::max(8, (int)((end - start) * radius / 5.0f));
        for (int i = 0; i < segments; ++i) {
            const float a0 = start + (end - start) * i / segments;
            const float a1 = start + (end - start) * (i + 1) / segments;
            r.drawLine(menuCenterX() + (int)(cosf(a0) * radius), menuCenterY() + (int)(sinf(a0) * radius),
                       menuCenterX() + (int)(cosf(a1) * radius), menuCenterY() + (int)(sinf(a1) * radius),
                       color, thickness, alpha, false);
        }
    }

    void fillRadialMenu(Renderer& r) {
        static const uint32_t fills[MAIN_MENU_ITEM_COUNT] = {
            0xFF432A35, 0xFF203B48, 0xFF493021, 0xFF332A48,
            0xFF23413A, 0xFF303342, 0xFF452832
        };
        static const uint32_t dimFills[MAIN_MENU_ITEM_COUNT] = {
            0xFF261B25, 0xFF1B242B, 0xFF2B201A, 0xFF241E31,
            0xFF1B2B28, 0xFF22232C, 0xFF2A1D22
        };
        const int maxR = MENU_SELECTED_OUTER_R;
        const int minR2 = MENU_SELECTED_INNER_R * MENU_SELECTED_INNER_R;
        const int maxR2 = maxR * maxR;
        // The sector a pixel belongs to depends only on the screen size and the fixed
        // menu geometry, yet the first version recomputed atan2/floor per pixel every
        // frame — about 4,5 мс на кадр в меню. The map is built once per resolution.
        static std::vector<uint8_t> sectorMap;
        static int mapW = -1, mapH = -1, mapCx = -1, mapCy = -1;
        const int cx = menuCenterX(), cy = menuCenterY();
        if (mapW != SCREEN_W || mapH != SCREEN_H || mapCx != cx || mapCy != cy) {
            mapW = SCREEN_W; mapH = SCREEN_H; mapCx = cx; mapCy = cy;
            sectorMap.assign((size_t)SCREEN_W * SCREEN_H, 0);
            const float half = menuSectorHalfAngle();
            const float step = menuSectorStep();
            for (int y = std::max(0, cy - maxR); y <= std::min(SCREEN_H - 1, cy + maxR); ++y) {
                const int dy = y - cy;
                const int rem = maxR2 - dy * dy;
                if (rem < 0) continue;
                const int extent = (int)sqrtf((float)rem);
                const int x0 = std::max(0, cx - extent), x1 = std::min(SCREEN_W - 1, cx + extent);
                for (int x = x0; x <= x1; ++x) {
                    const int dx = x - cx;
                    const int d2 = dx * dx + dy * dy;
                    if (d2 < minR2 || d2 > maxR2) continue;
                    const float angle = atan2f((float)dy, (float)dx);
                    float relative = angle + MENU_PI * 0.5f;
                    if (relative < 0.0f) relative += MENU_TAU;
                    else if (relative >= MENU_TAU) relative -= MENU_TAU;
                    const int item = ((int)floorf((relative + step * 0.5f) / step)) % MAIN_MENU_ITEM_COUNT;
                    if (fabsf(wrapMenuAngle(angle - menuSectorCenterAngle(item))) > half) continue;
                    sectorMap[(size_t)y * SCREEN_W + x] = (uint8_t)(item + 1);
                }
            }
        }
        for (int y = std::max(0, cy - maxR); y <= std::min(SCREEN_H - 1, cy + maxR); ++y) {
            const int dy = y - cy;
            const int rem = maxR2 - dy * dy;
            if (rem < 0) continue;
            const int extent = (int)sqrtf((float)rem);
            const int x0 = std::max(0, cx - extent), x1 = std::min(SCREEN_W - 1, cx + extent);
            const uint8_t* rowMap = sectorMap.data() + (size_t)y * SCREEN_W;
            for (int x = x0; x <= x1; ++x) {
                const uint8_t slot = rowMap[x];
                if (slot == 0) continue;
                const int item = slot - 1;
                const int dx = x - cx;
                const int d2 = dx * dx + dy * dy;
                const bool selected = item == mainMenuFocus;
                const int inner = selected ? MENU_SELECTED_INNER_R : MENU_INNER_R;
                const int outer = selected ? MENU_SELECTED_OUTER_R : MENU_OUTER_R;
                if (d2 < inner * inner || d2 > outer * outer) continue;
                r.blendPixel(x, y, selected ? fills[item] : dimFills[item], selected ? 250 : 238);
            }
        }
    }

    void activateMainMenuItem(int item) {
        sound.play(SFX_CLICK, 0.02f);
        switch (item) {
            case 0: gameMode = MODE_STORY; openHeroSelectForMode(MODE_STORY); break;
            case 1: gameMode = MODE_ENDLESS; openHeroSelectForMode(MODE_ENDLESS); break;
            case 2: state = STATE_SHOP; break;
            case 3: arsenalScrollRow = 0; state = STATE_HELP; break;
            case 4: hallPage = 0; state = STATE_HALL; break;
            case 5: settingsReturnState = STATE_MENU; state = STATE_SETTINGS; break;
            case 6: exitConfirmationOpen = true; exitDialogFocus = 1; break;
        }
    }

    void renderExitConfirmation(Renderer& r) {
        const int panelW = std::min(532, SCREEN_W - 32), panelH = 230;
        const int panelX = (SCREEN_W - panelW) / 2, panelY = (SCREEN_H - panelH) / 2;
        const int buttonW = (panelW - 84) / 2, yesX = panelX + 28;
        const int stayX = yesX + buttonW + 28, buttonY = panelY + 144, buttonH = 50;
        const bool yesHover = mouseX >= yesX && mouseX <= yesX + buttonW && mouseY >= buttonY && mouseY <= buttonY + buttonH;
        const bool stayHover = mouseX >= stayX && mouseX <= stayX + buttonW && mouseY >= buttonY && mouseY <= buttonY + buttonH;
        if ((mouseMovedSinceLaunch || mouseClicked) && (yesHover || stayHover)) exitDialogFocus = yesHover ? 0 : 1;

        r.fillRect(0, 0, SCREEN_W, SCREEN_H, 0xFF07050C, 208);
        r.addGlow(SCREEN_W / 2, panelY + 100, 190, 0xFF7F375C, 95);
        r.fillRect(panelX, panelY, panelW, panelH, 0xFF17111F, 255);
        r.drawRect(panelX, panelY, panelW, panelH, 0xFFB89BCB, 2);
        r.drawRect(panelX + 8, panelY + 8, panelW - 16, panelH - 16, 0xFF50415F, 1);
        r.drawTextCentered(FONT_LARGE, SCREEN_W / 2, panelY + 26, "ВЫЙТИ ИЗ ИГРЫ?", 0xFFFFD878);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, panelY + 72, "Точно закрыть игру?", 0xFFE8E1F0);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, panelY + 98, "Сохранённый прогресс останется доступен.", 0xFFBEB1CC);

        const bool yesActive = exitDialogFocus == 0;
        const bool stayActive = exitDialogFocus == 1;
        r.fillRect(yesX, buttonY, buttonW, buttonH, yesActive ? 0xFF71313B : 0xFF30232C);
        r.drawRect(yesX, buttonY, buttonW, buttonH, yesActive ? 0xFFFF8790 : 0xFF805A62, yesActive ? 3 : 1);
        r.drawTextCentered(FONT_MED, yesX + buttonW / 2, buttonY + 15, "ВЫЙТИ", 0xFFFFEAEC, false);
        r.fillRect(stayX, buttonY, buttonW, buttonH, stayActive ? 0xFF315D56 : 0xFF202B2B);
        r.drawRect(stayX, buttonY, buttonW, buttonH, stayActive ? 0xFF8EE6C8 : 0xFF5D827B, stayActive ? 3 : 1);
        r.drawTextCentered(FONT_MED, stayX + buttonW / 2, buttonY + 15, "ОСТАТЬСЯ", 0xFFE5FFF6, false);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, panelY + 201, "ENTER — ПОДТВЕРДИТЬ • ESC — ОСТАТЬСЯ", 0xFFAFA3BE);

        if (mouseClicked && yesHover) {
            exitDialogFocus = 0;
            exitConfirmationOpen = false;
            requestExit = true;
        } else if (mouseClicked && stayHover) {
            exitDialogFocus = 1;
            exitConfirmationOpen = false;
        }
    }

    void renderMainMenu(Renderer& r) {
        renderMenuHeader(r, "ВЫБЕРИ ПУНКТ • КОЛЕСО РЕЖИМОВ И РАЗДЕЛОВ");
        const int hovered = radialMenuItemAt(mouseX, mouseY);
        if (!exitConfirmationOpen && (mouseMovedSinceLaunch || mouseClicked) && hovered >= 0) mainMenuFocus = hovered;
        mainMenuFocus = std::clamp(mainMenuFocus, 0, MAIN_MENU_ITEM_COUNT - 1);

        static const char* labels[MAIN_MENU_ITEM_COUNT] = {
            "СЮЖЕТ", "ENDLESS", "КУЗНИЦА", "АРСЕНАЛ", "ЗАЛ СЛАВЫ", "НАСТРОЙКИ", "ВЫХОД"
        };
        static const char* subtitles[MAIN_MENU_ITEM_COUNT] = {
            "28 ДЕЛ", "ВОЛНЫ", "МАГАЗИН", "СПРАВКА", "РЕЕСТР", "ПАРАМЕТРЫ", "ИЗ ИГРЫ"
        };
        static const uint32_t accents[MAIN_MENU_ITEM_COUNT] = {
            0xFFFFC85A, 0xFF62D8F0, 0xFFFF9C56, 0xFFB58EFF,
            0xFF76E5B8, 0xFFA6C4FF, 0xFFFF7A83
        };
        static const uint32_t edgeDim[MAIN_MENU_ITEM_COUNT] = {
            0xFF917142, 0xFF507A89, 0xFF94644A, 0xFF756A91,
            0xFF4D8170, 0xFF66718A, 0xFF8F5660
        };

        fillRadialMenu(r);
        const float half = menuSectorHalfAngle();
        for (int item = 0; item < MAIN_MENU_ITEM_COUNT; ++item) {
            const bool selected = item == mainMenuFocus;
            const float center = menuSectorCenterAngle(item);
            const float start = center - half, end = center + half;
            const int inner = selected ? MENU_SELECTED_INNER_R : MENU_INNER_R;
            const int outer = selected ? MENU_SELECTED_OUTER_R : MENU_OUTER_R;
            if (selected) {
                const int gx = menuCenterX() + (int)(cosf(center) * 157.0f);
                const int gy = menuCenterY() + (int)(sinf(center) * 157.0f);
                r.addGlow(gx, gy, 78, accents[item], 92);
            }
            const uint32_t edge = selected ? accents[item] : edgeDim[item];
            drawRadialArc(r, outer, start, end, edge, selected ? 4 : 2, selected ? 255 : 180);
            drawRadialArc(r, inner, start, end, edge, selected ? 3 : 1, selected ? 238 : 140);
            r.drawLine(menuCenterX() + (int)(cosf(start) * inner), menuCenterY() + (int)(sinf(start) * inner),
                       menuCenterX() + (int)(cosf(start) * outer), menuCenterY() + (int)(sinf(start) * outer),
                       edge, selected ? 3 : 1, selected ? 238 : 135, true);
            r.drawLine(menuCenterX() + (int)(cosf(end) * inner), menuCenterY() + (int)(sinf(end) * inner),
                       menuCenterX() + (int)(cosf(end) * outer), menuCenterY() + (int)(sinf(end) * outer),
                       edge, selected ? 3 : 1, selected ? 238 : 135, true);

            const int labelRadius = selected ? 164 : 155;
            const int tx = menuCenterX() + (int)(cosf(center) * labelRadius);
            const int ty = menuCenterY() + (int)(sinf(center) * labelRadius);
            const BakedFont& titleFont = selected ? FONT_MED : FONT_SMALL;
            const uint32_t titleColor = selected ? 0xFFFFF1D0 : 0xFFD8D1E3;
            const uint32_t subColor = selected ? 0xFFFFD98A : 0xFFAAA3B8;
            if (item == 1) {
                r.drawTextCentered(titleFont, tx, ty - 17, labels[item], titleColor, false);
                r.drawTextCentered(FONT_SMALL, tx, ty + 1, subtitles[item], subColor, false);
                r.drawTextCentered(FONT_SMALL, tx, ty + 17, "БЕЗ КОНЦА", subColor, false);
            } else if (item == 2 || item == 3) {
                r.drawTextCentered(titleFont, tx, ty - 11, labels[item], titleColor, false);
                r.drawTextCentered(FONT_SMALL, tx, ty + 10, subtitles[item], subColor, false);
            } else {
                r.drawTextCentered(titleFont, tx, ty - 9, labels[item], titleColor, false);
                r.drawTextCentered(FONT_SMALL, tx, ty + 13, subtitles[item], subColor, false);
            }
        }

        // Central hub echoes the focused destination and confirms with Enter/click.
        r.fillCircle(menuCenterX(), menuCenterY(), 68, 0xFF120F1D, 255);
        r.drawCircleRing(menuCenterX(), menuCenterY(), 68, 3, accents[mainMenuFocus], 235, true);
        r.drawCircleRing(menuCenterX(), menuCenterY(), 56, 1, 0xFF5E526F, 180, true);
        r.drawTextCentered(FONT_SMALL, menuCenterX(), menuCenterY() - 27, "ВЫБРАНО", 0xFFBEB1CC, false);
        r.drawTextCentered(FONT_SMALL, menuCenterX(), menuCenterY() - 6, labels[mainMenuFocus], 0xFFFFE8B2, false);
        r.drawTextCentered(FONT_SMALL, menuCenterX(), menuCenterY() + 20, "[ENTER]", 0xFFBEB1CC, false);

        if (!exitConfirmationOpen && mouseClicked) {
            if (hovered >= 0) activateMainMenuItem(hovered);
            else {
                const int dx = mouseX - menuCenterX(), dy = mouseY - menuCenterY();
                if (dx * dx + dy * dy <= 68 * 68) activateMainMenuItem(mainMenuFocus);
            }
        }
        if (exitConfirmationOpen) {
            renderExitConfirmation(r);
            return;
        }
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, SCREEN_H - 69,
            "F12 — АРЕНА ПРОСЛУШИВАНИЯ БОССА • БЕЗ ПРОГРЕССА И ИЗМЕНЕНИЙ СОХРАНЕНИЯ", 0xFFFFD878);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, SCREEN_H - 41,
            "СТРЕЛКИ / A-D — НАВИГАЦИЯ • ENTER — ВЫБРАТЬ • МЫШЬ — НАВЕСТИ ИЛИ НАЖАТЬ",
            0xFFB7AFD2);
        // Quiet build stamp: readable when you look for it, invisible while playing.
        char versionStamp[32];
        snprintf(versionStamp, sizeof(versionStamp), "v%s", GAME_VERSION);
        r.drawTextRight(FONT_SMALL, SCREEN_W - 14, SCREEN_H - 30, versionStamp, 0xFF6A6280, true, 128);
    }

    static const char* stateName(GameState value) {
        switch (value) {
            case STATE_MENU: return "MENU";
            case STATE_SHOP: return "SHOP";
            case STATE_HELP: return "HELP";
            case STATE_SETTINGS: return "SETTINGS";
            case STATE_PLAYING: return "PLAYING";
            case STATE_LEVELUP: return "LEVELUP";
            case STATE_CHEST: return "CHEST";
            case STATE_PAUSED: return "PAUSED";
            case STATE_GAMEOVER: return "GAMEOVER";
            case STATE_HALL: return "HALL";
            case STATE_DEATH: return "DEATH";
            case STATE_PATCH_NOTES: return "PATCH_NOTES";
            case STATE_CUTSCENE: return "CUTSCENE";
            case STATE_EXTRACTION: return "EXTRACTION";
            case STATE_CAMPAIGN_END: return "CAMPAIGN_END";
            case STATE_BOSS_MUSIC_TEST: return "MUSIC_TEST";
            case STATE_MISSION_SELECT: return "MISSION_SELECT";
            case STATE_MISSION_COMPLETE: return "MISSION_COMPLETE";
            case STATE_HERO_SELECT: return "HERO_SELECT";
        }
        return "?";
    }

    void renderHeroSelectMenu(Renderer& r) {
        char subtitle[96];
        snprintf(subtitle, sizeof(subtitle), "РЕЖИМ: %s  •  ТЕПЕРЬ ВЫБЕРИ ГЕРОЯ",
                 gameMode == MODE_STORY ? "СЮЖЕТ" : "ENDLESS");
        renderMenuHeader(r, subtitle);
        char buf[128];
        // Responsive hero cards: three columns on wider canvases, one readable card in portrait.
        const bool portraitLayout = SCREEN_W < SCREEN_H;
        const int cardCount = portraitLayout ? 1 : 3;
        const int gap = portraitLayout ? 0 : 35;
        const int cardW = portraitLayout ? SCREEN_W - 48 : std::min(340, (SCREEN_W - 48 - 2 * gap) / 3);
        const int cardH = portraitLayout ? 390 : 340;
        const int startX = (SCREEN_W - cardCount * cardW - (cardCount - 1) * gap) / 2;
        const int cardY = portraitLayout ? (SCREEN_H - cardH) / 2 : 145;

        for (int i = 0; i < cardCount; ++i) {
            int cx = startX + i * (cardW + gap);
            bool hov = (mouseX >= cx && mouseX <= cx + cardW && mouseY >= cardY && mouseY <= cardY + cardH);
            int heroId = portraitLayout ? selectedHero : (selectedHero + i + 4) % 5;
            bool sel = (selectedHero == heroId);

            bool unlocked = isHeroUnlocked(heroId);
            if (mouseClicked && hov && unlocked) {
                selectedHero = heroId;
                sound.play(SFX_CLICK, 0.02f);
            }

            const uint32_t classColor = HERO_DEFS[heroId].classColor;
            uint32_t topC = !unlocked ? 0xFF100D18 : (sel ? 0xFF34225C : (hov ? 0xFF261A44 : 0xFF18112C));
            uint32_t botC = sel ? 0xFF1C1236 : 0xFF100B1E;
            uint32_t brdC = sel ? 0xFFFFD848 : (hov ? classColor : 0xFF48386E);

            r.fillGradientV(cx, cardY, cardW, cardH, topC, botC, 245);
            r.drawRoundedRect(cx, cardY, cardW, cardH, 12, brdC, sel ? 3 : 2);
            r.fillRoundedRect(cx + 6, cardY + 5, cardW - 12, 4, 2, classColor, 235);
            r.fillRoundedRect(cx + 5, cardY + 14, 5, cardH - 28, 2, classColor, unlocked ? 210 : 70);

            if (sel) {
                r.fillRoundedRect(cx + 45, cardY + 12, cardW - 90, 22, 5, 0xFFD89820);
                snprintf(buf, sizeof(buf), "ВЫБРАН • #%d/5 • БОТ %d/5", heroId + 1, HERO_DEFS[heroId].botDifficultyRank);
                r.drawTextCentered(FONT_SMALL, cx + cardW / 2, cardY + 14, buf, 0xFFFFFFFF);
            } else {
                snprintf(buf, sizeof(buf), "ГЕРОЙ #%d/5 • БОТ %d/5", heroId + 1, HERO_DEFS[heroId].botDifficultyRank);
                r.drawTextCentered(FONT_SMALL, cx + cardW / 2, cardY + 14, buf, unlocked ? classColor : 0xFF777080);
            }

            // Hero Preview Pedestal & Animated Sprite
            r.fillCircle(cx + cardW / 2, cardY + 92, 38, 0xFF120C22);
            r.drawCircleRing(cx + cardW / 2, cardY + 92, 38, 2, unlocked ? classColor : 0xFF50465C);
            if (unlocked) {
                r.drawHeroSprite(cx + cardW / 2, cardY + 94, heroId, globalTime, 1, false, false);
                r.drawTextCentered(FONT_MED, cx + cardW / 2, cardY + 142, HERO_DEFS[heroId].name, 0xFFFFFFFF);
                r.drawTextCentered(FONT_SMALL, cx + cardW / 2, cardY + 161, HERO_DEFS[heroId].subtitle, classColor);
                r.drawTextCentered(FONT_SMALL, cx + cardW / 2, cardY + 179, HERO_DEFS[heroId].className, classColor);
                r.fillRoundedRect(cx + 22, cardY + 196, cardW - 44, 34, 6, 0xFF140E26);
                r.drawWeaponIcon(cx + 42, cardY + 213, HERO_DEFS[heroId].startWeaponId, 14);
                r.drawText(FONT_SMALL, cx + 66, cardY + 204, HERO_DEFS[heroId].startWeaponName, 0xFF80E8FF);
                r.drawTextWrapped(FONT_SMALL, cx + 22, cardY + 238, cardW - 44, HERO_DEFS[heroId].bonusDesc, 0xFFD0C8E8, true);

                const float hpMax = HERO_DEFS[2].baseHp + getMetaUpgradeEffectRank(1) * 10.0f;
                const float hpValue = HERO_DEFS[heroId].baseHp + getMetaUpgradeEffectRank(1) * 10.0f;
                const float hpRatio = hpMax > 0.0f ? hpValue / hpMax : 0.0f;
                const float speedRatio = HERO_DEFS[heroId].moveSpeed / HERO_DEFS[4].moveSpeed;
                const int statBarX = cx + 67, statBarW = 188, statBarH = 8;
                r.drawText(FONT_SMALL, cx + 22, cardY + 286, "ЗДР", 0xFFD0C8E8);
                r.fillRoundedRect(statBarX, cardY + 290, statBarW, statBarH, 4, 0xFF171321);
                r.fillRoundedRect(statBarX, cardY + 290, std::max(4, (int)(statBarW * hpRatio)), statBarH, 4, 0xFF58D990);
                snprintf(buf, sizeof(buf), "%d", (int)hpValue);
                r.drawTextRight(FONT_SMALL, cx + cardW - 20, cardY + 286, buf, 0xFFB9F4D1);
                r.drawText(FONT_SMALL, cx + 22, cardY + 307, "СКОР", 0xFFD0C8E8);
                r.fillRoundedRect(statBarX, cardY + 311, statBarW, statBarH, 4, 0xFF171321);
                r.fillRoundedRect(statBarX, cardY + 311, std::max(4, (int)(statBarW * speedRatio)), statBarH, 4, classColor);
                snprintf(buf, sizeof(buf), "%d", (int)HERO_DEFS[heroId].moveSpeed);
                r.drawTextRight(FONT_SMALL, cx + cardW - 20, cardY + 307, buf, classColor);
            } else {
                r.drawTextCentered(FONT_LARGE, cx + cardW / 2, cardY + 73, "?", 0xFFBDAFD0);
                const char* req = heroId == 3 ? "ПОБЕДИ МОРГАРА" : "ПОБЕДИ 3 БОССОВ";
                r.drawTextCentered(FONT_MED, cx + cardW / 2, cardY + 143, "ГЕРОЙ ЗАПЕЧАТАН", 0xFFAAA0B5);
                r.drawTextCentered(FONT_SMALL, cx + cardW / 2, cardY + 176, req, 0xFFFF9878);
                r.drawTextWrapped(FONT_SMALL, cx + 26, cardY + 230, cardW - 52, "Побеждай боссов, открывай новые главы и героев. Твой прогресс кампании сохраняется между забегами.", 0xFFA9A0B6, true);
                snprintf(buf, sizeof(buf), "ПОБЕДЫ НАД БОССАМИ: %d", save.bossesDefeated);
                r.drawTextCentered(FONT_SMALL, cx + cardW / 2, cardY + 306, buf, 0xFFFFD878);
            }
        }


        const int instructionY = portraitLayout ? SCREEN_H - 41 : SCREEN_H - 82;
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, instructionY,
                           "КЛИК ИЛИ A / D  •  ЦИФРЫ 1–5 — ВЫБОР ГЕРОЯ",
                           0xFFC8BEDA);
        const int buttonH = 54;
        const int buttonY = portraitLayout ? SCREEN_H - 126 : SCREEN_H - 190;
        const int buttonW = std::min(360, (SCREEN_W - 72) / 2);
        const int buttonGap = 24;
        const int backX = (SCREEN_W - 2 * buttonW - buttonGap) / 2;
        const int continueX = backX + buttonW + buttonGap;
        const bool backHover = mouseX >= backX && mouseX <= backX + buttonW && mouseY >= buttonY && mouseY <= buttonY + buttonH;
        const bool continueHover = mouseX >= continueX && mouseX <= continueX + buttonW && mouseY >= buttonY && mouseY <= buttonY + buttonH;
        r.fillRect(backX, buttonY, buttonW, buttonH, backHover ? 0xFF49336F : 0xFF2A2040);
        r.drawRect(backX, buttonY, buttonW, buttonH, 0xFF8F78BA, 2);
        r.drawTextCentered(FONT_SMALL, backX + buttonW / 2, buttonY + 17, "НАЗАД К РЕЖИМАМ [ESC]", 0xFFFFFFFF);
        r.fillRect(continueX, buttonY, buttonW, buttonH, continueHover ? 0xFFB84042 : 0xFF762A38);
        r.drawRect(continueX, buttonY, buttonW, buttonH, 0xFFFFD878, 2);
        r.drawTextCentered(FONT_SMALL, continueX + buttonW / 2, buttonY + 17,
                           gameMode == MODE_STORY ? "К ВЫБОРУ МИССИИ [ENTER]" : "НАЧАТЬ ENDLESS [ENTER]", 0xFFFFFFFF);
        if (mouseClicked && backHover) state = STATE_MENU;
        else if (mouseClicked && continueHover) confirmHeroSelection();
    }

    void toggleOption(int optionId) {
        if (optionId == 1) sound.enabled = !sound.enabled.load(std::memory_order_relaxed);
        else if (optionId == 2) options.hitEffectsEnabled = !options.hitEffectsEnabled;
        else if (optionId == 3) options.screenShakeEnabled = !options.screenShakeEnabled;
        else if (optionId == 4) options.advisorEnabled = !options.advisorEnabled;
        else if (optionId == 5) options.hardModeEnabled = !options.hardModeEnabled;
        else if (optionId == 6) options.projectileTrailsEnabled = !options.projectileTrailsEnabled;
        else if (optionId == 7) options.skillIconStyle = (options.skillIconStyle + 1) % 3;
        else if (optionId == 8) {
            options.messageSpeechEnabled = options.messageSpeechEnabled != 0 ? 0 : 1;
            sound.setVoiceEnabled(options.messageSpeechEnabled != 0);
            if (options.messageSpeechEnabled != 0) sound.speakMessage("voice_on");
        }
        saveOptions();
        sound.play(SFX_CLICK, 0.02f);
    }

    void setSoundVolume(int channel, float value) {
        value = std::max(0.0f, std::min(1.0f, value));
        if (channel == 0) sound.sfxVolume.store(value, std::memory_order_relaxed);
        else sound.musicVolume.store(value, std::memory_order_relaxed);
        saveOptions();
    }

    void adjustSoundVolume(int direction) {
        const float current = settingsVolumeFocus == 0
            ? sound.sfxVolume.load(std::memory_order_relaxed)
            : sound.musicVolume.load(std::memory_order_relaxed);
        setSoundVolume(settingsVolumeFocus, current + direction * 0.05f);
        sound.play(SFX_CLICK, 0.015f);
    }

    void renderSettingsMenu(Renderer& r) {
        static int lastRefreshSecond = -1;
        if ((int)globalTime != lastRefreshSecond) { lastRefreshSecond = (int)globalTime; refreshVoiceStats(); }
        renderSubmenuFrame(r, "НАСТРОЙКИ", "shadow_survivors.cfg • звук • эффекты • стиль иконок");

        const char* labels[8] = {
            "Звуковые эффекты", "Эффекты попаданий", "Тряска камеры",
            "Советник по сборке (ИИ)", "БЕЗДНА — давление выше; пауэрапы только с умных", "Следы снарядов",
            "Стиль иконок предметов", "Озвучка сообщений в бою"
        };
        // Narrow portraits need shorter labels: the ВКЛ/ВЫКЛ value is right-aligned on the row.
        static const char* labelsPortrait[8] = {
            "Звуковые эффекты", "Эффекты попаданий", "Тряска камеры",
            "Советник по сборке (ИИ)", "БЕЗДНА — сложнее", "Следы снарядов",
            "Стиль иконок предметов", "Озвучка сообщений в бою"
        };
        const bool values[8] = {
            sound.enabled.load(std::memory_order_relaxed), options.hitEffectsEnabled != 0,
            options.screenShakeEnabled != 0, options.advisorEnabled != 0,
            options.hardModeEnabled != 0, options.projectileTrailsEnabled != 0, options.skillIconStyle != 0,
            options.messageSpeechEnabled != 0
        };
        const bool portraitLayout = SCREEN_W < SCREEN_H;
        const char* const* rowLabels = portraitLayout ? labelsPortrait : labels;
        const int x = portraitLayout ? 24 : (SCREEN_W < 1100 ? 24 : 260);
        const int w = portraitLayout || SCREEN_W < 1100 ? SCREEN_W - 48 : 760;
        const int h = portraitLayout ? 54 : 36, startY = portraitLayout ? 150 : 96, gap = portraitLayout ? 8 : 2;
        char stateText[24];
        for (int i = 0; i < 8; ++i) {
            int y = startY + i * (h + gap);
            bool hov = mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h;
            uint32_t border = hov ? 0xFFFFD848 : (values[i] ? 0xFF68DDA0 : 0xFF685878);
            r.fillRoundedRect(x, y, w, h, 9, hov ? 0xFF2D2147 : 0xFF1A132C);
            r.drawRoundedRect(x, y, w, h, 9, border, hov ? 3 : 2);
            r.drawText(FONT_MED, x + 24, y + 12, rowLabels[i], 0xFFEAE4F8);
            if (i == 6) {
                static const char* iconStyles[3] = {"ВЕКТОР", "ИИ-АРТ", "ЦВЕТ"};
                snprintf(stateText, sizeof(stateText), "%s", iconStyles[std::max(0, std::min(2, options.skillIconStyle))]);
            }
            else snprintf(stateText, sizeof(stateText), "%s", values[i] ? "ВКЛ" : "ВЫКЛ");
            r.drawTextRight(FONT_MED, x + w - 24, y + (h - 18) / 2, stateText, values[i] ? 0xFF68F0A0 : 0xFFFF8B98);
            if (mouseClicked && hov) toggleOption(i + 1);
        }

        const int volumeY = startY + 8 * (h + gap) + 22;
        auto drawVolume = [&](int channel, int y, const char* label, float value) {
            const int rowX = x, rowW = w, rowH = portraitLayout ? 44 : 38;
            const int trackW = std::min(310, rowW * 38 / 100);
            const int trackX = rowX + rowW * 43 / 100, trackY = y + rowH / 2;
            const bool focused = settingsVolumeFocus == channel;
            const bool hov = mouseX >= trackX - 12 && mouseX <= trackX + trackW + 12 && mouseY >= y && mouseY <= y + rowH;
            r.fillRoundedRect(rowX, y, rowW, rowH, 8, focused ? 0xFF2A2040 : 0xFF181326);
            r.drawRoundedRect(rowX, y, rowW, rowH, 8, hov ? 0xFFFFD858 : (focused ? 0xFFB68BFF : 0xFF514766), focused ? 2 : 1);
            r.drawText(FONT_SMALL, rowX + 18, y + 11, label, 0xFFEAE4F8);
            r.fillRoundedRect(trackX, trackY, trackW, 7, 3, 0xFF0D0B14);
            const int fillW = (int)(trackW * value);
            r.fillRoundedRect(trackX, trackY, std::max(2, fillW), 7, 3, channel == 0 ? 0xFF64D9FF : 0xFFB78AFF);
            r.fillCircle(trackX + fillW, trackY + 3, 8, focused ? 0xFFFFD878 : 0xFFE3D6FF, 250);
            char pct[16]; snprintf(pct, sizeof(pct), "%d%%", (int)roundf(value * 100.0f));
            r.drawTextRight(FONT_MED, rowX + rowW - 18, y + 9, pct, 0xFFFFFFFF);
            if (mouseClicked && mouseX >= trackX - 12 && mouseX <= trackX + trackW + 12 && mouseY >= y && mouseY <= y + rowH) {
                settingsVolumeFocus = channel;
                setSoundVolume(channel, (float)(mouseX - trackX) / (float)trackW);
                sound.play(SFX_CLICK, 0.015f);
            }
        };
        const int volumeRowH = portraitLayout ? 44 : 38;
        drawVolume(0, volumeY, "УРОВЕНЬ ЭФФЕКТОВ", sound.sfxVolume.load(std::memory_order_relaxed));
        drawVolume(1, volumeY + volumeRowH + 6, "УРОВЕНЬ МУЗЫКИ", sound.musicVolume.load(std::memory_order_relaxed));

        // Music comes only from audio/ now, so the old ten-way theme picker is gone: this is a status line.
        const int infoY = volumeY + 2 * (volumeRowH + 6) + 8;
        char musicLine[160];
        if (sound.externalMusicActive())
            snprintf(musicLine, sizeof(musicLine), "МУЗЫКА: %d ТРЕК(ОВ) ИЗ audio/  •  ВСТРОЕННАЯ ТЕМА В РЕЗЕРВЕ", sound.externalMusicTrackCount());
        else
            snprintf(musicLine, sizeof(musicLine), "МУЗЫКА: ПАПКА audio/ ПУСТА — ИГРАЕТ ВСТРОЕННАЯ ТЕМА");
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, infoY, musicLine, 0xFFAAA0C8);
        char voiceLine[160];
        if (voiceClips == 0)
            snprintf(voiceLine, sizeof(voiceLine), "ОЗВУЧКА: ПАПКА audio/voice/ ПУСТА — ФРАЗЫ ЖДУТ ЗАПИСИ");
        else if (voiceMissing > 0)
            snprintf(voiceLine, sizeof(voiceLine), "ОЗВУЧКА: %d ИЗ %d ФРАЗ В audio/voice/ (%d ЕЩЁ НЕ ЗАПИСАНО)", voiceClips, MessageRegistry::voicedDefs(), voiceMissing);
        else
            snprintf(voiceLine, sizeof(voiceLine), "ОЗВУЧКА: ВСЕ %d ФРАЗ НА МЕСТЕ — audio/voice/", voiceClips);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, infoY + 20, voiceLine, voiceMissing > 0 ? 0xFFFFC98A : 0xFF8CD8B0);
        if (portraitLayout) {
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, infoY + 40, "[TAB] ФОКУС • [←/→] ГРОМКОСТЬ • [1–8] ПЕРЕКЛЮЧАТЕЛИ", 0xFFAAA0C8);
        } else {
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 574, "[TAB] ФОКУС • [←/→] ГРОМКОСТЬ • [1–8] ПЕРЕКЛЮЧАТЕЛИ • КЛИК МЫШЬЮ", 0xFFAAA0C8);
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 612, "ГОЛОС — ЗАПИСАННЫЕ ФРАЗЫ В audio/voice/; СПИСОК И СЧЁТЧИКИ В F3", 0xFF8C82A8);
        }
        int bw = std::min(340, SCREEN_W - 48), bh = 50;
        int bx = (SCREEN_W - bw) / 2, by = portraitLayout ? SCREEN_H - 76 : 651;
        bool backHov = mouseX >= bx && mouseX <= bx + bw && mouseY >= by && mouseY <= by + bh;
        r.fillRoundedRect(bx, by, bw, bh, 10, backHov ? 0xFF483480 : 0xFF281C48);
        r.drawRoundedRect(bx, by, bw, bh, 10, 0xFFFFD858, 2);
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, by + 12, "НАЗАД [ESC]", 0xFFFFFFFF);
        if (mouseClicked && backHov) state = settingsReturnState;
    }

    void closePatchNotes() {
        if (patchNotesDoNotShowAgain) {
            options.patchNotesHiddenVersion = PATCH_NOTES_VERSION;
            saveOptions();
        }
        patchNotesDoNotShowAgain = false;
        patchNotesPage = 0;
        patchNotesScroll = 0.0f;
        state = STATE_MENU;
    }

    void patchNotesGoPage(int delta) {
        patchNotesPage = std::max(0, std::min(PATCH_NOTES_COUNT - 1, patchNotesPage + delta));
        patchNotesScroll = 0.0f;   // новая заметка читается с начала
        patchNotesScrollMax = 0;
    }

    void renderPatchNotes(Renderer& r) {
        renderWorld(r, -globalTime * 8.0f, -globalTime * 4.0f);
        r.fillRect(0, 0, SCREEN_W, SCREEN_H, 0xFF080612, 220);
        const bool compactPatch = SCREEN_W < 1200, portraitPatch = SCREEN_W < SCREEN_H;
        const int panelX = compactPatch ? 16 : 130, panelY = compactPatch ? 16 : 28;
        const int panelW = compactPatch ? SCREEN_W - 32 : 1020, panelH = compactPatch ? SCREEN_H - 32 : 650;
        r.fillGradientV(panelX, panelY, panelW, panelH, 0xFF211637, 0xFF100B1E, 250);
        r.drawRoundedRect(panelX, panelY, panelW, panelH, 16, 0xFFFFD058, 3);
        const std::string chronicleTitle = std::string("ХРОНИКА ОБНОВЛЕНИЙ • 2.2–") + GAME_VERSION;
        r.drawTextCentered(FONT_LARGE, SCREEN_W / 2, panelY + 20, chronicleTitle.c_str(), 0xFFFFE068);
                // Chronicle: one entry per release, newest last. Arrays are unsized on purpose:
        // a version without a note is a compile error instead of an empty page.
                // Chronicle: one entry per release, newest last. Arrays are unsized on purpose:
        // a version without a note is a compile error instead of an empty page.
                // Chronicle: one entry per release, newest last. Arrays are unsized on purpose:
        // a version without a note is a compile error instead of an empty page.
        static const char* const versions[] = {
            "2.2 • ПЕПЕЛЬНЫЕ РУИНЫ", "2.3 • ПЕЧАТИ И ОХОТА", "2.4 • КУЗНИЦА ДУШ",
            "2.5 • РЫВОК И СИМБИОЗЫ", "2.6 • УМНЫЕ ВРАГИ И ЛАБОРАТОРИЯ", "2.7 • ПРОРЫВ И СОВЕТНИК",
            "2.8 • ПОРТАЛЫ И БЕСКОНЕЧНЫЙ РЕЖИМ", "2.8.1 • МУЗЫКА И ОТКЛИКИ", "2.8.2 • ТИХОЕ МЕНЮ И МЯГЧЕ ЗВУК",
            "2.8.3 • СЮЖЕТ И БОССЫ", "2.8.4 • ПОЛИРОВКА И ЧЕСТНЫЙ АРТ", "2.8.5 • УМНЫЕ СОВЕТЫ ИИ",
            "2.9.0 • ПОСЛЕДНЯЯ ЗАПИСЬ", "2.9.1 • ЗВУКОВОЙ МИКС", "2.9.2 • ГОТИЧЕСКАЯ МУЗЫКА",
            "2.9.3 • РЕЗОНАТОРЫ И МАТРЁШКИ", "2.9.4 • НОВАЯ ПАРТИТУРА", "2.9.5 • ДЕСЯТЬ МУЗЫКАЛЬНЫХ ВАРИАНТОВ",
            "2.9.6 • ДЛИННАЯ ФОРМА МУЗЫКИ", "2.9.7 • ДЕСЯТЬ РАЗНЫХ МУЗЫКАЛЬНЫХ СТИЛЕЙ", "2.9.8 • НОВЫЕ СОЧИНЁННЫЕ МЕЛОДИИ",
            "2.9.9 • ОБНОВЛЕНИЕ ПИКСЕЛЬНЫХ СПРАЙТОВ", "2.9.10 • ПРЕДМЕТЫ И МИР В ПИКСЕЛЬНОМ СТИЛЕ", "2.9.11 • ЖИВЫЕ ПАРТИКЛЫ",
            "2.9.12 • ЧИСТЫЙ ИГРОВОЙ ИНТЕРФЕЙС", "2.9.28 • АРЕНА ПРОСЛУШИВАНИЯ БОССА", "2.9.29 • ПЕРЕКЛЮЧЕНИЕ БЕЗ ОЖИДАНИЯ ТАКТА",
            "2.9.30 • НОВАЯ МУЗЫКА БОССА И ПРОКАЧКИ", "2.9.31 • ВОЗВРАТ ПРИМЕРА МУЗЫКИ УЛУЧШЕНИЙ", "2.9.32 • МГНОВЕННАЯ МУЗЫКА И ТЕМП ПО ТОЛПЕ",
            "2.9.33 • БИЛДЫ, СПАВНЫ И ЛЕЧЕНИЕ", "2.9.34 • ТИРЫ, РЕЗОНАТОРЫ И ФИНАЛ", "2.9.35 • РЕЗОНАТОРЫ, ОПЫТ И ОПИСАНИЯ",
            "2.9.36 • МУЗЫКА, ОПЫТ И НОВЫЕ РЕЛИКВИИ", "2.9.37 • ЧЁТКАЯ ГАЛОЧКА", "2.9.38 • ТЕМП XP И ЗВУК ЗАМОРОЗКИ",
            "2.9.39 • ДАЛЬНИЕ РЕЗОНАТОРЫ И ТИРЫ", "2.9.40 • АКТИВКИ И ДОГОВОРЫ", "2.9.41 • ВЫБОР МИССИЙ И ТАЙМЕР БОССА",
            "2.9.42 • УСКОРЕНИЕ РАСТЕРИЗАЦИИ", "2.9.43 • ТЕМП, МИССИИ И РЕЗОНАТОРЫ", "2.9.44 • ДОСЬЕ И ПРОГРЕСС МИССИЙ",
            "2.9.45 • СНАЧАЛА РЕЖИМ, ПОТОМ ГЕРОЙ", "2.9.46 • ТЕМП И ФАЗЫ БОССОВ", "2.9.47 • ФОКУС ВЫБОРА РЕЖИМА",
            "2.9.48 • ЗВУК СНАРЯДОВ, КУЗНИЦА ДО 50", "2.9.49 • РАДИАЛЬНЫЕ МЕДАЛЬОНЫ", "2.9.50 • КОЛЕСО ИЗ СЕКТОРОВ",
            "2.9.51 • ВСЕ РАЗДЕЛЫ В РАДИАЛЬНОМ МЕНЮ", "2.9.52 • КОМПАКТНАЯ ШАПКА И ПОДТВЕРЖДЕНИЕ ВЫХОДА", "2.9.53 • ПОЛНАЯ АДАПТАЦИЯ LANDSCAPE И PORTRAIT",
            "2.9.54 • ЕДИНЫЙ СТИЛЬ ВСЕХ МЕНЮ", "2.9.55 • HUD, ФАЗЫ И УДАРЫ ПО ЗЕМЛЕ", "2.9.56 • ЗЕРКАЛО УКЛОНЕНИЯ",
            "2.9.57 • РЕЛИКВИИ, РЕЗОНАТОРЫ И ФАЗЫ БОССОВ", "2.9.58 • АВТОСОРТИРОВКА АРСЕНАЛА", "2.9.59 • ПЕЧАТЬ РАЗОБЛАЧЕНИЯ",
            "2.9.60 • ЧАША ПЕРЕПОЛНЕНИЯ", "2.9.61 • ОСКОЛОК ПОСЛЕДНЕГО УДАРА", "2.9.62 • КЛЮЧ ПЕПЕЛЬНОГО РИТМА",
            "2.9.63 • ПРИЗМА ПЕРЕКРЁСТНОГО ОГНЯ", "2.9.64 • УГОЛЬНЫЙ ФИТИЛЬ", "2.9.65 • ОСКОЛОК ОТТЕПЕЛИ",
            "2.9.66 • ПЕЧАТЬ ОБРАТНОГО ТОКА", "2.9.67 • ЗНАК ПАЛАЧА", "2.9.68 • ПЕЧАТЬ ВОЗМЕЗДИЯ",
            "2.9.69 • РУНА СВИДЕТЕЛЯ", "2.9.70 • ТИРЫ СЮЖЕТА", "2.9.71 • СБОРКА БЕЗ РАСХОЖДЕНИЙ",
            "2.9.72 • МУЗЫКА ИЗ audio/", "2.9.73 • КУЗНИЦА И ТИХИЙ УРОВЕНЬ", "2.9.74 • ГОЛОС, F3 И ЧИСТЫЙ МИР",
            "2.9.75 • ГОЛОС ИЗ ФАЙЛОВ", "2.9.76–2.9.80 • СЮЖЕТ, СЛОВАРЬ И ГОЛОС",
            "2.9.81 • ТИХИЕ РЕПЛИКИ В УГЛУ",
            "2.9.82 • КАЖДОЕ ДЕЛО — СВОЁ МЕСТО",
            "2.9.83 • ВЕРСИЯ, КАМПАНИЯ, СТРАЖИ",
            "2.9.84 • ЦВЕТ И КРЫСЫ",
            "2.9.85 • ГОЛОС ДЕЛА И ВТОРАЯ ДЕСЯТКА",
            "2.9.86 • ШЕСТЬ СТРАЖЕЙ И ИИ-ИКОНКИ",
            "2.9.87 • ВОЗВРАЩЕНИЕ СТРАЖЕЙ",
            "2.9.88 • ТРЕТЬЯ ДЕСЯТКА ИИ-ИКОНОК",
            "2.9.89 • ДЕЛА 11–20 ОЗВУЧЕНЫ",
            "2.9.90 • ОДИН ГОЛОС НА ВСЮ ИГРУ",
            "2.9.91 • ДЕЛА 07–10 ПЕРЕЧИТАНЫ",
            "2.9.92 • ДЕЛА 07 И 08 ПРОЧИТАНЫ РОВНО",
            "2.9.93 • УБРАНЫ СЛОВА С ПЛАВАЮЩИМ УДАРЕНИЕМ",
            "2.9.94 • ПРИЧИНА «АКЦЕНТА» — ФИЛЬТР ЧАСТОТЫ",
            "2.9.95 • УБРАНЫ СТЕЧЕНИЯ СОГЛАСНЫХ",
            "2.9.96 • АУДИТ: ПЕЧАТЬ, ПОЛОСА БОССА, СПИСОК ДЕЛ",
            "2.9.97 • ПЕРЕЧИТАНЫ ДЕСЯТЬ ХУДШИХ БОЕВЫХ РЕПЛИК",
            "2.9.98 • ВЕТКИ, НОВАЯ НОЧЬ, ФИНАЛ, ЧИТ-МЕНЮ, БОССЫ, МУЗЫКА",
            "2.9.99 • СТРАЖИ ЧИТАЮТСЯ С АРЕНЫ, ВЫБОР ВЕТКИ ОЗВУЧЕН",
            "2.10.0 • ПРОМПТЫ МУЗЫКИ ПО ДЕЛАМ, ФИНАЛЬНЫЙ БРИФИНГ ПЕРЕПИСАН",
            "2.10.1 • ТЕКСТЫ РЕПЛИК СОШЛИСЬ С ЗАПИСЬЮ",
            "2.10.2 • ГОЛОС СЛЫШЕН: ПЛОТНОСТЬ ВМЕСТО «УРОВНЯ В НОРМЕ»",
            "2.10.3 • ОБНОВЛЕНИЕ СНОВА ДОХОДИТ ДО КОМПЬЮТЕРА",
            "2.10.4 • СОРОК РАЗНЫХ ЦВЕТОВ И ЖИВЫЕ СТРАЖИ",
            "2.10.5 • РЕПЛИКА ГРОМЧЕ БОЯ: СВОЯ ШИНА И ПРИОРИТЕТ",
            "2.10.6 • UPDATE.BAT ОБНОВЛЯЕТ MAIN, А НЕ РАБОЧУЮ ВЕТКУ",
            "2.10.7 • СКАЧАННЫЙ С GITHUB UPDATE.BAT БОЛЬШЕ НЕ ПАДАЕТ",
            "2.10.8 • UPDATE.BAT: ТОЛЬКО ASCII, БЕЗ БЛОКОВ, С ПАУЗОЙ И ЛОГОМ",
            "2.10.9 • АПДЕЙТЕР ОБХОДИТ САМ СЕБЯ: СКАЧАННЫЙ БАТНИК БОЛЬШЕ НЕ БЛОКИРУЕТ",
            "2.10.10 • АПДЕЙТЕР ПРЕДСТАВЛЯЕТСЯ: updater build 2.10.10",
            "2.10.11 • ОБХОД САМОГО СЕБЯ ЗАРАБОТАЛ: ПРОВЕРКА ПОМЕХ ИСПРАВЛЕНА",
            "2.10.12 • ТЕКСТ АПДЕЙТЕРА ПО-РУССКИ И БЕЗ КРЯКОЗЯБР",
            "2.10.13 • КАМПАНИЯ: ВЕТКА Б БОЛЬШЕ НЕ ЗАПИРАЕТ АКТ V",
            "2.10.14 • КАМПАНИЯ ПЕРЕДЕЛАНА: ДВА СЛЕДА И ДЕРЕВО ГЛАВ",
            "2.10.15 • АПДЕЙТЕР: ТЕКСТ В CP866, ОФЛАЙН-СБОРКА, STASH",
            "2.10.16 • ВСЕ СОРОК ИКОНОК ПРЕДМЕТОВ НАРИСОВАНЫ И РАЗЛИЧАЮТСЯ ЦВЕТОМ",
            "2.10.17 • ИКОНКИ ПЕРЕРИСОВАНЫ: ЕДИНЫЙ СТИЛЬ, ФОН ПО ЦВЕТУ ПРЕДМЕТА",
            "2.10.18 • ЛИСТ ВСЕХ СОРОКА ИКОНОК ЛЕЖИТ В РЕПОЗИТОРИИ",
            "2.10.19 • СЕРДЦЕ ПЕРЕСТАЛО БЫТЬ ЗЕЛЁНЫМ, СТИЛЬ ИКОНОК СВЕДЕН",
            "2.10.20 • АПДЕЙТЕР: РУССКИЙ ТЕКСТ И САМАЯ СВЕЖАЯ ВЕТКА",
            "2.10.21 • КРЫС ВДВОЕ БОЛЬШЕ, ДВОЙНИК БЬЁТ В ГЕРОЯ, МУЗЫКА ПО ДЕЛАМ",
            "2.10.22 • ДЕБАГ-МЕНЮ ПО F9: КРЫСЫ, СТРАЖИ, ВСЕ ПРЕДМЕТЫ",
            "2.10.23 • АПДЕЙТЕР: МЁРТВАЯ ВЕТКА И СТАРЫЙ ФАЙЛ БОЛЬШЕ НЕ ЗАГЛУШКА",
            "2.10.24 • ПАТЧНОУТЫ ЛИСТАЮТСЯ, КНОПКИ НЕ ВЫЛЕЗАЮТ ЗА ПАНЕЛЬ",
        };
                // Keep each note to ~8 wrapped lines: the panel does not scroll.
        static const char* const notes[] = {
            "Основа игры: пять героев, автоматическое оружие и выбор прокачки во время боя.\nПервые руины, самоцветы опыта, сундуки и сохранение между забегами.",
            "Кампания обрела четыре печати и охоту на боссов.\nПобеды открывают героев, главы и страницы Зала славы.",
            "Добавлены биомы, постоянные улучшения Кузницы Душ, новая музыка\nи больше вариантов предметов для разных сборок.",
            "Рывок стал боевым инструментом. Оружие и артефакты образуют симбиозы,\nа суперумение даёт шанс переломить тяжёлую орду.",
            "Офлайн-лаборатория учит агентов на боях; в игре появились редкие умные мобы.\nУлучшены поздняя орда, звук, баланс и лимит активных существ.",
            "Починено дёргание разворота. Прорывы идут по уровням, боссы получили\nтелеграфируемые умения, появились настройки и обученный советник.",
            "Тиры предметов открываются по времени, а сундуки показывают каждый шаг. Резонаторы выпускают орды; давление адаптируется, Прорывы растут.\nВ Бездне боевые пауэрапы падают только с умных мобов. Договоры Кузницы усиливают врагов за золото. Щиты блокируют фронтальные обычные атаки, крит пробивает.\nВыбирай Сюжет или Endless. Боссы приходят по расписанию; в финале истории — троица. Редкий портал сохраняет забег и умножает добычу на 1,5.\nСледы снарядов отключаются. Музыка меняется для орды, босса, передышки и победы; магнит получил мягкий восходящий звук. Старые сохранения мигрируют без потерь.",
            "Победная тема получила светлую мажорную гармонию. Магнит звучит четырьмя мягкими восходящими нотами и повторяет мотив, пока притягивает добычу. Пустые строки в многострочном тексте теперь сохраняют интервал. Дополнены аудиотесты для музыкальных настроений.",
            "В меню включается отдельная медленная, разреженная и тихая тема. Сигнал орды и боевые эффекты приглушены, пики микса теперь смягчает лимитер. Три целых резонатора дают 36 врагов в минуту вместо прежних 99; каждый — +12/мин.",
            "Сюжет получил трёхчастное вступление и визуальные сцены глав; спавн виден на экране, а босс возвращается по направлению движения героя.",
            "Удалены отвергнутые фоновые PNG. Катсцены пересобраны из игровых пиксельных спрайтов и кодом нарисованных архитектурных слоёв. Феникс получил заметное крылатое возрождение и отдельный звук; SFX варьируются по высоте и громкости. Резонаторы появляются по ходу забега: максимум 8 на поле, каждый добавляет около 4 врагов/мин; компас ведёт к ближайшему, разрушение выпускает 24 врага. Советник оценивает угрозу, здоровье и дистанцию до симбиоза, а не печатает универсальную фразу. Добавлены отсутствовавшие стрелки ← и →.",
            "Советник теперь сравнивает все четыре карты за один проход по состоянию боя, показывает основную рекомендацию и сильную альтернативу. Оценка учитывает реальную плотность врагов, расстояние до ближайшей угрозы, активного босса, HP, число оружий и точные ранги до симбиоза. Пояснение называет конкретную пользу или цену выбора; при низком здоровье, толпе и боссе приоритет меняется. Без фальшивых процентов уверенности.",
            "Переписан сюжет «Последняя запись»: Архив хранит копии последних мгновений, а не возвращает умерших. Четыре печати сохраняются между попытками; встреча со стражем открывает улику даже при последующем поражении. В Зале славы появился Реестр Свидетеля с мотивами пяти героев, четырьмя стражами и найденными записями. Финальное трио — защитная копия трёх приказов, оно открывается после четырёх побед над стражами. Новые короткие катсцены и реплики нарисованы/собраны только из уже имеющихся пиксельных ресурсов; повторы больше не прерываются главными сценами каждые 90 секунд.",
            "Полный аудиопроход: раздельные сохранённые уровни SFX и музыки в настройках, мягкое затухание при выключении/включении, приоритеты голосов и защита важных сигналов от вытеснения частыми эффектами. Микс слегка приглушает музыку на критических SFX, ограничивает перегрузку и плавно выравнивает громкость. Самоцветы остаются тихими, с интервалом и пентатоническим чередованием нот. Добавлены тесты настроек, полифонии и пикового уровня.",
            "Музыкальный стиль собран как тёмное фэнтези с готическим фолк-оркестровым оттенком: минорный остинато-бас, струнно-хоровая подкладка, щипковые арпеджио и колокольный мотив. Слои орды и босса добавляют тяжёлый пульс и фригийское напряжение; переходы ждут ближайшую восьмую долю и плавно смешиваются. Всё синтезируется прямо в игре, без музыкальных файлов.",
            "Строения на поле теперь однозначно называются резонаторами; счётчик склоняет название по числу: 1 резонатор, 2–4 резонатора, 5–8 резонаторов. Добавлен редкий враг-матрёшка: после смерти он раскрывается и выпускает двух обычных врагов. Атаки боссов и цвет снарядов приведены к их силуэтам: кости Моргара, зелёная чума Малакора, алые серпы Кардинала и пепельное пламя Азарота.",
            "Музыка полностью переписана: 8-тактовая тема в ре миноре, 4/4, 96 BPM; две фразы ведут от i через VII/VI/iv к доминанте A7, а C# разрешается в D. Бас, арпеджио и мелодия теперь имеют отдельные осмысленные линии. Орда и босс добавляют синхронные ударные; передышка разрежает фактуру, победа поднимает тему в мажор. Настроения переключаются на тактовой черте. Новые и возвращающиеся с дальних координат монстры выбирают точку внутри безопасного видимого поля; дети матрёшки сохраняют локальное место гибели только если полностью помещаются в кадр. В описании предмета оставлен «Тир II» без тайм-кода.",
            "Теперь можно сравнить десять музыкальных эскизов прямо в игре: от ре минора и мажора до северного ми минора, кинематографического до минора, электронного фа-диез минора, хрустального си минора и ре дорийского. F1–F10 выбирают вариант во время боя; в настройках есть выбор стрелками и мышью. Все темы синхронизированы по 96 BPM и переходят на следующей тактовой черте с коротким кроссфейдом. Текущий выбор сохраняется. 100-секундное превью содержит по 10 секунд каждой версии.",
            "По отзыву игрока увеличена длина основного «Пепла»: теперь в нём 32 такта — четыре разные восьмитактовые секции (A/B/C/реприза), полный цикл длится 80 секунд. Остальные варианты тоже получили разные гармонические последовательности и мелодические контуры, а не только перенос в другую тональность. F1–F10 по-прежнему переключают треки на музыкальной границе. Добавлено отдельное 80-секундное превью полного «Пепла».",
            "Вместо десяти похожих гармонических эскизов — десять разных аранжировок с собственным темпом (72–128 BPM), инструментальными тембрами и ритмом: струнно-кинематографический «Пепел», арфа «Рассвета», ледяные колокольчики «Севера», орган, пиццикато, хор, электронный синт, глокеншпиль, маршевый брасс и флейта с ручным барабаном. В настройках показаны темп, инструменты и настроение; F1–F10 переключают темы в игре.",
            "Музыка сочинена заново: у каждой из десяти тем собственная четырёхтактовая мелодическая фраза, аккордовая последовательность, басовая партия и ритм; используются размеры 4/4, 3/4 и 6/8. В «Пепле» минорная тема Dm–B♭–F–A7 завершается ведущим C♯ и разрешением в D. Короткие мелодические мотивы и ясные каденции теперь задают основу аранжировке. В меню выбора темы показываются размер, темп и инструменты; переход на новый трек по-прежнему происходит на границе такта с кроссфейдом.",
            "Пересобраны встроенные пиксельные спрайты пяти героев, 11 типов обычных врагов и четырёх боссов. Уточнены силуэты классов, плащи, броня и знаки фракций; единый контур и свет сверху слева помогают формам читаться на игровом поле. У всех сохранены два анимированных кадра; исправлена таблица указателей 11-го типа врага, чтобы матрёшка корректно отображалась; внешние изображения и story-фоны не добавлялись.",
            "Обновлены не персонажные спрайты: оружие и артефакты получили отдельные пиксельные иконки, самоцветы/добыча/сундук/портал — рисунки на поле, а резонатор стал фасетированным колокольным кристаллом с каменной опорой. Сохранены цветовые подсказки, игровые свечения и анимация. Атлас встроен в EXE; механика и баланс не менялись.",
            "Частицы перестали быть однотипными точками: горячие вспышки тянутся угольными хвостами, холодные снаряды оставляют узкие направленные следы, осколки стали гранёными, зелёные/фиолетовые эффекты — руническими мотыльками, а незаряженные разлёты медленно рассеиваются как пепел. Вращение и масштаб меняются по жизни частицы, затухание плавное; число частиц и механику боя не увеличивали.",
            "Обновлён интерфейс боя: сгруппированы здоровье и рывок, экипировка помещается в две компактные строки, таймер отделён от биома и фазы. Панели получили общую систему акцентов, а предупреждения босса, сюжетные сообщения и сигналы сложности теперь разнесены и переносят длинный текст без обрезки. Стартовый экран получил отдельную титульную плашку и больше воздуха между режимом и действиями. Правила, баланс и управление не менялись.",
            "Добавлена безопасная арена прослушивания босса: F12 из меню открывает сцену сразу, F1–F10 выбирают музыкальный вариант, H/B переключают контексты орды и босса, Esc возвращает в меню. Забег и сохранение не меняются. В режиме босса используется выбранная игроком версия B с чёткими долями и меньшим конфликтом акцентов. Victory-каденция получила мрачный, но более победный оборот; она остаётся preview-only до отдельного одобрения.",
            "В музыкальной тест-арене F12, H/B и F1–F10 теперь меняют контекст/тему немедленно с коротким кроссфейдом, без ожидания границы такта. Это действует только в тестовой сцене: нормальная музыка игры по-прежнему переходит на тактовой черте, чтобы сохранять фразировку.",
            "Босс получил самостоятельную тему с новой мелодией и гармонией, а не основную тему на ускорении. Экран прокачки теперь звучит быстрее и загадочнее: 120 BPM, 3/4, целеста, хроматические полутона и открытая доминанта. Урон/сек и SFX/MUSIC разнесены по разным строкам HUD.",
            "По отзыву заменён отвергнутый быстрый cue прокачки: восстановлен вариант A — более спокойная исходная тема 72 BPM, 4/4. Новая тема босса и разнос DPS/SFX по HUD сохранены.",
            "Музыкальный режим и темп теперь переключаются сразу с коротким 100 мс кроссфейдом, не дожидаясь окончания ноты или такта. Темп автоматически растёт вместе с числом врагов: медленно (<40), средне (40–89), быстро (90–159), очень быстро (160+); границы имеют гистерезис.",
            "Оружия получили разные темпы роста, более сильные поздние ранги и отдельные подсказки по эффектам; пассивки заметно усилились.\nОбычные враги теперь входят из-за экрана, стартовые резонаторы отодвинуты от героя.\nЛечение может выпасть с любого убитого врага, если герой ранен; чем ниже здоровье, тем выше шанс.\nПосле выбора улучшения боевая тема босса восстанавливается сразу. Реплика охоты заменена на понятную цель: подобрать шесть монет после боя.",
            "Из описания Тира III убрано упоминание времени открытия; само условие открытия не менялось. Кровавый осколок даёт +30% к криту за ранг, а крит сверх 100% становится суперкритом ×4. Редкие тиры получили больший вес в выборе. Сборка начинается с 5 предметов, растёт после побед над боссами и с уровнями.\nСоветчик учитывает тиры и обновлённые значения предметов. ХП резонатора равно 5-кратному среднему DPS за последнюю минуту (на старте — калибровка 100 DPS). Магниты проверены: исчезают через 60 секунд активной игры.\nПосле убийства трёх эхо-боссов кампания заканчивается без портала; повторное прохождение тоже не зависает на 5/5.",
            "Исправлено смещение резонаторов при раздвигании толпы. Скорость прокачки сбалансирована: порог опыта теперь растёт по уровню без резкого лавинообразного набора; обычные самоцветы дают 3/6 опыта, элитные — 18, Книга мудрости — +10% за ранг. Тексты предметов Тиров II–V проверены: указаны эффекты, время открытия из описаний убрано.",
            "Раздвигание толпы больше не сдвигает резонаторы. Повышения уровня стали реже: прогрессия опыта скорректирована, а стартовый темп проверен симуляцией. После выбора улучшения тема прокачки передаёт музыку боя без наложения двух мелодий. Добавлены четыре реликвии: Клеймо Охотника, Руна Возврата, Сердце Последнего Эха и Покров Междумирья. Описания всех тир-предметов содержат эффекты, но не время открытия.",
            "Галочка «Больше не показывать патчноуты этой версии» больше не зависит от отсутствующего символа шрифта: она нарисована контрастными линиями внутри подсвеченной рамки и хорошо видна в отмеченном состоянии.",
            "Первый уровень теперь достигается раньше: начальный порог XP снижен, но рост требований на следующих уровнях сохранён умеренным. Ледяное кольцо получило отдельный тихий кристальный звук, который звучит при успешной заморозке хотя бы одного врага.",
            "Кольца древних печатей на земле стали многослойными и анимированными: вращаются насечки и глифы, мягко пульсирует свет. Ледяная волна получила более сложную рунную геометрию. Рамка карточек, сундуков и предметов в сборке теперь кодирует тир цветом; на карточке также есть подпись тира. Резонаторы размещаются за пределами камеры на расстояниях 10–20 км и остаются достижимыми по компасу.",
            "Добавлены четыре активных оружия высоких тиров: Осколок Кометы (II), Рунный Разворот (III), Копьё Разлома (IV) и Звезда Пепла (V) — с собственными залпами, импульсом, цепным разрядом и метеорным взрывом. Постоянных договоров теперь шесть, у каждого до 10 рангов; новые клятвы увеличивают натиск и награду за золото. Сохранения обновляются до v6 без потери старых рангов и story-масок; миграция v5 проверяется отдельно.",
            "Кампания получила отдельный экран выбора из шести миссий; новые уровни открываются печатями, каждый начинается отдельным забегом со своей целью и боссом. На карточке уровня заранее указана примерная длительность партии и время выхода финального босса. Во время боя HUD показывает обратный отсчёт до босса и длительность миссии; после победы выдаётся сундук, затем экран завершения с повтором или переходом к следующей открытой миссии. Endless и прежние сохранения кампании сохранены.",
            "Ускорена растеризация кругов, колец и свечения; результат совпал с прошлой версией, а стабильная сцена стала примерно на 9% быстрее.",
            "Уточнены бейджи тиров и темп резонаторов; экран кампании получил отдельный переход, музыкальные сигналы сложности стали заметнее. XP pacing проверен регрессией.",
            "Выбор кампании получил компактный список и подробное досье каждой миссии: иллюстрацию, цель и тайминги. Статусы прохождения хорошо различимы; закрытые миссии можно просмотреть, но нельзя запустить.",
            "Главное меню теперь начинает с выбора режима — Сюжет или Endless. После режима отдельно выбирается герой; затем Сюжет ведёт к выбору миссии, а Endless запускает забег. Управление мышью и клавиатурой сохранено.",
            "Музыка ускоряется по всем десяти вариантам; быстрые уровни получили более заметные темповые множители. В Endless боссы идут бесконечным циклом каждые 90 секунд, четверо стражей повторяются по кругу, а лимит трёх одновременных боссов лишь задерживает следующий спавн. У каждого босса три боевые фазы с подписью, усилением атак, телеграфами и отдельными тяжёлыми SFX. Повышение тира после победы над боссом получило крупный HUD-сигнал и фанфару. Прояснён текст миссии «Архив», исправлено переполнение верхней плашки, режимные карточки дополнены круглыми пиктограммами.",
            "Главный выбор режима стал доступен без мыши: стрелки влево/вправо и A/D перемещают явный фокус между Сюжетом и Endless, Enter продолжает. Активная карточка получает контрастную рамку, подсветку и бейдж «ВЫБРАНО»; наведение мышью временно переносит акцент. Радиальные мотивы оставлены в виде крупных тематических иконок, а не превращены в скрытый pie-menu — карточки остаются понятными и для новых игроков.",
            "Снаряды получили отдельные выразительные звуки полёта и удара при попадании; фазовый сигнал босса стал длиннее и многослойнее. Переход фазы отмечается большой цветной плашкой. Зал славы получил кнопку выхода, Арсенал — прокрутку и перенос текста. Постоянные улучшения Кузницы можно развивать до 50 рангов: ранги после XII дают меньший вклад, а цены растут плавнее.",
            "Временная попытка обновить меню круглыми медальонами улучшила значки, но не дала настоящего колесного выбора. В следующей итерации такой интерфейс заменён секторным pie-wheel.",
            "Главное меню теперь использует настоящее колесо выбора из двух крупных полусекторов: слева Сюжет, справа Endless; центральная область — нейтральная зона подтверждения. Клик выбирает сектор по стороне и углу, наведение меняет подсветку, стрелки/A-D перемещают фокус. Внутри секторов — открытая книга и спиральные штрихи, но не отдельные круглые кнопки. Вторичные разделы остаются в компактной нижней панели.",
            "В радиальное меню перенесены все семь главных пунктов: Сюжет, Endless, Кузница, Арсенал/справка, Зал славы, Настройки и Выход. Каждый пункт — отдельный сектор с заметным угловым зазором; пустые промежутки не кликабельны. Наведение мышью или фокус клавиатуры одновременно подсвечивает выбранный сектор и увеличивает его наружу/к центру, остальные остаются компактнее. Стрелки и WASD обходят колесо по кругу, Enter или клик подтверждают; центральный хаб показывает текущий выбор. Нижняя панель удалена.",
            "Исправлена подпись Endless: пояснение разделено на короткие строки «ВОЛНЫ / БЕЗ КОНЦА» и остаётся внутри сектора. Заголовок меню стал компактнее и получил прямоугольную панель без скруглений — так проще компоновать интерфейс на узком экране. При выборе «Выход» появляется вопрос с действиями «Выйти» и «Остаться»; безопасный выбор по умолчанию — остаться. Escape отменяет, Enter подтверждает, работают мышь и клавиатура.",
            "Окно игры меняет логический canvas под реальные пропорции: ландшафт, 4:3 и портрет показывают полное поле без растяжения и обрезки. Переразмещены круговое меню, герой, выбор миссии с досье, настройки, Зал славы, Кузница, Арсенал, HUD, игровые карточки и модальные окна. В портрете HUD складывается в компактные верхние панели; интерактивные элементы остаются в пределах экрана. Добавлены проверки рендера и кликов для 16:9, 4:3 и portrait.",
            "Вторичные меню получили единый тёмный фон, прямоугольные золотые шапки и согласованные акцентные панели — на landscape, 4:3 и portrait. Общая рамка применяется к настройкам, Залу славы, Кузнице, Арсеналу, выборам и итоговым экранам без изменения игровых функций. Добавлены пиксельные проверки общей шапки; все адаптивные regression-тесты проходят.",
            "Исправлено центрирование центрального HUD и облачков истории в широкоформатных окнах. Фаза босса теперь показывается компактной нижней плашкой вместо затемнения и крупной панели поверх поля. Для семи основных оружий, четырёх высокотировых атак, рывка и суперумения добавлены разные эффекты попаданий на земле, под спрайтами; метки не рисуются вблизи героя и ограничены по количеству. В Арсенале подписи прокрутки перенесены в две отдельные строки, карточки центрируются по ширине, а кнопка возврата больше не перекрывает текст.",
            "Добавлен реликт III тира «Зеркало уклонения»: точный рывок, прошедший сквозь врага, снаряд или опасную зону босса, даёт один заряд. Каждый рывок может дать не более одного заряда; серия из 4/3/2 успешных уклонений (по рангу) выпускает круговой контрудар, который наносит урон рядом и отталкивает обычных врагов. Контратака усиливается радиусом сборки, но не поражает резонаторы. Заряд и обратная связь видны во время боя; активируется с 5:00, максимум 3 ранга.",
            "Переосмыслены пиктограммы артефактов: магниты, перчатки, перья Феникса, линзы, метки охотника, возвращающаяся руна, вихревой импульс, копьё и метеор теперь считываются по силуэту, а не по повторяющемуся спрайту. Резонаторы появляются ближе: размещаются за границей видимой камеры, а не в тысячах игровых единиц; новые цели также ближе и с безопасным расстоянием между ними. Для семи основных оружий разблокировано девятое улучшение только при соответствующем артефакте IV ранга; выбор ранга 9 сразу превращает оружие в супероружие, с явным именем эволюции. При смене фазы босс получает 0,5 с неуязвимости; защита включается сразу на пороге, чтобы серия атак в одном кадре не пропустила фазу.",
            "Арсенал автоматически выстраивает предметы от тира V к I; внутри каждого тира сначала идут оружия, затем артефакты, а элементы каждого типа имеют стабильный порядок. На карточках сохранены обозначения тира и типа, заголовок сообщает принцип сортировки. Прокрутка и раскладки 16:9, 4:3 и portrait работают поверх нового порядка.",
            "Новый артефакт IV тира «Печать Разоблачения» отмечает цель серией попаданий: 4/3/2 удара по одному противнику за 2,5 секунды открывают слабое место на 1,9/2,2/2,5 секунды. Пока метка активна, все атаки наносят этой цели на 10/14/18% больше урона. Повторные попадания не продлевают метку; накопление сбрасывается, если прервать серию. Появились отдельная пиктограмма, фиолетовый контур цели, карточка прогресса и контекстный советчик; проверки покрывают все ранги, таймеры и реальный бонус урона.",
            "Добавлен артефакт IV тира «Чаша Переполнения» (3 ранга, открывается на 8:00): 30/35/40% лечения сверх максимального HP превращаются в щит с пределом 16/26/36 ед. Щит не заменяет здоровье и поглощает урон раньше «Покрова Междумирья». Золотое свечение героя и полоска HUD показывают запас. Все источники лечения используют общий путь конверсии; тесты проверяют перелечение, пределы рангов, порядок поглощения и фактический урон.",
            "Добавлен реликт IV тира «Осколок Последнего Удара» (до 3 рангов, открывается на 9:00): при убийстве обычного врага избыток урона направляется в короткий импульс по ближайшим 2/3/3 целям. Конверсия — 16/20/24% избытка, с пределом 12/17/22 ед. на цель; радиус 92/108/124, глобальная перезарядка 1,25/1,05/0,85 с. Осколки не цепляются, не поражают боссов и резонаторы; импульс не может критовать. Добавлены отдельная пиктограмма, контекстный советчик и тесты порядка целей, ограничений, перезарядки, отсутствия каскада и фактического урона.",
            "Введён «Ключ Пепельного Ритма»: самоцветы опыта заряжают всплеск, который уменьшает оставшееся время атак активного оружия на 0,15/0,22/0,30 с. Убийства сами по себе не дают заряд; каждый самоцвет учитывается один раз. Добавлены видимый счётчик/всплеск, иконка и проверка реального ускорения оружий.",
            "Добавлена «Призма Перекрёстного Огня»: 4/3/3 разных активных оружия должны поразить одну цель за 2,2/2,5/2,8 с. Затем рикошет бьёт 2/3/4 соседей. Отдельные кулдауны цели и всей сборки ограничивают частоту, боссы и резонаторы не поражаются, цепочка невозможна.",
            "«Угольный Фитиль» превращает крит активного оружия в ограниченный по времени ожог обычной цели или элиты. Горение не складывается, не критикует и не запускает новые пассивные эффекты. Добавлены читаемый огонь, иконка, ранги/перезарядка и проверки.",
            "«Осколок Оттепели» позволяет врагу, пережившему заморозку Ледяным кольцом, выпустить волну осколков по ближайшим целям. Откол не замораживает и не может вызвать следующий откол; добавлены дистанционные ограничения и перезарядка.",
            "«Печать Обратного Тока» перехватывает только обычный вражеский снаряд перед попаданием и обращает его в ответный выстрел по ближайшему врагу. Боссовые снаряды, контакт и наземные атаки исключены; при отсутствии цели снаряд поглощается. КД фиксирован на срабатывании.",
            "«Знак Палача»: нелетальное попадание активного оружия добивает обычного врага или элиту ниже 8/12/16% HP. Боссы/резонаторы и пассивные атаки исключены; лишний урон не создаётся, поэтому цепочка осколка последнего удара не запускается.",
            "Новый реликт IV тира «Печать Возмездия» (3 ранга, доступен с 10:00): реальная потеря HP заряжает один ответный удар на 4,5 с. Следующее попадание активного оружия наносит +35/50/65% урона; заряд не суммируется. Полное поглощение удара щитом его не выдаёт. Пассивные атаки, рывок и заблокированный попаданием щит врага заряд не расходуют. Индикатор встроен в HUD; проверены три ранга, срок, потребление, источники входящего урона и исключения.",
            "«Руна Свидетеля» (ID39, IV тир, открывается на 10:15) приносит +8/12/16% суперзаряда за разрушение резонатора. Бонус учитывает уже собранные множители суперзаряда, ограничен общей шкалой 100% и выдаётся только один раз при фактическом разрушении; простое повреждение и повторный удар по нулевому HP его не дают. Добавлены контекстный советчик, ранговое описание и отдельная пиктограмма. Проверяются все ранги, коэффициент, кап, draft, резонаторы и повторное срабатывание.",
            "Сюжетная прокачка привязана к миссиям: первая миссия начинается с тира I; следующие открывают тиры II, III, IV и V сразу с первого драфта (финал остаётся на V). Победа над боссом текущей миссии повышает тир сборки ещё на один в пределах V. В Endless сохранена прежняя разблокировка предметов по времени. Каждая следующая сюжетная миссия повышает HP, урон и скорость врагов, а также темп их появления на 3% относительно предыдущей миссии (до +15% в финале). В досье миссии показаны стартовый тир и прибавка сложности.",
            "Версия теперь задаётся в одном месте: build_windows.bat передаёт её компилятору, и она же попадает в имя файла, в свойства EXE, в заголовок окна и в эту хронику. Сборка идёт в отдельный каталог build и заменяет готовый EXE только после успешной компиляции, поэтому неудачная попытка больше не оставляет проект без рабочего билда. build_windows.bat принимает версию аргументом, корректно читает пустой version.txt и хранится с CRLF.",
            "Музыка теперь берётся из ваших файлов: игра читает MP3 из папки audio/ рядом с .exe (ss_menu_main, ss_gameplay_run, ss_boss_theme, ss_victory_boss, ss_gameover_death, ss_forge_shop) и играет их вместо встроенной синтезированной темы. Треки декодируются на ходу, поэтому 3–5 МБ файлы не нужно ни во что конвертировать; громкость музыки, клавиша [N] и приглушение во время громких эффектов работают и для них. ss_levelup_ding и ss_upgrade_pick звучат как короткие сигналы при новом уровне и при выборе улучшения. Если файла для сцены нет — для неё остаётся встроенная тема, поэтому игра не замолкает даже с пустой папкой audio/",
            "Кузница: описание улучшения больше не уходит под кнопку «КУПИТЬ» — иконка, текст, шкала рангов и кнопка теперь раскладываются от краёв карточки, поэтому в широкоформатном и портретном варианте ничего не наезжает друг на друга. В договорах кадр иконки больше не вылезает над карточкой и не перекрывает подсказку «Покупка усиливает каждого следующего врага», а счётчик рангов переехал к шкале прогресса, чтобы длинные названия вроде «Марш голодной стаи» помещались целиком. Сетки карточек центрируются на широких экранах. Пока открыта карточка нового уровня, музыка боя становится на 80% тише и так же плавно возвращается. На экране смерти из строки урона убрано «ед.».",
            "Озвучка сообщений: системный голос Windows читает баннеры боя, сюжетные строки\nи предупреждения о боссах; в настройках появился переключатель «Озвучка сообщений».\nВсе сообщения собраны в единый реестр, битые строки видны в оверлее F3.\nF3 — техническая панель как в Minecraft: версия, FPS, сущности, звук и озвучка.\nСообщения боя — прозрачное окно в левом нижнем углу вместо полосы по центру.\nМетки на мобах больше не «диаметр»: спрайты центрируются по видимому рисунку.\nПункт «Музыка» и выбор вариантов убраны: музыка берётся только из audio/.",
            "Озвучка сообщений больше не пользуется системным синтезатором Windows: все фразы —\nзаписанные файлы в audio/voice/, имя файла равно ключу сообщения из реестра.\nФразы идут по шине эффектов, музыка приглушается, пока реплика звучит.\nНет файла — строка молчит, а F3 и настройки показывают, сколько фраз не записано.\ntools/check_voice.py сверяет реестр с папкой; VOICE_LINES.md — таблица всех фраз.\nМетка щита на мобах — дуга со стороны блока вместо «диаметра» рядом с мобом.",
"Главное за 2.9.76–2.9.80.\n• Кампания: 28 дел в пяти актах — нуар-детектив с уликами, допросами копий и четырьмя печатями. Сценарий — STORY.md.\n• Сейв v7 хранит прогресс маской из 28 дел; старые сейвы переносятся, ветки требуют оба следа.\n• Выход босса очищает арену, затем секунду спавн идёт ×5. Тихая охота — возможность, прорыв — стена.\n• Словарь терминов по G. Озвучка: 36 фраз, все записаны и выровнены по громкости.",
            "Боевые сообщения живут только в голосе и в левом нижнем углу: страж, фаза босса, тир сборки, сложность, тихая охота и сюжетная строка собраны в одну стопку плашек. Центр поля больше ничем не перекрывается.\n• Элитные и особые враги получили янтарный отсвет и светящийся силуэт — их видно в толпе.\n• Тихая охота не путает итог: у победы и у провала отдельные фразы и отдельная озвучка.",
            "Пять мест вместо одной арены: Руины, Крипта, Собор, Пепельный трон и Сердце Архива отличаются кладкой пола, светом, огнём, ориентирами и воздухом. Endless проходит эти же места по ходу ночи.\n• Гарнизон у каждого места свой: крипта выставляет щиты и тяжеловесов, собор — стрелков, трон — броню.\n• Отряды приходят линией с одного края, а не бесконечной россыпью.\n• У каждой семьи своя походка и своя смерть: кости, слизь, искры, пепел.\n• Каждый страж метит пол своим знаком.",
            "Номер версии больше не берётся из локального файла: его читает src/version.h, поэтому сборка, заголовок окна и хроника больше не расходятся.\n• Кампания открывается на последнем доступном деле, а не на первом.\n• У каждого стража свой набор атак: Моргар бьёт волной и зовёт щитоносцев, Малакор льёт чуму и пускает самонаводящиеся сгустки, Кардинал идёт на рывок с узким веером, Азарот выпускает кольцо искр с проходом и спираль. Общих приёмов больше нет.",
            "Третий стиль иконок — «ЦВЕТ»: клинки синие, шар оранжевый, вихрь фиолетовый, молния золотая, аура алая, топор стальной, лёд бирюзовый, книга костяная, сапоги зелёные, магнит пурпурный. Один предмет — один цвет и один силуэт; остальные тридцать предметов — рунные печати в том же цвете.\n• Новая угроза: крысиная стая. Крысы не преследуют — они пересекают арену по прямой линией, кусают на бегу и уходят; их можно фармить по дороге.",
            "Описания дел озвучены: у каждого Дела своя запись, она читается при выборе дела и при старте. Записаны первые десять; остальные пока читает общая фраза.\n• Вторая десятка иконок «ЦВЕТ» готова: сердце, перчатка, часы, монета, перо, око, печать, осколок луны, линза и рассвет.",
            "Шесть стражей вместо трёх: каждый акт получил своего.\n• Моргар — волна по кругу и щит: правило «выйди из радиуса».\n• Малакор — зоны чумы и наводящиеся сгустки.\n• Кардинал — рывок по линии с узким веером.\n• Азарот — кольцо искр с прорезью и спираль.\n• Протоколист — вал строк поперёк арены с пробелом: надо не отойти, а встать в пробел.\n• Двойник — меняется местами с отражением и бьёт крестом из двух точек.\n• Все шесть нарисованы по-разному: скрижали, капюшон с колбой, шляпа и книга, парящее ядро, стойка с раскрытым томом, два стеклянных слепка.\n• Иконки «ЦВЕТ» для первых десяти предметов — нарисованные ИИ картинки с одним заданным тоном, а не код.\n• Крысы стали мельче, идут ровнее и разворачиваются по ходу движения.",
            "Стражи возвращаются: четыре дела второго прохода сводят с уже знакомым врагом.\n• Тот же силуэт и те же приёмы, но Архив поднял его злее: +45% HP, +15% урона, быстрее, открывается сразу со второй фазы и зовёт на двух врагов больше.\n• В досье дела стоит плашка «ВОЗВРАЩЕНИЕ • ТОТ ЖЕ СТРАЖ ЗЛЕЕ», в бою — второй контур и корона осколков, в полосе HP — подпись «ВОЗВРАЩЕНИЕ».\n• Так сюжет даёт шесть разных стражей и ещё четыре усиленных боя, не выдавая копию за нового босса.\n• Иконки «ЦВЕТ» для предметов 11–20 тоже нарисованы ИИ: сердце титана, перчатка, часы, монета, перо, око, печать, осколок луны, линза и рассвет — каждая в своём тоне.",
            "Третья десятка «ЦВЕТ» тоже нарисована ИИ: клеймо охотника, руна возврата, сердце последнего эха, покров междумирья, осколок кометы, рунный разворот, копьё разлома, звезда пепла, зеркало уклонения и печать разоблачения.\n• У каждой картинки в промпте задан один тон, поэтому предмет не может «покраснеть» в чужую десятку: 30 из 40 иконок больше не нарисованы кодом.",
            "Описания дел записаны дальше: теперь озвучены двадцать из двадцати восьми — дела 11–20 читаются своим голосом при выборе и при старте.\n• Все клипы приведены к одному уровню и одному формату новым tools/normalize_voice.py: 16 кГц моно, равная громкость, настоящие нарастания на краях вместо обнуления.\n• Оставшиеся восемь дел пока читает общая фраза.",
            "Озвучка сведена к одному рассказчику.\n• Все двадцать описаний дел перечитаны тем же голосом: раньше первая десятка звучала другим тембром, и на стыке дел это было слышно.\n• Громкость всех 59 записей выровнена по RMS новым tools/normalize_voice.py: 16 кГц моно, настоящие нарастания на краях, 80 мс перед первой согласной.\n• tools/check_voice.py получил два контроля: --levels (громкость и формат) и --voice (высота тона). Второй находит реплики, записанные другим голосом, до того как их услышит игрок.\n• Правило записано в audio/voice/VOICE.md: 29 боевых реплик ещё читает старый голос, они перечитываются партиями по десять — счётчик виден в проверке.",
            "Дела 07–10 перечитаны заново: «Крипта голосов», «Сестра, которая не слышит», «Холодный след» и «Тёплый след».\n• В «Тёплом следе» убрана рваная пауза после первой фразы: текст переведён на запятые, и в записи остался один короткий переход вместо двух длинных.\n• Все четыре клипа — тот же рассказчик, что и остальные описания (91–96 Гц), громкость выровнена.",
            "Дела 07 и 08 переписаны и перечитаны заново.\n• «Крипта голосов»: из озвучиваемой строки убрано имя, на котором рассказчик уходил в чужое ударение. Теперь «В колбах лежат последние слова сестры».\n• «Сестра, которая не слышит»: вместо «одной интонации» — «тот же голос», фраза перестроена без тире, ударения больше не гуляют.\n• Текст дела и запись правятся вместе: карточка и голос не расходятся.",
            "Акцент в озвучке лечится не голосом, а текстом.\n• Из описаний убраны слова, на которых синтез уходит в чужое ударение: «сестры» → «говорила сестра», «задал» → «спрашивал», «начал» → «стёр и своё имя», «понял» → «услышал», «тиснение» → «оттиск», «выбиты» → «резаны», «писарей» → «писцов», «интонацию» → «тот же голос».\n• Перечитаны дела 07, 08, 12, 13, 14, 15 и 19 — те, где такие слова были.\n• tools/check_voice.py --stress сам ищет в записанных описаниях слова с плавающим ударением и падает в строгом режиме, поэтому новая запись с «сЁстры» не уйдёт в сборку незамеченной.",
            "Найдена настоящая причина странного произношения.\n• Реплики приходят в 44,1 кГц, а нормализатор понижал частоту линейной интерполяцией без фильтра: верхние гармоники складывались обратно в слышимую полосу.\n• Замер: было 2,95% энергии в полосе 7–8 кГц, стало 0,11%. Именно этот «звон» читался как лишние гласные — «прыотыокыол» вместо «протокол».\n• В нормализатор добавлен срез выше 7,2 кГц перед пересчётом, а tools/check_voice.py --alias считает эту долю у каждого клипа.\n• Дела 01–10 перезаписаны новым рассказчиком через исправленный тракт; остальные 47 клипов отмечены как поражённые и перечитываются партиями.",
            "Остаток «акцента» лечится текстом: голос вставляет гласные на стечениях согласных.\n• «тронут штормом» звучало как «троунут штоурмоум» — теперь «Морг цел. Буря его не задела».\n• «после шторма» заменено на «после бури» в первом деле.\n• В tools/check_voice.py --stress добавлены эти два слова, чтобы такая запись не прошла проверку молча.",
            "Аудит всей игры пройден целиком: 28 дел достижимы от пустого сейва, акты открываются, стаи крыс расходятся, 12 боёв и десятиминутный забег без утечек.\n• Закрытие дела с усиленным стражем больше не ставит пятую печать: бит ставят только первые четыре стража.\n• Полоса босса у Протоколиста с «возвращением» вылезала за край на 537 px — теперь подпись сжимается до «• ВОЗВР.».\n• В списке дел «ЗАКРЫТО» наезжало на длинные названия; если места нет, статус не рисуется.\n• tools/apply_campaign.py при каждой записи добавлял лишнюю «;», поэтому его --check не проходил никогда.\n• Меню считает карту секторов один раз на разрешение: кадр 15,8 → 8,9 мс. Картинка прежняя, пиксель в пиксель.",
            "Перечитаны десять худших боевых реплик — те, где старый тракт оставлял больше всего «звона».\n• Страж приближается, Орда не отступит, Симбиоз пробуждён, Крысы идут через поле, Бесконечная ночь начинается, Стая ушла по следу, Феникс возрождает тебя, Свидетель без имени, Стая уничтожена, Подбери монеты.\n• Звон в 7–8 кГц у каждой: было 5,9–11,9 %, стало 0,00 %; высота тона 93–111 Гц — тот же рассказчик.\n• Нормализатор больше не оставляет короткую реплику тише остальных: если пик не даёт добрать громкость, работает мягкое колено.\n• В банке осталось 37 клипов старого тракта и 16 — старого голоса.",
            "• Ветки — выбор: вторая запирается до «новой ночи» (в списке развилка А/Б).\n• После финала — НОВАЯ НОЧЬ: дела заново, другой след, золото в делах ×2.\n• Финал получил мораль и зависит от того, что ты сделал с копией.\n• Крысы идут кучкой, а не шеренгой, и исчезают, убежав далеко.\n• Оружие — квадратная пластина, артефакт — круглая; сорок разных цветов.\n• Азарот, Протоколист и Двойник перерисованы и анимированы.\n• Дела 21–28 озвучены: 28 из 28. Дело 09 перечитано, реплики громче.\n• Музыка: своя запись на каждое дело; в эндлесе случайный плейлист.\n• F9 — дебаг-меню по коду v404.",
            "• Азарот: плечи легли на балахон, корона стала гребнем из клиньев — раньше тонкие лучи с шариком на конце читались как усики антенны.\n• Протоколист: том стал меньше и опустился — теперь видно мантию и грудь, а не «книга под подбородком».\n• Оба варианта проверены на арене при ярости и в эхо-облике.\n• Реплика выбора ветки переписана и перезаписана.",
            "• Бриф последнего боя переписан: «Контур разорван… Он оставит живым право помнить самим». Слово «разомкнут» читалось с плавающим ударением, а «вернёт… вернёт» повторялось.\n• Паузы между мыслями теперь ставятся при сборке: длинный бриф собирается из трёх коротких кусков, поэтому синтез не угадывает ударение по всему абзацу.\n• music_prompts.md: промпты под каждое из 28 дел, под шесть стражей и их возвращения, под эндлес и служебные экраны — с темпом и «боевой» подложкой.\n• Файл ss_gameplay_caseNN теперь играет дело NN один-к-одному, без сдвига.\n• check_voice ловит слова-омографы («разомкнут», «замок», «черти») до записи.",
            "• Подписи «Страж зовёт подмогу» и «Сборка поднялась на тир выше» теперь совпадают с тем, что звучит в записи: старые строки читались, но в субтитрах оставался прежний текст.",
            "• Причина «тихо» была не в громкости, а в пик-факторе: при rms 3750 у свежих фраз пик уходил в 7,4 от среднего, а у банка — 4,7, и фраза слышалась тише соседей при формально равном RMS.\n• Весь банк пересобран режимом --dense: rms 4851–5242, пик-фактор 3,9–4,7 — теперь реплики звучат вровень, и это на 2,4 дБ громче прежнего.\n• check_voice проверяет не только RMS, но и плотность: клип с пик-фактором больше 5,4 больше не пройдёт как «нормальный».\n• Перезаписаны своим голосом player_death, portal_found, portal_used, resonator_spawn, story_act4, story_case_closed, voice_on, breakthrough; чужим голосом осталось 2 клипа.\n• Бриф последнего боя записан одним проходом: раньше склейка из трёх кусков давала неверную интонацию на «Контур разорван» и «Последний бой не вернёт погибших».\n• «Уходи из зоны удара!» стало «Зона удара! Уходи!» — приказ в два слова звучит как приказ.\n• update.bat печатает версию до и после git pull.\n• Вся ветка наконец влита в main: раньше работа шла в ветке, а git pull тянул main, который не менялся с 2.9.83.",
            "• Причина «версия 2.9.99, а по факту 2.9.83» найдена: работа шла в ветке, а git pull тянул main, который не менялся с 2.9.83. Вся ветка влита в main — теперь update.bat действительно обновляет игру.\n• update.bat печатает ветку, коммит и версию до и после pull и прямо пишет, если обновлений не было.\n• build_windows.bat после сборки перезаписывает ShadowSurvivors.exe в корне: рядом больше не остаётся старая сборка под привычным именем.",
            "• Иконки: раньше цвет шёл по кругу из десяти оттенков, и предметы 00, 10, 20, 30 были одного тона. Теперь у каждого из сорока свой цвет: семейство задаёт смысл (огонь — оранжевый, кровь — красный, лёд — бирюза, ветер — зелёный, руны — фиолет, магнит — пурпур, пергамент — янтарь), вариант — вес предмета. Минимальное расстояние между тонами выросло с 12 до 30.\n• Подложка: у оружия квадратная стальная пластина, у артефакта круглая каменная шайба — форма читается раньше цвета.\n• Азарот: каждый удар сердца расходится кольцом, в ярости пепел идёт в 1,6 раза быстрее.\n• Протоколист: страница тома переворачивается раз в 2,6 с, по скрижали-лицу идёт сканирующая строка.\n• Двойник: у настоящего тела появился шлейф из двух запоздавших слепков.",
            "Причина «тихие реплики» — в микшере, а не в записях: все семь фраз были в коридоре банка (rms 4,9–5,2 тыс., речь −14…−15 дБ), но реплика входила в сумму с усилением 1,20 против 1,75 у эффектов и проходила через общий лимитер — на прорыве её и было слышно тише боя.\n• Своя шина ×1,45; под фразой эффекты 0,58 (−4,7 дБ), музыка 0,34.\n• Лимитер видит только музыку и эффекты: голос идёт поверх него, под своим ограничителем 0,94.\n• Замер рендером: голос над боем +15,5 дБ вместо +13,9 дБ; без реплики микс совпадает с прежним до сэмпла.\n• В F3 — строка «ГОЛОС ШИНА»: усиление шины и живой присед эффектов.",
            "Причина: update.bat делал git pull той ветки, которая открыта, а готовая игра собирается из main. На ветке arena/... это ломалось двумя способами: пока ветка жива, pull честно отвечал «Already up to date», и скрипт молча собирал старую версию; после merge ветку удаляют на GitHub, и pull падал с «no such ref was fetched», не дойдя до сборки.\n• Теперь цель — origin/main: скрипт печатает версию и коммит, которые лежат на GitHub, переключает рабочую копию на main (свои правки остаются в прежней ветке), привязывает upstream, если его нет, и только потом делает pull --ff-only.\n• Если переключиться нечем или main разошёлся с origin, печатается точная причина и команды — вместо общего «обновление не завершено».\n• Консоль переводится в UTF-8: русский текст скрипта больше не читается как мусор.",
            "Скачанный с GitHub update.bat закрывался сразу после запуска. Причина не в коде: атрибут «*.bat text eol=crlf» держал в репозитории LF, поэтому кнопка Raw и ZIP отдавали файл без CRLF, а cmd.exe на таком файле падает при разборе первого же блока.\n• update.bat теперь хранится как есть, с CRLF внутри (-text -eol): Raw, ZIP и git checkout дают одинаковые байты.\n• Проверено кругом: коммит -> клон с core.autocrlf=true -> CRLF 151 из 151, одиночных LF нет.\n• Остальные батники остались текстовыми ради читаемых diff, но при скачивании Raw их так же нужно приводить к CRLF — правило записано в .gitattributes.",
            "Окно апдейтера закрывалось раньше, чем что-либо успевало напечатать. Две причины разом: скрипт был написан длинными русскими строками, а cmd.exe читает .bat построчно в кодовой странице консоли, и любой выход по условию завершался мгновенно, без паузы — сообщение об ошибке уходило в никуда.\n• update.bat теперь чистый ASCII, без блоков if ( ... ) — только однострочные if и goto.\n• Любой выход ставит паузу, поэтому окно не исчезает.\n• Ход обновления пишется в build\\update.log, вывод каждой git-команды — в build\\update_step.log и печатается при ошибке.\n• Если что-то всё же не так: cmd /k update.bat — окно останется, лог будет на месте.",
            "Скачанный руками update.bat сам блокировал обновление: git switch в main отказывался, потому что этот файл изменён в рабочей копии, и скрипт останавливался на «could not switch to main».\n• Теперь, если единственный изменённый файл — сам update.bat, апдейтер копирует его в build\\update.local.bat, возвращает версию из ветки, переключается, делает pull и кладёт скачанный файл обратно.\n• То же перед pull: если main когда-нибудь тронет update.bat, локальная копия больше не ломает git pull.\n• Если изменены и другие файлы, скрипт печатает их список и ничего не трогает — вместо общего «обновление не завершено».\n• Проверено тремя сценариями на копиях репозитория: ветка со скачанным батником, main меняет update.bat, и сторонние незакоммиченные файлы.",
            "Дважды подряд запускался старый апдейтер, скачанный до исправления: по консоли и логу нельзя было понять, какая копия работает, и одинаковый текст ошибки выглядел как «исправление не помогло».\n• Теперь вторая строка вывода — updater build X.Y.Z, та же строка пишется в build\\update.log.\n• Если номер в консоли старше обещанного — скачалась закешированная копия: обновите страницу с Ctrl+F5 или добавьте к адресу ?v=X.Y.Z.\n• Размер файла тоже говорит о версии: 2.10.8 — 6,5 КБ, 2.10.9 — 9,2 КБ, 2.10.10 — 9,4 КБ.",
            "Обход собственного файла не срабатывал: строка «for /f tokens=2*» кладёт в первую переменную второй токен, а во вторую — остаток строки. Путь попадал в первую, сравнивался пустой хвост — и апдейтер всегда видел помеху, даже когда мешал только он сам.\n• Проверка переписана: «for /f tokens=1,*» плюс подпрограмма :note_change. Помехой считается любой изменённый путь, кроме самого update.bat.\n• Проверено таблицей случаев: только update.bat — обход и переключение; update.bat со сторонним файлом — остановка; путь с пробелом — остановка.\n• При остановке скрипт пишет, что ничего не удалено, и даёт три команды: git stash -u, update.bat, git stash pop.",
            "Сообщения апдейтера шли крякозябрами: файл был в UTF-8, а cmd.exe читает .bat в кодовой странице консоли, и одна такая строка вообще исполнилась как команда.\n• Теперь текст в cp866 и первой строкой chcp 866: русские сообщения читаются как есть; вся логика остаётся ASCII, поэтому даже без chcp апдейтер работает.\n• Строка «версия апдейтера 2.10.12» во втором выводе и в build\\update.log показывает, какая копия запущена.\n• Ни одна ветка не печатает нераскрытые !ПЕРЕМЕННЫЕ!: значения проверяются до вывода.",
            "Тупик в кампании: у дела 025 в пререквизитах стояло только дело 023, поэтому выбор «СЖЕЧЬ КОПИЮ» (024) закрывал акт V навсегда.\n• Теперь дело 025 открывается любым финалом акта IV.\n• Перебор всех шестнадцати комбинаций выборов на графе дел подтверждает: до финала доходят все, за прохождение берётся 24 дела из 28 — по одному из каждой пары.\n• Сама развилка пока меняет только сторону истории: следующее дело у обоих следов одно и то же.",
            "Кампания переделана: после акта I игрок выбирает след - СВИДЕТЕЛЬ или КОПИЯ - и дальше это два разных прохождения по десять дел.\nСписок глав стал деревом: ствол из пяти актов, у актов II-IV по две полосы, выбранный след светится, второй перечёркнут до новой ночи.\nФинальный выбор сохранить или сжечь копию переехал в акт V и снова взаимоисключающий.\nСейв v8: закрытые дела переносятся по номерам записей, выбор следа восстанавливается сам.\nЗаписи описаний остались прежними: каждое дело играет свой клип, новых текстов нет.",
            "Апдейтер переписан: русский текст снова читается - файл сохранён в cp866, а все команды остались ASCII.\nРусский, который в консоли превращался в кракозябры и выполнялся как команды, вернулся в нормальные echo.\nОбновление больше не обрывается: нет сети, нет ветки на GitHub или pull не проходит - собирается то, что уже скачано.\nЕсли мешают несохранённые правки, апдейтер прячет их в git stash, обновляется, собирает и возвращает тебя на прежнюю ветку вместе с правками.\nСкачанный руками update.bat откладывается и возвращается после обновления; любой выход пишет build\\update.log и ждёт нажатия клавиши.",
            "Предметы 31-40 получили нарисованные иконки: чаша, осколок удара, ключ ритма, призма, фитиль, осколок оттепели, две печати, знак палача и руна свидетеля.\nВсе сорок иконок монохромные: один предмет - один цвет, форма и оттенок не повторяются у соседей по списку.\nАпгрейды теперь различаются цветом, а не только цифрой ранга: рамка слота и иконка читаются с одного взгляда.",
            "Пять иконок перерисованы с нуля: знак палача больше не похож на \"стоп\", фитиль не выглядит загогулиной, осколок оттепели стал гранёным, печать обратного тока читается как стрелка, призма не упирается в края.\nВсе новые иконки в одном стиле: крупные пиксели, толстый контур, блоки света без мягкого свечения.\nПодложка теперь берётся из самой картинки, а не из старой таблицы цветов: перо, луна, рунный разворот и звезда пепла больше не стоят на серой плите.\nЯркость выровнена по всему набору: самые тёмные иконки подняты, пересвеченные приглушены, чтобы ни одна не выпадала в списке.",
            "Все сорок иконок собраны в один лист design/icon_sheet_40.png: видно, как предметы выглядят рядом и как они различаются цветом.\nЛист собирается из тех же исходников, что запекаются в атлас, поэтому показывает игру, а не отдельный эскиз.",
            "Сердце титана перерисовано: зелёное стало багровым гравированным сердцем с двумя поясами и вшитыми камнями.\nВ тот же стиль приведены четыре иконки, которые читались как плоская заливка: магнит стал резным камнем с рунами, перо феникса - гравюрным белым пером, вуаль - золотым рваным капюшоном с пустотой вместо лица, разворот - плотной книгой с тиснением.\nФитиль больше не зелёный: это витой багровый шнур с тлеющим угольком и дымом.\nЯркость набора сузилась с 69-165 до 72-147: ни одна иконка не выбивается ни в тёмное, ни в пересвет.",
            "Апдейтер показывал русский текст крякозябрами, если его файл был сохранён в UTF-8: cmd.exe читает .bat в кодовой странице консоли.\n• Текст апдейтера теперь живёт в tools/update_bat_src.txt обычным UTF-8, а update.bat собирается из него в cp866: python3 tools/build_update_bat.py.\n• Сборщик проверяет, что каждая буква влезает в cp866, что кириллица стоит только в echo и rem, что каждый goto находит метку и что номер апдейтера совпадает с версией игры.\n• Если запущена всё-таки испорченная копия, апдейтер видит это по длине слова-маячка, пишет предупреждение по-английски и пробует вернуть файл из репозитория.\n• Ветка для сборки выбирается сама: main или ветка arena/* с самой свежей версией в src/version.h (руками: update.bat arena/...). Раньше апдейтер тянул ту ветку, где стоял, и честно отвечал «обновлять нечего», пока на GitHub лежала версия новее.\n• Скачанный руками update.bat больше не побеждает версию из ветки автоматически: сравниваются номера, и остаётся свежий.\n• Кодовая страница консоли возвращается к прежней после выхода.",
            "• Крысиная стая выросла вдвое: 16–24 крысы вместо 8–12, кучка стала плотной.\n• Описание дела больше не читается дважды: экран выбора уже озвучил его, и старт забега не повторяет ту же запись. Если запись ещё играет, цель дела читается после её конца.\n• Двойник: обмен местами больше не встаёт вплотную к герою (зеркало держит 190 px, у края арены — 150 px), поэтому телепорт перестал убивать касанием. Крест снайперский: один луч смотрит прямо в героя, у старой точки залп сдвинут на полшага.\n• Музыка дел: audio/ss_case_01.mp3 … ss_case_28.mp3 играет своё дело, нет файла — общий плейлист боя, нет и его — встроенная тема. Смена дела перезапускает трек.\n• Музыка стражей: audio/ss_boss_guardian00.mp3 … 05.mp3 закреплена за типом стража, общий ss_boss_theme остаётся запасным.\n• Эндлес играет весь боевой набор в перемешанном порядке и без повторов, пока не кончится список: в пул входят ss_gameplay_*, ss_case_NN и ss_gameplay_endless_*.\n• Нашлась причина, по которой «своя музыка дела» не работала: сцена искалась по префиксу ss_gameplay_run, и файлы ss_gameplay_caseNN.mp3 не читались вовсе. Теперь читаются оба имени.\n• music_prompts.md: та же схема имён, что читает игра, готовый список файлов и объяснение, как всё это выбирается.\n• Если апдейтер собрал ту же версию, что уже была, значит он работал на старой ветке: с 2.10.20 он сам выбирает ветку с самой свежей версией и печатает её номер до сборки.",
            "• Дебаг-меню было, но открывалось только по секретному коду v404, и со стороны выглядело так, будто отладки в игре нет.\n• Теперь F9 открывает его сразу в любом состоянии; клавиша ` — запасная, для ноутбуков, где F-ряд спрятан за Fn. В меню паузы появилась кнопка ОТЛАДКА [F9].\n• Пока меню открыто, бой замирает: раньше отладка шла поверх живого боя и можно было умереть, пока выбираешь пункт.\n• Строки выбираются стрелками и Enter или цифрами 1…9 и 0; состояние переключателей видно справа.\n• Новые пункты под недавние жалобы: СПАВН СТАИ КРЫС (проверить, что крыс вдвое больше), ПРИЗВАТЬ СТРАЖА (по очереди Моргар, Малакор, Кардинал, Азарот, Протоколист, Двойник — проверить крест), ВСЕ 40 ПРЕДМЕТОВ ПО 3 РАНГА (увидеть все иконки в сборке), ПОЛНЫЙ ЗАРЯД СУПЕРУМЕНИЯ, F3-оверлей.\n• Бессмертие, золото, все дела, все герои, Кузница, убить всех, +5 уровней и новая ночь остались на месте.",
            "• Обновления не было, потому что локальная ветка удалена на GitHub: апдейтер честно отвечал «Already up to date» для ветки, которой больше нет.\n• Теперь на старте он это видит и прямо пишет: ветки такой нет на GitHub.\n• На мёртвую ветку он больше не возвращается - остаётся на собранной, и следующий запуск идёт уже с неё. Раньше цикл повторялся каждый раз.\n• Скачанный руками update.bat попадал в stash и при возврате ложился поверх свежего из ветки: следующий запуск снова поднимал старый скрипт. Теперь после возврата из stash файлы сравниваются по номеру и остаётся более новый.\n• Кнопка Raw на GitHub отдаёт update.bat из main, а main - 2.10.4 с древним скриптом в UTF-8. Поэтому «перекачал новый» снова давало старый: свежий файл лежит только в рабочей ветке (см. UPDATE_HOWTO.md).\n• UPDATE_HOWTO.md: один разовый набор команд, чтобы перейти на рабочую ветку и получить свежий апдейтер.\n• Патчноуты листаются: длинная заметка прокручивается колесом мыши или PgUp/PgDn (стрелки вверх/вниз — по строке, Home/End — начало и конец), справа видна полоса прокрутки.\n• Кнопки и галочка в патчноутах больше не вылезают за панель в маленьком окне: раскладка считается от размеров панели, а не от 1280x720.",
            "• Длинный патчноут уезжал за нижнюю кромку панели, и дочитать его было нельзя. Теперь заметка листается: колесо мыши над текстом, PgUp / PgDn — страницами, стрелки вверх / вниз — по строке, Home / End — начало и конец.\n• Справа в заметке появилась полоса прокрутки, а подсказка снизу меняется: «прокрутка заметки», если текст длиннее окна, и «заметка целиком», если нет.\n• При смене версии прокрутка сбрасывается: новая заметка открывается с начала.\n• Раньше кнопки «ПРЕДЫДУЩАЯ / СЛЕДУЮЩАЯ / В ИГРУ», галочка и подсказка стояли на жёстких координатах для 1280x720 и в маленьком окне вылезали за панель. Теперь вся раскладка считается от размеров панели и проверена на 1920x1080, 1280x720, 1024x600, 900x520 и портретных 720x1280.\n• Обрезка текста живёт в рендерере (setClip / clearClip), поэтому такие же длинные заметки в словаре и других панелях можно листать тем же способом.",        };
        static_assert(sizeof(versions) / sizeof(versions[0]) == PATCH_NOTES_COUNT,
                      "хроника: versions[] должен совпадать с PATCH_NOTES_COUNT");
        static_assert(sizeof(notes) / sizeof(notes[0]) == PATCH_NOTES_COUNT,
                      "хроника: у каждой версии должна быть заметка");
        // Вся геометрия ниже считается от панели, а не от чисел «для 1280x720»:
        // раньше кнопки стояли на y=555/617, и в маленьком окне они вылезали
        // за пределы панели вместе с длинной заметкой.
        const int versionY = panelY + (portraitPatch ? 64 : 80);
        const int closeH = 46;
        const int closeW = portraitPatch ? 300 : (compactPatch ? 260 : 275);
        const int closeY = panelY + panelH - closeH - 12;
        const int cby = closeY - FONT_SMALL.lineHeight - 12;
        const int btnH = 42;
        const int btnY = cby - btnH - 12;
        const int pageHintY = btnY - FONT_SMALL.lineHeight - 6;
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, versionY, versions[patchNotesPage], 0xFFFFC75A);
        const int notesX = compactPatch ? panelX + 18 : panelX + 60;
        const int notesY = versionY + FONT_MED.lineHeight + 16;
        const int notesW = compactPatch ? panelW - 36 : panelW - 120;
        // Заметка занимает всё место между версией и подсказкой; всё, что не
        // влезло, листается колесом или PgUp/PgDn.
        const int notesH = std::max(96, pageHintY - notesY - 12);
        r.fillRoundedRect(notesX, notesY, notesW, notesH, 10, 0xFF120D22, 240);
        r.drawRoundedRect(notesX, notesY, notesW, notesH, 10, 0xFF665187, 2);
        const int notesTextX = notesX + (compactPatch ? 18 : 40);
        const int notesTextW = notesW - (compactPatch ? 36 : 80);
        const int notesTextY = notesY + 28;
        const int notesInnerTop = notesY + 8;
        const int notesInnerH = notesH - 16;
        // Прокрутка: длинная заметка не режется по кромке, а листается.
        const int notesContentH = r.measureTextWrapped(FONT_MED, notesTextW, notes[patchNotesPage]);
        patchNotesScrollMax = std::max(0, notesContentH + 20 - notesInnerH);
        if (mouseWheelDelta != 0 && mouseX >= notesX && mouseX <= notesX + notesW &&
            mouseY >= notesY && mouseY <= notesY + notesH) {
            const int notches = std::max(1, std::abs(mouseWheelDelta) / 120);
            patchNotesScroll += (mouseWheelDelta < 0 ? 1.0f : -1.0f) * 48.0f * (float)notches;
        }
        patchNotesScroll = std::max(0.0f, std::min((float)patchNotesScrollMax, patchNotesScroll));
        const bool pageScrolls = patchNotesScrollMax > 0;
        if (pageScrolls) {
            // Текст режется ровно по внутреннему прямоугольнику панели.
            r.setClip(notesX + 2, notesInnerTop, notesW - 4, notesInnerH);
        }
        r.drawTextWrapped(FONT_MED, notesTextX, notesTextY - (int)patchNotesScroll, notesTextW,
                          notes[patchNotesPage], 0xFFE5DDF4, false);
        if (pageScrolls) {
            r.clearClip();
            const int barX = notesX + notesW - 9, barTop = notesInnerTop, barH = notesInnerH;
            r.fillRoundedRect(barX, barTop, 4, barH, 2, 0xFF241C38);
            const int thumbH = std::max(22, barH * notesInnerH / std::max(1, notesContentH));
            const float t = (float)patchNotesScroll / (float)std::max(1, patchNotesScrollMax);
            const int thumbY = barTop + (int)((barH - thumbH) * t);
            r.fillRoundedRect(barX, thumbY, 4, thumbH, 2, 0xFF9E8FD0);
        }
        // Подсказка не должна быть шире панели: в узком окне она обрезалась краем.
        const char* pageHint = compactPatch
            ? (pageScrolls ? "КОЛЕСО / PGDN — ПРОКРУТКА  •  <-/-> — ЗАМЕТКА" : "ЗАМЕТКА ЦЕЛИКОМ  •  <-/-> — ДРУГАЯ")
            : (pageScrolls ? "КОЛЕСО МЫШИ ИЛИ PGDN / PGUP — ПРОКРУТКА ЗАМЕТКИ  •  ЛИСТАЙ [<- / ->] ИЛИ КНОПКАМИ"
                           : "ЗАМЕТКА ЦЕЛИКОМ НА ЭКРАНЕ  •  ЛИСТАЙ [<- / ->] ИЛИ КНОПКАМИ");
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, pageHintY, pageHint, pageScrolls ? 0xFFFFD878 : 0xFFB9ADCC);
        const int btnW = compactPatch ? (panelW - 60) / 2 : 280;
        const int prevX = compactPatch ? panelX + 20 : panelX + 155;
        const int nextX = prevX + btnW + (compactPatch ? 16 : 150);
        r.fillRoundedRect(prevX, btnY, btnW, btnH, 8, 0xFF30234B);
        r.drawRoundedRect(prevX, btnY, btnW, btnH, 8, 0xFF7F69A8, 1);
        r.drawTextCentered(FONT_SMALL, prevX + btnW/2, btnY + 12, "◄ ПРЕДЫДУЩАЯ", 0xFFFFFFFF);
        r.fillRoundedRect(nextX, btnY, btnW, btnH, 8, 0xFF30234B);
        r.drawRoundedRect(nextX, btnY, btnW, btnH, 8, 0xFF7F69A8, 1);
        r.drawTextCentered(FONT_SMALL, nextX + btnW/2, btnY + 12, "СЛЕДУЮЩАЯ ►", 0xFFFFFFFF);
        const int cbx = compactPatch ? panelX + 20 : panelX + 118;
        const uint32_t checkboxFill = patchNotesDoNotShowAgain ? 0xFF72E5A0 : 0xFF211A31;
        const uint32_t checkboxBorder = patchNotesDoNotShowAgain ? 0xFFC7FFD8 : 0xFFB6A5D5;
        r.fillRoundedRect(cbx, cby, 24, 24, 4, checkboxFill);
        r.drawRoundedRect(cbx, cby, 24, 24, 4, checkboxBorder, 2);
        if (patchNotesDoNotShowAgain) {
            // Draw the check as vector strokes: the bitmap font has no reliable ✓ glyph.
            r.drawLine(cbx + 5, cby + 12, cbx + 10, cby + 18, 0xFF182635, 4, 255, false);
            r.drawLine(cbx + 10, cby + 18, cbx + 20, cby + 5, 0xFF182635, 4, 255, false);
        }
        r.drawText(FONT_SMALL, cbx + 36, cby + 4, "Больше не показывать патчноуты этой версии", 0xFFE1D8F0);
        const int closeX = compactPatch ? SCREEN_W - closeW - panelX - 20 : panelX + 605;
        bool closeHover = mouseX >= closeX && mouseX <= closeX + closeW && mouseY >= closeY && mouseY <= closeY + closeH;
        r.fillRoundedRect(closeX, closeY, closeW, closeH, 8, closeHover ? 0xFF684A9C : 0xFF3B2B5A);
        r.drawRoundedRect(closeX, closeY, closeW, closeH, 8, 0xFFFFD858, 2);
        r.drawTextCentered(FONT_MED, closeX + closeW/2, closeY + 12, "В ИГРУ [ENTER]", 0xFFFFFFFF);
        if (mouseClicked) {
            if (mouseX >= cbx && mouseX <= cbx + 410 && mouseY >= cby - 2 && mouseY <= cby + 28) patchNotesDoNotShowAgain = !patchNotesDoNotShowAgain;
            else if (mouseX >= prevX && mouseX <= prevX + btnW && mouseY >= btnY && mouseY <= btnY + btnH) patchNotesGoPage(-1);
            else if (mouseX >= nextX && mouseX <= nextX + btnW && mouseY >= btnY && mouseY <= btnY + btnH) patchNotesGoPage(1);
            else if (mouseX >= closeX && mouseX <= closeX + closeW && mouseY >= closeY && mouseY <= closeY + closeH) closePatchNotes();
        }
    }

    static int cutsceneSceneOf(int id) {
        if (id == 0) return 0;        // prologue, act I
        if (id == 5) return 5;        // ending
        if (id >= 10 && id <= 13) return id - 9; // acts II..V
        return 0;
    }
    static int actOpeningCutsceneId(int act) { return act <= 1 ? 0 : 9 + act; }
    float cutsceneDuration(int id) const {
        const int scene = cutsceneSceneOf(id);
        return scene == 5 ? 7.5f : 6.0f;
    }

    void beginCutscene(int id, GameState returnState) {
        cutsceneId = id; cutsceneTimer = 0.0f; cutsceneReturnState = returnState; state = STATE_CUTSCENE;
        // Every act opens with one recorded line, so the act change is heard and not only seen.
        const int scene = cutsceneSceneOf(id);
        if (scene >= 0 && scene <= 4) {
            static const GameMsgId actIds[CAMPAIGN_ACT_COUNT] = {
                GMSG_STORY_ACT1, GMSG_STORY_ACT2, GMSG_STORY_ACT3, GMSG_STORY_ACT4, GMSG_STORY_ACT5
            };
            speakOnly(actIds[scene], CAMPAIGN_ACT_TITLES[scene]);
        }
    }

    void finishCutscene() {
        const int scene = cutsceneSceneOf(cutsceneId);
        if (scene == 0) save.seenIntroCutscene = STORY_INTRO_VERSION;
        else if (scene >= 1 && scene <= 4) save.seenActIntros |= (1u << scene);
        if (cutsceneId == 5) { save.seenEndingCutscene = 1; save.storyChapter = CAMPAIGN_ACT_COUNT; }
        writeSave();
        cutsceneTimer = 0.0f;
        if (cutsceneId == 5 && gameMode == MODE_STORY) {
            recordRunEnd();
            sound.musicMode = 4;
            state = STATE_CAMPAIGN_END;
        } else state = cutsceneReturnState;
    }

    // Safe, non-recording preview scene so players can audition boss/horde music without
    // progressing a run or waiting for a scheduled boss. F12 opens it from the main menu.
    void startBossMusicTestArena() {
        const int previousMode = gameMode;
        gameMode = MODE_ENDLESS;
        initRun(selectedHero);
        gameMode = previousMode;
        monsters.clear(); projectiles.clear(); pickups.clear();
        particles.clear(); floatTexts.clear(); lightningArcs.clear(); frostWaves.clear(); shockwaves.clear(); bossHazards.clear();
        playerX = playerY = 0.0f;
        playerHp = playerMaxHp;
        spawnMonster(0, false, true, 0);
        bool bossPlaced = false;
        for (Monster& monster : monsters) {
            if (!monster.isBoss) continue;
            monster.x = playerX + 260.0f; monster.y = playerY;
            monster.vx = monster.vy = 0.0f; monster.speed = 0.0f;
            monster.abilityTimer = 9999.0f; monster.shootTimer = 9999.0f;
            monster.hp = monster.maxHp;
            bossPlaced = true;
            break;
        }
        if (!bossPlaced) {
            state = STATE_MENU;
            sound.musicMode.store(5, std::memory_order_relaxed);
            return;
        }
        bossTestMusicMode = 2;
        state = STATE_BOSS_MUSIC_TEST;
        sound.musicBossType.store(currentGameplayBossType(), std::memory_order_relaxed);
        sound.musicMode.store(2, std::memory_order_relaxed);
        sound.requestImmediateMusicChange(2, 0);
    }

    void startSelectedRun() {
        if (gameMode == MODE_STORY && !isCampaignMissionUnlocked(selectedStoryMission)) {
            openStoryMissionSelect();
            return;
        }
        initRun(selectedHero);
        if (gameMode != MODE_STORY) { state = STATE_PLAYING; return; }
        const int act = campaignActOf(selectedStoryMission);
        if (save.seenIntroCutscene < STORY_INTRO_VERSION) { beginCutscene(0, STATE_PLAYING); return; }
        if (act >= 2 && (save.seenActIntros & (1u << (act - 1))) == 0) {
            beginCutscene(actOpeningCutsceneId(act), STATE_PLAYING);
            return;
        }
        speakMissionFlavour(selectedStoryMission);
        state = STATE_PLAYING;
    }

    // The story backgrounds are assembled from code-drawn pixel-game primitives and the
    // game's own baked character sprites. No generated painting or external image is required.
    void drawStoryIllustration(Renderer& r, int scene) {
        static const uint32_t skyTop[6] = {0xFF26152C,0xFF101D20,0xFF102A3A,0xFF32180F,0xFF201328,0xFF342B35};
        static const uint32_t skyBottom[6] = {0xFF120E20,0xFF10151B,0xFF10212D,0xFF1A1018,0xFF100E1C,0xFF8A6040};
        const uint32_t accent = scene == 1 ? 0xFF85D65C : scene == 2 ? 0xFF5BD8EE : scene == 3 ? 0xFFFF8C43 : scene == 5 ? 0xFFFFD78A : 0xFFFFC96A;
        const uint32_t masonry = scene == 2 ? 0xFF162B3A : scene == 1 ? 0xFF1B2825 : scene == 3 ? 0xFF2B1A19 : 0xFF211B2A;
        r.fillGradientV(0, 0, SCREEN_W, SCREEN_H, skyTop[scene], skyBottom[scene]);

        // Far sky: broken moon / dawn disk, layered cloud bands, ash and distant ridgelines.
        const int moonX = scene == 2 ? 950 : (scene == 5 ? 1000 : 270);
        const int moonY = scene == 5 ? 180 : 160;
        const uint32_t moonColor = scene == 2 ? 0xFFB54361 : (scene == 5 ? 0xFFFFD68A : 0xFF9F4754);
        r.addGlow(moonX, moonY, scene == 5 ? 160 : 122, moonColor, 88);
        r.fillCircle(moonX, moonY, scene == 5 ? 48 : 38, moonColor, 228);
        r.fillCircle(moonX + 13, moonY - 8, 31, skyTop[scene], 185);
        r.drawCircleRing(moonX, moonY, 61 + (int)(sinf(globalTime * 0.7f) * 3.0f), 2, accent, 110, true);
        for (int row = 0; row < 4; ++row) {
            const int y = 112 + row * 37 + (int)(sinf(globalTime * 0.12f + row) * 3.0f);
            r.fillRect((row * 173 + 35) % 360 - 160, y, 410, 12 + row * 2, masonry, 44 + row * 7);
            r.fillRect(790 + (row * 119) % 210, y + 17, 360, 9 + row, masonry, 35 + row * 5);
        }
        for (int i = 0; i < 54; ++i) {
            const int x = (i * 113 + (int)(globalTime * (5 + i % 6))) % SCREEN_W;
            const int y = (i * 71 + (int)(globalTime * (2 + i % 4))) % 460;
            const uint32_t col = (i % 5 == 0) ? accent : (scene == 2 ? 0xFF8ED8E8 : 0xFFC4B6C8);
            r.fillRect(x, y, (i % 9 == 0) ? 3 : 1, (i % 9 == 0) ? 3 : 2, col, 55 + (i % 4) * 30);
        }

        // A stepped, layered city wall provides a consistent horizon behind each distinct landmark.
        for (int i = 0; i < 11; ++i) {
            int x = i * 133 - 36 + (int)(sinf(globalTime * 0.08f + i) * 5.0f);
            int h = 78 + (i * 47) % 112;
            const int top = 405 - h;
            r.fillRect(x, top, 103, h + 98, masonry, 220);
            r.fillRect(x + 11, top + 14, 81, 7, 0xFF080A13, 180);
            r.drawLine(x + 2, top + 19, x + 51, top - 15, 0xFF090A12, 5, 190);
            r.drawLine(x + 51, top - 15, x + 100, top + 19, 0xFF090A12, 5, 190);
            for (int wy = top + 31; wy < 438; wy += 36)
                for (int wx = x + 16; wx < x + 90; wx += 29)
                    r.fillRect(wx, wy, 8, 15, (i + wx + wy) % 3 == 0 ? accent : 0xFF66536F, 75 + (i % 3) * 22);
        }

        // Central landmark: a many-buttressed gothic archive with a rose window, nave and bell tower.
        r.fillRect(357, 276, 566, 282, masonry, 245);
        r.fillRect(390, 243, 500, 44, 0xFF0D0B14, 242);
        r.fillRect(420, 198, 48, 360, 0xFF17131F, 255);
        r.fillRect(812, 198, 48, 360, 0xFF17131F, 255);
        r.fillRect(410, 205, 68, 15, 0xFF302334, 255);
        r.fillRect(802, 205, 68, 15, 0xFF302334, 255);
        r.drawLine(420, 204, 444, 158, 0xFF0A0911, 7, 245);
        r.drawLine(444, 158, 468, 204, 0xFF0A0911, 7, 245);
        r.drawLine(812, 204, 836, 158, 0xFF0A0911, 7, 245);
        r.drawLine(836, 158, 860, 204, 0xFF0A0911, 7, 245);
        r.fillRect(440, 291, 400, 10, 0xFF080911, 250);
        for (int bay = 0; bay < 7; ++bay) {
            const int wx = 394 + bay * 72;
            r.fillRect(wx, 328, 33, 109, 0xFF090B15, 245);
            r.drawLine(wx - 2, 331, wx + 16, 307, 0xFF090B15, 4, 245);
            r.drawLine(wx + 16, 307, wx + 35, 331, 0xFF090B15, 4, 245);
            r.fillRect(wx + 5, 347, 5, 67, (bay % 2) ? accent : 0xFF7767A1, 105);
            r.fillRect(wx + 21, 347, 5, 67, (bay % 2) ? 0xFF7767A1 : accent, 95);
        }
        r.addGlow(640, 326, 74, accent, 66);
        r.drawCircleRing(640, 326, 53 + (int)(sinf(globalTime * 1.3f) * 2.0f), 5, accent, 205, true);
        r.drawCircleRing(640, 326, 34, 2, 0xFFFFE7A7, 205, true);
        for (int ray = 0; ray < 12; ++ray) {
            const float a = ray * 0.523599f + globalTime * 0.08f;
            r.drawLine(640 + (int)(cosf(a) * 16), 326 + (int)(sinf(a) * 16),
                       640 + (int)(cosf(a) * 48), 326 + (int)(sinf(a) * 48), accent, 2, 190, true);
        }
        // Broad pointed nave, battlements, stone courses and lit slit windows.
        r.drawLine(357, 278, 640, 214, 0xFF090A12, 8, 250);
        r.drawLine(640, 214, 923, 278, 0xFF090A12, 8, 250);
        for (int y = 466; y < 558; y += 17) r.fillRect(363, y, 554, 3, 0xFF080910, 150);
        for (int x = 372; x < 915; x += 37) {
            r.fillRect(x, 268, 19, 12, 0xFF17131F, 255);
            r.fillRect(x + 4, 257, 11, 13, 0xFF17131F, 255);
        }

        // Scene-specific foreground motifs make the locations visually distinct.
        if (scene == 1) { // green crypt: roots, sarcophagi and cold flame
            for (int i = 0; i < 7; ++i) {
                int x = 90 + i * 175;
                r.fillRect(x, 390, 24, 165, 0xFF0D1818, 255);
                r.drawLine(x + 12, 392, x - 7, 346, 0xFF26352A, 9, 245);
                r.drawLine(x + 12, 408, x + 41, 363, 0xFF243B2B, 7, 230);
                r.fillRect(x - 28, 518, 83, 28, 0xFF303936, 235);
                r.fillRect(x - 18, 511, 62, 10, 0xFF566256, 195);
                r.addGlow(x + 11, 478, 28, 0xFF67E777, 45);
                r.fillCircle(x + 11, 472, 4, 0xFFA7FF96, 205);
            }
        } else if (scene == 2) { // drowned cathedral: layered water, reeds and broken arches
            for (int layer = 0; layer < 6; ++layer) {
                const int y = 440 + layer * 24;
                for (int wave = 0; wave < 7; ++wave) {
                    const int x = (wave * 211 + layer * 59 + (int)(globalTime * (7 + layer))) % (SCREEN_W + 90) - 45;
                    r.drawLine(x, y + (wave % 2) * 5, x + 82 + layer * 7, y + (wave % 2) * 5,
                               layer % 2 ? 0xFF327F9A : 0xFF56B8CB, 2, 85 + layer * 12, true);
                }
            }
            for (int i = 0; i < 16; ++i) {
                const int x = 40 + i * 81;
                r.drawLine(x, 554, x - 10, 500 - (i % 4) * 12, 0xFF466953, 3, 155);
                r.drawLine(x - 10, 500 - (i % 4) * 12, x - 27, 482 - (i % 4) * 12, 0xFF466953, 2, 145);
            }
        } else if (scene == 3) { // ash throne: furnace, iron chains and a broken dais
            r.fillRect(475, 445, 330, 27, 0xFF130D12, 255);
            r.fillRect(510, 472, 260, 43, 0xFF221216, 255);
            r.fillRect(550, 511, 180, 41, 0xFF311817, 255);
            r.addGlow(640, 469, 120, 0xFFFF6B35, 112);
            for (int i = 0; i < 9; ++i) {
                const int x = 510 + i * 33;
                r.drawLine(x, 232, x + 5, 350 + (i % 3) * 13, 0xFF9E6246, 2, 150);
                r.drawCircleRing(x + 4, 359 + (i % 3) * 13, 5, 1, 0xFFE89A5A, 150, true);
            }
            for (int i = 0; i < 8; ++i) r.fillRect(80 + i * 150, 520, 44, 3, 0xFFFF8B43, 110);
        } else if (scene == 4) { // revealed archive: a vast clockwork, opened seals and memory threads
            r.addGlow(640, 375, 215, 0xFFBE7BFF, 72);
            for (int ring = 0; ring < 4; ++ring) {
                const int rad = 78 + ring * 35;
                r.drawCircleRing(640, 371, rad, ring % 2 ? 2 : 4, ring % 2 ? 0xFFFFD77A : 0xFFB87AFF, 110 + ring * 24, true);
            }
            for (int tooth = 0; tooth < 24; ++tooth) {
                const float a = tooth * 0.261799f + globalTime * 0.05f;
                const int x1 = 640 + (int)(cosf(a) * 162), y1 = 371 + (int)(sinf(a) * 162);
                const int x2 = 640 + (int)(cosf(a) * 185), y2 = 371 + (int)(sinf(a) * 185);
                r.drawLine(x1, y1, x2, y2, 0xFFFFD77A, 4, 160, true);
            }
            for (int thread = 0; thread < 7; ++thread) {
                int x = 330 + thread * 104;
                r.drawLine(x, 238, 640 + (thread - 3) * 35, 440, 0xFFD9A7FF, 2, 100, true);
            }
        } else if (scene == 5) { // dawn: the road leads out of the city beneath a broken bell
            r.fillGradientV(0, 510, SCREEN_W, 210, 0xFF5D4236, 0xFF16121C, 245);
            for (int i = 0; i < 10; ++i) {
                int y = 535 + i * 19;
                r.drawLine(270 - i * 20, y, 1010 + i * 20, y, 0xFFAC7950, 2, 105);
            }
            r.addGlow(1005, 202, 185, 0xFFFFCE75, 116);
            r.drawCircleRing(1005, 202, 77, 3, 0xFFFFE6A5, 180, true);
            r.drawLine(600, 161, 640, 206, 0xFFFFD889, 5, 190, true);
            r.drawLine(640, 206, 687, 160, 0xFFFFD889, 5, 190, true);
        }

        // Foreground parapet, broken flagstones and a slow, parallax ash fall.
        r.fillRect(0, 555, SCREEN_W, 165, 0xFF080910, 218);
        for (int i = 0; i < 21; ++i) {
            int x = i * 67 - 6;
            r.fillRect(x, 560 + (i % 3) * 7, 61, 5, (i % 2) ? 0xFF46374D : 0xFF584153, 165);
            r.drawLine(x + 8, 581 + (i % 3) * 7, x + 1, 611 + (i % 4) * 9, 0xFF0D0C14, 3, 160);
        }

        const int frame = ((int)(globalTime * 3.0f)) & 1;
        if (scene >= 1 && scene <= 3) {
            const uint32_t bossGlow = scene == 1 ? 0xFF91E84B : (scene == 2 ? 0xFFFF4269 : 0xFFFF8A35);
            r.addGlow(915, 418, 117, bossGlow, 95);
            r.drawBakedSpriteScaled(BOSS_SPRITES[scene - 1][frame], 812, 312, 188, 188, 248);
            r.drawBakedSpriteScaled(HERO_SPRITES[selectedHero % 5][frame], 351, 405, 88, 88, 255);
        } else if (scene == 0 || scene == 4) {
            r.drawBakedSpriteScaled(HERO_SPRITES[selectedHero % 5][frame], 352, 418, 82, 82, 245);
            if (scene == 4) {
                r.addGlow(902, 401, 114, 0xFFBD7AFF, 98);
                r.drawBakedSpriteScaled(BOSS_SPRITES[3][frame], 824, 326, 150, 150, 215);
            }
        } else {
            r.drawBakedSpriteScaled(HERO_SPRITES[selectedHero % 5][frame], 360, 420, 82, 82, 245);
        }
    }

    void renderCutscene(Renderer& r) {
        // One opening scene per act plus the ending. Index 0 is the prologue in Act I,
        // 1..4 are the openings of Acts II..V, 5 is the final scene.
        static const char* titles[6] = {
            "АКТ I • ПЕПЕЛ НА СТОЛЕ",
            "АКТ II • ГОЛОС В КОЛБЕ",
            "АКТ III • РЕЕСТР",
            "АКТ IV • АРХИВ",
            "АКТ V • СЕРДЦЕ АРХИВА",
            "ФИНАЛ • ПРАВО ПОМНИТЬ"
        };
        static const char* beats[6][3] = {
            {
                "На двадцать первый удар пепельный шторм накрыл город. Рассвет не наступил, а дела остались.",
                "Архив Рассвета записывает последние минуты погибших. Он хранит копии — не возвращает людей.",
                "Я — Свидетель, чьего полного имени нет в реестре. Первое дело: участок у ворот и дозор, который ещё двигается."
            },
            {
                "Крипта под колокольней. В колбах — голоса, снятые с мёртвых до последнего вздоха.",
                "Малакор слушает сестру каждую ночь. Копия отвечает правильно и не узнаёт брата.",
                "Речь не о том, чтобы вернуть голос. Речь о том, чтобы перестать выдавать запись за человека."
            },
            {
                "Затопленный реестр. Имена смываются быстрее, чем гаснут свечи.",
                "Кардинал вычёркивал людей из книг, называя забвение милосердием. Имена вернулись на камень.",
                "Люди помнят друг друга без разрешения Архива — и это единственная запись, которую нельзя стереть."
            },
            {
                "Кабинет Архивариуса: пепельный трон и копия Свидетеля, которую я не заказывал.",
                "Копия отвечает на вопросы лучше меня. Значит, вопросы задавал не я.",
                "Память против права забыть. Выбор останется за мной — и за тем, кто будет читать протокол."
            },
            {
                "Сердце Архива — резервная копия приказа, который никто не отменил.",
                "Три эхо читают его хором. Пока приказ звучит, ночь будет начинаться заново.",
                "Последний бой ничего не воскресит. Он вернёт живым право помнить самим."
            },
            {
                "Приказ отменён. Архив больше не повторяет последние секунды умерших.",
                "Имена вернулись в общую книгу. Погибшие не вернулись — живые могут помнить их без разрешения машины.",
                "«Мы не вернули их. Мы вернули себе право помнить»."
            }
        };
        const int scene = cutsceneSceneOf(cutsceneId);
        const float beatSeconds = cutsceneDuration(cutsceneId) / 3.0f;
        const int beat = std::min(2, (int)(cutsceneTimer / beatSeconds));
        r.clear(0xFF07060D);
        drawStoryIllustration(r, scene);
        r.fillRect(0, 0, SCREEN_W, 24, 0xFF05040A, 215);
        r.fillRect(0, SCREEN_H - 24, SCREEN_W, 24, 0xFF05040A, 215);
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, 42, titles[scene], 0xFFFFD878);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 78, beat == 0 ? "ПАМЯТЬ • СЛЕД • ВЫБОР" : (beat == 1 ? "ПРАВДА ОТКРЫВАЕТСЯ" : "ТВОЯ ЦЕЛЬ"), 0xFFD8D0E8);
        for (int i = 0; i < 3; ++i) {
            const int x = SCREEN_W / 2 - 42 + i * 42;
            r.fillRoundedRect(x, 103, 32, 5, 2, i <= beat ? 0xFFFFD878 : 0xFF55485F);
        }
        const char* line = (scene == 0 && beat == 1) ? heroWitnessLine(selectedHero) : beats[scene][beat];
        const bool compactCutscene = SCREEN_W < 1200, portraitCutscene = SCREEN_W < SCREEN_H;
        const int scenePanelW = compactCutscene ? SCREEN_W - 40 : 1136;
        const int scenePanelH = portraitCutscene ? 180 : 132;
        const int scenePanelX = (SCREEN_W - scenePanelW) / 2;
        const int scenePanelY = portraitCutscene ? SCREEN_H - 300 : 536;
        r.fillGradientV(scenePanelX, scenePanelY, scenePanelW, scenePanelH, 0xF5090710, 0xFC090710, 255);
        r.drawRoundedRect(scenePanelX, scenePanelY, scenePanelW, scenePanelH, 12,
                          scene == 5 ? 0xFFFFD78A : CAMPAIGN_ACT_COLORS[std::min(CAMPAIGN_ACT_COUNT - 1, scene)], 2, 245);
        r.drawTextWrapped(FONT_MED, scenePanelX + 28, scenePanelY + 22, scenePanelW - 56, line, 0xFFF1ECF8, false);
        r.drawTextRight(FONT_SMALL, SCREEN_W - 28, portraitCutscene ? SCREEN_H - 58 : 686, "[ENTER / ESC] ПРОПУСТИТЬ", 0xFFD0C7DE);
        if (mouseClicked) finishCutscene();
    }

    void renderHallPortrait(Renderer& r) {
        renderSubmenuFrame(r, hallPage == 0 ? "ЗАЛ СЛАВЫ" : "РЕЕСТР СВИДЕТЕЛЯ",
            hallPage == 0 ? "Хроника забегов • личные рекорды" : "Улики и печати сохраняются между попытками");
        const int pad = 20, contentW = SCREEN_W - 2 * pad;
        if (hallPage == 1) {
            char progress[128];
            snprintf(progress, sizeof(progress), "СЛОМАНО ПЕЧАТЕЙ: %d / 4  •  ЗАПИСИ: %d / 4", storySealCount(), storyEntryCount());
            r.fillRoundedRect(pad, 96, contentW, 36, 7, 0xFF34244A);
            r.drawRoundedRect(pad, 96, contentW, 36, 7, 0xFFFFC850, 1);
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 106, progress, 0xFFFFE090);
            r.drawTextWrapped(FONT_SMALL, pad + 4, 140, contentW - 8, heroWitnessLine(selectedHero), 0xFFB9E8FF, false);
            static const char* sealNames[4] = {"I • ДОЛГ / МОРГАР", "II • ГОЛОС / МАЛАКОР", "III • ИМЯ / КАРДИНАЛ", "IV • СВИДЕТЕЛЬСТВО / АЗАРОТ"};
            static const char* locked[4] = {
                "Запись ещё не найдена. Встреть Моргара у Ворот.", "Запись ещё не найдена. Услышь, что хранит Малакор.",
                "Запись ещё не найдена. Найди подводный реестр Кардинала.", "Запись ещё не найдена. Дойди до Пепельного трона и встреть Азарота."
            };
            static const char* clues[4] = {
                "Баррикада сложена со стороны города. Капитан удерживает ворота и снова повторяет приказ той ночи.",
                "Малакор собирает последние слова сестры. На цилиндре одна фраза заедает и повторяется без ответа.",
                "Имена вымараны в книге, но написаны заново на камнях и семейных лентах.",
                "Азарот — первый Архивариус. Центральный механизм записывал память; клетку для повтора добавили позже."
            };
            static const char* resolved[4] = {
                "Моргар закрыл ворота, чтобы выиграть время для эвакуации. Архив сохранил приказ «держать», но потерял людей, ради которых он был отдан.",
                "Запись сестры точно повторяет её последние слова, но не узнаёт брата. Копия памяти — не сама любимая.",
                "Кардинал стирал имена из страха перед новыми эхо. Люди продолжали помнить друг друга без официального разрешения.",
                "Азарот хотел не дать ни одному имени исчезнуть. Архив хранит копии умерших, забирая память у живых и повторяя ночь."
            };
            const int cardY = 180, cardH = 166, gap = 10;
            for (int i = 0; i < 4; ++i) {
                const int y = cardY + i * (cardH + gap);
                const bool met = (save.storyEntryMask & (1u << i)) != 0;
                const bool sealed = (save.storySealMask & (1u << i)) != 0;
                const uint32_t edge = sealed ? 0xFF72DDA0 : (met ? 0xFFB889FF : 0xFF51475F);
                r.fillRect(pad, y, contentW, cardH, met ? 0xFF241B38 : 0xFF17131F, 245);
                r.drawRect(pad, y, contentW, cardH, edge, sealed ? 2 : 1);
                r.drawText(FONT_SMALL, pad + 12, y + 9, sealNames[i], sealed ? 0xFF9FF0B7 : (met ? 0xFFFFD878 : 0xFF8B8495));
                const char* body = !met ? locked[i] : (sealed ? resolved[i] : clues[i]);
                r.drawTextWrapped(FONT_SMALL, pad + 12, y + 34, contentW - 24, body, !met ? 0xFF827A8B : 0xFFE1D9EC, false);
                const char* status = sealed ? "ПЕЧАТЬ СЛОМАНА • ЗАПИСЬ В КУЗНИЦЕ" : (met ? "УЛИКА СОХРАНЕНА • ПЕЧАТЬ ЕЩЁ ДЕРЖИТСЯ" : "ЗАПИСЬ ЗАКРЫТА");
                r.drawText(FONT_SMALL, pad + 12, y + cardH - 20, status, sealed ? 0xFF78E5A0 : (met ? 0xFFBDA8D7 : 0xFF706A79));
            }
            const int truthY = cardY + 4 * (cardH + gap) + 2;
            r.fillRect(pad, truthY, contentW, 118, 0xFF171321, 245);
            r.drawRect(pad, truthY, contentW, 118, archiveTruthKnown() ? 0xFFB987E8 : 0xFF51475F, 1);
            const char* truth = archiveTruthKnown()
                ? "ПРАВДА АРХИВА: он хранит копии последних мгновений, не души. После смерти ночь пересобирается; Кузница удерживает память Свидетеля вне колокольного контура."
                : "ЗАКРЫТАЯ ЗАПИСЬ: собери четыре печати — и Архив раскроет, что именно переживает каждый новый цикл.";
            r.drawTextWrapped(FONT_SMALL, pad + 12, truthY + 12, contentW - 24, truth, 0xFFE3D8F2, false);
        } else {
            const char* rank = save.storyChapter >= 4 ? "СВИДЕТЕЛЬ РАССВЕТА" : save.bossesDefeated >= 3 ? "ХРАНИТЕЛЬ ПЕЧАТЕЙ" :
                               save.bossesDefeated >= 1 ? "ОХОТНИК НА БОССОВ" : save.bestKills >= 100 ? "ВЫЖИВШИЙ" : "НОВИЧОК";
            const int rankW = std::min(500, contentW);
            r.fillRoundedRect((SCREEN_W - rankW) / 2, 101, rankW, 36, 8, 0xFF34244A);
            r.drawRoundedRect((SCREEN_W - rankW) / 2, 101, rankW, 36, 8, 0xFFFFC850, 2);
            r.drawTextCentered(FONT_MED, SCREEN_W / 2, 108, rank, 0xFFFFE090);
            int mins = save.bestTimeSec / 60, seconds = save.bestTimeSec % 60;
            char values[4][64];
            snprintf(values[0], sizeof(values[0]), "%02d:%02d", mins, seconds);
            snprintf(values[1], sizeof(values[1]), "%d", save.bestKills);
            snprintf(values[2], sizeof(values[2]), "%d", save.bossesDefeated);
            snprintf(values[3], sizeof(values[3]), "%d", save.totalRuns);
            static const char* titles[4] = {"РЕКОРД ВЫЖИВАНИЯ", "РЕКОРД УБИЙСТВ", "ПОВЕРЖЕНО БОССОВ", "ЗАВЕРШЕНО ЗАБЕГОВ"};
            static const uint32_t colors[4] = {0xFFFFD858, 0xFFFF7888, 0xFFFF9C48, 0xFF70D8FF};
            const int cardGap = 12, cardW = (contentW - cardGap) / 2, cardH = 116;
            for (int i = 0; i < 4; ++i) {
                const int x = pad + (i % 2) * (cardW + cardGap), y = 154 + (i / 2) * (cardH + 12);
                r.fillGradientV(x, y, cardW, cardH, 0xFF251A38, 0xFF120D20, 245);
                r.drawRoundedRect(x, y, cardW, cardH, 10, 0xFF655080, 2);
                r.drawTextCentered(FONT_SMALL, x + cardW / 2, y + 18, titles[i], 0xFFC9BEDD);
                r.drawTextCentered(FONT_LARGE, x + cardW / 2, y + 51, values[i], colors[i]);
            }
            r.drawText(FONT_MED, pad + 8, 418, "ЗНАКИ ОТЛИЧИЯ", 0xFFFFD878);
            const char* badges[4] = {
                save.bestKills >= 100 ? "[X] СОТНЯ ВРАГОВ" : "[ ] СОТНЯ — 100 УБИЙСТВ",
                storySealCount() >= 1 ? "[X] ПЕРВАЯ ПЕЧАТЬ" : "[ ] ПЕРВАЯ ПЕЧАТЬ — СТРАЖ",
                storySealCount() >= 3 ? "[X] ТРИ ПЕЧАТИ" : "[ ] ТРИ ПЕЧАТИ — ТРИ СТРАЖА",
                save.seenEndingCutscene ? "[X] ПРАВО ПОМНИТЬ" : "[ ] ФИНАЛ — ЧЕТЫРЕ ПЕЧАТИ"
            };
            bool earned[4] = {save.bestKills >= 100, storySealCount() >= 1, storySealCount() >= 3, save.seenEndingCutscene != 0};
            for (int i = 0; i < 4; ++i) {
                const int x = pad + 8 + (i % 2) * (contentW / 2), y = 452 + (i / 2) * 34;
                r.drawText(FONT_SMALL, x, y, badges[i], earned[i] ? 0xFF74E8A0 : 0xFF9189A6);
            }
            char value[128];
            snprintf(value, sizeof(value), "КАЗНА: %d  •  ПЕЧАТИ: %d/4  •  ЗАБЕГОВ: %d", save.gold, storySealCount(), save.totalRuns);
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 540, value, 0xFFD5CBE8);
        }
        const int by = SCREEN_H - 76, bh = 52, gap = 12, bw = std::min(320, (SCREEN_W - 3 * gap) / 2);
        const int leftX = (SCREEN_W - (2 * bw + gap)) / 2, rightX = leftX + bw + gap;
        const bool pageHov = mouseX >= leftX && mouseX <= leftX + bw && mouseY >= by && mouseY <= by + bh;
        const bool exitHov = mouseX >= rightX && mouseX <= rightX + bw && mouseY >= by && mouseY <= by + bh;
        r.fillRect(leftX, by, bw, bh, pageHov ? 0xFF4A3480 : 0xFF281C48);
        r.drawRect(leftX, by, bw, bh, 0xFFBFA0E8, 2);
        r.drawTextCentered(FONT_SMALL, leftX + bw / 2, by + 18, hallPage == 0 ? "РЕЕСТР СВИДЕТЕЛЯ [TAB]" : "ЗАЛ СЛАВЫ [TAB]", 0xFFFFFFFF);
        r.fillRect(rightX, by, bw, bh, exitHov ? 0xFF543D36 : 0xFF281C26);
        r.drawRect(rightX, by, bw, bh, 0xFFB86A53, 2);
        r.drawTextCentered(FONT_SMALL, rightX + bw / 2, by + 18, "В МЕНЮ [ESC]", 0xFFFFFFFF);
        if (mouseClicked && pageHov) { hallPage = 1 - hallPage; sound.play(SFX_CLICK, 0.02f); }
        if (mouseClicked && exitHov) { state = STATE_MENU; sound.play(SFX_CLICK, 0.02f); }
    }

    void renderHallMenu(Renderer& r) {
        if (SCREEN_W < SCREEN_H) { renderHallPortrait(r); return; }
        const bool compactHall = SCREEN_W < 1200;
        renderSubmenuFrame(r, hallPage == 0 ? "ЗАЛ СЛАВЫ" : "РЕЕСТР СВИДЕТЕЛЯ",
            hallPage == 0 ? "Хроника забегов • личные рекорды" : "Улики и печати сохраняются между попытками");

        if (hallPage == 1) {
            char progress[128];
            snprintf(progress, sizeof(progress), "СЛОМАНО ПЕЧАТЕЙ: %d / 4     •     ЗАПИСИ ОТКРЫТЫ: %d / 4",
                     storySealCount(), storyEntryCount());
            const int progressX = compactHall ? 20 : 150, progressW = compactHall ? SCREEN_W - 40 : 980;
            const int witnessX = compactHall ? 20 : 112, witnessW = compactHall ? SCREEN_W - 40 : 1056;
            r.fillRoundedRect(progressX, 96, progressW, 34, 7, 0xFF34244A);
            r.drawRoundedRect(progressX, 96, progressW, 34, 7, 0xFFFFC850, 1);
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 105, progress, 0xFFFFE090);
            r.drawTextWrapped(FONT_SMALL, witnessX, 139, witnessW, heroWitnessLine(selectedHero), 0xFFB9E8FF, false);

            static const char* sealNames[4] = {"I • ДОЛГ / МОРГАР", "II • ГОЛОС / МАЛАКОР", "III • ИМЯ / КАРДИНАЛ", "IV • СВИДЕТЕЛЬСТВО / АЗАРОТ"};
            static const char* locked[4] = {
                "Запись ещё не найдена. Встреть Моргара у Ворот.",
                "Запись ещё не найдена. Услышь, что хранит Малакор.",
                "Запись ещё не найдена. Найди подводный реестр Кардинала.",
                "Запись ещё не найдена. Дойди до Пепельного трона и встреть Азарота."
            };
            static const char* clues[4] = {
                "Баррикада сложена со стороны города. Капитан удерживает ворота и снова повторяет приказ той ночи.",
                "Малакор собирает последние слова сестры. На цилиндре одна фраза заедает и повторяется без ответа.",
                "Имена вымараны в книге, но написаны заново на камнях и семейных лентах.",
                "Азарот — первый Архивариус. Центральный механизм записывал память; клетку для повтора добавили позже."
            };
            static const char* resolved[4] = {
                "Моргар закрыл ворота, чтобы выиграть время для эвакуации. Архив сохранил приказ «держать», но потерял людей, ради которых он был отдан.",
                "Запись сестры точно повторяет её последние слова, но не узнаёт брата. Копия памяти — не сама любимая.",
                "Кардинал стирал имена из страха перед новыми эхо. Люди продолжали помнить друг друга без официального разрешения.",
                "Азарот хотел не дать ни одному имени исчезнуть. Архив хранит копии умерших, забирая память у живых и повторяя ночь."
            };
            const int storyCardGap = compactHall ? 16 : 30;
            const int storyCardW = compactHall ? (SCREEN_W - 40 - storyCardGap) / 2 : 525;
            const int storyCardX = compactHall ? 20 : 90;
            for (int i = 0; i < 4; ++i) {
                const int x = storyCardX + (i % 2) * (storyCardW + storyCardGap);
                const int y = 176 + (i / 2) * 170;
                const int w = storyCardW, h = 156;
                const bool met = (save.storyEntryMask & (1u << i)) != 0;
                const bool sealed = (save.storySealMask & (1u << i)) != 0;
                const uint32_t edge = sealed ? 0xFF72DDA0 : (met ? 0xFFB889FF : 0xFF51475F);
                r.fillGradientV(x, y, w, h, met ? 0xFF241B38 : 0xFF17131F, 0xFF100D18, 245);
                r.drawRoundedRect(x, y, w, h, 10, edge, sealed ? 2 : 1);
                r.drawText(FONT_SMALL, x + 18, y + 13, sealNames[i], sealed ? 0xFF9FF0B7 : (met ? 0xFFFFD878 : 0xFF8B8495));
                const char* body = !met ? locked[i] : (sealed ? resolved[i] : clues[i]);
                r.drawTextWrapped(FONT_SMALL, x + 18, y + 40, w - 36, body,
                                  !met ? 0xFF827A8B : 0xFFE1D9EC, false);
                const char* status = sealed ? "ПЕЧАТЬ СЛОМАНА • ЗАПИСЬ В КУЗНИЦЕ" : (met ? "УЛИКА СОХРАНЕНА • ПЕЧАТЬ ЕЩЁ ДЕРЖИТСЯ" : "ЗАПИСЬ ЗАКРЫТА");
                r.drawText(FONT_SMALL, x + 18, y + 132, status, sealed ? 0xFF78E5A0 : (met ? 0xFFBDA8D7 : 0xFF706A79));
            }
            const int truthX = compactHall ? 20 : 90, truthW = compactHall ? SCREEN_W - 40 : 1100;
            r.fillRoundedRect(truthX, 527, truthW, 74, 8, 0xFF171321, 245);
            r.drawRoundedRect(truthX, 527, truthW, 74, 8, archiveTruthKnown() ? 0xFFB987E8 : 0xFF51475F, 1);
            const char* truth = archiveTruthKnown()
                ? "ПРАВДА АРХИВА: он хранит копии последних мгновений, не души. После смерти ночь пересобирается; Кузница удерживает память Свидетеля вне колокольного контура. Финальное трио — три сохранённых приказа, а не воскресшие стражи."
                : "ЗАКРЫТАЯ ЗАПИСЬ: собери четыре печати — и Архив раскроет, что именно переживает каждый новый цикл.";
            r.drawTextWrapped(FONT_SMALL, truthX + 22, 540, truthW - 44, truth, 0xFFE3D8F2, false);
        } else {
            const char* rank = save.storyChapter >= 4 ? "СВИДЕТЕЛЬ РАССВЕТА" :
                               save.bossesDefeated >= 3 ? "ХРАНИТЕЛЬ ПЕЧАТЕЙ" :
                               save.bossesDefeated >= 1 ? "ОХОТНИК НА БОССОВ" :
                               save.bestKills >= 100 ? "ВЫЖИВШИЙ" : "НОВИЧОК";
            const int rankW = std::min(500, SCREEN_W - 80), rankX = (SCREEN_W - rankW) / 2;
            r.fillRoundedRect(rankX, 101, rankW, 36, 8, 0xFF34244A);
            r.drawRoundedRect(rankX, 101, rankW, 36, 8, 0xFFFFC850, 2);
            r.drawTextCentered(FONT_MED, SCREEN_W / 2, 108, rank, 0xFFFFE090);

            int minutes = save.bestTimeSec / 60, seconds = save.bestTimeSec % 60;
            struct HallStat { int x, y; const char* title; const char* value; uint32_t color; };
            const int statX0 = compactHall ? 20 : 90, statX1 = compactHall ? SCREEN_W / 2 + 10 : 660;
            const int statW = compactHall ? (SCREEN_W - 60) / 2 : 530;
            HallStat stats[4] = {
                {statX0, 158, "РЕКОРД ВЫЖИВАНИЯ", nullptr, 0xFFFFD858},
                {statX1, 158, "РЕКОРД УБИЙСТВ", nullptr, 0xFFFF7888},
                {statX0, 312, "ПОВЕРЖЕНО БОССОВ", nullptr, 0xFFFF9C48},
                {statX1, 312, "ЗАВЕРШЕНО ЗАБЕГОВ", nullptr, 0xFF70D8FF}
            };
            char v0[64], v1[64], v2[64], v3[64];
            snprintf(v0, sizeof(v0), "%02d:%02d", minutes, seconds);
            snprintf(v1, sizeof(v1), "%d", save.bestKills);
            snprintf(v2, sizeof(v2), "%d", save.bossesDefeated);
            snprintf(v3, sizeof(v3), "%d", save.totalRuns);
            const char* vals[4] = {v0, v1, v2, v3};
            for (int i = 0; i < 4; ++i) {
                const auto& st = stats[i];
                r.fillGradientV(st.x, st.y, statW, 132, 0xFF251A38, 0xFF120D20, 245);
                r.drawRoundedRect(st.x, st.y, statW, 132, 12, 0xFF655080, 2);
                r.drawText(FONT_SMALL, st.x + 14, st.y + 20, st.title, 0xFFC9BEDD);
                r.drawTextCentered(FONT_LARGE, st.x + statW / 2, st.y + 57, vals[i], st.color);
            }

            r.drawText(FONT_MED, 104, 478, "ЗНАКИ ОТЛИЧИЯ", 0xFFFFD878);
            const char* badges[4] = {
                save.bestKills >= 100 ? "[X] СОТНЯ ВРАГОВ" : "[ ] СОТНЯ ВРАГОВ — 100 УБИЙСТВ",
                storySealCount() >= 1 ? "[X] ПЕРВАЯ ПЕЧАТЬ" : "[ ] ПЕРВАЯ ПЕЧАТЬ — ПОБЕДИ СТРАЖА",
                storySealCount() >= 3 ? "[X] ТРИ ПЕЧАТИ" : "[ ] ТРИ ПЕЧАТИ — НАЙДИ ТРЁХ СТРАЖЕЙ",
                save.seenEndingCutscene ? "[X] ПРАВО ПОМНИТЬ" : "[ ] ФИНАЛ — СОБЕРИ ЧЕТЫРЕ ПЕЧАТИ"
            };
            bool earned[4] = {save.bestKills >= 100, storySealCount() >= 1, storySealCount() >= 3, save.seenEndingCutscene != 0};
            for (int i = 0; i < 4; ++i) {
                const int x = (compactHall ? 20 : 104) + (i % 2) * (compactHall ? SCREEN_W / 2 + 10 : 555);
                const int y = 512 + (i / 2) * 36;
                r.drawText(FONT_SMALL, x, y, badges[i], earned[i] ? 0xFF74E8A0 : 0xFF9189A6);
            }
            char value[128];
            snprintf(value, sizeof(value), "КАЗНА: %d   •   ПЕЧАТИ: %d/4   •   ЗАБЕГОВ: %d", save.gold, storySealCount(), save.totalRuns);
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 590, value, 0xFFD5CBE8);
        }

        const int by = 632, bw = compactHall ? std::min(320, (SCREEN_W - 60) / 2) : 320, bh = 48;
        const int gap = 20, leftX = (SCREEN_W - (2 * bw + gap)) / 2, rightX = leftX + bw + gap;
        const bool pageHov = mouseX >= leftX && mouseX <= leftX + bw && mouseY >= by && mouseY <= by + bh;
        const bool exitHov = mouseX >= rightX && mouseX <= rightX + bw && mouseY >= by && mouseY <= by + bh;
        r.fillRoundedRect(leftX, by, bw, bh, 9, pageHov ? 0xFF4A3480 : 0xFF281C48);
        r.drawRoundedRect(leftX, by, bw, bh, 9, 0xFFBFA0E8, 2);
        r.drawTextCentered(FONT_MED, leftX + bw / 2, by + 12,
            hallPage == 0 ? "РЕЕСТР СВИДЕТЕЛЯ [TAB]" : "ЗАЛ СЛАВЫ [TAB]", 0xFFFFFFFF);
        r.fillRoundedRect(rightX, by, bw, bh, 9, exitHov ? 0xFF543D36 : 0xFF281C26);
        r.drawRoundedRect(rightX, by, bw, bh, 9, exitHov ? 0xFFFF9C72 : 0xFFB86A53, 2);
        r.drawTextCentered(FONT_MED, rightX + bw / 2, by + 12, "В МЕНЮ [ESC]", 0xFFFFFFFF);
        if (mouseClicked && pageHov) { hallPage = 1 - hallPage; sound.play(SFX_CLICK, 0.02f); }
        if (mouseClicked && exitHov) { state = STATE_MENU; sound.play(SFX_CLICK, 0.02f); }
    }

    // Wraps text into at most maxRows lines of maxWidth, trimming the tail with an ellipsis
    // so a card never spills over the button below it.
    std::string fitWrapped(Renderer& r, const BakedFont& font, const char* text, int maxWidth, int maxRows) const {
        if (!text || maxWidth <= 0 || maxRows <= 0) return std::string();
        std::string src(text), out, line, word;
        int rows = 1;
        for (size_t i = 0; i <= src.size(); ++i) {
            const char c = (i < src.size()) ? src[i] : ' ';
            if (c == ' ' || c == '\n') {
                if (!word.empty()) {
                    const std::string cand = line.empty() ? word : (line + " " + word);
                    if (r.textWidth(font, cand.c_str()) > maxWidth && !line.empty()) {
                        if (rows == maxRows) {
                            out += (out.empty() ? "" : "\n") + line + "…";
                            return out;
                        }
                        out += (out.empty() ? "" : "\n") + line;
                        ++rows;
                        line = word;
                    } else line = cand;
                    word.clear();
                }
            } else word.push_back(c);
        }
        if (!line.empty()) out += (out.empty() ? "" : "\n") + line;
        return out;
    }

    void renderShopMenu(Renderer& r) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Золото в казне: %d • сохраняется между забегами", save.gold);
        renderSubmenuFrame(r, shopTab == 0 ? "КУЗНИЦА ДУШ • УЛУЧШЕНИЯ" : "КУЗНИЦА ДУШ • ДОГОВОРЫ", buf);

        const bool portraitShop = SCREEN_W < SCREEN_H;
        const bool compactShop = SCREEN_W < 1200;
        const int tabY = 94, tabW = compactShop ? (SCREEN_W - 60) / 2 : 270, tabH = compactShop ? 32 : 28;
        const int tabGap = compactShop ? 12 : 40;
        // Centred tab pair: fixed offsets drifted left on ultrawide logical canvases.
        const int tabX1 = compactShop ? 18 : (SCREEN_W - (2 * tabW + tabGap)) / 2;
        const int tabX2 = tabX1 + tabW + tabGap;
        bool forgeTabHov = mouseX >= tabX1 && mouseX <= tabX1 + tabW && mouseY >= tabY && mouseY <= tabY + tabH;
        bool oathTabHov = mouseX >= tabX2 && mouseX <= tabX2 + tabW && mouseY >= tabY && mouseY <= tabY + tabH;
        r.fillRoundedRect(tabX1, tabY, tabW, tabH, 6, shopTab == 0 ? 0xFF543878 : 0xFF211833);
        r.drawRoundedRect(tabX1, tabY, tabW, tabH, 6, forgeTabHov || shopTab == 0 ? 0xFFFFD858 : 0xFF5B4A78, 1);
        r.drawTextCentered(FONT_SMALL, tabX1 + tabW / 2, tabY + 8, "УЛУЧШЕНИЯ", 0xFFFFFFFF);
        r.fillRoundedRect(tabX2, tabY, tabW, tabH, 6, shopTab == 1 ? 0xFF7A3228 : 0xFF211833);
        r.drawRoundedRect(tabX2, tabY, tabW, tabH, 6, oathTabHov || shopTab == 1 ? 0xFFFF9878 : 0xFF5B4A78, 1);
        r.drawTextCentered(FONT_SMALL, tabX2 + tabW / 2, tabY + 8, "ДОГОВОРЫ ВРАГОВ", 0xFFFFFFFF);
        if (mouseClicked && forgeTabHov) shopTab = 0;
        if (mouseClicked && oathTabHov) shopTab = 1;

        if (shopTab == 0) {
        const int cardW = portraitShop ? SCREEN_W - 48 : (compactShop ? (SCREEN_W - 60) / 2 : 520);
        const int cardH = compactShop && !portraitShop ? 140 : 132;
        const int columns = portraitShop ? 1 : 2;
        const int gapX = portraitShop ? 0 : (compactShop ? 20 : 50);
        const int startX = portraitShop ? 24 : (compactShop ? 20 : std::max(20, (SCREEN_W - (columns * cardW + (columns - 1) * gapX)) / 2));
        const int startY = portraitShop ? 138 : 130, gapY = portraitShop ? 8 : (compactShop ? 12 : 22);

        // Иконки кузницы для каждого улучшения (FORGE_ICONS 0–5)
        static const int FORGE_ICON_ID[6] = {0, 1, 2, 3, 4, 5};

        for (int i = 0; i < 6; ++i) {
            int col = i % columns;
            int row = i / columns;
            int cx = startX + col * (cardW + gapX);
            int cy = startY + row * (cardH + gapY);

            const MetaUpgradeDef& def = META_DEFS[i];
            int rank = std::max(0, std::min(def.maxRank, save.metaRanks[i]));
            bool maxed = (rank >= def.maxRank);
            int cost = getMetaUpgradeCost(i);
            bool canAfford = (!maxed && save.gold >= cost);

            r.fillRoundedRect(cx, cy, cardW, cardH, 10, 0xFF1E1538, 245);
            r.drawRoundedRect(cx, cy, cardW, cardH, 10, maxed ? 0xFFFFD848 : 0xFF5A4888, 2);

            const bool compactCard = compactShop && !portraitShop;
            const int iconSize = compactCard ? 36 : (portraitShop ? 48 : 56);
            // drawForgeIcon draws a 2*size frame around the centre: keep the whole frame inside the card.
            const int iconX = cx + 14 + iconSize;
            const int iconY = cy + cardH / 2;
            r.drawForgeIcon(iconX, iconY, FORGE_ICON_ID[i], iconSize, false);

            // Text starts right of the icon frame.
            const int textX = cx + 14 + iconSize * 2 + 12;
            const int textRight = cx + cardW - 16;

            // Buy button lives in the bottom-right corner, so the description gets the space above it.
            // 153 px is the widest label ("КУПИТЬ (900 ЗОЛ)"); 178 keeps 12 px of padding on both sides.
            const int bw = 178, bh = compactCard ? 40 : 42;
            int bx = cx + cardW - bw - 16, by = cy + cardH - bh - 8;

            const int rankW = r.textWidth(FONT_SMALL, "Ранг 50 / 50") + 12;
            const std::string name = fitWrapped(r, FONT_MED, def.name, textRight - textX - rankW, 1);
            r.drawText(FONT_MED, textX, cy + 14, name.c_str(), 0xFFFFFFFF);
            snprintf(buf, sizeof(buf), "Ранг %d / %d", rank, def.maxRank);
            r.drawTextRight(FONT_SMALL, textRight, cy + 18, buf, 0xFFFFD848);

            static const char* compactMetaDesc[6] = {
                "+4% урона за ранг; после XII вклад снижен.",
                "+10 к запасу HP за ранг; после XII вклад снижен.",
                "+3% к скорости за эффективный ранг.",
                "+10% радиус сбора и +5% золота за ранг.",
                "+5% опыта и +2% к критическому шансу за ранг.",
                "Снижает входящий урон на 0.8 за эффективный ранг."
            };
            // The description never reaches the button: width stops at the button column and the
            // number of rows is derived from the free height above it.
            const int descY = cy + 36;
            const int descW = textRight - textX;
            const int descRows = std::max(1, (by - 6 - descY) / std::max(1, FONT_SMALL.lineHeight));
            const std::string desc = fitWrapped(r, FONT_SMALL, compactCard ? compactMetaDesc[i] : def.desc, descW, descRows);
            r.drawTextWrapped(FONT_SMALL, textX, descY, descW, desc.c_str(), 0xFFD0C8E8, false);

            // Ten compact segments represent five ranks each; 50 levels never overflow the card.
            const int pipStep = std::max(10, std::min(compactCard ? 18 : 24, (bx - 12 - textX) / 10));
            const int pipW = std::max(8, pipStep - 2);
            for (int p = 0; p < 10; ++p) {
                const int sx = textX + p * pipStep;
                const int filledRanks = std::max(0, std::min(5, rank - p * 5));
                const int pipY = by + (bh - 10) / 2;
                r.fillRoundedRect(sx, pipY, pipW, 10, 3, 0xFF2E2448);
                if (filledRanks > 0)
                    r.fillRoundedRect(sx, pipY, std::max(4, pipW * filledRanks / 5), 10, 3, 0xFF50FF90);
            }
            bool hov = (mouseX >= bx && mouseX <= bx + bw && mouseY >= by && mouseY <= by + bh);
            uint32_t btnCol = maxed ? 0xFF38304C : (canAfford ? (hov ? 0xFFE8A020 : 0xFFB87818) : 0xFF3A2438);
            r.fillRoundedRect(bx, by, bw, bh, 7, btnCol);
            r.drawRoundedRect(bx, by, bw, bh, 7, canAfford ? 0xFFFFE878 : 0xFF685878, 2);

            if (maxed) {
                r.drawTextCentered(FONT_SMALL, bx + bw / 2, by + 12, "МАКСИМУМ", 0xFFFFD848);
            } else {
                snprintf(buf, sizeof(buf), "КУПИТЬ (%d ЗОЛ)", cost);
                r.drawTextCentered(FONT_SMALL, bx + bw / 2, by + 12, buf, canAfford ? 0xFFFFFFFF : 0xFFA898B8);
            }

            if (mouseClicked && hov && canAfford) {
                save.gold -= cost;
                save.metaRanks[i]++;
                writeSave();
                sound.play(SFX_LEVELUP, 0.02f);
            }
        }
        } else {
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 134, "Покупка усиливает каждого следующего врага и повышает всю золотую добычу. Договоры постоянны.", 0xFFFFC7A8);
            const int oathColumns = portraitShop ? 2 : 3;
            const int oathGapX = compactShop ? 12 : 20, oathGapY = portraitShop ? 12 : 14;
            const int oathPad = compactShop ? (portraitShop ? 24 : 20) : 50;
            const int oathW = compactShop ? (SCREEN_W - 2 * oathPad - (oathColumns - 1) * oathGapX) / oathColumns : 360;
            const int oathH = portraitShop ? 164 : 182, oathY = portraitShop ? 146 : 160;
            const int oathX0 = compactShop ? oathPad
                : std::max(oathPad, (SCREEN_W - (oathColumns * oathW + (oathColumns - 1) * oathGapX)) / 2);
            for (int i = 0; i < ENEMY_OATH_COUNT; ++i) {
                const int col = i % oathColumns, row = i / oathColumns;
                int x = oathX0 + col * (oathW + oathGapX);
                int y = oathY + row * (oathH + oathGapY);
                int rank = getEnemyOathRank(i);
                const MetaUpgradeDef& def = ENEMY_OATHS[i];
                int cost = getEnemyOathCost(i);
                bool maxed = rank >= def.maxRank, canBuy = !maxed && save.gold >= cost;
                r.fillGradientV(x, y, oathW, oathH, 0xFF321928, 0xFF171020, 245);
                r.drawRoundedRect(x, y, oathW, oathH, 10, maxed ? 0xFFFFD858 : 0xFF9D5548, 2);

                // Иконка договора в левом верхнем углу карты (из FORGE_ICONS 6–11).
                // Кадр иконки — 2*size вокруг центра, поэтому центр сдвинут внутрь карты,
                // иначе кадр вылезал над картой и закрывал подсказку сверху.
                const int oathIconSize = compactShop ? 26 : 32;
                const int oathIconCx = x + 10 + oathIconSize;
                const int oathIconCy = y + 8 + oathIconSize;
                const int oathTextX = x + 10 + oathIconSize * 2 + 12;
                r.drawForgeIcon(oathIconCx, oathIconCy, 6 + i, oathIconSize, false);

                snprintf(buf, sizeof(buf), "%d / %d", rank, def.maxRank);
                // The rank moves down to the pip row: on a 360 px card the title needs the full width,
                // otherwise "Марш голодной стаи" would be cut right next to the counter.
                const std::string oathName = fitWrapped(r, FONT_MED, def.name, x + oathW - 16 - oathTextX, 1);
                r.drawText(FONT_MED, oathTextX, y + 10, oathName.c_str(), 0xFFFFE5D0);
                r.drawTextRight(FONT_SMALL, x + oathW - 16, y + 86, buf, 0xFFFFD878);
                const int oathDescY = y + 38;
                const int oathDescW = x + oathW - 16 - oathTextX;
                const int oathDescRows = std::max(1, (y + 84 - oathDescY) / std::max(1, FONT_SMALL.lineHeight));
                const std::string oathDesc = fitWrapped(r, FONT_SMALL, def.desc, oathDescW, oathDescRows);
                r.drawTextWrapped(FONT_SMALL, oathTextX, oathDescY, oathDescW, oathDesc.c_str(), 0xFFE6D5D8, true);
                const int pipRoom = x + oathW - 16 - r.textWidth(FONT_SMALL, "10 / 10") - 10 - oathTextX;
                const int oathPipStep = std::max(12, std::min(25, pipRoom / std::max(1, def.maxRank)));
                for (int pip = 0; pip < def.maxRank; ++pip)
                    r.fillRoundedRect(oathTextX + pip * oathPipStep, y + 88, std::max(8, oathPipStep - 6), 10, 3,
                                      pip < rank ? 0xFFFF7158 : 0xFF382638);
                int bx = x + oathW - 160, by = y + 112, bw = 144, bh = 48;
                bool hov = mouseX >= bx && mouseX <= bx + bw && mouseY >= by && mouseY <= by + bh;
                r.fillRoundedRect(bx, by, bw, bh, 7, maxed ? 0xFF38304C : (canBuy ? (hov ? 0xFFD14C32 : 0xFF913624) : 0xFF3A2438));
                r.drawRoundedRect(bx, by, bw, bh, 7, canBuy ? 0xFFFFB890 : 0xFF685878, 2);
                snprintf(buf, sizeof(buf), maxed ? "МАКСИМУМ" : "%d золота", cost);
                r.drawTextCentered(FONT_SMALL, bx + bw / 2, by + 15, buf, canBuy || maxed ? 0xFFFFFFFF : 0xFFA898B8);
                if (mouseClicked && hov && canBuy) {
                    save.gold -= cost; setEnemyOathRank(i, rank + 1); writeSave(); sound.play(SFX_LEVELUP, 0.02f);
                }
            }
            const int oathInfoY = portraitShop ? oathY + 3 * (oathH + oathGapY) + 8 : 540;
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, oathInfoY, "Шесть постоянных договоров • до 10 рангов каждый • рост сложности и награды складываются.", 0xFFE0B8AA);
        }

        // Back & Start Battle Buttons
        const int footerY = portraitShop ? SCREEN_H - 76 : 595;
        const int footerW = std::min(320, (SCREEN_W - 48) / 2), footerGap = 16;
        const int footerX = (SCREEN_W - (2 * footerW + footerGap)) / 2;
        UIButton btns[2] = {
            {footerX, footerY, footerW, 52, "НАЗАД В МЕНЮ [ESC]", 1},
            // Было «СРАЗУ В БОЙ!»: кнопка молча кидала в забег с последним героем и
            // последним делом, поэтому было непонятно, куда именно она ведёт.
            // Теперь путь из кузницы идёт через выбор героя, а дальше — выбор дела.
            {footerX + footerW + footerGap, footerY, footerW, 52, "В БОЙ → ГЕРОЙ", 2}
        };
        for (int i = 0; i < 2; ++i) {
            const auto& b = btns[i];
            bool hov = (mouseX >= b.x && mouseX <= b.x + b.w && mouseY >= b.y && mouseY <= b.y + b.h);
            r.fillRoundedRect(b.x, b.y, b.w, b.h, 10, (b.id == 2) ? (hov ? 0xFFE83858 : 0xFFB81E3C) : (hov ? 0xFF423074 : 0xFF281C48));
            r.drawRoundedRect(b.x, b.y, b.w, b.h, 10, 0xFFFFD858, 2);
            r.drawTextCentered(FONT_MED, b.x + b.w / 2, b.y + 14, b.label, 0xFFFFFFFF);
            if (mouseClicked && hov) {
                sound.play(SFX_CLICK, 0.02f);
                if (b.id == 1) state = STATE_MENU;
                else openHeroSelectForMode(MODE_STORY);
            }
        }
    }

    // Plain-language glossary. The game throws a lot of its own vocabulary at the player
    // ("тихая охота", "прорыв", "резонатор", "печать"), so every term gets a short,
    // honest explanation here instead of being explained by trial and death.
    struct GlossaryEntry { const char* term; const char* text; uint32_t color; };
    static int glossaryRowCount() { return 12; }
    static const GlossaryEntry& glossaryRow(int i) {
        static const GlossaryEntry rows[12] = {
            {"ТИХАЯ ОХОТА",
             "Возможность, а не стена. На след выходит конечная стая из 20 зверей: они оставляют монеты "
             "по мере гибели, а за добитую стаю платят награду. Не успел за 30 секунд — стая уходит, "
             "и ты теряешь только награду. Прогресс не блокирует, смертью не грозит.", 0xFFFFC848},
            {"ПРОРЫВ",
             "Стена, а не возможность. Каждые 7–15 уровней на 18 секунд приходит усиленная орда: отсидеться "
             "нельзя — надо получить один удар, иначе она не отступит. После неё 18 секунд передышки, "
             "а следующий прорыв будет злее. Обойти нельзя — в отличие от тихой охоты.", 0xFFFF7088},
            {"ТИР СБОРКИ",
             "Общая мощь сборки, I–V. Растёт от новых предметов и прорывов; открывает более сильные версии оружия "
             "и держит рост характеристик в узде.", 0xFF80E58A},
            {"РЕЗОНАТОР",
             "Структура Архива на арене. Пока она цела, враги появляются чаще и подходят плотнее. "
             "Разбей резонатор — и спавн просядет.", 0xFF5BD8EE},
            {"ПЕЧАТЬ",
             "Решение, которое город принял в ночь катастрофы и с тех пор не отменял: Долг, Голос, Имя, "
             "Свидетельство. Ломается только в финале дела — боссом.", 0xFFE9A0E0},
            {"СИМБИОЗ",
             "Эволюция оружия на максимальном уровне прокачки. Меняет механику предмета, "
             "а не просто добавляет цифры урона.", 0xFFC18AFF},
            {"МАТРЁШКА",
             "Враг, из которого при смерти вылезают другие враги. Не оставляй её за спиной "
             "и не трать на неё одиночный удар в самом начале боя.", 0xFFFF8C43},
            {"ЭХО-ПОРТАЛ",
             "Выход с поля боя. Забирает добычу и найденные улики, но закрывает текущее дело без босса: "
             "печати и прогресс кампании остаются нетронутыми.", 0xFF64D9F4},
            {"КУЗНИЦА ДУШ",
             "Мета-прогресс. Сюда идёт золото из всех забегов; здесь покупаются постоянные улучшения, "
             "которые работают в каждом новом деле.", 0xFFFF9C56},
            {"АРХИВ",
             "Машина, построенная, чтобы никто не исчезал. Записывает последние минуты умирающих и хранит "
             "их копии. Копия отвечает и помнит — и не является человеком.", 0xFFA6C4FF},
            {"СВИДЕТЕЛЬ",
             "Ты. Полного имени в реестре нет, поэтому Архив не знает, кому отвечает, — и ты можешь "
             "задавать вопросы, на которые машина не отвечает.", 0xFFFFE078},
            {"ДЕЛО И ВЕТКИ",
             "Кампания — 28 дел в пяти актах. Дело открывается, когда закрыто предыдущее; на ветках "
             "нужны оба следа, потому что Архив подшивает дело только при двойном подтверждении.", 0xFF9FE5A4},
        };
        return rows[std::max(0, std::min(11, i))];
    }

    void renderGlossary(Renderer& r) {
        const bool portraitGloss = SCREEN_W < SCREEN_H;
        const bool compactGloss = SCREEN_W < 1100;
        renderSubmenuFrame(r, "СЛОВАРЬ ТЕРМИНОВ",
                           "Что значит «тихая охота», «прорыв», «печать» и всё остальное", 0xFF9FE5A4, 68);
        const int pad = compactGloss ? 20 : 42, w = SCREEN_W - 2 * pad;
        const int rows = portraitGloss ? 5 : 4;
        const int rowH = portraitGloss ? 116 : 104, rowStep = rowH + 8;
        const int total = glossaryRowCount();
        const int maxScroll = std::max(0, total - rows);
        glossaryScroll = std::max(0, std::min(maxScroll, glossaryScroll));
        const int top = 84;
        if (mouseWheelDelta != 0) {
            const int step = mouseWheelDelta < 0 ? 1 : -1;
            glossaryScroll = std::max(0, std::min(maxScroll, glossaryScroll + step));
        }
        for (int i = 0; i < rows; ++i) {
            const int index = glossaryScroll + i;
            if (index >= total) break;
            const GlossaryEntry& e = glossaryRow(index);
            const int y = top + i * rowStep;
            r.fillRoundedRect(pad, y, w, rowH, 9, 0xFF18122E, 248);
            r.drawRoundedRect(pad, y, w, rowH, 9, e.color, 2, 220);
            r.fillRoundedRect(pad + 8, y + 10, 4, rowH - 20, 2, e.color);
            r.drawText(compactGloss ? FONT_SMALL : FONT_MED, pad + 22, y + 8, e.term, e.color);
            r.drawTextWrapped(FONT_SMALL, pad + 22, y + (compactGloss ? 30 : 34), w - 44, e.text, 0xFFDDD5EA, false);
        }
        char range[64];
        snprintf(range, sizeof(range), "ТЕРМИНЫ %d–%d ИЗ %d", glossaryScroll + 1,
                 std::min(total, glossaryScroll + rows), total);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, top + rows * rowStep + 6, range, 0xFFBEB3D4);
        const int bw = std::min(320, SCREEN_W - 48), bh = 48;
        const int bx = (SCREEN_W - bw) / 2, by = SCREEN_H - 56;
        const bool hov = mouseX >= bx && mouseX <= bx + bw && mouseY >= by && mouseY <= by + bh;
        r.fillRoundedRect(bx, by, bw, bh, 10, hov ? 0xFF483480 : 0xFF281C48);
        r.drawRoundedRect(bx, by, bw, bh, 10, 0xFFFFD858, 2);
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, by + 12, "К АРСЕНАЛУ [ESC]", 0xFFFFFFFF);
        if (mouseClicked && hov) { sound.play(SFX_CLICK, 0.02f); helpPage = 0; }
    }

    void renderHelpMenu(Renderer& r) {
        if (helpPage == 1) { renderGlossary(r); return; }
        const bool portraitHelp = SCREEN_W < SCREEN_H;
        const bool compactHelp = SCREEN_W < 1100;
        renderSubmenuFrame(r, "АРСЕНАЛ ОРУЖИЯ И УПРАВЛЕНИЕ",
                           "Автопорядок: тир V→I • внутри тира — оружие, затем артефакты", 0xFFFFD878, 68);
        const int helpPad = compactHelp ? 20 : 42, helpW = SCREEN_W - 2 * helpPad;
        r.fillRoundedRect(helpPad, 82, helpW, 82, 10, 0xFF1E1538);
        r.drawRoundedRect(helpPad, 82, helpW, 82, 10, 0xFF6852A0, 2);
        r.drawText(FONT_SMALL, helpPad + 18, 92, "WASD / СТРЕЛКИ — движение  •  ПРОБЕЛ / SHIFT — рывок  •  Q — суперумение", 0xFFE0D8F8);
        r.drawText(FONT_SMALL, helpPad + 18, 113, "1–4 / ЛКМ — улучшение  •  R — замена  •  ПКМ — бег за курсором  •  M / N — звук", 0xFFE0D8F8);
        r.drawText(FONT_SMALL, helpPad + 18, 134, "O — настройки  •  ESC — назад  •  1% врагов применяет тактики перехвата и уклонения", 0xFFBEEAFF);

        const int visibleRows = portraitHelp ? 8 : 4;
        const int totalRows = (ITEM_COUNT + 1) / 2;
        const int maxScroll = std::max(0, totalRows - visibleRows);
        const std::vector<int> arsenalOrder = getArsenalItemOrder();
        arsenalScrollRow = std::max(0, std::min(maxScroll, arsenalScrollRow));
        const int listTop = 174, cardH = 104, rowStep = 111;
        const int colGap = compactHelp ? 20 : 24;
        const int cardW = compactHelp
            ? (SCREEN_W - 3 * helpPad - colGap) / 2
            : std::min(720, (SCREEN_W - 2 * helpPad - colGap - 24) / 2);
        const int leftX = (SCREEN_W - (2 * cardW + colGap)) / 2;
        const int rightX = leftX + cardW + colGap;
        const int listBottom = listTop + visibleRows * rowStep - 8;

        if (mouseWheelDelta != 0 && mouseX >= 12 && mouseX <= SCREEN_W - 12 && mouseY >= listTop && mouseY <= listBottom) {
            const int steps = std::max(1, std::abs(mouseWheelDelta) / 120);
            arsenalScrollRow = std::max(0, std::min(maxScroll, arsenalScrollRow + (mouseWheelDelta < 0 ? steps : -steps)));
        }

        for (int row = 0; row < visibleRows; ++row) {
            for (int col = 0; col < 2; ++col) {
                const int displayIndex = (arsenalScrollRow + row) * 2 + col;
                if (displayIndex >= ITEM_COUNT) continue;
                const int item = arsenalOrder[displayIndex];
                const int x = col == 0 ? leftX : rightX;
                const int y = listTop + row * rowStep;
                const uint32_t edge = getItemTierColor(item);
                r.fillRoundedRect(x, y, cardW, cardH, 9, 0xFF18122E, 247);
                r.drawRoundedRect(x, y, cardW, cardH, 9, edge, 2);
                r.drawWeaponIcon(x + 24, y + 26, item, 17);
                r.drawText(compactHelp ? FONT_SMALL : FONT_MED, x + 50, y + 9, ITEM_DEFS[item].name, 0xFFFFE068);
                char tier[32];
                snprintf(tier, sizeof(tier), "%s • ТИР %d", ITEM_DEFS[item].isWeapon ? "ОРУЖИЕ" : "АРТЕФАКТ", getItemTier(item));
                r.drawTextRight(FONT_SMALL, x + cardW - 18, y + (compactHelp ? 23 : 13), tier, edge);
                r.drawTextWrapped(FONT_SMALL, x + 50, y + 37, cardW - 70,
                                  ITEM_DEFS[item].desc, 0xFFD0C8E8, false);
            }
        }

        // Compact scroll bar: each wheel notch/arrow moves one full row (two entries).
        const int trackX = leftX + 2 * cardW + colGap + 16, trackY = listTop;
        const int trackH = listBottom - listTop;
        const int thumbH = std::max(54, trackH * visibleRows / std::max(visibleRows, totalRows));
        const int thumbTravel = trackH - thumbH;
        const int thumbY = trackY + (maxScroll > 0 ? (arsenalScrollRow * thumbTravel) / maxScroll : 0);
        r.fillRoundedRect(trackX, trackY, 12, trackH, 6, 0xFF20192D);
        r.drawRoundedRect(trackX, trackY, 12, trackH, 6, 0xFF5D4B77, 1);
        r.fillRoundedRect(trackX - 1, thumbY, 14, thumbH, 7, 0xFFAE8CDD);
        r.drawRoundedRect(trackX - 1, thumbY, 14, thumbH, 7, 0xFFE3CFFF, 1);
        if (mouseClicked && mouseX >= trackX - 8 && mouseX <= trackX + 20 && mouseY >= trackY && mouseY <= trackY + trackH) {
            if (maxScroll > 0 && thumbTravel > 0) {
                const float normalized = (mouseY - trackY - thumbH * 0.5f) / (float)thumbTravel;
                arsenalScrollRow = std::max(0, std::min(maxScroll, (int)roundf(normalized * maxScroll)));
            }
        }
        const int firstItem = arsenalScrollRow * 2 + 1;
        const int lastItem = std::min(ITEM_COUNT, (arsenalScrollRow + visibleRows) * 2);
        char range[96];
        snprintf(range, sizeof(range), "ПРЕДМЕТЫ %d–%d ИЗ %d", firstItem, lastItem, ITEM_COUNT);
        const int hintY = listBottom + 4;
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, hintY, range, 0xFFFFD878);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, hintY + FONT_SMALL.lineHeight,
            "КОЛЁСО МЫШИ  •  ↑ / ↓ — СТРОКА  •  PGUP / PGDN — ЭКРАН", 0xFFBEB3D4);

        const int bw = std::min(320, SCREEN_W - 48), bh = 48;
        const int bx = (SCREEN_W - bw) / 2, by = SCREEN_H - 56;
        const bool hov = mouseX >= bx && mouseX <= bx + bw && mouseY >= by && mouseY <= by + bh;
        r.fillRoundedRect(bx, by, bw, bh, 10, hov ? 0xFF483480 : 0xFF281C48);
        r.drawRoundedRect(bx, by, bw, bh, 10, 0xFFFFD858, 2);
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, by + 12, "ВЕРНУТЬСЯ В МЕНЮ [ESC]", 0xFFFFFFFF);
        if (mouseClicked && hov) {
            sound.play(SFX_CLICK, 0.02f);
            state = STATE_MENU;
        }
        // Glossary lives next to the arsenal: the same screen explains the vocabulary.
        const int gw = std::min(300, SCREEN_W - 48), gh = 48;
        const int gx = SCREEN_W - gw - (SCREEN_W - 48 - gw > 40 ? 40 : 12), gy = by;
        const bool ghov = mouseX >= gx && mouseX <= gx + gw && mouseY >= gy && mouseY <= gy + gh;
        r.fillRoundedRect(gx, gy, gw, gh, 10, ghov ? 0xFF2F5A46 : 0xFF1D3A30);
        r.drawRoundedRect(gx, gy, gw, gh, 10, 0xFF9FE5A4, 2);
        r.drawTextCentered(FONT_MED, gx + gw / 2, gy + 12, "СЛОВАРЬ [G]", 0xFFFFFFFF);
        if (mouseClicked && ghov) { sound.play(SFX_CLICK, 0.02f); helpPage = 1; glossaryScroll = 0; }
    }

    static constexpr int CHEAT_ITEM_COUNT = 14;

    static const char* cheatItemLabel(int index) {
        static const char* const labels[CHEAT_ITEM_COUNT] = {
            "+5000 ЗОЛОТА",
            "ВСЕ ДЕЛА ОТКРЫТЫ",
            "СБРОСИТЬ КАМПАНИЮ",
            "ВСЕ ГЕРОИ",
            "МАКС. КУЗНИЦА ДУШ",
            "БЕССМЕРТИЕ",
            "УБИТЬ ВСЕХ ВРАГОВ",
            "+5 УРОВНЕЙ",
            "ВСЕ 40 ПРЕДМЕТОВ ПО 3 РАНГА",
            "СПАВН СТАИ КРЫС",
            "ПРИЗВАТЬ СТРАЖА",
            "ПОЛНЫЙ ЗАРЯД СУПЕРУМЕНИЯ",
            "F3-ОВЕРЛЕЙ",
            "НОВАЯ НОЧЬ (xЗОЛОТО)"
        };
        return labels[index < 0 || index >= CHEAT_ITEM_COUNT ? 0 : index];
    }

    // Что показать справа от пункта: состояние переключателя или подпись.
    std::string cheatItemState(int index) {
        switch (index) {
            case 5: return cheatGodMode ? "ВКЛ" : "ВЫКЛ";
            case 10: {
                static const char* const names[6] = {"МОРГАР", "МАЛАКОР", "КАРДИНАЛ",
                                                     "АЗАРОТ", "ПРОТОКОЛИСТ", "ДВОЙНИК"};
                return names[cheatBossCycle % 6];
            }
            case 12: return debugOverlay ? "ВКЛ" : "ВЫКЛ";
            default: return "";
        }
    }

    void openCheatMenu() {
        cheatMenuOpen = true;
        cheatSelection = 0;
        sound.play(SFX_LEVELUP, 0.05f);
    }

    void closeCheatMenu() {
        cheatMenuOpen = false;
        sound.play(SFX_CLICK, 0.03f);
    }

    void runCheat(int id) {
        switch (id) {
            case 0: save.gold += 5000; break;                       // золото
            case 1: {                                               // все дела закрыты
                uint32_t mask = 0;
                for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i) mask |= 1u << i;
                save.campaignMissionsDone = mask;
                save.storySealMask = 0x0Fu;
                save.storyEntryMask |= 0x3Fu;
                save.seenEndingCutscene = 1;
                break;
            }
            case 2: save.campaignMissionsDone = 0; save.storySealMask = 0; save.storyEntryMask = 0; break;
            case 3: save.bossesDefeated = 5; save.storySealMask = 0x0Fu; break;   // все герои
            case 4: for (int i = 0; i < 6; ++i) save.metaRanks[i] = 10; break;    // Кузница душ
            case 5: cheatGodMode = !cheatGodMode; break;                          // бессмертие
            case 6:                                                              // убить всех
                for (auto& m : monsters) if (!m.isBoss) m.hp = 0.0f;
                break;
            case 7: {                                             // +5 уровней
                for (int i = 0; i < 5; ++i) {
                    ++playerLevel;
                    xpToNext = getXpRequirementForLevel(playerLevel);
                }
                playerXp = 0;
                if (state == STATE_PLAYING) generateUpgradeChoices();
                break;
            }
            case 8:                                               // все предметы: проверка иконок
                for (int i = 0; i < ITEM_COUNT; ++i)
                    if (itemLevels[i] < 3 && getItemMaxLevel(i) >= 3) itemLevels[i] = 3;
                break;
            case 9:                                               // стая крыс: проверка плотности
                if (state == STATE_PLAYING) spawnRatSwarm();
                break;
            case 10: {                                            // страж по очереди: проверка боссов
                if (state == STATE_PLAYING) {
                    const int type = cheatBossCycle % 6;
                    clearArenaForBoss();
                    spawnMonster(type, false, true, type);
                    if (!monsters.empty()) {
                        static const char* const names[6] = {"МОРГАР", "МАЛАКОР", "КАРДИНАЛ",
                                                             "АЗАРОТ", "ПРОТОКОЛИСТ", "ДВОЙНИК"};
                        announce(GMSG_BOSS_ABILITY, std::string("ОТЛАДКА: ") + names[type], 2.4f);
                    }
                    cheatBossCycle = (cheatBossCycle + 1) % 6;
                }
                break;
            }
            case 11: superCharge = 100.0f; break;                 // полный заряд
            case 12: debugOverlay = !debugOverlay; break;         // F3-оверлей
            case 13: beginNewCampaignCycle(); break;              // новая ночь (xзолото)
            default: break;
        }
        writeSave();
        sound.play(SFX_CLICK, 0.05f);
    }

    void renderCheatMenu(Renderer& r) {
        r.fillRect(0, 0, SCREEN_W, SCREEN_H, 0xFF05040A, 214);
        const int w = std::min(660, SCREEN_W - 40);
        const int rowStep = 32;
        const int h = std::min(SCREEN_H - 32, 118 + CHEAT_ITEM_COUNT * rowStep);
        const int x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
        r.fillRoundedRect(x, y, w, h, 12, 0xFF14101F, 250);
        r.drawRoundedRect(x, y, w, h, 12, 0xFFFF7A83, 2);
        r.drawTextCentered(FONT_MED, x + w / 2, y + 16, "ДЕБАГ-МЕНЮ", 0xFFFFD858);
        r.drawTextCentered(FONT_SMALL, x + w / 2, y + 48,
                           "↑ / ↓ — СТРОКА  •  ENTER — ВЫПОЛНИТЬ  •  1…9, 0 — БЫСТРО  •  ESC — ЗАКРЫТЬ  (БОЙ ЗАМЕР)",
                           0xFFC6BBD8, false);
        for (int i = 0; i < CHEAT_ITEM_COUNT; ++i) {
            const int iy = y + 76 + i * rowStep;
            if (iy + 28 > y + h - 12) break;
            const bool hov = mouseX >= x + 20 && mouseX <= x + w - 20 && mouseY >= iy && mouseY <= iy + 26;
            const bool sel = (i == cheatSelection);
            r.fillRoundedRect(x + 20, iy, w - 40, 26, 6,
                              sel ? 0xFF3F2C60 : (hov ? 0xFF32254C : 0xFF1D1729));
            r.drawRoundedRect(x + 20, iy, w - 40, 26, 6,
                              sel ? 0xFFFFD858 : (hov ? 0xFFFFE068 : 0xFF4A3F63), sel ? 2 : 1);
            char label[128];
            if (i < 10) snprintf(label, sizeof(label), "%d  %s", i + 1, cheatItemLabel(i));
            else snprintf(label, sizeof(label), "    %s", cheatItemLabel(i));
            r.drawText(FONT_SMALL, x + 34, iy + 6, label, sel ? 0xFFFFF2C8 : 0xFFE8E1F0, false);
            const std::string state = cheatItemState(i);
            if (!state.empty())
                r.drawTextRight(FONT_SMALL, x + w - 34, iy + 6, state.c_str(),
                                (i == 5 && cheatGodMode) || (i == 12 && debugOverlay) ? 0xFF8AF0AE : 0xFFB9ADCC, false);
            // Клик срабатывает только по своей строке: раньше условие захватывало
            // и подсвеченную клавиатурой строку, поэтому клик мимо пункта запускал её.
            if (mouseClicked && hov) { cheatSelection = i; runCheat(i); }
        }
    }

    void onKeyPress(int key) {
        if (cheatMenuOpen) {
            if (key == 27) { closeCheatMenu(); return; }
            if (key == 38 || key == 'W' || key == 'w') {
                cheatSelection = (cheatSelection + CHEAT_ITEM_COUNT - 1) % CHEAT_ITEM_COUNT;
                sound.play(SFX_CLICK, 0.02f);
                return;
            }
            if (key == 40 || key == 'S' || key == 's') {
                cheatSelection = (cheatSelection + 1) % CHEAT_ITEM_COUNT;
                sound.play(SFX_CLICK, 0.02f);
                return;
            }
            if (key == 13 || key == 32) { runCheat(cheatSelection); return; }
            // Первые девять строк — цифрами 1…9, десятая — нулём. Дальше стрелки.
            if (key >= '1' && key <= '9') { cheatSelection = key - '1'; runCheat(cheatSelection); return; }
            if (key == '0') { cheatSelection = 9; runCheat(9); return; }
            return;
        }
        // F9 открывает отладку в любом состоянии; клавиша ` — для ноутбуков,
        // где F-ряд спрятан за Fn.
        if (key == 0x78 || key == 0xC0) { openCheatMenu(); return; } // VK_F9 / VK_OEM_3
        if (state == STATE_MENU && exitConfirmationOpen) {
            if (key == 27) { exitConfirmationOpen = false; exitDialogFocus = 1; }
            else if (key == 37 || key == 39 || key == 38 || key == 40 || key == 'A' || key == 'a' || key == 'D' || key == 'd' || key == 'W' || key == 'w' || key == 'S' || key == 's') {
                mouseMovedSinceLaunch = false;
                exitDialogFocus = 1 - exitDialogFocus;
                sound.play(SFX_CLICK, 0.02f);
            } else if (key == 13 || key == 32) {
                if (exitDialogFocus == 0) { requestExit = true; exitConfirmationOpen = false; }
                else { exitConfirmationOpen = false; }
            }
            return;
        }
        // Music has no in-game variants any more: tracks come from audio/ only.
        if (key == 'M' || key == 'm') { sound.enabled.store(!sound.enabled.load(std::memory_order_relaxed), std::memory_order_relaxed); saveOptions(); return; }
        if (key == 'N' || key == 'n') { sound.musicEnabled.store(!sound.musicEnabled.load(std::memory_order_relaxed), std::memory_order_relaxed); saveOptions(); return; }
        if (state == STATE_PATCH_NOTES) {
            const float pageStep = 6.0f * (float)FONT_MED.lineHeight;
            if (key == 37 || key == 'A' || key == 'a') patchNotesGoPage(-1);
            else if (key == 39 || key == 'D' || key == 'd') patchNotesGoPage(1);
            // Прокрутка заметки: колесо уже обработано в render, здесь клавиши.
            else if (key == 33) patchNotesScroll = std::max(0.0f, patchNotesScroll - pageStep);        // PageUp
            else if (key == 34) patchNotesScroll = std::min((float)patchNotesScrollMax, patchNotesScroll + pageStep); // PageDown
            else if (key == 38 || key == 'W' || key == 'w') patchNotesScroll = std::max(0.0f, patchNotesScroll - (float)FONT_MED.lineHeight);
            else if (key == 40 || key == 'S' || key == 's') patchNotesScroll = std::min((float)patchNotesScrollMax, patchNotesScroll + (float)FONT_MED.lineHeight);
            else if (key == 36) patchNotesScroll = 0.0f;                                              // Home
            else if (key == 35) patchNotesScroll = (float)patchNotesScrollMax;                        // End
            else if (key == 13 || key == 27 || key == 32) closePatchNotes();
            return;
        }
        if (state == STATE_CUTSCENE) {
            if (key == 13 || key == 27 || key == 32) finishCutscene();
            return;
        }
        if (state == STATE_MENU) {
            if (key == 0x7B) startBossMusicTestArena(); // VK_F12: safe direct access to the boss-music test arena.
            else if (key == 'O' || key == 'o') { mainMenuFocus = 5; activateMainMenuItem(5); }
            else if (key == 37 || key == 'A' || key == 'a' || key == 38 || key == 'W' || key == 'w') {
                mouseMovedSinceLaunch = false; // keyboard focus remains authoritative until the pointer moves again.
                mainMenuFocus = (mainMenuFocus + MAIN_MENU_ITEM_COUNT - 1) % MAIN_MENU_ITEM_COUNT;
                sound.play(SFX_CLICK, 0.02f);
            } else if (key == 39 || key == 'D' || key == 'd' || key == 40) {
                mouseMovedSinceLaunch = false;
                mainMenuFocus = (mainMenuFocus + 1) % MAIN_MENU_ITEM_COUNT;
                sound.play(SFX_CLICK, 0.02f);
            } else if (key == 'S' || key == 's') { mainMenuFocus = 0; activateMainMenuItem(0); }
            else if (key == 'E' || key == 'e') { mainMenuFocus = 1; activateMainMenuItem(1); }
            else if (key == 13 || key == 32) activateMainMenuItem(mainMenuFocus);
        } else if (state == STATE_HERO_SELECT) {
            if (key == 27) state = STATE_MENU;
            else if (key >= '1' && key <= '5' && isHeroUnlocked(key - '1')) selectedHero = key - '1';
            else if (key == 37 || key == 'A' || key == 'a') cycleHero(-1);
            else if (key == 39 || key == 'D' || key == 'd') cycleHero(1);
            else if (key == 13 || key == 32) { sound.play(SFX_CLICK, 0.02f); confirmHeroSelection(); }
        } else if (state == STATE_MISSION_SELECT) {
            if (key == 27) state = STATE_HERO_SELECT;
            // Дерево: ↔ — дела внутри полосы, ↕ (и Q/E) — акты. Вкладок больше нет.
            else if (key == 'Q' || key == 'q') moveCampaignCursor(-1, 0);
            else if (key == 'E' || key == 'e') moveCampaignCursor(1, 0);
            else if (key == 37 || key == 'A' || key == 'a') moveCampaignCursor(0, -1);
            else if (key == 39 || key == 'D' || key == 'd') moveCampaignCursor(0, 1);
            else if (key == 38 || key == 'W' || key == 'w') moveCampaignCursor(-1, 0);
            else if (key == 40 || key == 'S' || key == 's') moveCampaignCursor(1, 0);
            else if (key == 13 || key == 32) confirmCampaignSelection(storyMissionCursor);
        } else if (state == STATE_MISSION_COMPLETE) {
            if (key == 27) { storyMissionCursor = selectedStoryMission; state = STATE_MISSION_SELECT; }
            else if (key == 13 || key == 32) {
                int next = selectedStoryMission;
                for (int i = 0; i < CAMPAIGN_MISSION_COUNT; ++i) {
                    if (isCampaignMissionUnlocked(i) && !isCampaignMissionCompleted(i)) { next = i; break; }
                }
                if (next != selectedStoryMission && isCampaignMissionUnlocked(next)) launchStoryMission(next);
                else startSelectedRun();
            }
        } else if (state == STATE_BOSS_MUSIC_TEST) {
            if (key == 27) { state = STATE_MENU; sound.musicMode.store(5, std::memory_order_relaxed); }
            else if (key >= '1' && key <= '4') {
                const int type = key - '1';
                monsters.erase(std::remove_if(monsters.begin(), monsters.end(), [](const Monster& m) { return m.isBoss; }), monsters.end());
                spawnMonster(type, false, true, type);
                if (!monsters.empty()) {
                    Monster& boss = monsters.back();
                    boss.x = playerX + 260.0f; boss.y = playerY;
                    boss.vx = boss.vy = 0.0f; boss.speed = 0.0f;
                    boss.abilityTimer = boss.shootTimer = 9999.0f;
                    boss.hp = boss.maxHp;
                }
                bossTestMusicMode = 2;
                sound.musicBossType.store(type, std::memory_order_relaxed);
                sound.musicMode.store(2, std::memory_order_relaxed);
                sound.requestImmediateMusicChange(2, 0);
            } else if (key == 'H' || key == 'h') {
                bossTestMusicMode = 1;
                sound.musicMode.store(1,std::memory_order_relaxed);
                sound.requestImmediateMusicChange(1, 0);
            } else if (key == 'B' || key == 'b') {
                bossTestMusicMode = 2;
                sound.musicMode.store(2,std::memory_order_relaxed);
                sound.requestImmediateMusicChange(2, 0);
            }
        } else if (state == STATE_PLAYING) {
            if (key == 32) triggerDash(); // Space
            else if (key == 'Q' || key == 'q') triggerUltimate();
            else if (key == 27 || key == 'P' || key == 'p') state = STATE_PAUSED;
        } else if (state == STATE_LEVELUP) {
            if (key >= '1' && key <= '4') {
                int idx = key - '1';
                sound.play(SFX_CLICK, 0.02f);
                applyUpgrade(upgradeChoices[idx]);
                sound.playUpgradeCue();
                state = STATE_PLAYING;
                restoreGameplayMusicImmediately();
            } else if ((key == 'R' || key == 'r') && !upgradeRerollUsed) {
                generateUpgradeChoices();
                upgradeRerollUsed = true;
                sound.play(SFX_CLICK, 0.02f);
            }
        } else if (state == STATE_CHEST) {
            if (key == 32 || key == 13 || key == 27) finishTreasureChest();
        } else if (state == STATE_PAUSED) {
            if (key == 27 || key == 'P' || key == 'p') state = STATE_PLAYING;
            else if (key == 'O' || key == 'o') { settingsReturnState = STATE_PAUSED; state = STATE_SETTINGS; }
        } else if (state == STATE_SETTINGS) {
            if (key >= '1' && key <= '8') toggleOption(key - '0');
            else if (key == 9) settingsVolumeFocus = (settingsVolumeFocus + 1) % 2;
            else if (key == 37 || key == 39) adjustSoundVolume(key == 37 ? -1 : 1);
            else if (key == 27) state = settingsReturnState;
        } else if (state == STATE_HALL) {
            if (key == 9) hallPage = 1 - hallPage;
            else if (key == 27) state = STATE_MENU;
        } else if (state == STATE_SHOP) {
            if (key == 27) state = STATE_MENU;
        } else if (state == STATE_HELP) {
            if (helpPage == 1) {
                const int maxScroll = std::max(0, glossaryRowCount() - (SCREEN_W < SCREEN_H ? 5 : 4));
                if (key == 27 || key == 'G' || key == 'g') { helpPage = 0; }
                else if (key == 38 || key == 'W' || key == 'w') glossaryScroll = std::max(0, glossaryScroll - 1);
                else if (key == 40 || key == 'S' || key == 's') glossaryScroll = std::min(maxScroll, glossaryScroll + 1);
                else if (key == 33) glossaryScroll = std::max(0, glossaryScroll - 4);
                else if (key == 34) glossaryScroll = std::min(maxScroll, glossaryScroll + 4);
                return;
            }
            if (key == 'G' || key == 'g') { helpPage = 1; glossaryScroll = 0; return; }
            const int maxScroll = std::max(0, (ITEM_COUNT + 1) / 2 - 4);
            if (key == 27) state = STATE_MENU;
            else if (key == 38 || key == 'W' || key == 'w') arsenalScrollRow = std::max(0, arsenalScrollRow - 1);
            else if (key == 40 || key == 'S' || key == 's') arsenalScrollRow = std::min(maxScroll, arsenalScrollRow + 1);
            else if (key == 33) arsenalScrollRow = std::max(0, arsenalScrollRow - 4);
            else if (key == 34) arsenalScrollRow = std::min(maxScroll, arsenalScrollRow + 4);
            else if (key == 36) arsenalScrollRow = 0;
            else if (key == 35) arsenalScrollRow = maxScroll;
        } else if (state == STATE_EXTRACTION || state == STATE_CAMPAIGN_END) {
            if (key == 'E' || key == 'e') { gameMode = MODE_ENDLESS; startSelectedRun(); }
            else if (key == 13 || key == 32 || key == 27) state = STATE_MENU;
        } else if (state == STATE_GAMEOVER) {
            if (key == 13 || key == 32) {
                startSelectedRun();
            } else if (key == 27) {
                state = STATE_MENU;
            }
        }
    }

    void renderBossMusicTestArena(Renderer& r) {
        const float camX = playerX - SCREEN_W * 0.5f;
        const float camY = playerY - SCREEN_H * 0.5f;
        renderWorld(r, camX, camY);
        renderHUD(r, camX, camY);
        r.fillRoundedRect(250, 48, 780, 78, 10, 0xEF100B1C);
        r.drawRoundedRect(250, 48, 780, 78, 10, 0xFFFFD878, 2);
        r.drawTextCentered(FONT_MED, SCREEN_W / 2, 57, "АРЕНА ПРОСЛУШИВАНИЯ БОССА", 0xFFFFE078);
        r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, 86, "1–4: босс/тема   •   H: орда   •   ESC: меню", 0xFFE4D8C8);
        if (sound.externalMusicActive()) {
            // Bottom edge is free here; the panel above already sits over the boss phase bar.
            r.fillRoundedRect(SCREEN_W / 2 - 330, SCREEN_H - 92, 660, 26, 7, 0xEF100B1C);
            r.drawRoundedRect(SCREEN_W / 2 - 330, SCREEN_H - 92, 660, 26, 7, 0xFFB78AFF, 1);
            r.drawTextCentered(FONT_SMALL, SCREEN_W / 2, SCREEN_H - 86, "ИГРАЕТ ТРЕК ИЗ audio/  •  ВСТРОЕННАЯ ТЕМА В РЕЗЕРВЕ", 0xFFB78AFF);
        }
    }

    void renderMusicTempoEffects(Renderer& r) {
        // A screen-space tempo aura gives each crowd-driven music speed a distinct, readable identity.
        static const uint32_t tempoColors[4] = {0xFF9C8BFF, 0xFF45DDF1, 0xFFFFB64A, 0xFFFF4D78};
        const int tier = std::max(0, std::min(3, musicCrowdTempoTier));
        const uint32_t color = tempoColors[tier];
        const float pulse = 0.5f + 0.5f * sinf(globalTime * (0.85f + tier * 0.12f));
        const int bandAlpha = 11 + tier * 4 + (int)(pulse * 5.0f);
        // Soft colored edge glow washes the whole viewport without tinting the HUD or obscuring combat.
        r.fillRect(0, 0, SCREEN_W, 12, color, bandAlpha);
        r.fillRect(0, SCREEN_H - 12, SCREEN_W, 12, color, bandAlpha);
        r.fillRect(0, 12, 10, SCREEN_H - 24, color, bandAlpha);
        r.fillRect(SCREEN_W - 10, 12, 10, SCREEN_H - 24, color, bandAlpha);
        r.drawRect(5, 5, SCREEN_W - 10, SCREEN_H - 10, color, tier >= 2 ? 3 : 2,
                   26 + tier * 7 + (int)(pulse * 11.0f));

        // Faster tiers add more animated concentric wavefronts, pulsing gently rather than flashing.
        const int waveCount = tier + 1;
        for (int i = 0; i < waveCount; ++i) {
            const float wave = sinf(globalTime * (0.65f + tier * 0.16f) + i * 1.7f);
            const int radius = 210 + i * 92 + (int)(wave * (7 + tier * 3));
            r.drawCircleRing(SCREEN_W / 2, SCREEN_H / 2, radius, 2, color,
                             18 + tier * 6 + (int)(pulse * 5.0f), true);
        }

        // Small moving corner streaks make the tempo steps visibly denser as the score accelerates.
        const int streakCount = 1 + tier * 2;
        const float travel = fmodf(globalTime * (95.0f + tier * 105.0f), (float)SCREEN_W + 72.0f);
        for (int i = 0; i < streakCount; ++i) {
            const int x = (int)travel - 36 + i * (SCREEN_W / streakCount);
            const int y = (int)fmodf(globalTime * (70.0f + tier * 80.0f) + i * 113.0f, (float)SCREEN_H);
            const int alpha = 22 + tier * 7;
            r.drawLine(x, 10, x + 42, 10, color, 2, alpha, true);
            r.drawLine(x, SCREEN_H - 11, x + 42, SCREEN_H - 11, color, 2, alpha, true);
            r.drawLine(9, y, 9, y + 34, color, 2, alpha, true);
            r.drawLine(SCREEN_W - 10, y, SCREEN_W - 10, y + 34, color, 2, alpha, true);
        }
    }

    // The phase change is announced in the bottom-left stack (see drawCombatNotices), so this
    // hook stays empty on purpose: nothing is drawn in the middle of the battlefield any more.
    void renderBossPhaseTransition(Renderer& r) { (void)r; }

    void render(Renderer& r) {
        r.skillIconStyle = std::max(0, std::min(2, options.skillIconStyle));
        if (state == STATE_PATCH_NOTES) {
            renderPatchNotes(r);
        } else if (state == STATE_CUTSCENE) {
            renderCutscene(r);
        } else if (state == STATE_MENU) {
            renderMainMenu(r);
        } else if (state == STATE_HERO_SELECT) {
            renderHeroSelectMenu(r);
        } else if (state == STATE_MISSION_SELECT) {
            renderMissionSelectMenu(r);
        } else if (state == STATE_MISSION_COMPLETE) {
            renderMissionComplete(r);
        } else if (state == STATE_BOSS_MUSIC_TEST) {
            renderBossMusicTestArena(r);
        } else if (state == STATE_SHOP) {
            renderShopMenu(r);
        } else if (state == STATE_HELP) {
            renderHelpMenu(r);
        } else if (state == STATE_SETTINGS) {
            renderSettingsMenu(r);
        } else if (state == STATE_HALL) {
            renderHallMenu(r);
        } else if (state == STATE_EXTRACTION) {
            renderExtractionScreen(r);
        } else if (state == STATE_CAMPAIGN_END) {
            renderCampaignEnding(r);
        } else {
            float shakeX = 0.0f, shakeY = 0.0f;
            if (options.screenShakeEnabled && screenShake > 0.0f) {
                shakeX = (rand() % 16 - 8) * screenShake * 2.5f;
                shakeY = (rand() % 16 - 8) * screenShake * 2.5f;
            }
            float camX = playerX - SCREEN_W * 0.5f + shakeX;
            float camY = playerY - SCREEN_H * 0.5f + shakeY;

            renderWorld(r, camX, camY);
            renderMusicTempoEffects(r);
            renderHUD(r, camX, camY);
            if (state == STATE_PLAYING && bossPhaseEventTimer > 0.0f) renderBossPhaseTransition(r);

            if (state == STATE_DEATH) renderDeathModal(r);
            else if (state == STATE_LEVELUP) renderLevelUpModal(r);
            else if (state == STATE_CHEST) renderChestModal(r);
            else if (state == STATE_PAUSED) renderPauseModal(r);
            else if (state == STATE_GAMEOVER) renderGameOverModal(r);
        }

        if (cheatMenuOpen) renderCheatMenu(r);
        if (debugOverlay) drawDebugOverlay(r);

        // Custom glowing arcane cursor
        r.drawCircleRing(mouseX, mouseY, 8, 2, 0xFFFFE068, 220);
        r.fillCircle(mouseX, mouseY, 2, 0xFFFFFFFF);

        mouseClicked = false;
        mouseWheelDelta = 0;
    }
};
