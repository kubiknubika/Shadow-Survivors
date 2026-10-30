// Стенд «что значит за краем экрана».
//
// Проверяет каждую точку, где игра рождает врага, одним и тем же вопросом:
// **вся фигура моба вне логического кадра или нет.** Это не «далеко от героя»,
// а именно «за краем»: игрок не должен видеть, как моб возникает из воздуха,
// и не должен ловить моба, который «пришёл» из видимой точки.
//
// Логический кадр считается той же функцией, что и в игре
// (renderer.h: logicalScreenSize) — иначе стенд мерил бы окно, а игра рисовала
// другой прямоугольник.
//
//   g++ -O2 -std=c++17 -Isrc tools/spawn_probe.cpp -o build/spawn_probe
//   build/spawn_probe            # все сценарии, все экраны
//   build/spawn_probe 1920 1080  # один экран
//
// Код возврата 1, если сценарий не выполнил своё ожидание (за краем / в кадре /
// рядом с местом смерти), если моб целиком лёг в полосу HUD, если стенд и игра
// ответили по-разному или если сработала самопроверка.

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>
#include "renderer.h"
#include "game.h"

// ── Что считаем «за краем» ────────────────────────────────────────────────────
// Камера жёстко центрована на герое (camX = playerX - W/2), плюс тряска экрана
// до SHAKE_MAX px. Значит прямоугольник кадра в мировых координатах — это
// [player - W/2, player + W/2] × [player - H/2, player + H/2]. Моб стоит за
// краем, если его силуэт (радиус + запас на свечение и тень) не пересекает этот
// прямоугольник вообще.
static const float SHAKE_MAX = Game::SCREEN_SHAKE_PAD;   // то же число, что и в игре

// HUD занимает верхнюю и нижнюю полосы кадра — те же 130/42 px, что в
// isWorldPointVisible(). Полоса важна отдельно: моб, чей силуэт целиком лежит
// в HUD-полосе, формально «в кадре», но игрок его не видит; и наоборот, моб,
// торчащий из-за края в полосу, виден. Поэтому «за краем» считается только по
// полному прямоугольнику кадра, без вычитания HUD.
static const int HUD_TOP = 130;
static const int HUD_BOTTOM = 42;

// Границы силуэта берутся из тех же данных, которыми игра рисует моба:
// bakedSpriteVisualOffset даёт сдвиг видимых пикселей внутри спрайта, размеры
// спрайта — половину кадра. Радиус коллайдера для этого не годится: спрайт
// заметно больше, и моб «за краем по радиусу» может торчать из-за края хвостом.
struct Silhouette { float hw, hh, offX, offY; };

// Пределы силуэта берутся у игры (Game::monsterSilhouette) — стенд не должен
// держать свою копию чисел. Сдвиг видимых пикселей внутри спрайта (0–6 px) —
// уточнение отрисовки: он делает замер чуть строже, чем критерий игры.
static Silhouette silhouetteOf(const Renderer& r, const Monster& m) {
    Silhouette s;
    Game::monsterSilhouette(m, s.hw, s.hh);
    s.offX = 0.0f;
    s.offY = 0.0f;
    if (m.isResonator || m.type == RAT_ENEMY_TYPE) return s;
    const BakedSprite* spr = MONSTER_SPRITES[m.type % 11][0];
    int dx = 0, dy = 0;
    r.bakedSpriteVisualOffset(spr, dx, dy);
    s.offX = (float)dx;
    s.offY = (float)dy;
    return s;
}

struct Metrics {
    int total = 0;
    int failed = 0;
    int behind = 0;         // весь силуэт вне кадра
    int glimpsed = 0;       // силуэт пересекает кадр («поодаль, но видно»)
    int shakeRevealable = 0;// за краем, но ближе SHAKE_MAX: тряска может выдать
    float minGap = 1.0e9f;  // минимальный зазор до кадра (>0 — за краем)
    int hudOnly = 0;        // силуэт целиком в HUD-полосе: формально в кадре, но не виден (дефект)
    int mismatch = 0;       // стенд и игра отвечают на «за краем» по-разному (дефект)
    float minPlayer = 1.0e9f;
    double sumPlayer = 0.0;
    int worstType = -1;

    void add(const Renderer& rend, const Game& g, const Monster& m, float screenW, float screenH) {
        ++total;
        const Silhouette sil = silhouetteOf(rend, m);
        // Экранные координаты центра фигуры (спрайт рисуется со сдвигом видимых пикселей).
        const float cx = m.x - g.playerX + screenW * 0.5f - sil.offX;
        const float cy = m.y - g.playerY + screenH * 0.5f - sil.offY;
        const float left = cx - sil.hw, right = cx + sil.hw, top = cy - sil.hh, bottom = cy + sil.hh;
        const float gapX = std::max(0.0f - right, left - screenW);
        const float gapY = std::max(0.0f - bottom, top - screenH);
        const float gap = std::max(gapX, gapY);
        const float pdist = std::sqrt((m.x - g.playerX) * (m.x - g.playerX) +
                                      (m.y - g.playerY) * (m.y - g.playerY));
        if (gap > 0.0f) ++behind; else ++glimpsed;
        if (gap > 0.0f && gap < SHAKE_MAX) ++shakeRevealable;
        // Та же проверка игровой функцией: если она и стенд расходятся, значит
        // «за краем» в игре и в тесте — не одно и то же, и зелёный тест врёт.
        if (g.isMonsterBehindScreenEdge(m, 0.0f) != (gap > 0.0f)) ++mismatch;
        if (gap <= 0.0f && (bottom <= (float)HUD_TOP || top >= screenH - (float)HUD_BOTTOM)) ++hudOnly;
        if (gap < minGap) { minGap = gap; worstType = m.type; }
        if (pdist < minPlayer) minPlayer = pdist;
        sumPlayer += pdist;
    }
    void merge(const Metrics& o) {
        total += o.total; failed += o.failed; behind += o.behind; glimpsed += o.glimpsed;
        shakeRevealable += o.shakeRevealable; hudOnly += o.hudOnly; mismatch += o.mismatch;
        if (o.minGap < minGap) { minGap = o.minGap; worstType = o.worstType; }
        minPlayer = std::min(minPlayer, o.minPlayer);
        sumPlayer += o.sumPlayer;
    }
    double avgPlayer() const { return total ? sumPlayer / total : 0.0; }
};

// Что сценарий обязан доказать.
enum Expect {
    BEHIND,    // весь силуэт за краем кадра (волны, отряды, крысы, резонатор)
    IN_FRAME,  // силуэт в кадре намеренно (телепорт стража, свита у хозяина)
    LOCAL      // локальный эффект: дети рядом с местом смерти, а не «из-за края»
};

struct Scenario {
    const char* name;
    Expect expect;
    Metrics m;
    float localSpread = 0.0f;   // для LOCAL: максимальный разброс от места смерти
    float localLimit = 0.0f;    // предел разброса, px
};

static const int TYPE_COUNT = 12;

static void setScreen(int w, int h) { SCREEN_W = w; SCREEN_H = h; }

// Обычный моб: всё, что приходит волнами.
static void runOrdinary(Renderer& rend, Game& g, Metrics& out, int count) {
    for (int i = 0; i < count; ++i) {
        g.monsters.clear();
        g.spawnMonster(i % TYPE_COUNT, (i % 4) == 0, false);
        if (g.monsters.empty()) { ++out.failed; continue; }
        out.add(rend, g, g.monsters.back(), (float)SCREEN_W, (float)SCREEN_H);
    }
}

// Отряд: входит шеренгой с одной стороны.
static void runSquad(Renderer& rend, Game& g, Metrics& out, int rounds) {
    for (int i = 0; i < rounds; ++i) {
        g.monsters.clear();
        g.spawnSquad();
        for (const Monster& m : g.monsters) out.add(rend, g, m, (float)SCREEN_W, (float)SCREEN_H);
    }
}

// Крысиная стая: кучка вокруг точки входа.
static void runRats(Renderer& rend, Game& g, Metrics& out, int rounds) {
    for (int i = 0; i < rounds; ++i) {
        g.monsters.clear();
        g.spawnRatSwarm();
        for (const Monster& m : g.monsters) out.add(rend, g, m, (float)SCREEN_W, (float)SCREEN_H);
    }
}

// Резонаторы: лопаются пачкой вокруг места разрушения.
static void runResonatorBurst(Renderer& rend, Game& g, Metrics& out, int rounds) {
    for (int i = 0; i < rounds; ++i) {
        g.monsters.clear();
        g.pendingResonatorBurst = 12;
        g.releaseResonatorBurst();
        for (const Monster& m : g.monsters) out.add(rend, g, m, (float)SCREEN_W, (float)SCREEN_H);
    }
}

// Матрёшка: дети вылезают из места смерти родителя. Это нарочно локальный
// эффект, поэтому проверяется не «за краем», а «рядом с местом смерти».
static float g_matryoshkaSpread = 0.0f;
static Metrics g_matryoshkaParent;   // родитель появляется по общему правилу «за краем»
static int g_matryoshkaBehind = 0;   // ребёнок, вылетевший за край, — это уже не локальный эффект
static void runMatryoshka(Renderer& rend, Game& g, Metrics& out, int rounds) {
    g_matryoshkaSpread = 0.0f;
    g_matryoshkaParent = Metrics{};
    g_matryoshkaBehind = 0;
    for (int i = 0; i < rounds; ++i) {
        g.monsters.clear();
        g.spawnMonster(MATRYOSHKA_ENEMY_TYPE, false, false);
        if (g.monsters.empty()) { ++out.failed; continue; }
        // Место смерти чередуем: центр, у правого края, у верхнего (под HUD), в углу.
        // Раньше матрёшка, убитая у края, отправляла детей в точку спавнера — они
        // появлялись в трёх сотнях пикселей от места смерти, то есть «из-за края».
        const float hw = SCREEN_W * 0.5f, hh = SCREEN_H * 0.5f;
        float mx = g.playerX, my = g.playerY;
        switch (i % 4) {
            case 1: mx += hw - 60.0f; break;                        // у правого края
            case 2: my -= hh - 160.0f; break;                       // под верхним HUD
            case 3: mx -= hw - 60.0f; my += hh - 60.0f; break;      // угол
            default: mx += (float)(rand() % 400 - 200); my += (float)(rand() % 300 - 150); break;
        }
        g.pendingMatryoshkaSplits.push_back({mx, my});
        g.releaseMatryoshkaChildren();
        for (const Monster& m : g.monsters) {
            if (m.type == MATRYOSHKA_ENEMY_TYPE) {
                // Родитель рождается обычным путём — его «за краем» меряем отдельно,
                // иначе он разбавляет процент детей в таблице.
                g_matryoshkaParent.add(rend, g, m, (float)SCREEN_W, (float)SCREEN_H);
                continue;
            }
            const float d = std::sqrt((m.x - mx) * (m.x - mx) + (m.y - my) * (m.y - my));
            if (d > g_matryoshkaSpread) g_matryoshkaSpread = d;
            if (g.isMonsterBehindScreenEdge(m, 0.0f)) ++g_matryoshkaBehind;
            out.add(rend, g, m, (float)SCREEN_W, (float)SCREEN_H);
        }
    }
}

// Сам резонатор: появляется по кольцу далеко за краем и должен быть невидим
// целиком, включая ореол, полосу HP и подпись над кристаллом.
static void runResonatorSpawn(Renderer& rend, Game& g, Metrics& out, int rounds) {
    for (int i = 0; i < rounds; ++i) {
        g.monsters.clear();
        g.spawnResonator(i < 3 ? i : 3 + (i % 6));
        for (const Monster& m : g.monsters) {
            if (!m.isResonator) continue;
            out.add(rend, g, m, (float)SCREEN_W, (float)SCREEN_H);
        }
    }
}

// Страж: обязан быть в кадре (иначе игрок не поймёт, откуда приём).
static void runBoss(Renderer& rend, Game& g, Metrics& out, int rounds) {
    for (int i = 0; i < rounds; ++i) {
        g.monsters.clear();
        g.spawnMonster(i % 6, false, true, i % 6);
        if (g.monsters.empty()) { ++out.failed; continue; }
        out.add(rend, g, g.monsters.back(), (float)SCREEN_W, (float)SCREEN_H);
    }
}

// Свита: не «за краем» — она поднимается у хозяина, и это осознанно.
// Мерим не «видно/не видно», а расстояние до хозяина и до героя.
// Свита поднимается у хозяина, а не приходит из-за края, — но и на герое она
// появляться не должна. Хозяина ставим вплотную (как Моргар в своём коридоре)
// и в полутора сотнях пикселей; меряем расстояние до хозяина, до героя и кадр.
static void runRetinue(Renderer& rend, Game& g, Metrics& out, int rounds, double* bossDist, double* playerDist) {
    double bossSum = 0.0, playerSum = 0.0;
    int counted = 0;
    for (int i = 0; i < rounds; ++i) {
        g.monsters.clear();
        g.projectiles.clear();
        g.balance.spawnMultiplier = 0.0f;   // иначе в замер лезет обычная волна
        g.squadTimer = 1.0e9f;
        g.ratTimer = 1.0e9f;
        g.nextBossIndex = 999;
        g.spawnMonster(2, false, true, 2);
        if (g.monsters.empty()) { ++out.failed; continue; }
        // Чётные круги — хозяин вплотную к герою, нечётные — в 160 px.
        Monster& boss = g.monsters.back();
        boss.x = g.playerX + (i % 2 == 0 ? 84.0f : 160.0f);
        boss.y = g.playerY + (i % 2 == 0 ? 10.0f : -40.0f);
        boss.hp = boss.maxHp * 0.5f;
        boss.bossPhase = 2;
        boss.specialTimer = 0.0f;
        int seen = 0;
        for (int f = 0; f < 240; ++f) {
            g.update(1.0f / 60.0f);
            for (const Monster& m : g.monsters) if (m.isRetinue) ++seen;
            if (seen > 0) break;
        }
        for (const Monster& m : g.monsters) {
            if (!m.isRetinue) continue;
            out.add(rend, g, m, (float)SCREEN_W, (float)SCREEN_H);
            const float bd = std::sqrt((m.x - boss.x) * (m.x - boss.x) + (m.y - boss.y) * (m.y - boss.y));
            bossSum += bd;
            playerSum += std::sqrt((m.x - g.playerX) * (m.x - g.playerX) +
                                   (m.y - g.playerY) * (m.y - g.playerY));
            ++counted;
        }
        g.balance.spawnMultiplier = 1.0f;
    }
    *bossDist = bossSum;
    *playerDist = playerSum;
    if (counted == 0) ++out.failed;
}

// ── Самопроверка проверки ────────────────────────────────────────────────────
// Тест на «за краем» бесполезен, если он сам не ловит два типовых ложных
// ответа: (1) «моб далеко от героя, значит за краем» и (2) «моб выше кадра,
// значит за краем» — хотя он висит в HUD-полосе на самом экране. Гоняем
// контрольные точки через ту же функцию, что использует игра, и ждём ответ.
static int runCheckerSelfTest(const Renderer& rend, const Game& g) {
    int bad = 0;
    const float halfW = SCREEN_W * 0.5f, halfH = SCREEN_H * 0.5f;
    (void)rend;
    auto expect = [&](const char* what, float dx, float dy, bool behind) {
        const bool got = g.isSpriteBehindScreenEdge(g.playerX + dx, g.playerY + dy, 20.0f, 20.0f, 0.0f);
        if (got != behind) {
            printf("  контроль «%s»: ожидалось «%s», получено «%s»\n", what,
                   behind ? "за краем" : "в кадре", got ? "за краем" : "в кадре");
            ++bad;
        }
    };
    // 1. Поодаль, но на экране: 0,7 полуширины в сторону при нулевом Y.
    expect("поодаль, но на экране", halfW * 0.7f, 0.0f, false);
    // 2. Вплотную к правому краю изнутри — виден.
    expect("у самого края изнутри", halfW - 10.0f, 0.0f, false);
    // 3. Торчит в кадр из-за правого края — виден.
    expect("торчит из-за края", halfW + 10.0f, 0.0f, false);
    // 4. За краем с запасом — не виден.
    expect("за краем с запасом", halfW + 60.0f, 0.0f, true);
    // 5. Верх: силуэт целиком в HUD-полосе, формально в кадре — но это экран.
    expect("повис в HUD-полосе", 0.0f, -halfH + 20.0f, false);
    // 6. Выше кадра, но низ силуэта ещё в полосе HUD — виден.
    expect("свисает из HUD в кадр", 0.0f, -halfH - 10.0f, false);
    // 7. Полностью над кадром — не виден.
    expect("полностью над кадром", 0.0f, -halfH - 40.0f, true);
    return bad;
}

// ── Картинка к отчёту ────────────────────────────────────────────────────────
// Рисует то же, что проверяет замер: кадр одного формата, полосы HUD и точки,
// где игра реально родила моба. Зелёная рамка — весь силуэт за краем, красная —
// точка в кадре. Снимок делается по тем же вызовам спавна и тому же зерну, что
// и таблица, поэтому картинка не может «наврать» отдельно от замера.
struct Spot { float x, y, hw, hh; bool behind; };

static void collectSpots(Game& g, int limit, std::vector<Spot>& out) {
    auto take = [&](const Monster& m) {
        if ((int)out.size() >= limit) return;
        float hw = 0.0f, hh = 0.0f;
        Game::monsterSilhouette(m.type, m.isElite, hw, hh);
        out.push_back({m.x, m.y, hw, hh, g.isMonsterBehindScreenEdge(m, 0.0f)});
    };
    for (int i = 0; i < 400 && (int)out.size() < limit; ++i) {
        g.monsters.clear();
        g.spawnMonster(i % TYPE_COUNT, (i % 4) == 0, false);
        if (!g.monsters.empty()) take(g.monsters.back());
    }
    for (int i = 0; i < 160 && (int)out.size() < limit; ++i) {
        g.monsters.clear();
        g.spawnSquad();
        for (const Monster& m : g.monsters) take(m);
    }
    for (int i = 0; i < 60 && (int)out.size() < limit; ++i) {
        g.monsters.clear();
        g.spawnRatSwarm();
        for (const Monster& m : g.monsters) take(m);
    }
}

static int writeFigure(const char* path, int winW, int winH) {
    const int CANVAS_W = 1740, CANVAS_H = 780;
    bool ok = true;
    struct Panel { int x, y, w, h; float scale; const char* title; };
    std::vector<Panel> panels;

    // Снимаем точки в логическом кадре каждого окна, потом считаем раскладку.
    struct Shot { std::vector<Spot> spots; float playerX, playerY; int W, H; };
    std::vector<Shot> shots;
    const int wins[2][2] = {{winW, winH}, {1080, 1920}};
    for (int k = 0; k < 2; ++k) {
        int W = 0, H = 0;
        logicalScreenSize(wins[k][0], wins[k][1], W, H);
        setScreen(W, H);
        Game g;
        srand(20260201);
        g.init();
        g.state = STATE_PLAYING;
        g.playerX = 5000.0f;
        g.playerY = 7000.0f;
        Shot sh;
        sh.playerX = g.playerX; sh.playerY = g.playerY; sh.W = W; sh.H = H;
        collectSpots(g, 260, sh.spots);
        shots.push_back(sh);
    }

    setScreen(CANVAS_W, CANVAS_H);
    Renderer r;
    r.resize(CANVAS_W, CANVAS_H);
    r.clear(0xFF0E0B16);

    // Раскладка панелей: альбом 1:1 по ширине слева, портрет — справа, обе в одном масштабе по высоте.
    const float landscapeScale = 0.60f;
    panels.push_back({28, 118, (int)(shots[0].W * landscapeScale), (int)(shots[0].H * landscapeScale),
                      landscapeScale, "альбом"});
    const float portraitScale = 0.32f;
    panels.push_back({820, 118, (int)(shots[1].W * portraitScale), (int)(shots[1].H * portraitScale),
                      portraitScale, "портрет"});

    struct Caption { int x, y; std::string text; uint32_t col; };
    std::vector<Caption> captions;
    for (size_t k = 0; k < panels.size() && k < shots.size(); ++k) {
        const Panel& pn = panels[k];
        const Shot& sh = shots[k];
        r.fillRect(pn.x, pn.y, pn.w, pn.h, 0xFF171325);
        // Полосы HUD: именно они сбивают с толку проверку «вне видимой области».
        r.fillRect(pn.x, pn.y, pn.w, (int)(HUD_TOP * pn.scale), 0xFFFF7A45, 46);
        r.fillRect(pn.x, pn.y + pn.h - (int)(HUD_BOTTOM * pn.scale), pn.w,
                   (int)(HUD_BOTTOM * pn.scale) + 1, 0xFFFF7A45, 46);
        for (const Spot& sp : sh.spots) {
            const float cx = pn.x + (sp.x - sh.playerX + sh.W * 0.5f) * pn.scale;
            const float cy = pn.y + (sp.y - sh.playerY + sh.H * 0.5f) * pn.scale;
            // Точки дальше 90 px от кадра не рисуем: они всё равно за краем, а
            // на картинке залезали бы на соседнюю панель и в подписи.
            if (cx < pn.x - 55 || cx > pn.x + pn.w + 55 || cy < pn.y - 55 || cy > pn.y + pn.h + 55) continue;
            const int hw = std::max(2, (int)(sp.hw * pn.scale));
            const int hh = std::max(2, (int)(sp.hh * pn.scale));
            const uint32_t col = sp.behind ? 0xFF5FE08C : 0xFFE85C4A;
            r.drawRect((int)cx - hw, (int)cy - hh, hw * 2, hh * 2, col, 1, sp.behind ? 200 : 235);
        }
        r.drawRect(pn.x, pn.y, pn.w, pn.h, 0xFF5B5278, 1);
        // Герой — центр кадра: камера жёстко висит на нём.
        r.fillCircle(pn.x + pn.w / 2, pn.y + pn.h / 2, 4, 0xFFFFFFFF);
        int behind = 0;
        for (const Spot& sp : sh.spots) if (sp.behind) ++behind;
        // Подписи панелей рисуем после точек — иначе соседняя панель закрасит их.
        char cap[200];
        snprintf(cap, sizeof(cap), "%s: кадр %dx%d, %d точек, за краем %d, в кадре %d",
                 pn.title, sh.W, sh.H, (int)sh.spots.size(), behind, (int)sh.spots.size() - behind);
        captions.push_back({pn.x, pn.y + pn.h + 8, std::string(cap),
                            behind == (int)sh.spots.size() ? 0xFF5FE08C : 0xFFE85C4A});
        if (behind != (int)sh.spots.size()) ok = false;
    }

    // Легенда справа от портрета.
    for (const Caption& cap : captions)
        r.drawText(FONT_SMALL, cap.x, cap.y, cap.text.c_str(), cap.col);

    // Заголовок рисуем поверх панелей: точки спавна могут уходить выше кадра.
    r.drawText(FONT_LARGE, 28, 20, "«ЗА КРАЕМ ЭКРАНА» — ЭТО КАДР, А НЕ «ДАЛЕКО ОТ ГЕРОЯ»", 0xFFF2E9D8, false);
    r.drawText(FONT_SMALL, 28, 66, "Каждая рамка — точка, где игра родила моба. Проверка идёт по всему силуэту спрайта,", 0xFF9C93B4, false);
    r.drawText(FONT_SMALL, 28, 86, "с запасом 24 px на тряску экрана: зелёная точка — за краем, красная — в кадре (дефект).", 0xFF9C93B4, false);

    const int lx = 1150;
    r.fillRect(lx - 14, 112, 3, 400, 0xFF3B3357);
    int ly = 118;
    auto legend = [&](uint32_t col, bool outline, const char* a, const char* b) {
        if (outline) r.drawRect(lx, ly + 4, 16, 12, col, 1, 235);
        else r.fillRect(lx, ly + 4, 16, 12, col);
        r.drawText(FONT_SMALL, lx + 26, ly, a, 0xFFF2E9D8);
        r.drawText(FONT_SMALL, lx + 26, ly + 20, b, 0xFF9C93B4);
        ly += 48;
    };
    legend(0xFF5FE08C, true, "ЗА КРАЕМ", "силуэт целиком вне кадра");
    legend(0xFFE85C4A, true, "В КАДРЕ", "игрок увидит появление");
    legend(0xFFFF7A45, false, "ПОЛОСЫ HUD — 130 / 42 px", "интерфейс, а не кадр игры");
    legend(0xFFFFFFFF, false, "ГЕРОЙ", "камера центрирована на нём");
    r.fillRect(lx, ly + 4, 16, 12, 0xFF5B5278);
    r.drawText(FONT_SMALL, lx + 26, ly, "ГРАНИЦА КАДРА", 0xFFF2E9D8);
    r.drawText(FONT_SMALL, lx + 26, ly + 20, "±W/2, ±H/2 от героя", 0xFF9C93B4);

    // Подписи снизу: то, что важно из стенда.
    int by = 620;
    const char* notes[] = {
        "1. Логический кадр считает одна функция на игру и на стенд — logicalScreenSize():",
        "   800x600 и 1024x768 дают кадр 960x720, 1366x768 и 1920x1080 — 1280x720, портрет 1080x1920 — 720x1280.",
        "2. Один и тот же моб в 500 px от героя виден на 1280x720 (полу-ширина 640) и не виден в портрете (полу-ширина 360),",
        "   поэтому «далеко от героя» критерием не является: проверка идёт по прямоугольнику кадра.",
        "3. Моб, остановившийся в полосе HUD, формально «в кадре» — стенд считает это дефектом и печатает отдельную строку.",
    };
    for (const char* line : notes) {
        r.drawText(FONT_SMALL, 28, by, line, line[0] >= '0' && line[0] <= '9' ? 0xFFCFC6E0 : 0xFF9C93B4);
        by += 26;
    }

    FILE* f = fopen(path, "wb");
    if (!f) { printf("не удалось записать %s\n", path); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", CANVAS_W, CANVAS_H);
    for (int i = 0; i < CANVAS_W * CANVAS_H; ++i) {
        const uint32_t c = r.pixels[i];
        const unsigned char rgb[3] = {(unsigned char)((c >> 16) & 0xFF),
                                      (unsigned char)((c >> 8) & 0xFF),
                                      (unsigned char)(c & 0xFF)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("%s\n", path);
    return ok ? 0 : 1;
}

struct Window { int w, h; };

int main(int argc, char** argv) {
    if (argc >= 5 && strcmp(argv[1], "-f") == 0) {
        return writeFigure(argv[2], atoi(argv[3]), atoi(argv[4]));
    }
    std::vector<Window> windows;
    if (argc >= 3) {
        windows.push_back({atoi(argv[1]), atoi(argv[2])});
    } else {
        windows = {
            {800, 600}, {1024, 768}, {1280, 720}, {1366, 768}, {1600, 900},
            {1920, 1080}, {2560, 1440}, {3440, 1440}, {1080, 1920}, {720, 1280}
        };
    }

    Game g;
    int failures = 0;
    printf("«за краем» = весь силуэт моба (спрайт по данным отрисовки, у элиты ореол 34 px)\n");
    printf("целиком вне логического кадра; запас на тряску экрана %.0f px.\n", SHAKE_MAX);
    printf("Логический кадр игры: в альбоме высота 720, в портрете ширина 720.\n\n");

    for (const Window& win : windows) {
        int W = 0, H = 0;
        logicalScreenSize(win.w, win.h, W, H);
        setScreen(W, H);
        Renderer rend;   // нужен только для bakedSpriteVisualOffset: точные границы силуэта
        printf("окно %dx%d → кадр %dx%d   (половина %.0f×%.0f, диагональ %.0f)\n",
               win.w, win.h, W, H, W * 0.5, H * 0.5, std::sqrt(W * 0.5f * W * 0.5f + H * 0.5f * H * 0.5f));

        std::vector<Scenario> scenarios = {
            {"обычный моб", BEHIND, {}},
            {"отряд", BEHIND, {}},
            {"крысы", BEHIND, {}},
            {"резонатор", BEHIND, {}},
            {"резонатор-точка", BEHIND, {}},
            // Дети матрёшки — локальный эффект: считаем их разброс от места смерти,
            // а не «за краем». Предел 80 px — с запасом к 28 + радиус (≈48).
            {"матрёшка", LOCAL, {}, 0.0f, 80.0f},
            {"страж", IN_FRAME, {}},
            {"свита", IN_FRAME, {}},
        };

        srand(20260201);
        g.init();
        failures += runCheckerSelfTest(rend, g);
        g.state = STATE_PLAYING;
        g.playerX = 5000.0f;
        g.playerY = 7000.0f;
        runOrdinary(rend, g, scenarios[0].m, 3000);
        runSquad(rend, g, scenarios[1].m, 200);
        runRats(rend, g, scenarios[2].m, 150);
        runResonatorBurst(rend, g, scenarios[3].m, 150);
        runResonatorSpawn(rend, g, scenarios[4].m, 300);
        runMatryoshka(rend, g, scenarios[5].m, 150);
        scenarios[5].localSpread = g_matryoshkaSpread;
        runBoss(rend, g, scenarios[6].m, 300);
        double bossSum = 0.0, playerSum = 0.0;
        runRetinue(rend, g, scenarios[7].m, 60, &bossSum, &playerSum);
        const int retinueCount = scenarios[7].m.total;

        printf("%-14s %8s %9s %8s %10s %12s %12s\n",
               "сценарий", "спавнов", "за краем", "в кадре", "мин.зазор", "до героя", "до героя ср.");
        for (const Scenario& sc : scenarios) {
            const Metrics& m = sc.m;
            bool ok = m.failed == 0 && m.mismatch == 0 && m.hudOnly == 0;
            // Запас на тряску — часть обещания «за краем»: моб, которого камера
            // может выдать при толчке, — это тот же дефект, только на 20 px позже.
            if (sc.expect == BEHIND) ok = ok && m.glimpsed == 0 && m.shakeRevealable == 0;
            else if (sc.expect == IN_FRAME) ok = ok && m.behind == 0;
            else ok = ok && sc.localSpread <= sc.localLimit && g_matryoshkaBehind == 0;
            if (!ok) ++failures;
            printf("%-14s %8d %8.1f%% %7.1f%% %+9.0f %11.0f %12.0f %s\n",
                   sc.name, m.total, m.total ? 100.0 * m.behind / m.total : 0.0,
                   m.total ? 100.0 * m.glimpsed / m.total : 0.0,
                   m.total ? m.minGap : 0.0f, m.minPlayer, m.avgPlayer(),
                   ok ? "" : "  ← ПРОВАЛ");
            if (m.shakeRevealable > 0)
                printf("%-14s   тряска экрана может выдать %d из %d (зазор < %.0f px)\n",
                       "", m.shakeRevealable, m.total, SHAKE_MAX);
            if (m.hudOnly > 0)
                printf("%-14s   силуэт целиком в HUD-полосе: %d из %d\n", "", m.hudOnly, m.total);
            if (m.mismatch > 0)
                printf("%-14s   стенд и игра расходятся в «за краем»: %d из %d\n",
                       "", m.mismatch, m.total);
            if (m.failed > 0)
                printf("%-14s   не удалось поставить точку: %d\n", "", m.failed);
            if (sc.expect == LOCAL) {
                printf("%-14s   разброс от места смерти %.0f px (предел %.0f), за краем %d\n",
                       "", sc.localSpread, sc.localLimit, g_matryoshkaBehind);
                printf("%-14s   родитель: за краем %.1f%%, мин. зазор %+.0f px\n", "",
                       100.0 * g_matryoshkaParent.behind / std::max(1, g_matryoshkaParent.total),
                       g_matryoshkaParent.minGap);
            }
        }
        if (retinueCount > 0)
            printf("%-14s   свита: %.0f px от хозяина, %.0f px от героя\n\n", "",
                   bossSum / retinueCount, playerSum / retinueCount);
        else
            printf("\n");
    }

    printf("Порог «далеко от героя» ничего не значит без кадра: один и тот же моб\n");
    printf("в 500 px от героя виден на 1280x720 (полу-ширина 640) и не виден\n");
    printf("в портрете 720x1280 (полу-ширина 360) — поэтому проверка идёт по кадру.\n");
    if (failures) {
        printf("\nПРОВАЛОВ: %d\n", failures);
        return 1;
    }
    printf("\nвсе ожидания выполнены\n");
    return 0;
}
