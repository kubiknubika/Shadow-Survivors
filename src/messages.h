#pragma once
// Registry of every on-screen combat/story message.
//
// Rule of the project: a message that is shown to the player must exist here, with
// an explicit decision about whether it is voiced. That way a new banner cannot be
// added silently and forgotten by the announcer — the F3 overlay counts how many
// registry rows already have a recorded clip in audio/voice/, and the headless
// check fails when a row has no key, no note or no voice line.
//
// Each key is also the clip file name: audio/voice/<key>.mp3.
// `voiceLine` is the phrase recorded in that file, so the text and the audio can
// never drift apart.

#include <stdint.h>
#include <stdio.h>
#include <string>

enum GameMsgId : int {
    GMSG_BOSS_PHASE = 0,      // boss phase changes
    GMSG_BREAKTHROUGH_START,  // breakthrough (прорыв) begins
    GMSG_BREAKTHROUGH_HELD,   // breakthrough survived
    GMSG_EVOLUTION,           // weapon evolution / symbiosis
    GMSG_HOSTILE_HORDE,       // special hostile horde
    GMSG_MISSION_BRIEFING,    // story mission briefing
    GMSG_ENDLESS_START,       // endless run flavour line
    GMSG_WORLD_EVENT,         // random world event
    GMSG_SEAL_LORE,           // seal / lore line after a boss
    GMSG_CAMPAIGN_END,        // campaign finished
    GMSG_PORTAL_FOUND,        // echo portal appears
    GMSG_PORTAL_USED,         // portal taken / closed
    GMSG_RESONATOR_SPAWN,     // new resonator appears
    GMSG_RESONATOR_DESTROYED, // resonator shattered
    GMSG_TRIO_INTRO,          // final trio introduction
    GMSG_MISSION_OBJECTIVE,   // mission objective banner
    GMSG_BOSS_SUMMON,         // boss summon line
    GMSG_BOSS_ABILITY,        // boss ability line
    GMSG_PHOENIX_REVIVE,      // phoenix revive
    GMSG_PLAYER_DEATH,        // death line
    GMSG_BUILD_TIER_UP,       // build tier increased
    GMSG_BREAKTHROUGH_HELD2,  // breakthrough survived banner
    GMSG_BREAKTHROUGH_PRESSURE, // horde will not stop warning
    GMSG_BREAKTHROUGH_WARNING,  // "next level brings a breakthrough"
    GMSG_BOSS_WARNING,        // "страж приближается"
    GMSG_COIN_HINT,           // "pick up the coins the pack left"
    GMSG_VOICE_ON,            // confirmation after the player enables speech
    GMSG_EXPOSED_WEAK_POINT,  // exposure seal triggered (combat popup)
    GMSG_SUPERCRIT,           // super crit popup
    GMSG_SMART_KILL,          // smart enemy one-tap popup
    GMSG_MATRYOSHKA,          // matryoshka opens
    GMSG_COUNT
};

struct GameMsgDef {
    GameMsgId id;
    const char* key;       // stable identifier == clip file name in audio/voice/
    bool voiced;           // has a recorded phrase
    const char* voiceLine; // exactly what the recorded clip says ("" when silent)
    const char* note;      // why it is / is not voiced
};

// Every row must have a key and a note; `voiced` is the explicit decision.
static const GameMsgDef GAME_MESSAGES[GMSG_COUNT] = {
    {GMSG_BOSS_PHASE,         "boss_phase",         true,  "Новая фаза боя.", "смена фазы босса — важное событие боя"},
    {GMSG_BREAKTHROUGH_START, "breakthrough",       true,  "Особая орда. Прорыв!", "начало прорыва, меняет правила боя"},
    {GMSG_BREAKTHROUGH_HELD,  "breakthrough_held",  true,  "Прорыв отбит.", "прорыв отбит"},
    {GMSG_EVOLUTION,          "evolution",          true,  "Симбиоз пробуждён.", "симбиоз оружия — меняет сборку"},
    {GMSG_HOSTILE_HORDE,      "hostile_horde",      true,  "Особая орда идёт на тебя.", "особая орда"},
    {GMSG_MISSION_BRIEFING,   "mission_briefing",   true,  "Миссия начинается.", "вступление миссии"},
    {GMSG_ENDLESS_START,      "endless_start",      true,  "Бесконечная ночь.", "старт бесконечного режима"},
    {GMSG_WORLD_EVENT,        "world_event",        true,  "Тихая охота. Стая вызвана.", "событие мира"},
    {GMSG_SEAL_LORE,          "seal_lore",          true,  "Печать снята.", "строка лора после печати"},
    {GMSG_CAMPAIGN_END,       "campaign_end",       true,  "Кампания завершена.", "финал кампании"},
    {GMSG_PORTAL_FOUND,       "portal_found",       true,  "Найден эхо-портал.", "портал найден"},
    {GMSG_PORTAL_USED,        "portal_used",        true,  "Портал закрыт.", "портал использован"},
    {GMSG_RESONATOR_SPAWN,    "resonator_spawn",    true,  "Новый резонатор на поле.", "новый резонатор"},
    {GMSG_RESONATOR_DESTROYED, "resonator_destroyed", true, "Резонатор разрушен.", "резонатор уничтожен"},
    {GMSG_TRIO_INTRO,         "trio_intro",         true,  "Три эха защищают сердце архива.", "финальное трио"},
    {GMSG_MISSION_OBJECTIVE,  "mission_objective",  true,  "Цель обновлена.", "цель миссии"},
    {GMSG_BOSS_SUMMON,        "boss_summon",        true,  "Босс призывает помощь.", "босс призывает помощь"},
    {GMSG_BOSS_ABILITY,       "boss_ability",       true,  "Опасность. Уходи из зоны удара.", "способность босса"},
    {GMSG_PHOENIX_REVIVE,     "phoenix_revive",     true,  "Феникс возрождает тебя.", "возрождение феникса"},
    {GMSG_PLAYER_DEATH,       "player_death",       true,  "Твоя искра погасла.", "гибель героя"},
    {GMSG_BUILD_TIER_UP,      "build_tier_up",      true,  "Тир сборки повышен.", "повышение тира сборки"},
    {GMSG_BREAKTHROUGH_HELD2, "breakthrough_held_banner", true, "Прорыв отбит.", "баннер: прорыв отбит (тот же клип)"},
    {GMSG_BREAKTHROUGH_PRESSURE, "breakthrough_pressure", true, "Орда не остановится, пока ты не получишь удар.", "баннер: орда не остановится"},
    {GMSG_BREAKTHROUGH_WARNING,  "breakthrough_warning",  true, "Внимание, на следующем уровне прорыв.", "предупреждение о прорыве"},
    {GMSG_BOSS_WARNING,       "boss_warning",       true,  "Страж приближается.", "баннер: страж приближается"},
    {GMSG_COIN_HINT,          "coin_hint",          true,  "Подбери монеты, которые оставила стая.", "подсказка про монеты после стаи"},
    {GMSG_VOICE_ON,           "voice_on",           true,  "Озвучка сообщений включена.", "подтверждение включения озвучки"},
    {GMSG_EXPOSED_WEAK_POINT, "exposed_weak_point", false, "", "боевой попап на мобе: звучал бы слишком часто"},
    {GMSG_SUPERCRIT,          "supercrit",          false, "", "числовой попап урона, не сообщение"},
    {GMSG_SMART_KILL,         "smart_kill",         false, "", "попап добивания, звучит пачками"},
    {GMSG_MATRYOSHKA,         "matryoshka",         false, "", "попап раскрытия матрёшки"},
};

struct MessageRegistry {
    int uses[GMSG_COUNT] = {};
    int voicedSent = 0;
    int voicedSuppressed = 0; // speech is off in the settings
    int missingClips = 0;     // voiced rows whose audio/voice/<key>.mp3 is absent

    void note(GameMsgId id) {
        if (id >= 0 && id < GMSG_COUNT) ++uses[(int)id];
    }
    int usedKinds() const {
        int n = 0;
        for (int i = 0; i < GMSG_COUNT; ++i) if (uses[i] > 0) ++n;
        return n;
    }
    static int voicedDefs() {
        int n = 0;
        for (int i = 0; i < GMSG_COUNT; ++i) if (GAME_MESSAGES[i].voiced) ++n;
        return n;
    }
    // Rows without a key, a note or (when voiced) a spoken line mean somebody added a
    // message and skipped the registry discipline. Must stay 0.
    static int invalidRows() {
        int n = 0;
        for (int i = 0; i < GMSG_COUNT; ++i) {
            const GameMsgDef& def = GAME_MESSAGES[i];
            if (def.id != (GameMsgId)i) ++n;
            if (!def.key || !def.key[0]) ++n;
            if (!def.note || !def.note[0]) ++n;
            if (def.voiced && (!def.voiceLine || !def.voiceLine[0])) ++n;
        }
        return n;
    }
    static bool voiced(GameMsgId id) {
        return id >= 0 && id < GMSG_COUNT && GAME_MESSAGES[(int)id].voiced;
    }
    static const char* key(GameMsgId id) {
        return (id >= 0 && id < GMSG_COUNT) ? GAME_MESSAGES[(int)id].key : "?";
    }
    static const char* voiceLine(GameMsgId id) {
        return (id >= 0 && id < GMSG_COUNT) ? GAME_MESSAGES[(int)id].voiceLine : "";
    }
    // Compact enough for the F3 column, complete enough to spot a forgotten message.
    std::string summary() const {
        char buf[128];
        snprintf(buf, sizeof(buf), "%d/%d • ОЗВ. %d • НЕТ ФАЙЛА %d • БИТЫХ %d",
                 usedKinds(), GMSG_COUNT, voicedSent, missingClips, invalidRows());
        return std::string(buf);
    }
};
