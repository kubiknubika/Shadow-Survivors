#pragma once
// Registry of every on-screen combat/story message.
//
// Rule of the project: a message that is shown to the player must exist here, with
// an explicit decision about whether it is voiced. That way a new banner cannot be
// added silently and forgotten by the announcer — the debug overlay (F3) and the
// headless self-check both report registry rows that are missing a key/note, and
// the message panel only ever displays strings that came through announce().

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
    GMSG_SKY_EVENT,           // random sky event
    GMSG_WORLD_EVENT,         // random world event
    GMSG_SEAL_LORE,           // seal / lore line after a boss
    GMSG_CAMPAIGN_END,        // campaign finished
    GMSG_PORTAL_FOUND,        // echo portal appears
    GMSG_PORTAL_USED,         // portal taken / closed
    GMSG_RESONATOR_SPAWN,     // new resonator
    GMSG_TRIO_INTRO,          // final trio introduction
    GMSG_MISSION_OBJECTIVE,   // mission objective banner
    GMSG_BOSS_SUMMON,         // boss summon line
    GMSG_BOSS_ABILITY,        // boss ability line
    GMSG_PHOENIX_REVIVE,      // phoenix revive
    GMSG_PLAYER_DEATH,        // death line
    GMSG_BUILD_TIER_UP,       // build tier increased
    GMSG_EXPOSED_WEAK_POINT,  // exposure seal triggered (combat popup)
    GMSG_SUPERCRIT,           // super crit popup
    GMSG_SMART_KILL,          // smart enemy one-tap popup
    GMSG_MATRYOSHKA,          // matryoshka opens
    GMSG_BREAKTHROUGH_HELD2,  // breakthrough survived banner
    GMSG_BREAKTHROUGH_PRESSURE, // horde will not stop warning
    GMSG_BREAKTHROUGH_WARNING,  // "next level brings a breakthrough"
    GMSG_BOSS_WARNING,        // "страж приближается"
    GMSG_COIN_HINT,           // "pick up the coins the pack left"
    GMSG_COUNT
};

struct GameMsgDef {
    GameMsgId id;
    const char* key;    // stable identifier for logs, tests and the debug overlay
    bool voiced;        // sent to the speech synthesizer
    const char* note;   // why it is / is not voiced
};

// Every row must have a key and a note; `voiced` is the explicit decision.
static const GameMsgDef GAME_MESSAGES[GMSG_COUNT] = {
    {GMSG_BOSS_PHASE,         "boss_phase",         true,  "смена фазы босса — важное событие боя"},
    {GMSG_BREAKTHROUGH_START, "breakthrough",       true,  "начало прорыва, меняет правила боя"},
    {GMSG_BREAKTHROUGH_HELD,  "breakthrough_held",  true,  "прорыв отбит"},
    {GMSG_EVOLUTION,          "evolution",          true,  "симбиоз оружия — меняет сборку"},
    {GMSG_HOSTILE_HORDE,      "hostile_horde",      true,  "особая орда"},
    {GMSG_MISSION_BRIEFING,   "mission_briefing",   true,  "вступление миссии"},
    {GMSG_ENDLESS_START,      "endless_start",      true,  "старт бесконечного режима"},
    {GMSG_SKY_EVENT,          "sky_event",          true,  "небесное событие"},
    {GMSG_WORLD_EVENT,        "world_event",        true,  "событие мира"},
    {GMSG_SEAL_LORE,          "seal_lore",          true,  "строка лора после печати"},
    {GMSG_CAMPAIGN_END,       "campaign_end",       true,  "финал кампании"},
    {GMSG_PORTAL_FOUND,       "portal_found",       true,  "портал найден"},
    {GMSG_PORTAL_USED,        "portal_used",        true,  "портал использован"},
    {GMSG_RESONATOR_SPAWN,    "resonator_spawn",    true,  "новый резонатор"},
    {GMSG_TRIO_INTRO,         "trio_intro",         true,  "финальное трио"},
    {GMSG_MISSION_OBJECTIVE,  "mission_objective",  true,  "цель миссии"},
    {GMSG_BOSS_SUMMON,        "boss_summon",        true,  "босс призывает помощь"},
    {GMSG_BOSS_ABILITY,       "boss_ability",       true,  "способность босса"},
    {GMSG_PHOENIX_REVIVE,     "phoenix_revive",     true,  "возрождение феникса"},
    {GMSG_PLAYER_DEATH,       "player_death",       true,  "гибель героя"},
    {GMSG_BUILD_TIER_UP,      "build_tier_up",      true,  "повышение тира сборки"},
    {GMSG_EXPOSED_WEAK_POINT, "exposed_weak_point", false, "боевой попап на мобе: звучит слишком часто"},
    {GMSG_SUPERCRIT,          "supercrit",          false, "числовой попап урона, не сообщение"},
    {GMSG_SMART_KILL,         "smart_kill",         false, "попап добивания, звучит пачками"},
    {GMSG_MATRYOSHKA,         "matryoshka",         false, "попап раскрытия матрёшки"},
    {GMSG_BREAKTHROUGH_HELD2, "breakthrough_held_banner", true, "баннер: прорыв отбит"},
    {GMSG_BREAKTHROUGH_PRESSURE, "breakthrough_pressure", true, "баннер: орда не остановится"},
    {GMSG_BREAKTHROUGH_WARNING,  "breakthrough_warning",  true, "предупреждение о прорыве"},
    {GMSG_BOSS_WARNING,       "boss_warning",       true,  "баннер: страж приближается"},
    {GMSG_COIN_HINT,          "coin_hint",          true,  "подсказка про монеты после стаи"},
};

struct MessageRegistry {
    int uses[GMSG_COUNT] = {};
    int voicedSent = 0;
    int voicedSuppressed = 0; // speech is off in the settings

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
    // Rows without a key or a note mean somebody added a message and skipped the
    // registry discipline. Must stay 0; F3 and the headless check surface it.
    static int invalidRows() {
        int n = 0;
        for (int i = 0; i < GMSG_COUNT; ++i) {
            const GameMsgDef& def = GAME_MESSAGES[i];
            if (def.id != (GameMsgId)i) ++n;
            if (!def.key || !def.key[0]) ++n;
            if (!def.note || !def.note[0]) ++n;
        }
        return n;
    }
    static bool voiced(GameMsgId id) {
        return id >= 0 && id < GMSG_COUNT && GAME_MESSAGES[(int)id].voiced;
    }
    static const char* key(GameMsgId id) {
        return (id >= 0 && id < GMSG_COUNT) ? GAME_MESSAGES[(int)id].key : "?";
    }
    // Compact enough for the F3 column, complete enough to spot a forgotten message.
    std::string summary() const {
        char buf[128];
        snprintf(buf, sizeof(buf), "%d/%d • ОЗВ. %d • ВЫКЛ. %d • БИТЫХ %d",
                 usedKinds(), GMSG_COUNT, voicedSent, voicedSuppressed, invalidRows());
        return std::string(buf);
    }
};
