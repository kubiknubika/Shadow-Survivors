// ── Снимок экрана игры в BMP ─────────────────────────────────────────────────
//
// Стенд для глазами-проверок: рисует один экран игры в заданном логическом
// размере и пишет BMP (24-бит он же 32-бит с альфой, читается любым вьюером).
// Нужен там, где хеша мало: хеш говорит «картинка изменилась», а снимок — что
// именно поехало. Например, лишняя строка в настройках могла наехать на
// ползунки громкости, и это видно только на снимке.
//
//     g++ -O2 -std=c++17 -pthread -Isrc tools/screen_shot.cpp -o build/shot
//     build/shot settings 1280x720 settings.bmp
//     build/shot settings 720x1280 settings_portrait.bmp 2
//     build/shot play 960x720 play.bmp 1 3
//
// Аргументы: экран, размер, файл, [потоков], [ступень графики 1..4].
// Экраны: menu, settings, help, patchnotes, hall, hero, play, pause, chest,
// levelup, debug (бой с открытой панелью F3).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include "renderer.h"
#include "game.h"

static void writeBmp(const char* path, const uint32_t* px, int w, int h) {
    const int dataSize = w * 4 * h;
    unsigned char header[54] = {0};
    header[0] = 'B'; header[1] = 'M';
    const int fileSize = 54 + dataSize;
    header[2] = fileSize & 0xFF; header[3] = (fileSize >> 8) & 0xFF;
    header[4] = (fileSize >> 16) & 0xFF; header[5] = (fileSize >> 24) & 0xFF;
    header[10] = 54;
    header[14] = 40;
    header[18] = w & 0xFF; header[19] = (w >> 8) & 0xFF;
    header[22] = h & 0xFF; header[23] = (h >> 8) & 0xFF;
    header[26] = 1; header[28] = 32;
    header[34] = dataSize & 0xFF; header[35] = (dataSize >> 8) & 0xFF;
    header[36] = (dataSize >> 16) & 0xFF; header[37] = (dataSize >> 24) & 0xFF;
    FILE* f = fopen(path, "wb");
    if (!f) { printf("не смог открыть %s\n", path); return; }
    fwrite(header, 1, 54, f);
    for (int y = h - 1; y >= 0; --y) fwrite(px + (size_t)y * w, 4, (size_t)w, f);
    fclose(f);
}

static bool parseSize(const char* s, int& w, int& h) {
    const char* x = strchr(s, 'x');
    if (!x) return false;
    w = atoi(s);
    h = atoi(x + 1);
    return w >= 240 && h >= 240;
}

int main(int argc, char** argv) {
    const char* screen = argc > 1 ? argv[1] : "settings";
    int w = 1280, h = 720;
    if (argc > 2) parseSize(argv[2], w, h);
    const char* out = argc > 3 ? argv[3] : "/tmp/shot.bmp";
    const int threads = argc > 4 ? std::max(1, atoi(argv[4])) : 1;
    const int graphics = argc > 5 ? std::max(1, std::min(4, atoi(argv[5]))) : 1;
    // Любой аргумент «touch» включает телефонный интерфейс: подсказки без
    // названий клавиш и экран выбора схемы движения.
    bool touchUi = false;
    int buttonScale = TOUCH_SIZE_DEFAULT;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "touch") == 0) touchUi = true;
        // size140 — тот же экран с размером кнопок 140 %: так видно, что настройка
        // двигает геометрию, а не только цифру в строке.
        else if (strncmp(argv[i], "size", 4) == 0) buttonScale = atoi(argv[i] + 4);
    }

    struct Case { const char* name; int state; } cases[] = {
        {"menu", STATE_MENU}, {"settings", STATE_SETTINGS}, {"help", STATE_HELP},
        {"patchnotes", STATE_PATCH_NOTES}, {"hall", STATE_HALL}, {"hero", STATE_HERO_SELECT},
        {"play", STATE_PLAYING}, {"pause", STATE_PAUSED}, {"chest", STATE_CHEST},
        {"levelup", STATE_LEVELUP}, {"controls", STATE_CONTROLS_SETUP},
        {"look", STATE_BUTTON_LOOK},
    };
    int state = strcmp(screen, "debug") == 0 ? STATE_PLAYING : STATE_SETTINGS;
    for (const Case& c : cases) if (strcmp(c.name, screen) == 0) state = c.state;

    SCREEN_W = w; SCREEN_H = h;
    Renderer r;
    r.resize(w, h);
    Game g;
    g.simulationMode = true;
    g.touchControls = touchUi;
    srand(4242);
    g.init();
    g.gameMode = MODE_ENDLESS;
    g.runTime = 240.0f;
    g.cheatGodMode = true;
    g.options.graphicsLevel = graphics;
    g.options.touchButtonScale = touchClampSize(buttonScale);
    g.renderThreads = threads;
    for (int i = 0; i < 5; ++i) g.itemLevels[i] = 3;
    g.state = (GameState)state;
    if (state == STATE_PLAYING || state == STATE_LEVELUP || state == STATE_CHEST || state == STATE_PAUSED) {
        for (int i = 0; i < 60; ++i) g.spawnMonster(i % 8, i % 9 == 0, false);
        for (int f = 0; f < 3; ++f) g.update(1.0f / 60.0f);
    }
    g.state = (GameState)state;
    // «debug» — та же сцена боя с открытой панелью F3: там видна строка
    // «ГРАФИКА» (уровень, ступень, цена кадра, потоки).
    if (strcmp(screen, "debug") == 0) g.debugOverlay = true;
    r.clear(0xFF000000);
    g.render(r);
    writeBmp(out, r.pixels, w, h);
    printf("%s  %dx%d  потоков %d  ступень %d\n", out, w, h, threads, graphics);
    return 0;
}
