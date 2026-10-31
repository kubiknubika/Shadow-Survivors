// ── Стенд вёрстки: текст за краем кадра и текст поверх текста ───────────────
//
// Вопрос, на который отвечает стенд: «нигде ничего не пересекается и не
// вылезает за экран — на любом разрешении и в любом меню». Глазами это не
// проверить: экранов два десятка, кадров — десяток. Поэтому проверяется не
// картинка, а журнал: рендерер записывает каждую нарисованную строку
// (Renderer::textLayouts), а стенд считает по метрикам атласа её чернильный
// прямоугольник и ищет два дефекта:
//
//   вылет      — чернила выходят за кадр (срезанная буква, обрезанная строка);
//   наложение  — чернила двух строк интерфейса пересекаются.
//
// Текст мира (подписи над существами, числа урона) идёт с тегом 1 и в
// наложения не считается: он перекрывается по природе.
//
//     g++ -O2 -std=c++17 -pthread -Isrc tools/ui_probe.cpp -o build/ui_probe
//     build/ui_probe              # все окна, только сводка
//     build/ui_probe --verbose    # и каждая находка с координатами
//     build/ui_probe 1280x720     # одно окно
//
// Код возврата 0 — чисто. Иначе 1.
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "renderer.h"
#include "game.h"
#include "touch_input.h"

// ── Окна, которые бывают у игрока. Не «размеры кадра», а размеры окна:
// логический кадр из них выводит logicalScreenSize, и один и тот же кадр
// получается из нескольких окон. Проверяются все окна, а рисуется — по разу на
// каждый достижимый кадр, иначе один и тот же дефект считался бы дважды.
struct Window { int w, h; const char* note; };
static const Window kWindows[] = {
    {640, 480, "старый 4:3"},
    {800, 600, "4:3"},
    {1024, 768, "4:3"},
    {1152, 864, "4:3 крупный"},
    {1280, 720, "16:9 по умолчанию"},
    {1366, 768, "ноутбук"},
    {1440, 900, "16:10"},
    {1600, 900, "16:9"},
    {1680, 1050, "16:10"},
    {1920, 1080, "Full HD"},
    {2048, 1152, "2K"},
    {2560, 1440, "QHD"},
    {3440, 1440, "сверхширокий"},
    {800, 480, "нетбук"},
    {1280, 800, "16:10 малый"},
    {1920, 1200, "WUXGA"},
    {768, 1024, "планшет 4:3 портрет"},
    {800, 1280, "планшет 16:10 портрет"},
    {834, 1194, "планшет портрет"},
    {720, 1280, "телефон портрет 16:9"},
    {1080, 1920, "телефон портрет Full HD"},
    {1080, 2340, "телефон портрет 19,5:9"},
    {1080, 2400, "телефон портрет 20:9"},
};
static const int kWindowCount = sizeof(kWindows) / sizeof(kWindows[0]);

// ── Экраны. Поле state — состояние игры, page — страница внутри экрана.
struct Screen {
    const char* name;
    int state;
    int page;      // 0 — как есть, 1 — вторая страница (словарь)
    int holiday;   // -1 нет, 0..7 праздник, -2 календарь на 31 октября 2026
};
static const Screen kScreens[] = {
    {"меню", STATE_MENU, 0, -1},
    {"настройки", STATE_SETTINGS, 0, -1},
    {"справка", STATE_HELP, 0, -1},
    {"словарь", STATE_HELP, 1, -1},
    {"патчноуты", STATE_PATCH_NOTES, 0, -1},
    {"зал славы", STATE_HALL, 0, -1},
    {"выбор героя", STATE_HERO_SELECT, 0, -1},
    {"выбор миссии", STATE_MISSION_SELECT, 0, -1},
    {"кузница", STATE_SHOP, 0, -1},
    {"управление", STATE_CONTROLS_SETUP, 0, -1},
    {"вид кнопок", STATE_BUTTON_LOOK, 0, -1},
    {"праздники", STATE_HOLIDAY, 0, -2},
    {"дары дня", STATE_DAILY, 0, -2},
    {"бой", STATE_PLAYING, 0, -1},
    {"бой праздник", STATE_PLAYING, 0, 0},
    {"пауза", STATE_PAUSED, 0, -1},
    {"сундук", STATE_CHEST, 0, -1},
    {"прокачка", STATE_LEVELUP, 0, -1},
    {"смерть", STATE_DEATH, 0, -1},
    {"конец забега", STATE_GAMEOVER, 0, -1},
    {"выход с добычей", STATE_EXTRACTION, 0, -1},
    {"финал кампании", STATE_CAMPAIGN_END, 0, -1},
    {"арена музыки", STATE_BOSS_MUSIC_TEST, 0, -1},
    {"катсцена", STATE_CUTSCENE, 0, -1},
    {"дело закрыто", STATE_MISSION_COMPLETE, 0, -1},
};
static const int kScreenCount = sizeof(kScreens) / sizeof(kScreens[0]);

// ── Метрики шрифта ────────────────────────────────────────────────────────
struct Box { int x0 = 0, y0 = 0, x1 = 0, y1 = 0; bool ok = false; };

static uint32_t nextCp(const char*& p) {
    const unsigned char* s = (const unsigned char*)p;
    uint32_t cp;
    int len;
    if (s[0] < 0x80) { cp = s[0]; len = 1; }
    else if ((s[0] & 0xE0) == 0xC0) { cp = s[0] & 0x1F; len = 2; }
    else if ((s[0] & 0xF0) == 0xE0) { cp = s[0] & 0x0F; len = 3; }
    else if ((s[0] & 0xF8) == 0xF0) { cp = s[0] & 0x07; len = 4; }
    else { p += 1; return 0xFFFD; }
    for (int i = 1; i < len; ++i) cp = (cp << 6) | (s[i] & 0x3F);
    p += len;
    return cp;
}

static const BakedGlyph* glyphOf(const BakedFont& font, uint32_t cp) {
    int lo = 0, hi = font.glyphCount - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        const uint32_t mcp = font.glyphs[mid].codepoint;
        if (mcp == cp) return &font.glyphs[mid];
        if (mcp < cp) lo = mid + 1; else hi = mid - 1;
    }
    return nullptr;
}

// Чернильный прямоугольник строки: не lineHeight, а реальные рамки глифов.
static Box inkBox(const BakedFont& font, int x, int y, const char* text) {
    Box b;
    if (!text || !*text) return b;
    int curX = x;
    const char* p = text;
    while (*p) {
        const uint32_t cp = nextCp(p);
        const BakedGlyph* g = glyphOf(font, cp);
        if (!g) continue;
        if (g->width > 0 && g->height > 0) {
            const int gx = curX + g->bearingX, gy = y + g->bearingY;
            if (!b.ok) { b.x0 = gx; b.y0 = gy; b.x1 = gx + g->width; b.y1 = gy + g->height; b.ok = true; }
            else {
                b.x0 = std::min(b.x0, gx);
                b.y0 = std::min(b.y0, gy);
                b.x1 = std::max(b.x1, gx + (int)g->width);
                b.y1 = std::max(b.y1, gy + (int)g->height);
            }
        }
        curX += g->advance;
    }
    return b;
}

static bool hasLetters(const char* text) {
    if (!text) return false;
    const char* p = text;
    while (*p) {
        const uint32_t cp = nextCp(p);
        if ((cp >= 'A' && cp < 0x7F) || cp >= 0xC0) return true;
    }
    return false;
}

static bool intersects(const Box& a, int x0, int y0, int x1, int y1) {
    return a.x1 > x0 && a.x0 < x1 && a.y1 > y0 && a.y0 < y1;
}

static int overlapW(const Box& a, const Box& b) {
    return std::min(a.x1, b.x1) - std::max(a.x0, b.x0);
}
static int overlapH(const Box& a, const Box& b) {
    return std::min(a.y1, b.y1) - std::max(a.y0, b.y0);
}

struct Visible {
    Box ink;
    std::string text;
    int cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0;   // куда режется
    bool letters = false;
};

static bool prepare(Game& g, const Screen& sc) {
    g.simulationMode = true;
    g.ignoreSavedState = true;
    g.touchControls = false;
    srand(4242);
    g.init();
    g.options.holidayDateShift = 0;
    g.options.holidaysEnabled = sc.holiday >= 0 ? 1 : 0;
    g.options.holidayOverride = sc.holiday >= 0 ? sc.holiday : -1;
    if (sc.holiday == -2) {
        g.options.holidaysEnabled = 1;
        g.holidayPinYear = 2026; g.holidayPinMonth = 10; g.holidayPinDay = 31;
    }
    g.activeHoliday = g.resolveActiveHoliday();
    g.holidayBannerShown = false;
    g.holidayWaveTimer = 40.0f;
    g.gameMode = sc.state == STATE_MISSION_COMPLETE ? MODE_STORY : MODE_ENDLESS;
    g.runTime = 240.0f;
    g.cutsceneId = 0;
    g.cutsceneTimer = 3.0f;
    g.renderThreads = 1;
    for (int i = 0; i < 5; ++i) g.itemLevels[i] = 3;
    g.state = (GameState)sc.state;
    if (sc.page == 1) g.helpPage = 1;
    if (sc.state == STATE_DAILY) {
        g.ensureDaily();
        g.save.dailyProgress[0] = g.save.dailyTarget[0];
        g.save.dailyProgress[1] = g.save.dailyTarget[1] / 3;
        g.save.dailyClaimed = 1 << 2;
        g.save.dailyProgress[2] = g.save.dailyTarget[2];
        g.save.boosters[BOOSTER_RAGE] = 2;
        g.save.boosters[BOOSTER_LUCK] = 1;
        g.save.activeBooster = BOOSTER_RAGE;
    }
    if (sc.state == STATE_SHOP) {
        g.save.gold = 150000;
        for (int i = 0; i < 6; ++i) g.save.metaRanks[i] = i * 2;
    }
    if (sc.state == STATE_PLAYING || sc.state == STATE_LEVELUP || sc.state == STATE_CHEST ||
        sc.state == STATE_PAUSED || sc.state == STATE_DEATH || sc.state == STATE_GAMEOVER) {
        for (int i = 0; i < 60; ++i) g.spawnMonster(i % 8, i % 9 == 0, false);
    }
    if (sc.state == STATE_BOSS_MUSIC_TEST) {
        for (int i = 0; i < 3; ++i) g.spawnMonster(i, true, false);
    }
    for (int f = 0; f < 3; ++f) {
        if (g.state == STATE_LEVELUP) break;
        g.update(1.0f / 60.0f);
    }
    g.state = (GameState)sc.state;
    return true;
}

int main(int argc, char** argv) {
    bool verbose = false;
    int onlyW = 0, onlyH = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--verbose") == 0) verbose = true;
        else if (sscanf(argv[i], "%dx%d", &onlyW, &onlyH) == 2) {}
    }

    // Кадры, в которые игра реально рисует: окна схлопываются в один кадр.
    std::vector<std::pair<int, int>> frames;
    for (int i = 0; i < kWindowCount; ++i) {
        int lw = 0, lh = 0;
        logicalScreenSize(kWindows[i].w, kWindows[i].h, lw, lh);
        const std::pair<int, int> f(lw, lh);
        if (std::find(frames.begin(), frames.end(), f) == frames.end()) frames.push_back(f);
    }
    printf("логические кадры (%d):", (int)frames.size());
    for (const auto& f : frames) printf(" %dx%d", f.first, f.second);
    printf("\n\n");

    std::map<std::string, int> countByScreen, countByKind;
    std::map<std::string, std::string> whereByKey;
    int clips = 0, overlaps = 0, cuts = 0;
    long long linesChecked = 0;
    std::string detail;

    for (const auto& frame : frames) {
        if (onlyW && (frame.first != onlyW || frame.second != onlyH)) continue;
        SCREEN_W = frame.first;
        SCREEN_H = frame.second;
        for (int si = 0; si < kScreenCount * 3; ++si) {
            // Каждый экран проверяется дважды: на компьютере и на телефоне.
            // Телефонный интерфейс — это другие подсказки (без названий клавиш),
            // другие кнопки в настройках и касания в бою, и он тоже должен
            // влезать в кадр.
            const Screen& sc = kScreens[si % kScreenCount];
            const int mode = si / kScreenCount;   // 0 компьютер, 1 телефон, 2 телефон с крупными кнопками
            const bool phone = mode > 0;
            const char* modeSuffix = mode == 0 ? "" : (mode == 1 ? " (телефон)" : " (телефон, кнопки 160%)");
            Renderer r;
            r.resize(SCREEN_W, SCREEN_H);
            r.textJournalEnabled = true;   // без него журнал пуст
            Game g;
            prepare(g, sc);
            if (phone) {
                g.touchControls = true;
                if (mode == 2) g.options.touchButtonScale = TOUCH_SIZE_MAX;
                g.state = (GameState)sc.state;
            }
            r.clear(0xFF000000);
            r.clearTextLayouts();
            g.render(r);
            // Телефонное управление рисуется не в Game::render, а отдельным
            // слоем поверх кадра (так его зовёт main_android.cpp). Без этого
            // вызова стенд не видел подписи кнопок стика и крестовины — то
            // есть самую частую картинку на телефоне.
            if (phone) {
                TouchLayout touchLayout;
                touchLayout.layout(SCREEN_W, SCREEN_H, g.options.touchScheme, g.options.touchButtonScale);
                TouchPointer pointers[TOUCH_MAX_POINTERS] = {};
                drawTouchOverlay(r, g, pointers, touchLayout, g.touchSkin());
            }

            std::vector<Visible> lines;
            for (const auto& tl : r.textLayouts) {
                if (tl.tag != 0 || !tl.font) continue;
                const Box ink = inkBox(*tl.font, tl.x, tl.y, tl.text.c_str());
                if (!ink.ok) continue;
                const int cx0 = tl.clipped ? tl.clipX0 : 0;
                const int cy0 = tl.clipped ? tl.clipY0 : 0;
                const int cx1 = tl.clipped ? tl.clipX1 : SCREEN_W;
                const int cy1 = tl.clipped ? tl.clipY1 : SCREEN_H;
                // Строка, не попавшая в своё окно, не рисуется вовсе — это не дефект.
                if (!intersects(ink, cx0, cy0, cx1, cy1)) continue;

                // 1. Вылет за кадр: часть строки видна, часть срезана краем.
                if (ink.x0 < 0 || ink.y0 < 0 || ink.x1 > SCREEN_W || ink.y1 > SCREEN_H) {
                    const std::string key = std::string(sc.name) + (std::string(modeSuffix) + "|") + tl.text;
                    char buf[256];
                    snprintf(buf, sizeof(buf),
                             "  [вылет] \"%s\" %d,%d %dx%d при кадре %dx%d\n",
                             tl.text.c_str(), ink.x0, ink.y0, ink.x1 - ink.x0, ink.y1 - ink.y0,
                             SCREEN_W, SCREEN_H);
                    ++clips;
                    countByScreen[std::string(sc.name) + modeSuffix] += 1;
                    countByKind["вылет"] += 1;
                    if (!whereByKey.count(key)) whereByKey[key] = buf;
                    if (verbose) detail += buf;
                }
                // 2. Строка шире своего окна: по горизонтали её режет клип.
                if (tl.clipped && (ink.x1 - ink.x0) > (cx1 - cx0)) {
                    const std::string key = std::string(sc.name) + (std::string(modeSuffix) + "|") + tl.text + "|cut";
                    char buf[256];
                    snprintf(buf, sizeof(buf),
                             "  [обрезка] \"%s\" %dx%d в окне %d при кадре %dx%d\n",
                             tl.text.c_str(), ink.x1 - ink.x0, cx1 - cx0, cx1 - cx0,
                             SCREEN_W, SCREEN_H);
                    ++cuts;
                    countByScreen[std::string(sc.name) + modeSuffix] += 1;
                    countByKind["обрезка"] += 1;
                    if (!whereByKey.count(key)) whereByKey[key] = buf;
                    if (verbose) detail += buf;
                }

                ++linesChecked;
                Visible v;
                v.ink = ink;
                v.text = tl.text;
                v.cx0 = cx0; v.cy0 = cy0; v.cx1 = cx1; v.cy1 = cy1;
                v.letters = hasLetters(tl.text.c_str());
                lines.push_back(v);
            }

            // Сколько строк нарисовал экран: по этой цифре видно, что экран
            // действительно отрисовался, а не остался пустым кадром.
            if (verbose)
                detail += "  .. " + std::string(sc.name) + modeSuffix +
                          ": строк " + std::to_string(lines.size()) + "\n";

            // 3. Наложение: чернила двух строк интерфейса пересекаются.
            //    Числа без букв не считаются — иначе каждый показатель HUD
            //    давал бы пару со своей подписью.
            for (size_t a = 0; a < lines.size(); ++a) {
                for (size_t b = a + 1; b < lines.size(); ++b) {
                    if (!lines[a].letters || !lines[b].letters) continue;
                    if (overlapW(lines[a].ink, lines[b].ink) <= 1) continue;
                    if (overlapH(lines[a].ink, lines[b].ink) <= 1) continue;
                    // Разные окна прокрутки не пересекаются на экране.
                    if (lines[a].cx1 <= lines[b].cx0 || lines[b].cx1 <= lines[a].cx0) continue;
                    if (lines[a].cy1 <= lines[b].cy0 || lines[b].cy1 <= lines[a].cy0) continue;
                    const std::string key = std::string(sc.name) + std::string(modeSuffix) + "|" +
                                            lines[a].text + "|" + lines[b].text;
                    char buf[512];
                    snprintf(buf, sizeof(buf),
                             "  [наложение] \"%s\" (%d,%d %dx%d) × \"%s\" (%d,%d %dx%d) при %dx%d\n",
                             lines[a].text.c_str(), lines[a].ink.x0, lines[a].ink.y0,
                             lines[a].ink.x1 - lines[a].ink.x0, lines[a].ink.y1 - lines[a].ink.y0,
                             lines[b].text.c_str(), lines[b].ink.x0, lines[b].ink.y0,
                             lines[b].ink.x1 - lines[b].ink.x0, lines[b].ink.y1 - lines[b].ink.y0,
                             SCREEN_W, SCREEN_H);
                    ++overlaps;
                    countByScreen[std::string(sc.name) + modeSuffix] += 1;
                    countByKind["наложение"] += 1;
                    if (!whereByKey.count(key)) whereByKey[key] = buf;
                    if (verbose) detail += buf;
                }
            }
        }
    }

    if (verbose && !detail.empty()) printf("%s\n", detail.c_str());

    if (!whereByKey.empty()) {
        printf("сводка по экранам:\n");
        for (const auto& kv : countByScreen) {
            printf("  %-16s %d замечаний\n", kv.first.c_str(), kv.second);
        }
        printf("\nгде именно:\n");
        for (const auto& kv : whereByKey) printf("%s", kv.second.c_str());
        printf("\n");
    }

    printf("\nпроверено: %d кадров x %d экранов x 3 интерфейса (компьютер, телефон "
           "и телефон с кнопками 160 %%), %lld строк\n",
           (int)frames.size(), kScreenCount, linesChecked);
    printf("вылетов: %d, наложений: %d, обрезок: %d\n", clips, overlaps, cuts);
    if (clips == 0 && overlaps == 0) {
        printf("ОК: ни одного вылета и ни одного наложения\n");
        return 0;
    }
    printf("ЕСТЬ ДЕФЕКТЫ ВЁРСТКИ\n");
    return 1;
}
