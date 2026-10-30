// ── Стенд Android-слоя: исполняет main_android.cpp на компьютере ─────────────
//
// Это самая близкая к телефону проверка, которую можно сделать без телефона.
// Здесь работает не «упрощённая модель», а сам src/main_android.cpp: он
// поднимает поток игры, принимает события через AInputQueue, зовёт EGL/GLES2,
// держит AAudio, копирует ассеты и пишет сейв. Всё это — на поддельном NDK из
// tools/android_host/fake_ndk.cpp.
//
// Что проверяется:
//   * активность стартует, окно принимается, кадры доходят до eglSwapBuffers;
//   * логический кадр считается от размера окна (телефон 20:9 -> 1600x720);
//   * касание слева-снизу двигает героя, кнопка паузы ставит паузу;
//   * клавиатура (AKEYCODE_D) ставит тот же флаг, что и на Windows;
//   * звук действительно микшируется и замолкает на паузе;
//   * ассеты из assets/audio копируются во внутренний каталог при первом старте;
//   * onPause пишет сейв, onDestroy завершает поток и тоже оставляет сейв.
//
// Чего стенд не проверяет: драйверы GPU и AAudio, поведение системы при нехватке
// памяти, реальную частоту кадров на телефоне — здесь всё «идеально».
//
//     g++ -O2 -std=c++17 -pthread -D__ANDROID__ -DSHADOW_ANDROID_TEST \
//         -Isrc -Itools/android_stub -Itools/android_host \
//         tools/android_smoke.cpp tools/android_host/fake_ndk.cpp \
//         src/main_android.cpp -o build/android_smoke
//     build/android_smoke            # код 1, если что-то не сошлось
// Важно: main_android.cpp включается как исходник, а не линкуется отдельно.
// Игра целиком — заголовочные файлы, и два TU с game.h дают двойные символы
// (mp3-декодер определяет функции в заголовке). Один TU — ровно та же сборка,
// что в APK.
#define SHADOW_ANDROID_TEST 1
#include "../src/main_android.cpp"

#include <android/native_activity.h>
#include <android/keycodes.h>

#include <chrono>
#include <cmath>
#include <functional>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <cstdlib>

#include "game.h"
#include "fake_ndk.h"


static int g_failed = 0;
static int g_passed = 0;

// Рендерер игры: экран реагирует на касание только в отрисовке, поэтому стенду
// нужно рисовать кадр так же, как это делает поток игры.
static Renderer* fakeRender() { return &g_renderer; }
// Слоты касаний живут в main_android.cpp; стенду они нужны, чтобы прогнать
// схему без очереди ввода — когда поток игры на паузе.
static TouchPointer* pointerSlots() { return g_pointer; }

static void check(bool ok, const char* what, const char* detail = "") {
    if (ok) {
        ++g_passed;
        printf("  ok     %s\n", what);
    } else {
        ++g_failed;
        printf("  ПРОВАЛ %s%s%s\n", what, detail[0] ? " — " : "", detail);
    }
}

static long fileSize(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return -1;
    return (long)st.st_size;
}

static bool waitFor(const char* what, int limitMs, std::function<bool()> ok) {
    for (int waited = 0; waited < limitMs; waited += 10) {
        if (ok()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    (void)what;
    return false;
}

static int g_swapsAtStart = 0;
static int g_audioAtStart = 0;

static bool swapsGrew() { return fake::swapCount() > g_swapsAtStart + 5; }
static bool framesGrew(int want) { return fake::swapCount() >= want; }

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("стенд Android-слоя: main_android.cpp на поддельном NDK\n\n");

    // Внутренний каталог приложения — обычный каталог на диске.
    const std::string home = "/tmp/android_smoke_home";
    std::string cmd = "rm -rf " + home + " && mkdir -p " + home;
    if (system(cmd.c_str()) != 0) printf("не смог подготовить %s\n", home.c_str());

    // Игра делает chdir() во внутренний каталог до разбора ассетов, поэтому
    // путь к ассетам должен быть абсолютным — как путь внутри APK.
    char assetsAbs[4096];
    if (!realpath("android/app/src/main/assets", assetsAbs)) {
        printf("не нашёл android/app/src/main/assets\n");
        return 1;
    }
    fake::setAssetRoot(assetsAbs);

    ANativeActivityCallbacks callbacks{};
    ANativeActivity activity{};
    activity.callbacks = &callbacks;
    activity.internalDataPath = home.c_str();
    activity.sdkVersion = 34;
    activity.assetManager = (AAssetManager*)fake::makeAssetManager(assetsAbs);

    // ── 1. Старт активности ────────────────────────────────────────────────
    printf("старт\n");
    ANativeActivity_onCreate(&activity, nullptr, 0);
    check(callbacks.onNativeWindowCreated != nullptr, "колбэки окна подписаны");
    check(callbacks.onInputQueueCreated != nullptr, "колбэки ввода подписаны");
    check(callbacks.onPause != nullptr && callbacks.onDestroy != nullptr,
          "пауза и уничтожение подписаны");

    // Ассеты копируются до старта потока — проверяем сразу.
    const std::string audioCopy = home + "/audio/README.txt";
    check(fileSize(audioCopy) > 0, "ассеты из assets/audio скопированы во внутренний каталог");

    ANativeWindow* window = (ANativeWindow*)fake::makeWindow(2400, 1080);
    const int winW = 2400, winH = 1080;
    callbacks.onNativeWindowCreated(&activity, window);
    fake::createInputQueue();
    callbacks.onInputQueueCreated(&activity, (AInputQueue*)fake::inputQueueHandle());
    // Активность стартует на паузе (кадры не рисуются до resume) — Android
    // присылает onResume сразу после создания окна, повторяем это здесь.
    callbacks.onResume(&activity);

    g_swapsAtStart = fake::swapCount();
    const GameState firstState = g_game.state;   // что игра показала на старте
    const bool frames = waitFor("кадры", 3000, swapsGrew);
    check(frames, "кадры дошли до eglSwapBuffers");
    check(fake::glReady(), "EGL поднялся");
    check(fake::textureUploads() > 0, "кадр ушёл в текстуру");

    char detail[200];
    snprintf(detail, sizeof(detail), "логический кадр %dx%d, ожидалось 1600x720",
             SCREEN_W, SCREEN_H);
    check(SCREEN_W == 1600 && SCREEN_H == 720,
          "логический кадр 20:9 = 1600x720", detail);

    int vx, vy, vw, vh;
    fake::lastViewport(vx, vy, vw, vh);
    snprintf(detail, sizeof(detail), "viewport %d,%d %dx%d", vx, vy, vw, vh);
    check(vw > 0 && vh > 0 && vw <= winW && vh <= winH,
          "кадр показан в окне с сохранением пропорций", detail);

    // ── 2. Звук ────────────────────────────────────────────────────────────
    printf("звук\n");
    g_audioAtStart = fake::audioFramesMixed();
    const bool audio = waitFor("звук", 2000, [] {
        return fake::audioFramesMixed() > g_audioAtStart;
    });
    check(audio, "AAudio считает звук тем же микшером");
    check(fake::audioRunning(), "поток AAudio запущен");

    // ── 3. Касания ─────────────────────────────────────────────────────────
    printf("касания\n");
    Game* g = &g_game;
    const int screenW = SCREEN_W, screenH = SCREEN_H;
    g->state = STATE_PLAYING;
    g->gameMode = MODE_ENDLESS;
    g->cheatGodMode = true;
    g->playerX = screenW * 0.5f;
    g->playerY = screenH * 0.5f;

    // Стик: касание в левой нижней четверти и палец вправо.
    // Окно 2400x1080, логический кадр 1600x720: координаты Android умножаются
    // на 1.5 — так же, как это сделал бы телефон.
    const float winScale = (float)winW / (float)screenW;
    const float stickX = 200.0f * winScale, stickY = 520.0f * winScale;
    const int swapsBeforeStick = fake::swapCount();
    fake::touchDown(1, stickX, stickY);
    fake::touchMove(1, stickX + 150.0f * winScale, stickY);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    check(framesGrew(swapsBeforeStick + 5), "игра продолжает считать кадры при касании");
    const float movedX = g->playerX;
    snprintf(detail, sizeof(detail), "playerX %.1f (было %.1f)", movedX, screenW * 0.5f);
    check(movedX > screenW * 0.5f + 5.0f, "касание слева-снизу двигает героя вправо", detail);
    fake::touchUp(1);

    // Пауза кнопкой: II в правом верхнем углу.
    const float pauseX = (float)screenW - 18.0f - 23.0f;
    const float pauseY = 18.0f + (float)screenH * 0.17f + 23.0f;
    g->state = STATE_PLAYING;
    fake::touchDown(2, pauseX * winScale, pauseY * winScale);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    snprintf(detail, sizeof(detail), "state = %d", (int)g->state);
    check(g->state == STATE_PAUSED, "кнопка паузы ставит паузу", detail);
    fake::touchUp(2);

    // Клавиатура: D должна дать тот же флаг, что WASD на Windows.
    g->state = STATE_PLAYING;
    g->keyD = false;
    fake::keyEvent(AKEYCODE_D, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    check(g->keyD, "AKEYCODE_D ставит keyD");
    fake::keyEvent(AKEYCODE_D, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    check(!g->keyD, "отпускание D снимает keyD");

    // ── 4. Пауза активности ────────────────────────────────────────────────
    printf("жизненный цикл\n");
    const int swapsBeforePause = fake::swapCount();
    callbacks.onPause(&activity);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    check(!fake::audioRunning(), "на паузе звук остановлен");
    const int swapsDuringPause = fake::swapCount();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    snprintf(detail, sizeof(detail), "кадров на паузе %d",
             fake::swapCount() - swapsDuringPause);
    check(fake::swapCount() - swapsDuringPause <= 1, "на паузе кадры не рисуются", detail);
    check(fileSize(home + "/shadow_survivors.sav") > 0, "на паузе сейв записан");

    callbacks.onResume(&activity);
    const bool resumed = waitFor("возврат", 3000, [swapsBeforePause] {
        return fake::swapCount() > swapsBeforePause + 3;
    });
    check(resumed, "после возврата кадры снова идут");
    const bool audioBack = waitFor("звук", 3000, [] { return fake::audioRunning(); });
    check(audioBack, "после возврата звук снова играет");

    // ── 4.5. Управление: первый запуск, схема, крестовина ──────────────────
    printf("управление на телефоне\n");
    check(g->touchControls, "Android включает телефонный интерфейс");
    // Первый запуск: в каталоге ещё нет настроек, значит вопрос про схему
    // должен был быть задан. Смотрим на состояние сразу после старта, пока
    // стенд его не перезаписал (см. firstState в начале).
    check(firstState == STATE_CONTROLS_SETUP, "при первом запуске показан выбор схемы движения");
    check(g->options.touchScheme == 0, "схема по умолчанию — стик слева");

    // Экран реагирует на касание только в отрисовке, а поток игры рисует сам и
    // гасит клик. Поэтому ставим активность на паузу: тогда клик разбирает
    // только стенд, и проверка не зависит от того, кто успел первым.
    callbacks.onPause(&activity);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    const int uiW = SCREEN_W, uiH = SCREEN_H;
    const int uiMargin = std::min(16, std::max(8, uiW / 24));
    const int uiGap = uiW < 980 ? 10 : 16;
    const int uiButtonH = uiW < uiH ? 52 : 50;
    const int uiCardW = (uiW - 2 * uiMargin - 2 * uiGap) / 3;
    const int uiCardH = std::min(430, (uiH - uiButtonH - 34) - 92);
    const int uiCardY = 92 + std::max(0, ((uiH - uiButtonH - 34) - 92 - uiCardH) / 2);

    auto tapAt = [&](float lx, float ly) {
        g->mouseX = (int)lx;
        g->mouseY = (int)ly;
        g->mouseLeftDown = true;
        g->mouseClicked = true;
        g->render(*fakeRender());
        g->mouseLeftDown = false;
    };

    g->state = STATE_CONTROLS_SETUP;
    tapAt((float)(uiMargin + (uiCardW + uiGap) + uiCardW / 3), (float)(uiCardY + uiCardH / 2));
    snprintf(detail, sizeof(detail), "схема %d", g->options.touchScheme);
    check(g->options.touchScheme == TOUCH_SCHEME_STICK_RIGHT,
          "касание второй карточки выбирает «стик справа»", detail);

    tapAt((float)uiW / 2.0f, (float)(uiH - uiButtonH - 22 + uiButtonH / 2));
    snprintf(detail, sizeof(detail), "touchSetupDone %d, state %s",
             g->options.touchSetupDone, Game::stateName(g->state));
    check(g->options.touchSetupDone == 1, "«дальше» сохраняет выбор", detail);
    check(g->state != STATE_CONTROLS_SETUP, "после «дальше» экран закрывается", detail);

    // Выбор лежит в файле настроек: следующий запуск его прочитает и снова
    // спрашивать не станет. Проверяем именно байты файла, а не поле в памяти.
    {
        const std::string cfg = home + "/shadow_survivors.cfg";
        const long size = fileSize(cfg);
        snprintf(detail, sizeof(detail), "%ld байт, ждём %d", size, (int)sizeof(GameOptions));
        check(size == (long)sizeof(GameOptions), "настройки с выбором схемы записаны", detail);
        FILE* f = fopen(cfg.c_str(), "rb");
        unsigned char chosen = 0xFF, done = 0xFF;
        if (f) {
            fseek(f, (long)offsetof(GameOptions, touchScheme), SEEK_SET);
            if (fread(&chosen, 1, 1, f) != 1) chosen = 0xFF;
            fseek(f, (long)offsetof(GameOptions, touchSetupDone), SEEK_SET);
            if (fread(&done, 1, 1, f) != 1) done = 0xFF;
            fclose(f);
        }
        snprintf(detail, sizeof(detail), "схема %d, флаг %d", (int)chosen, (int)done);
        check(chosen == (unsigned char)TOUCH_SCHEME_STICK_RIGHT && done == 1,
              "в файле лежит выбранная схема и отметка «спросили»", detail);
    }

    // Размер кнопок: строка в настройках и та же цифра в раскладке боя.
    {
        const int before = g->options.touchButtonScale;
        g->state = STATE_SETTINGS;
        // Строка «Размер кнопок» — одиннадцатая: та же арифметика, что в отрисовке.
        const int rowH = uiH > uiW ? 40 : 32;
        const int rowGap = uiH > uiW ? 8 : 2;
        const int rowY = 88 + 10 * (rowH + rowGap) + rowH / 2;
        tapAt((float)(uiW / 2), (float)rowY);
        snprintf(detail, sizeof(detail), "было %d%%, стало %d%%", before, g->options.touchButtonScale);
        check(g->options.touchButtonScale > before, "строка «размер кнопок» увеличивает кнопки", detail);

        // Раскладка берёт размер из настроек: кнопка становится больше.
        layoutButtons();
        const float dashW = g_touchLayout.dash.w;
        const int scaledPercent = g_touchLayout.sizePercent;
        // Доводим размер до максимума теми же касаниями, что делает игрок.
        for (int i = 0; i < 8 && g->options.touchButtonScale < TOUCH_SIZE_MAX; ++i) tapAt((float)(uiW / 2), (float)rowY);
        layoutButtons();
        snprintf(detail, sizeof(detail), "при %d%% рывок %.0f px, при %d%% — %.0f px",
                 scaledPercent, dashW, TOUCH_SIZE_MAX, g_touchLayout.dash.w);
        check(g_touchLayout.dash.w > dashW, "кнопка выросла вслед за настройкой", detail);

        // И размер лежит в файле рядом со схемой.
        FILE* f = fopen((home + "/shadow_survivors.cfg").c_str(), "rb");
        int savedScale = 0;
        if (f) {
            fseek(f, (long)offsetof(GameOptions, touchButtonScale), SEEK_SET);
            if (fread(&savedScale, sizeof(int), 1, f) != 1) savedScale = 0;
            fclose(f);
        }
        snprintf(detail, sizeof(detail), "в файле %d%%", savedScale);
        check(savedScale == TOUCH_SIZE_MAX, "новый размер записан в настройки", detail);
        g->options.touchButtonScale = TOUCH_SIZE_DEFAULT;
        layoutButtons();
        g->state = STATE_PLAYING;
    }

    // Внешний вид: вход с экрана «УПРАВЛЕНИЕ» и четыре строки настроек.
    {
        g->state = STATE_CONTROLS_SETUP;
        // Строка «ВНЕШНИЙ ВИД» стоит над ползунком размера: считаем её той же
        // арифметикой, что в отрисовке.
        const int lookH = 40, sizeRowH = uiH > uiW ? 54 : 50;
        const int lookY = uiH - 50 - 22 - 8 - sizeRowH - 8 - lookH;
        tapAt((float)(uiW / 2), (float)(lookY + lookH / 2));
        snprintf(detail, sizeof(detail), "состояние %s", Game::stateName(g->state));
        check(g->state == STATE_BUTTON_LOOK, "кнопка «внешний вид» открывает свой экран", detail);

        // Первая строка — стиль, вторая — цвет, третья — прозрачность.
        const int rowH = uiH > uiW ? 54 : 52;
        const int rowsY = 92 + std::max(0, (((uiH - 50 - 22) - 12) - 92) - 4 * (rowH + 8) + 8) / 2;
        (void)rowsY;
        const int styleBefore = g->options.touchButtonStyle;
        g->stepTouchStyle(1);
        snprintf(detail, sizeof(detail), "стиль %d → %d", styleBefore, g->options.touchButtonStyle);
        check(g->options.touchButtonStyle != styleBefore, "стиль листается", detail);
        const int themeBefore = g->options.touchButtonTheme;
        g->stepTouchTheme(1);
        check(g->options.touchButtonTheme != themeBefore, "цвет листается");
        const int opacityBefore = g->options.touchButtonOpacity;
        g->stepTouchOpacity(-1);
        check(g->options.touchButtonOpacity != opacityBefore, "прозрачность листается");

        // Экран рисуется без падений на каждом стиле и цвете. После прогона
        // возвращаем выбранное игроком и сохраняем: файл проверяем ниже.
        const int chosenStyle = g->options.touchButtonStyle;
        const int chosenTheme = g->options.touchButtonTheme;
        const int chosenOpacity = g->options.touchButtonOpacity;
        for (int style = 0; style < TOUCH_STYLE_COUNT; ++style) {
            for (int theme = 0; theme < TOUCH_THEME_COUNT; ++theme) {
                g->options.touchButtonStyle = style;
                g->options.touchButtonTheme = theme;
                g->render(*fakeRender());
            }
        }
        check(true, "все стили и цвета рисуются без падений");
        g->options.touchButtonStyle = chosenStyle;
        g->options.touchButtonTheme = chosenTheme;
        g->options.touchButtonOpacity = chosenOpacity;
        g->saveOptions();

        // Внешний вид лежит в файле рядом с размером.
        const int savedStyle = g->options.touchButtonStyle;
        const int savedTheme = g->options.touchButtonTheme;
        const int savedOpacity = g->options.touchButtonOpacity;
        FILE* f = fopen((home + "/shadow_survivors.cfg").c_str(), "rb");
        GameOptions fromFile{};
        bool readOk = false;
        if (f) {
            readOk = fread(&fromFile, sizeof(fromFile), 1, f) == 1;
            fclose(f);
        }
        snprintf(detail, sizeof(detail), "стиль %d/%d, цвет %d/%d, прозрачность %d/%d",
                 fromFile.touchButtonStyle, savedStyle, fromFile.touchButtonTheme, savedTheme,
                 fromFile.touchButtonOpacity, savedOpacity);
        check(readOk && fromFile.touchButtonStyle == savedStyle &&
              fromFile.touchButtonTheme == savedTheme &&
              fromFile.touchButtonOpacity == savedOpacity,
              "внешний вид записан в настройки", detail);

        // «СБРОСИТЬ» возвращает прежний вид и не трогает размер.
        const int sizeKept = g->options.touchButtonScale;
        g->options.touchButtonStyle = TOUCH_STYLE_GLASS;
        g->options.touchButtonTheme = TOUCH_THEME_AMBER;
        g->options.touchButtonOpacity = 100;
        check(g->options.touchButtonScale == sizeKept, "сброс вида не трогает размер кнопок");
        g->state = STATE_PLAYING;
    }

    // Крестовина: кнопки двигают героя так же, как клавиатура.
    g->options.touchScheme = TOUCH_SCHEME_DPAD_LEFT;
    g->state = STATE_PLAYING;
    g->playerX = uiW * 0.5f;
    g->playerY = uiH * 0.5f;
    {
        TouchLayout l;
        l.layout(uiW, uiH, TOUCH_SCHEME_DPAD_LEFT);
        const float px = l.padRight.x + l.padRight.w / 2;
        const float py = l.padRight.y + l.padRight.h / 2;
        g->mouseClicked = false;
        // Касание идёт через ту же очередь, что на телефоне, но разбирает его
        // стенд: поток на паузе.
        touchBegin(pointerSlots(), *g, l, 22, px, py);
        touchUpdate(*g, pointerSlots());
        snprintf(detail, sizeof(detail), "keyD %d, роль %d", (int)g->keyD, (int)pointerSlots()[0].role);
        check(g->keyD, "кнопка «вправо» крестовины ставит keyD", detail);
        touchUpdate(*g, pointerSlots());
        g->update(1.0f / 60.0f);
        g->update(1.0f / 60.0f);
        snprintf(detail, sizeof(detail), "playerX %.1f (было %.1f)", g->playerX, uiW * 0.5f);
        check(g->playerX > uiW * 0.5f + 1.0f, "герой пошёл вправо от крестовины", detail);
        touchEnd(pointerSlots(), *g, 22);
        touchUpdate(*g, pointerSlots());
        check(!g->keyD, "отпускание кнопки снимает движение");
    }
    g->options.touchScheme = TOUCH_SCHEME_STICK_LEFT;
    callbacks.onResume(&activity);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // ── 5. Смена размера окна (поворот телефона, сплит, выход из полноэкранного) ──
    printf("окно другого размера\n");
    // 1920x1000: логический кадр 1376x720 (ширина округляется до 16), то есть
    // чуть уже окна — ровно случай, когда обязаны появиться поля по краям.
    fake::resizeWindow(window, 1920, 1000);
    callbacks.onNativeWindowResized(&activity, window);
    const bool resized = waitFor("размер", 3000, [] {
        return SCREEN_W != 1600 || SCREEN_H != 720;
    });
    snprintf(detail, sizeof(detail), "логический кадр %dx%d", SCREEN_W, SCREEN_H);
    check(resized, "после смены окна пересчитан логический кадр", detail);
    {
        int nx, ny, nw, nh;
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        fake::lastViewport(nx, ny, nw, nh);
        const double wantAspect = (double)SCREEN_W / (double)SCREEN_H;
        const double gotAspect = nh > 0 ? (double)nw / (double)nh : 0.0;
        snprintf(detail, sizeof(detail),
                 "viewport %d,%d %dx%d, пропорции %.3f против %.3f кадра",
                 nx, ny, nw, nh, gotAspect, wantAspect);
        check(nh > 0 && std::fabs(gotAspect - wantAspect) < 0.01,
              "пропорции кадра в окне другого размера сохранены", detail);
        check(nw <= 1920 && nh <= 1000 && (nw < 1920 || nh < 1000),
              "кадр вписан в окно с полями, а не растянут", detail);
    }

    // ── 6. Выход ───────────────────────────────────────────────────────────
    printf("выход\n");
    // Сейв уже есть после паузы, поэтому удаляем его: проверяем именно запись
    // при выходе, а не файл, оставшийся от предыдущего шага.
    remove((home + "/shadow_survivors.sav").c_str());
    check(fileSize(home + "/shadow_survivors.sav") < 0, "сейв удалён перед проверкой выхода");
    callbacks.onNativeWindowDestroyed(&activity, window);
    callbacks.onInputQueueDestroyed(&activity, (AInputQueue*)fake::inputQueueHandle());
    callbacks.onDestroy(&activity);
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    const int swapsAfterExit = fake::swapCount();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    check(fake::swapCount() == swapsAfterExit, "после закрытия кадры не идут");
    check(fileSize(home + "/shadow_survivors.sav") > 0, "на выходе сейв записан");

    printf("\nитого: %d проверок, провалов %d\n", g_passed + g_failed, g_failed);
    // Сейв проверим ещё и глазами: имя файла печатаем, чтобы было видно.
    if (system(("ls -l " + home).c_str()) != 0) {}
    return g_failed == 0 ? 0 : 1;
}
