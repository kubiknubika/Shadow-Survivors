// ── Android: платформа для той же игры ───────────────────────────────────────
//
// Игра (src/game.h + src/renderer.h) ничего не знает про окно: она рисует кадр
// в память и читает мышь с клавиатурой из полей Game. Значит порт — это ровно
// три вещи:
//
//   1. показать кадр (EGL + GLES2: залить pixels в текстуру и растянуть квадом);
//   2. превратить касания в мышь и клавиши (виртуальный стик, кнопки, тап);
//   3. дать звук (AAudio зовёт sound.mixSamples — тот же микшер, что на Windows).
//
// Ничего из этого не живёт в game.h, поэтому Windows-сборка не меняется.
//
// Точка входа — ANativeActivity_onCreate: манифест объявляет android.app.NativeActivity
// с android.app.lib_name = shadow_survivors, поэтому Java-кода в проекте нет
// вообще. Колбэки активности приходят в главном потоке Android, а игра крутится
// в своём потоке: колбэки только складывают события под замком, поток игры их
// разбирает. Иначе первый же блокирующий вызов остановил бы систему.
//
// Сборка: android/app/src/main/cpp/CMakeLists.txt (см. design/android.md).
#ifdef __ANDROID__

#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/looper.h>
#include <android/asset_manager.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <aaudio/AAudio.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "version.h"
#include "game.h"
#include "touch_input.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ShadowSurvivors", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "ShadowSurvivors", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ShadowSurvivors", __VA_ARGS__)

static Game g_game;
static Renderer g_renderer;

// ── События из главного потока Android в поток игры ─────────────────────────
struct AppLink {
    ANativeActivity* activity = nullptr;
    std::mutex m;
    std::condition_variable cv;

    ANativeWindow* windowToTake = nullptr;   // пришёл onNativeWindowCreated
    bool windowToDrop = false;               // пришёл onNativeWindowDestroyed
    AInputQueue* inputToTake = nullptr;      // пришёл onInputQueueCreated
    bool inputToDrop = false;
    bool resized = false;
    bool paused = true;                      // активность на паузе — кадры не рисуем
    bool saveRequest = false;                // сохраниться на паузе (сейв пишет только поток игры)
    bool quit = false;
};
static AppLink g_link;

// ── Графика (EGL + GLES2) ────────────────────────────────────────────────────
struct Gfx {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    EGLConfig config = nullptr;
    GLuint texture = 0;
    GLuint program = 0;
    GLint attrPos = -1, attrUv = -1, uniTex = -1;
    int texW = 0, texH = 0;
    int winW = 0, winH = 0;
    bool ready = false;
};
static Gfx g_gfx;

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512] = {0};
        glGetShaderInfoLog(s, sizeof(log) - 1, nullptr, log);
        LOGE("шейдер не собрался: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

// Кадр игры — уже готовая картинка, поэтому вся графика сводится к одному
// кваду на весь экран: текстура, линейный фильтр, чёрные поля по краям.
static bool initGl() {
    static const char* vs =
        "attribute vec2 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "varying vec2 v_uv;\n"
        "void main() { v_uv = a_uv; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
    static const char* fs =
        "precision mediump float;\n"
        "varying vec2 v_uv;\n"
        "uniform sampler2D u_tex;\n"
        "void main() { gl_FragColor = texture2D(u_tex, v_uv); }\n";

    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) return false;
    g_gfx.program = glCreateProgram();
    glAttachShader(g_gfx.program, v);
    glAttachShader(g_gfx.program, f);
    glLinkProgram(g_gfx.program);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(g_gfx.program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512] = {0};
        glGetProgramInfoLog(g_gfx.program, sizeof(log) - 1, nullptr, log);
        LOGE("программа не слинковалась: %s", log);
        return false;
    }
    g_gfx.attrPos = glGetAttribLocation(g_gfx.program, "a_pos");
    g_gfx.attrUv = glGetAttribLocation(g_gfx.program, "a_uv");
    g_gfx.uniTex = glGetUniformLocation(g_gfx.program, "u_tex");

    glGenTextures(1, &g_gfx.texture);
    glBindTexture(GL_TEXTURE_2D, g_gfx.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return true;
}

static void destroyGl() {
    if (g_gfx.texture) { glDeleteTextures(1, &g_gfx.texture); g_gfx.texture = 0; }
    if (g_gfx.program) { glDeleteProgram(g_gfx.program); g_gfx.program = 0; }
    if (g_gfx.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_gfx.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_gfx.context != EGL_NO_CONTEXT) eglDestroyContext(g_gfx.display, g_gfx.context);
        if (g_gfx.surface != EGL_NO_SURFACE) eglDestroySurface(g_gfx.display, g_gfx.surface);
        eglTerminate(g_gfx.display);
    }
    g_gfx.display = EGL_NO_DISPLAY;
    g_gfx.surface = EGL_NO_SURFACE;
    g_gfx.context = EGL_NO_CONTEXT;
    g_gfx.ready = false;
}

static bool startGl(ANativeWindow* window) {
    g_gfx.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_gfx.display == EGL_NO_DISPLAY) { LOGE("нет EGL-дисплея"); return false; }
    if (!eglInitialize(g_gfx.display, nullptr, nullptr)) { LOGE("eglInitialize"); return false; }

    const EGLint cfgAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    EGLint num = 0;
    if (!eglChooseConfig(g_gfx.display, cfgAttribs, &g_gfx.config, 1, &num) || num < 1) {
        LOGE("eglChooseConfig: подходящего конфига нет");
        return false;
    }
    const EGLint winAttribs[] = {EGL_NONE};
    g_gfx.surface = eglCreateWindowSurface(g_gfx.display, g_gfx.config, window, winAttribs);
    if (g_gfx.surface == EGL_NO_SURFACE) { LOGE("eglCreateWindowSurface"); return false; }

    const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    g_gfx.context = eglCreateContext(g_gfx.display, g_gfx.config, EGL_NO_CONTEXT, ctxAttribs);
    if (g_gfx.context == EGL_NO_CONTEXT) { LOGE("eglCreateContext"); return false; }
    if (!eglMakeCurrent(g_gfx.display, g_gfx.surface, g_gfx.surface, g_gfx.context)) {
        LOGE("eglMakeCurrent");
        return false;
    }
    // Просим вертикальную синхронизацию: кадр не рвётся, и цикл сам садится на
    // частоту экрана (60/90/120 Гц), а не крутится вхолостую.
    eglSwapInterval(g_gfx.display, 1);
    if (!initGl()) return false;
    g_gfx.texW = g_gfx.texH = 0;
    g_gfx.ready = true;
    return true;
}

// Логический кадр -> прямоугольник в окне с сохранением пропорций (чёрные поля,
// а не растянутая картинка). Те же числа нужны и для перевода касаний.
static void viewRect(int& x, int& y, int& w, int& h) {
    const int vw = g_gfx.winW > 0 ? g_gfx.winW : SCREEN_W;
    const int vh = g_gfx.winH > 0 ? g_gfx.winH : SCREEN_H;
    const float srcAspect = (float)SCREEN_W / (float)SCREEN_H;
    const float dstAspect = (float)vw / (float)vh;
    if (dstAspect > srcAspect) {
        h = vh;
        w = (int)((float)vh * srcAspect + 0.5f);
        x = (vw - w) / 2;
        y = 0;
    } else {
        w = vw;
        h = (int)((float)vw / srcAspect + 0.5f);
        x = 0;
        y = (vh - h) / 2;
    }
    if (w < 1) w = 1;
    if (h < 1) h = 1;
}

static void present() {
    if (!g_gfx.ready) return;
    if (g_gfx.texW != SCREEN_W || g_gfx.texH != SCREEN_H) {
        glBindTexture(GL_TEXTURE_2D, g_gfx.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_W, SCREEN_H, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, g_renderer.pixels);
        g_gfx.texW = SCREEN_W;
        g_gfx.texH = SCREEN_H;
    } else {
        glBindTexture(GL_TEXTURE_2D, g_gfx.texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_W, SCREEN_H,
                        GL_RGBA, GL_UNSIGNED_BYTE, g_renderer.pixels);
    }

    int vx, vy, vw, vh;
    viewRect(vx, vy, vw, vh);
    glViewport(0, 0, g_gfx.winW, g_gfx.winH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(vx, g_gfx.winH - vy - vh, vw, vh);

    static const GLfloat pos[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
    static const GLfloat uv[] = {0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    glUseProgram(g_gfx.program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_gfx.texture);
    glUniform1i(g_gfx.uniTex, 0);
    glEnableVertexAttribArray((GLuint)g_gfx.attrPos);
    glVertexAttribPointer((GLuint)g_gfx.attrPos, 2, GL_FLOAT, GL_FALSE, 0, pos);
    glEnableVertexAttribArray((GLuint)g_gfx.attrUv);
    glVertexAttribPointer((GLuint)g_gfx.attrUv, 2, GL_FLOAT, GL_FALSE, 0, uv);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray((GLuint)g_gfx.attrPos);
    glDisableVertexAttribArray((GLuint)g_gfx.attrUv);
    eglSwapBuffers(g_gfx.display, g_gfx.surface);
}

// ── Звук (AAudio) ────────────────────────────────────────────────────────────
static AAudioStream* g_audio = nullptr;

static aaudio_data_callback_result_t audioCallback(AAudioStream* stream, void*, void* audioData, int32_t frames) {
    int16_t* out = (int16_t*)audioData;
    const int channels = AAudioStream_getChannelCount(stream);
    if (channels <= 1) {
        g_game.sound.mixSamples(out, (int)frames);
    } else {
        // Микшер игры монофонический: считаем в буфер и раскладываем по каналам.
        static std::vector<int16_t> mono;
        if ((int)mono.size() < frames) mono.resize((size_t)frames);
        g_game.sound.mixSamples(mono.data(), (int)frames);
        for (int i = 0; i < frames; ++i)
            for (int c = 0; c < channels; ++c) out[i * channels + c] = mono[(size_t)i];
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void startAudio() {
    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) { LOGW("AAudio: нет билдера"); return; }
    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    // Микшер игры считает на 44100 — просим тот же формат, чтобы не менять
    // высоту тона. Если устройство отдаст другой, AAudio отресемплирует сам.
    AAudioStreamBuilder_setSampleRate(builder, 44100);
    AAudioStreamBuilder_setChannelCount(builder, 1);
    AAudioStreamBuilder_setDataCallback(builder, audioCallback, nullptr);
    const aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &g_audio);
    AAudioStreamBuilder_delete(builder);
    if (result != AAUDIO_OK || !g_audio) { LOGW("AAudio: поток не открылся (%d)", (int)result); g_audio = nullptr; return; }
    if (AAudioStream_requestStart(g_audio) != AAUDIO_OK) { LOGW("AAudio: поток не запустился"); }
}

static void stopAudio() {
    if (!g_audio) return;
    AAudioStream_requestStop(g_audio);
    AAudioStream_close(g_audio);
    g_audio = nullptr;
}

// ── Касания ─────────────────────────────────────────────────────────────────
// Схема управления живёт в src/touch_input.h: её рисует и проверяет десктопный
// стенд tools/touch_preview.cpp, а телефон использует тот же код.
static TouchPointer g_pointer[TOUCH_MAX_POINTERS];
static TouchLayout g_touchLayout;

// Схему движения выбирает игрок (экран «УПРАВЛЕНИЕ»): раскладка пересчитывается
// на кадр, поэтому смена схемы видна сразу, без перезапуска.
static void layoutButtons() {
    g_touchLayout.layout(SCREEN_W, SCREEN_H, g_game.options.touchScheme);
}

// Экранные координаты -> логический кадр (обратное к viewRect).
static void toLogical(float sx, float sy, float& lx, float& ly) {
    int vx, vy, vw, vh;
    viewRect(vx, vy, vw, vh);
    lx = (sx - (float)vx) * (float)SCREEN_W / (float)vw;
    ly = (sy - (float)vy) * (float)SCREEN_H / (float)vh;
    if (lx < 0.0f) lx = 0.0f;
    if (ly < 0.0f) ly = 0.0f;
    if (lx > SCREEN_W - 1) lx = (float)(SCREEN_W - 1);
    if (ly > SCREEN_H - 1) ly = (float)(SCREEN_H - 1);
}

// ── Ввод из Android ─────────────────────────────────────────────────────────
static int32_t pointerIndexFromAction(int32_t action) {
    return (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
}

static void handleMotion(AInputEvent* event) {
    const int32_t action = AMotionEvent_getAction(event);
    const int32_t kind = action & AMOTION_EVENT_ACTION_MASK;
    const size_t count = AMotionEvent_getPointerCount(event);
    if (kind == AMOTION_EVENT_ACTION_DOWN || kind == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        const size_t idx = (size_t)pointerIndexFromAction(action);
        if (idx < count) {
            const int32_t id = AMotionEvent_getPointerId(event, idx);
            float lx, ly;
            toLogical(AMotionEvent_getX(event, idx), AMotionEvent_getY(event, idx), lx, ly);
            touchBegin(g_pointer, g_game, g_touchLayout, id, lx, ly);
        }
    } else if (kind == AMOTION_EVENT_ACTION_MOVE) {
        for (size_t i = 0; i < count; ++i) {
            const int32_t id = AMotionEvent_getPointerId(event, i);
            float lx, ly;
            toLogical(AMotionEvent_getX(event, i), AMotionEvent_getY(event, i), lx, ly);
            touchMove(g_pointer, g_game, id, lx, ly);
        }
    } else if (kind == AMOTION_EVENT_ACTION_UP || kind == AMOTION_EVENT_ACTION_POINTER_UP) {
        const size_t idx = (size_t)pointerIndexFromAction(action);
        if (idx < count) touchEnd(g_pointer, g_game, AMotionEvent_getPointerId(event, idx));
    } else if (kind == AMOTION_EVENT_ACTION_CANCEL) {
        for (int i = 0; i < TOUCH_MAX_POINTERS; ++i) touchEnd(g_pointer, g_game, g_pointer[i].id);
    }
}

static void handleKey(AInputEvent* event) {
    const int32_t code = AKeyEvent_getKeyCode(event);
    const int32_t action = AKeyEvent_getAction(event);
    const bool down = action == AKEY_EVENT_ACTION_DOWN;
    if (!down && action != AKEY_EVENT_ACTION_UP) return;
    if (!down) {
        switch (code) {
            case AKEYCODE_W: case AKEYCODE_DPAD_UP: g_game.keyW = g_game.keyUp = false; break;
            case AKEYCODE_S: case AKEYCODE_DPAD_DOWN: g_game.keyS = g_game.keyDown = false; break;
            case AKEYCODE_A: case AKEYCODE_DPAD_LEFT: g_game.keyA = g_game.keyLeft = false; break;
            case AKEYCODE_D: case AKEYCODE_DPAD_RIGHT: g_game.keyD = g_game.keyRight = false; break;
            default: break;
        }
        return;
    }
    switch (code) {
        case AKEYCODE_W: case AKEYCODE_DPAD_UP: g_game.keyW = g_game.keyUp = true; break;
        case AKEYCODE_S: case AKEYCODE_DPAD_DOWN: g_game.keyS = g_game.keyDown = true; break;
        case AKEYCODE_A: case AKEYCODE_DPAD_LEFT: g_game.keyA = g_game.keyLeft = true; break;
        case AKEYCODE_D: case AKEYCODE_DPAD_RIGHT: g_game.keyD = g_game.keyRight = true; break;
        case AKEYCODE_SPACE: g_game.onKeyPress(32); break;
        case AKEYCODE_Q: g_game.onKeyPress('q'); break;
        case AKEYCODE_BACK: g_game.onKeyPress(27); break;
        case AKEYCODE_F3: g_game.debugOverlay = !g_game.debugOverlay; break;
        default: break;
    }
}

static void handleInputQueue(AInputQueue* queue) {
    AInputEvent* event = nullptr;
    while (AInputQueue_getEvent(queue, &event) >= 0) {
        if (AInputQueue_preDispatchEvent(queue, event)) continue;
        const int32_t type = AInputEvent_getType(event);
        if (type == AINPUT_EVENT_TYPE_MOTION) handleMotion(event);
        else if (type == AINPUT_EVENT_TYPE_KEY) handleKey(event);
        AInputQueue_finishEvent(queue, event, 1);
    }
}

// ── Файлы: рабочий каталог и ассеты ─────────────────────────────────────────
// Внутренний каталог приложения — единственное место, куда можно писать без
// разрешений. Делаем его рабочим каталогом: тогда save, cfg и audio/ ищутся
// относительно него, и весь код игры про пути остаётся прежним.
static void copyAssetFile(AAssetManager* mgr, const std::string& asset, const std::string& dstDir) {
    AAsset* src = AAssetManager_open(mgr, asset.c_str(), AASSET_MODE_STREAMING);
    if (!src) return;
    const size_t slash = asset.find_last_of('/');
    const std::string name = slash == std::string::npos ? asset : asset.substr(slash + 1);
    const std::string dstPath = dstDir + "/" + name;
    FILE* dst = fopen(dstPath.c_str(), "rb");
    if (dst) {                       // уже распакован на прошлом запуске
        fclose(dst);
        AAsset_close(src);
        return;
    }
    dst = fopen(dstPath.c_str(), "wb");
    if (!dst) { AAsset_close(src); return; }
    char buf[8192];
    int n;
    while ((n = AAsset_read(src, buf, sizeof(buf))) > 0) fwrite(buf, 1, (size_t)n, dst);
    fclose(dst);
    AAsset_close(src);
    LOGI("ассет скопирован: %s", dstPath.c_str());
}

static void extractAssetsRecursive(AAssetManager* mgr, const std::string& assetPath, const std::string& dstDir) {
    AAssetDir* dir = AAssetManager_openDir(mgr, assetPath.c_str());
    if (!dir) return;
    const char* name = nullptr;
    while ((name = AAssetDir_getNextFileName(dir)) != nullptr) {
        const std::string childAsset = assetPath.empty() ? std::string(name) : assetPath + "/" + name;
        const std::string childDir = dstDir + "/" + name;
        AAsset* probe = AAssetManager_open(mgr, childAsset.c_str(), AASSET_MODE_UNKNOWN);
        if (probe) {
            AAsset_close(probe);
            copyAssetFile(mgr, childAsset, dstDir);
        } else {
            // Каталога в ассетах нет как файла: создаём и идём внутрь.
            mkdir(childDir.c_str(), 0770);
            extractAssetsRecursive(mgr, childAsset, childDir);
        }
    }
    AAssetDir_close(dir);
}

// ── Поток игры ──────────────────────────────────────────────────────────────
static double nowSeconds() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void sleepMs(int ms) {
    timespec ts{};
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, nullptr);
}

static void applyWindowSize(ANativeWindow* window) {
    g_gfx.winW = ANativeWindow_getWidth(window);
    g_gfx.winH = ANativeWindow_getHeight(window);
    int lw, lh;
    logicalScreenSize(g_gfx.winW, g_gfx.winH, lw, lh);
    if (lw != SCREEN_W || lh != SCREEN_H) {
        SCREEN_W = lw;
        SCREEN_H = lh;
        g_renderer.resize(SCREEN_W, SCREEN_H);
        g_game.mouseX = SCREEN_W / 2;
        g_game.mouseY = SCREEN_H / 2;
        g_gfx.texW = g_gfx.texH = 0;   // текстура пересоздастся в present()
        LOGI("логический кадр %dx%d (окно %dx%d)", SCREEN_W, SCREEN_H, g_gfx.winW, g_gfx.winH);
    }
}

static void gameThread() {
    ANativeWindow* window = nullptr;
    AInputQueue* input = nullptr;
    ALooper* looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    bool audioRunning = true;

    while (!g_link.quit) {
        ANativeWindow* takeWindow = nullptr;
        AInputQueue* takeInput = nullptr;
        bool dropWindow = false, dropInput = false, resized = false, paused = false;
        {
            std::unique_lock<std::mutex> lock(g_link.m);
            g_link.cv.wait_for(lock, std::chrono::milliseconds(50), [&] {
                return g_link.windowToTake || g_link.windowToDrop || g_link.inputToTake ||
                       g_link.inputToDrop || g_link.resized || g_link.quit;
            });
            takeWindow = g_link.windowToTake; g_link.windowToTake = nullptr;
            takeInput = g_link.inputToTake; g_link.inputToTake = nullptr;
            dropWindow = g_link.windowToDrop; g_link.windowToDrop = false;
            dropInput = g_link.inputToDrop; g_link.inputToDrop = false;
            resized = g_link.resized; g_link.resized = false;
            paused = g_link.paused;
            if (g_link.quit && !takeWindow && !dropWindow) break;
        }

        if (takeWindow) {
            window = takeWindow;
            if (!startGl(window)) {
                LOGE("не удалось поднять EGL — кадров не будет");
                destroyGl();
            } else {
                applyWindowSize(window);
            }
        }
        if (takeInput) {
            input = takeInput;
            AInputQueue_attachLooper(input, looper, 1, nullptr, nullptr);
        }
        if (dropInput && input) {
            AInputQueue_detachLooper(input);
            input = nullptr;
        }
        if (dropWindow) {
            destroyGl();
            window = nullptr;
        }
        if (resized && window && g_gfx.ready) applyWindowSize(window);

        bool saveNow = false;
        {
            std::lock_guard<std::mutex> lock(g_link.m);
            saveNow = g_link.saveRequest;
            g_link.saveRequest = false;
        }
        if (saveNow) g_game.writeSave();

        if (paused || !g_gfx.ready || !window) {
            // На паузе не рисуем и не микшируем: в кармане телефон не должен
            // ни греть процессор, ни играть музыку.
            if (audioRunning) { stopAudio(); audioRunning = false; }
            sleepMs(8);
            continue;
        }
        if (!audioRunning) { startAudio(); audioRunning = true; }

        // Ввод: забираем всё, что накопилось, — по одному событию на вызов.
        if (input) {
            int events = 0;
            ALooper_pollOnce(0, nullptr, &events, nullptr);
            if (events & ALOOPER_EVENT_INPUT) handleInputQueue(input);
        }

        layoutButtons();
        touchUpdate(g_game, g_pointer);

        static double last = nowSeconds();
        const double now = nowSeconds();
        float dt = (float)(now - last);
        last = now;
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.1f) dt = 0.1f;      // после паузы не «прыгаем» на полсекунды

        const double workStart = nowSeconds();
        g_game.update(dt);
        g_game.render(g_renderer);
        drawTouchOverlay(g_renderer, g_game, g_pointer, g_touchLayout);
        const double workEnd = nowSeconds();
        // Ступень графики в АВТО двигается по времени работы, как на Windows.
        g_game.observeFrameTime((float)((workEnd - workStart) * 1000.0));

        present();
        if (g_game.requestExit) break;
        // Если вертикальной синхронизации нет или она не включилась, кадр всё
        // равно держим около 60 в секунду, иначе телефон греется вхолостую.
        const double frameEnd = nowSeconds();
        const double budget = 1.0 / 60.0 - (frameEnd - now);
        if (budget > 0.0) sleepMs((int)(budget * 1000.0));
    }

    if (input) AInputQueue_detachLooper(input);
    stopAudio();
    g_game.writeSave();
    g_game.sound.shutdown();
    destroyGl();
    LOGI("поток игры завершён");
}

// ── Колбэки активности ──────────────────────────────────────────────────────
static void onAppCmd(ANativeActivity* activity) {
    (void)activity;
}

static void onWindowCreated(ANativeActivity*, ANativeWindow* window) {
    std::lock_guard<std::mutex> lock(g_link.m);
    g_link.windowToTake = window;
    g_link.cv.notify_all();
}
static void onWindowResized(ANativeActivity*, ANativeWindow*) {
    std::lock_guard<std::mutex> lock(g_link.m);
    g_link.resized = true;
    g_link.cv.notify_all();
}
static void onWindowDestroyed(ANativeActivity*, ANativeWindow*) {
    std::lock_guard<std::mutex> lock(g_link.m);
    g_link.windowToDrop = true;
    g_link.cv.notify_all();
}
static void onInputQueueCreated(ANativeActivity*, AInputQueue* queue) {
    std::lock_guard<std::mutex> lock(g_link.m);
    g_link.inputToTake = queue;
    g_link.cv.notify_all();
}
static void onInputQueueDestroyed(ANativeActivity*, AInputQueue*) {
    std::lock_guard<std::mutex> lock(g_link.m);
    g_link.inputToDrop = true;
    g_link.cv.notify_all();
}
static void onPause(ANativeActivity*) {
    std::lock_guard<std::mutex> lock(g_link.m);
    g_link.paused = true;
    g_link.saveRequest = true;
}
static void onResume(ANativeActivity*) {
    std::lock_guard<std::mutex> lock(g_link.m);
    g_link.paused = false;
    g_link.cv.notify_all();
}
static void onDestroy(ANativeActivity*) {
    std::lock_guard<std::mutex> lock(g_link.m);
    g_link.quit = true;
    g_link.saveRequest = true;
    g_link.cv.notify_all();
}

// Аварийный след: если что-то падает, на телефоне это иначе не увидеть.
static void crashHandler(int sig) {
    const char* text = "crash: signal ";
    char num[8] = {0};
    num[0] = (char)('0' + sig % 10);
    int fd = open("crash.log", O_WRONLY | O_CREAT | O_TRUNC, 0660);
    if (fd >= 0) {
        write(fd, text, strlen(text));
        write(fd, num, 1);
        write(fd, "\n", 1);
        close(fd);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

extern "C" __attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
    (void)savedState;
    (void)savedStateSize;

    g_link.activity = activity;
    activity->callbacks->onStart = onAppCmd;
    activity->callbacks->onResume = onResume;
    activity->callbacks->onSaveInstanceState = nullptr;
    activity->callbacks->onPause = onPause;
    activity->callbacks->onStop = onAppCmd;
    activity->callbacks->onDestroy = onDestroy;
    activity->callbacks->onWindowFocusChanged = nullptr;
    activity->callbacks->onNativeWindowCreated = onWindowCreated;
    activity->callbacks->onNativeWindowResized = onWindowResized;
    activity->callbacks->onNativeWindowRedrawNeeded = nullptr;
    activity->callbacks->onNativeWindowDestroyed = onWindowDestroyed;
    activity->callbacks->onInputQueueCreated = onInputQueueCreated;
    activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;
    activity->callbacks->onContentRectChanged = nullptr;
    activity->callbacks->onConfigurationChanged = nullptr;
    activity->callbacks->onLowMemory = nullptr;

    // Полный экран и не гасим подсветку во время боя.
    ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_FULLSCREEN | AWINDOW_FLAG_KEEP_SCREEN_ON, 0);

    // Рабочий каталог — внутренняя папка приложения, поэтому сейв, настройки и
    // audio/ оказываются рядом и переживают перезапуск.
    if (activity->internalDataPath && chdir(activity->internalDataPath) != 0)
        LOGW("не смог перейти в %s", activity->internalDataPath);
    if (activity->assetManager) {
        mkdir("audio", 0770);
        extractAssetsRecursive(activity->assetManager, "audio", "audio");
    }
    signal(SIGSEGV, crashHandler);
    signal(SIGABRT, crashHandler);

    srand((unsigned int)time(nullptr));
    SCREEN_W = 1280;
    SCREEN_H = 720;
    g_renderer.resize(SCREEN_W, SCREEN_H);
    // Ставится до init(): он решает, показать ли при первом запуске выбор схемы
    // движения, и рисует подсказки без названий клавиш.
    g_game.touchControls = true;
    g_game.init();
    {
        unsigned cores = std::thread::hardware_concurrency();
        if (cores < 1) cores = 1;
        if (cores > 4) cores = 4;     // больше полос, чем нужно, только мешает
        g_game.renderThreads = (int)cores;
    }
    startAudio();

    std::thread(gameThread).detach();
    LOGI("ShadowSurvivors %s запущен: ядер %d", GAME_VERSION, g_game.renderThreads);
}

#endif  // __ANDROID__
