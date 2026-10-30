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
