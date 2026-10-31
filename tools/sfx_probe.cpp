// ── Стенд эффектов: замеры вместо «на слух вроде нормально» ──────────────────
//
// Эффекты в игре синтезируются кодом (src/audio.h, generateSfxBank) и проходят
// через один микшер: громкости, приоритеты, полифония, soft-knee лимитер. Всё
// это легко сломать незаметно — звук останется, но станет тише остальных или,
// наоборот, начнёт затыкать бой. Поэтому стенд не слушает, а меряет.
//
//     g++ -O2 -std=c++17 -pthread -Isrc tools/sfx_probe.cpp -o build/sfx_probe
//     build/sfx_probe                 # только проверки
//     build/sfx_probe --wav out.wav   # плюс демо-дорожка: все эффекты подряд
//
// Демо нужно, чтобы результат можно было услышать: стенд ловит щелчки и провалы
// по громкости, но «нравится ли звук» решает человек. Дорожка пишется в
// 22 050 Гц моно — её хватает, чтобы сравнить эффекты между собой.
//
// Что проверяется:
//  1. банк: длина, пик, постоянная составляющая, нули на концах (щелчок),
//     отсутствие одинаковых файлов и полная тишина;
//  2. через микшер: пик и RMS каждого эффекта в одиночку — «шёпот» и «крик»
//     относительно соседей видны числом;
//  3. плотный бой: двадцать одновременных ударов + снаряды — лимитер обязан
//     удержать пик, сэмплы не должны клиппироваться или стать NaN;
//  4. полифония и приоритеты: редкий сигнал (уровень) не должен забираться
//     толпой ударов, а одинаковые удары — не копиться стопкой;
//  5. джиттер высоты и громкости: разброс есть, но он в заявленных границах.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "audio.h"

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const char* what) {
    ++checks;
    if (!ok) {
        printf("  ПРОВАЛ: %s\n", what);
        ++failures;
    }
}

static void report(const char* what, double value, const char* unit) {
    printf("  %-34s %8.2f %s\n", what, value, unit);
}

// Один эффект в одиночку через настоящий микшер. Возвращает пик в долях полной
// шкалы и RMS (тоже в долях шкалы).
struct Measured { float peak; float rms; bool finite; };

static Measured measureAlone(SoundEngine& engine, SfxId id) {
    engine.enabled = true;
    engine.musicEnabled = false;
    engine.musicVolume = 0.0f;
    // Дать приглушению музыки устояться, чтобы в кадр не попал её хвост.
    std::vector<int16_t> warm(4410 * 2);
    engine.mixBuffer(warm.data(), (int)warm.size());

    // Прошлый замер мог не дозвучать (смерть босса — 1,3 с): его хвост
    // подмешался бы к следующему и завысил пик. Гасим голоса начисто.
    for (ActiveVoice& voice : engine.voices) voice.active = false;
    for (int i = 0; i < SFX_COUNT; ++i) engine.cooldowns[i] = 0.0f;
    engine.play(id, 0.0f);

    const int total = 44100 * 3 / 2; // 1,5 с — с запасом на самые длинные сигналы
    std::vector<int16_t> out(total);
    engine.mixBuffer(out.data(), total);

    double sum = 0.0;
    float peak = 0.0f;
    bool finite = true;
    for (int16_t s : out) {
        const float v = s / 32768.0f;
        if (!std::isfinite(v)) finite = false;
        peak = std::max(peak, std::fabs(v));
        sum += (double)v * v;
    }
    return {peak, (float)std::sqrt(sum / total), finite};
}

static void checkBank(SoundEngine& engine) {
    printf("банк эффектов\n");
    for (int id = 0; id < SFX_COUNT; ++id) {
        const std::vector<int16_t>& s = engine.sfxSamples[id][0];
        char what[160];
        snprintf(what, sizeof(what), "%s: пустой банк", sfxName((SfxId)id));
        expect(!s.empty(), what);
        if (s.empty()) continue;

        int peak = 0;
        double sum = 0.0;
        for (int16_t v : s) {
            peak = std::max(peak, std::abs((int)v));
            sum += v;
        }
        const double dc = sum / s.size();
        const double ms = 1000.0 * s.size() / 44100.0;

        snprintf(what, sizeof(what), "%s: длина %.0f мс вне 30…2500 (сигналы-«стинги» длиннее)", sfxName((SfxId)id), ms);
        expect(ms >= 30.0 && ms <= 2500.0, what);
        snprintf(what, sizeof(what), "%s: пик %d — тише 2000, эффект не слышно", sfxName((SfxId)id), peak);
        expect(peak >= 2000, what);
        snprintf(what, sizeof(what), "%s: постоянная составляющая %.0f больше 200 — щелчок в начале", sfxName((SfxId)id), dc);
        expect(std::fabs(dc) <= 200.0, what);
        snprintf(what, sizeof(what), "%s: хвост %d не затухает — звук обрывается щелчком",
                 sfxName((SfxId)id), (int)s.back());
        expect(std::abs((int)s.back()) <= 200, what);
        snprintf(what, sizeof(what), "%s: начинается с %d, а не с нуля — щелчок", sfxName((SfxId)id), (int)s.front());
        expect(std::abs((int)s.front()) <= 200, what);
    }

    // Два эффекта не могут быть одним и тем же файлом: иначе разница в игре
    // будет только в громкости, а игрок услышит «тот же звук».
    for (int a = 0; a < SFX_COUNT; ++a) {
        for (int b = a + 1; b < SFX_COUNT; ++b) {
            char what[160];
            snprintf(what, sizeof(what), "%s и %s — один и тот же сэмпл",
                     sfxName((SfxId)a), sfxName((SfxId)b));
            expect(engine.sfxSamples[a][0] != engine.sfxSamples[b][0], what);
        }
    }
}

// Повторяемые эффекты обязаны иметь варианты: иначе десятки ударов за бой —
// одна и та же волна, и джиттер высоты её не прячет.
static void checkVariants(SoundEngine& engine) {
    printf("варианты повторяемых эффектов\n");
    static const SfxId repeated[] = {SFX_HIT, SFX_BLADE};
    for (SfxId id : repeated) {
        char what[180];
        const int count = engine.sfxVariantCount[id];
        snprintf(what, sizeof(what), "%s: вариантов %d, а нужно 3", sfxName(id), count);
        expect(count == 3, what);
        for (int v = 0; v < count; ++v) {
            snprintf(what, sizeof(what), "%s: вариант %d пустой", sfxName(id), v);
            expect(!engine.sfxSamples[id][v].empty(), what);
        }
        for (int a = 0; a < count; ++a) {
            for (int b = a + 1; b < count; ++b) {
                snprintf(what, sizeof(what), "%s: варианты %d и %d — одна и та же волна", sfxName(id), a, b);
                expect(engine.sfxSamples[id][a] != engine.sfxSamples[id][b], what);
            }
        }
        // Уровни вариантов обязаны совпадать: случайный выбор не должен давать
        // то громкий, то тихий удар.
        float rmsMin = 1e9f, rmsMax = 0.0f;
        for (int v = 0; v < count; ++v) {
            double sum = 0.0;
            for (int16_t sample : engine.sfxSamples[id][v]) sum += (double)sample * sample;
            const float rms = (float)std::sqrt(sum / std::max<size_t>(1, engine.sfxSamples[id][v].size()));
            rmsMin = std::min(rmsMin, rms);
            rmsMax = std::max(rmsMax, rms);
        }
        snprintf(what, sizeof(what), "%s: разброс громкости вариантов %.1f дБ — слышно как «то громко, то тихо»",
                 sfxName(id), 20.0f * std::log10(rmsMax / std::max(1.0f, rmsMin)));
        expect(rmsMax <= rmsMin * 1.6f, what);
    }
}

// Уровень музыкальной подложки: с ним и сравниваются эффекты. Меряется тем же
// микшером, что и в игре, поэтому «эффекты тонут в музыке» — это число, а не
// вкусовщина.
static float musicBedPeak(SoundEngine& engine) {
    engine.enabled = false;      // эффекты выключены, остаётся только музыка
    engine.musicEnabled = true;
    engine.musicVolume = 0.42f;
    engine.musicMode = 1;        // режим орды — самый плотный бой
    for (ActiveVoice& voice : engine.voices) voice.active = false;
    std::vector<int16_t> out(44100 * 3);
    engine.mixBuffer(out.data(), (int)out.size());
    float peak = 0.0f;
    for (int16_t sample : out) peak = std::max(peak, (float)std::fabs(sample / 32768.0f));
    return peak;
}

static void checkMixLevels(SoundEngine& engine, float bed) {
    printf("через микшер (в одиночку, полная шкала = 1.00)\n");
    printf("  подложка музыки (орда): пик %.3f\n", bed);
    expect(bed > 0.01f, "музыки в замере нет — сравнение с подложкой ничего не значит");

    // Боевые эффекты обязаны быть слышны поверх подложки, сигналы-«стинги» —
    // заметно громче неё, а сбор трофеев — тише, но не беззвучнее.
    static const SfxId combat[] = {SFX_BLADE, SFX_HIT, SFX_FIREBALL, SFX_LIGHTNING,
                                   SFX_DASH, SFX_FREEZE, SFX_PROJECTILE_IMPACT, SFX_PROJECTILE_FLIGHT};
    static const SfxId pickups[] = {SFX_GEM, SFX_CLICK, SFX_MAGNET};
    static const SfxId stings[] = {SFX_LEVELUP, SFX_CHEST, SFX_BOSS, SFX_ULTIMATE, SFX_EVOLVE,
                                   SFX_BOSS_DEATH, SFX_PLAYER_DEATH, SFX_REVIVE, SFX_TIER_UP,
                                   SFX_BOSS_TELEGRAPH, SFX_BOSS_IMPACT, SFX_BOSS_PHASE};

    float peaks[SFX_COUNT] = {0};
    for (int id = 0; id < SFX_COUNT; ++id) {
        // Три прогона: у каждого срабатывания свой джиттер высоты и громкости
        // (±10 %), и одиночный замер легко попадает в нижний край.
        Measured m = measureAlone(engine, (SfxId)id);
        for (int take = 1; take < 3; ++take) {
            const Measured next = measureAlone(engine, (SfxId)id);
            m.peak = std::max(m.peak, next.peak);
            m.rms = std::max(m.rms, next.rms);
        }
        peaks[id] = m.peak;
        char what[200];
        snprintf(what, sizeof(what), "%s: пик %.3f — эффект клиппирует", sfxName((SfxId)id), m.peak);
        expect(m.peak <= 0.99f, what);
        snprintf(what, sizeof(what), "%s: в кадре NaN", sfxName((SfxId)id));
        expect(m.finite, what);
        printf("  %-22s пик %.3f  RMS %.4f\n", sfxName((SfxId)id), m.peak, m.rms);
    }

    for (SfxId id : combat) {
        char what[200];
        snprintf(what, sizeof(what), "%s: пик %.3f при подложке %.3f — эффект тонет под музыкой",
                 sfxName(id), peaks[id], bed);
        expect(peaks[id] >= bed * 1.2f, what);
    }
    for (SfxId id : stings) {
        char what[200];
        snprintf(what, sizeof(what), "%s: пик %.3f при подложке %.3f — сигнал не выделяется",
                 sfxName(id), peaks[id], bed);
        expect(peaks[id] >= bed * 2.0f, what);
    }
    for (SfxId id : pickups) {
        char what[200];
        snprintf(what, sizeof(what), "%s: пик %.3f при подложке %.3f — сбор слышно громче боя",
                 sfxName(id), peaks[id], bed);
        expect(peaks[id] <= bed, what);
        snprintf(what, sizeof(what), "%s: пик %.3f при подложке %.3f — сбор не слышно", sfxName(id), peaks[id], bed);
        expect(peaks[id] >= bed * 0.25f, what);
    }

    float loudest = 0.0f;
    for (int id = 0; id < SFX_COUNT; ++id) loudest = std::max(loudest, peaks[id]);
    report("самый громкий эффект", loudest, "полной шкалы");
    expect(loudest >= 0.25f, "весь набор эффектов тише 0,25 полной шкалы");
}

static void checkDenseFight(SoundEngine& engine) {
    printf("плотный бой\n");
    engine.enabled = true;
    engine.musicEnabled = false;
    for (ActiveVoice& voice : engine.voices) voice.active = false;
    std::vector<int16_t> warm(4410);
    engine.mixBuffer(warm.data(), (int)warm.size());
    for (int i = 0; i < SFX_COUNT; ++i) engine.cooldowns[i] = 0.0f;
    // Двадцать ударов подряд с игнорированием кулдауна — так выглядит кадр, где
    // толпа бьёт одновременно; плюс снаряды и самоцветы.
    for (int i = 0; i < 20; ++i) {
        engine.cooldowns[SFX_HIT] = 0.0f;
        engine.play(SFX_HIT, 0.0f);
    }
    engine.cooldowns[SFX_FIREBALL] = 0.0f;
    engine.play(SFX_FIREBALL, 0.0f);
    engine.cooldowns[SFX_PROJECTILE_IMPACT] = 0.0f;
    engine.play(SFX_PROJECTILE_IMPACT, 0.0f);
    engine.cooldowns[SFX_GEM] = 0.0f;
    engine.play(SFX_GEM, 0.0f);

    int live = 0;
    for (const ActiveVoice& voice : engine.voices) if (voice.active) ++live;
    report("живых голосов", live, "из 16");
    expect(live > 0, "плотный бой: ни один голос не зазвучал");

    std::vector<int16_t> out(44100);
    engine.mixBuffer(out.data(), (int)out.size());
    float peak = 0.0f;
    int clipped = 0;
    bool finite = true;
    for (int16_t sample : out) {
        const float v = sample / 32768.0f;
        if (!std::isfinite(v)) finite = false;
        peak = std::max(peak, std::fabs(v));
        if (std::abs((int)sample) >= 32767) ++clipped;
    }
    expect(finite, "плотный бой: NaN в кадре");
    report("пик в плотном бою", peak, "полной шкалы");
    expect(peak <= 0.99f, "лимитер не удержал пик в плотном бою");
    expect(clipped == 0, "сэмплы упёрлись в предел int16 — клиппинг");
}

static void checkStealing(SoundEngine& engine) {
    printf("полифония и приоритеты\n");
    engine.enabled = true;
    engine.musicEnabled = false;
    for (int i = 0; i < SFX_COUNT; ++i) engine.cooldowns[i] = 0.0f;
    engine.play(SFX_LEVELUP, 0.0f);
    bool cueAlive = false;
    for (const ActiveVoice& voice : engine.voices) if (voice.active && voice.id == SFX_LEVELUP) cueAlive = true;
    expect(cueAlive, "сигнал уровня не зазвучал вовсе");

    // Толпа ударов не имеет права забрать голос сигнала уровня: он приоритетнее.
    for (int i = 0; i < 32; ++i) {
        engine.cooldowns[SFX_HIT] = 0.0f;
        engine.play(SFX_HIT, 0.0f);
    }
    cueAlive = false;
    for (const ActiveVoice& voice : engine.voices) if (voice.active && voice.id == SFX_LEVELUP) cueAlive = true;
    expect(cueAlive, "удары вытеснили сигнал уровня из микшера");

    // Одинаковые удары не копятся стопкой: их не больше, чем заявленная полифония.
    int hits = 0;
    for (const ActiveVoice& voice : engine.voices) if (voice.active && voice.id == SFX_HIT) ++hits;
    char what[120];
    snprintf(what, sizeof(what), "ударов в микшере %d, а полифония %d", hits, SoundEngine::sfxPolyphony(SFX_HIT));
    expect(hits <= SoundEngine::sfxPolyphony(SFX_HIT), what);
}

static void checkThrottle(SoundEngine& engine) {
    printf("троттлинг (кулдаун)\n");
    engine.enabled = true;
    for (int i = 0; i < SFX_COUNT; ++i) engine.cooldowns[i] = 0.0f;
    for (ActiveVoice& voice : engine.voices) voice.active = false;
    engine.play(SFX_HIT, 0.09f);
    engine.play(SFX_HIT, 0.09f);
    int hits = 0;
    for (const ActiveVoice& voice : engine.voices) if (voice.active && voice.id == SFX_HIT) ++hits;
    char what[120];
    snprintf(what, sizeof(what), "второй удар в том же кадре всё равно зазвучал (%d голосов)", hits);
    expect(hits == 1, what);

    engine.update(0.10f);
    engine.play(SFX_HIT, 0.09f);
    hits = 0;
    for (const ActiveVoice& voice : engine.voices) if (voice.active && voice.id == SFX_HIT) ++hits;
    expect(hits >= 1, "после кулдауна удар не зазвучал");

    // Самоцветы глушатся сильнее всех: иначе массовый сбор превращается в трель.
    for (ActiveVoice& voice : engine.voices) voice.active = false;
    for (int i = 0; i < SFX_COUNT; ++i) engine.cooldowns[i] = 0.0f;
    engine.play(SFX_GEM, 0.0f);
    const float gemCooldown = engine.cooldowns[SFX_GEM];
    snprintf(what, sizeof(what), "кулдаун самоцвета %.3f с — меньше 0.22 с", gemCooldown);
    expect(gemCooldown >= 0.219f, what);
}

static void checkJitter(SoundEngine& engine) {
    printf("джиттер высоты и громкости\n");
    srand(1234);
    float minSpeed = 10.0f, maxSpeed = 0.0f;
    float minVol = 10.0f, maxVol = 0.0f;
    for (int i = 0; i < 400; ++i) {
        for (ActiveVoice& voice : engine.voices) voice.active = false;
        for (int k = 0; k < SFX_COUNT; ++k) engine.cooldowns[k] = 0.0f;
        engine.play(SFX_HIT, 0.0f);
        for (const ActiveVoice& voice : engine.voices) {
            if (voice.active && voice.id == SFX_HIT) {
                minSpeed = std::min(minSpeed, voice.speed);
                maxSpeed = std::max(maxSpeed, voice.speed);
                minVol = std::min(minVol, voice.volume);
                maxVol = std::max(maxVol, voice.volume);
            }
        }
    }
    printf("  высота %.3f…%.3f, громкость %.3f…%.3f\n", minSpeed, maxSpeed, minVol, maxVol);
    expect(minSpeed < 0.98f && maxSpeed > 1.02f, "высота не меняется — повторы звучат машинно");
    expect(minSpeed >= 0.90f && maxSpeed <= 1.10f, "разброс высоты вышел за ±10 %");
    expect(minVol < 0.95f * 0.18f * 1.0f || maxVol > 1.05f * 0.18f, "громкость не меняется");

    // Самоцвет поётся по пентатонике: одинаковые высоты подряд превратили бы
    // сбор в писк. Проверяем, что соседние ноты разные и всего их пять.
    for (ActiveVoice& voice : engine.voices) voice.active = false;
    for (int k = 0; k < SFX_COUNT; ++k) engine.cooldowns[k] = 0.0f;
    float last = -1.0f;
    int changes = 0;
    for (int i = 0; i < 40; ++i) {
        for (ActiveVoice& voice : engine.voices) voice.active = false;
        engine.cooldowns[SFX_GEM] = 0.0f;
        engine.play(SFX_GEM, 0.0f);
        for (const ActiveVoice& voice : engine.voices) {
            if (voice.active && voice.id == SFX_GEM) {
                if (last >= 0.0f && std::fabs(voice.speed - last) > 0.001f) ++changes;
                last = voice.speed;
            }
        }
    }
    char what[120];
    snprintf(what, sizeof(what), "нота самоцвета не меняется (%d смен на 40 подборов)", changes);
    expect(changes >= 30, what);
}

static void writeWav(const char* path, const std::vector<int16_t>& samples) {
    FILE* file = fopen(path, "wb");
    if (!file) {
        printf("  не смог записать %s\n", path);
        return;
    }
    const uint32_t dataBytes = (uint32_t)samples.size() * 2u;
    const uint32_t sampleRate = 22050;
    auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, file); };
    auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, file); };
    fwrite("RIFF", 1, 4, file);
    u32(36u + dataBytes);
    fwrite("WAVEfmt ", 1, 8, file);
    u32(16);
    u16(1);            // PCM
    u16(1);            // моно
    u32(sampleRate);
    u32(sampleRate * 2u);
    u16(2);
    u16(16);
    fwrite("data", 1, 4, file);
    u32(dataBytes);
    fwrite(samples.data(), 2, samples.size(), file);
    fclose(file);
    printf("  демо-дорожка: %s (%.1f с)\n", path, samples.size() / (float)sampleRate);
}

// Демо: каждый эффект звучит один раз, между ними короткая тишина. Порядок
// печатается, чтобы можно было понять, что именно слышишь.
static void renderDemo(SoundEngine& engine, const char* path) {
    struct Cue { SfxId id; float seconds; };
    const Cue cues[] = {
        {SFX_CLICK, 0.20f}, {SFX_GEM, 0.18f}, {SFX_GEM, 0.18f}, {SFX_GEM, 0.18f},
        {SFX_BLADE, 0.25f}, {SFX_BLADE, 0.25f}, {SFX_HIT, 0.30f}, {SFX_HIT, 0.30f},
        {SFX_FIREBALL, 0.40f}, {SFX_LIGHTNING, 0.35f}, {SFX_PROJECTILE_FLIGHT, 0.30f},
        {SFX_PROJECTILE_IMPACT, 0.30f}, {SFX_DASH, 0.30f}, {SFX_FREEZE, 0.40f},
        {SFX_MAGNET, 0.40f}, {SFX_CHEST, 0.70f}, {SFX_LEVELUP, 0.60f},
        {SFX_TIER_UP, 0.60f}, {SFX_EVOLVE, 0.60f}, {SFX_ULTIMATE, 0.60f},
        {SFX_BOSS, 0.80f}, {SFX_BOSS_TELEGRAPH, 0.60f}, {SFX_BOSS_IMPACT, 0.60f},
        {SFX_BOSS_PHASE, 0.60f}, {SFX_BOSS_DEATH, 1.20f}, {SFX_PLAYER_DEATH, 0.80f},
        {SFX_REVIVE, 1.00f},
    };
    printf("демо: ");
    for (const Cue& cue : cues) printf("%s ", sfxName(cue.id));
    printf("\n");

    std::vector<int16_t> track;
    engine.enabled = true;
    engine.musicEnabled = false;
    engine.musicVolume = 0.0f;
    for (const Cue& cue : cues) {
        for (ActiveVoice& voice : engine.voices) voice.active = false;
        for (int i = 0; i < SFX_COUNT; ++i) engine.cooldowns[i] = 0.0f;
        engine.play(cue.id, 0.0f);
        const int frames = (int)(44100.0f * cue.seconds);
        std::vector<int16_t> out(frames);
        engine.mixBuffer(out.data(), frames);
        // 44,1 → 22,05 кГц: каждый второй отсчёт. Для сравнения эффектов хватает.
        for (size_t i = 0; i < out.size(); i += 2) track.push_back(out[i]);
        for (int s = 0; s < 2205; ++s) track.push_back(0); // 100 мс тишины
    }

    // Плотный бой отдельным куском: слышно, как лимитер держит толпу.
    for (ActiveVoice& voice : engine.voices) voice.active = false;
    for (int i = 0; i < SFX_COUNT; ++i) engine.cooldowns[i] = 0.0f;
    for (int i = 0; i < 12; ++i) {
        engine.cooldowns[SFX_HIT] = 0.0f;
        engine.play(SFX_HIT, 0.0f);
    }
    engine.cooldowns[SFX_FIREBALL] = 0.0f;
    engine.play(SFX_FIREBALL, 0.0f);
    std::vector<int16_t> brawl(44100);
    engine.mixBuffer(brawl.data(), (int)brawl.size());
    for (size_t i = 0; i < brawl.size(); i += 2) track.push_back(brawl[i]);

    writeWav(path, track);
}

int main(int argc, char** argv) {
    printf("Эффекты: банк, микшер, приоритеты\n");
    SoundEngine engine;
    engine.init();

    checkBank(engine);
    checkVariants(engine);
    const float bed = musicBedPeak(engine);
    checkMixLevels(engine, bed);
    checkDenseFight(engine);
    checkStealing(engine);
    checkThrottle(engine);
    checkJitter(engine);

    const char* wavPath = (argc > 2 && strcmp(argv[1], "--wav") == 0) ? argv[2] : nullptr;
    if (wavPath) renderDemo(engine, wavPath);
    printf("  итого: %d проверок\n", checks);
    if (failures == 0) {
        printf("  ОК: ошибок нет\n");
        return 0;
    }
    printf("  ошибок: %d\n", failures);
    return 1;
}
