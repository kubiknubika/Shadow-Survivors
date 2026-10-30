// ── Реализация поддельного NDK (см. fake_ndk.h) ─────────────────────────────
//
// Заголовки берутся из tools/android_stub — те же, против которых идёт
// проверка типов, поэтому объявления здесь ровно те, что видит игра.
#include "fake_ndk.h"

#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/looper.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <aaudio/AAudio.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <dirent.h>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

// ── Внутренние структуры (в реальном NDK они непрозрачные) ─────────────────
struct ALooper { int unused = 0; };
struct ANativeWindow { int32_t width = 0, height = 0; };
struct AAssetManager { std::string root; };
struct AAsset { std::string path; size_t offset = 0; };
struct AAssetDir { std::vector<std::string> names; size_t cursor = 0; };

// Коды те же, что в NDK: KEY = 1, MOTION = 2. Перепутать их — значит
// отдать касание в обработчик клавиш, и стенд этого не заметит.
enum { FAKE_EVENT_KEY = AINPUT_EVENT_TYPE_KEY, FAKE_EVENT_MOTION = AINPUT_EVENT_TYPE_MOTION };

struct AInputEvent {
    int32_t type = 0;
    // motion
    int32_t action = 0;
    struct PointerPoint { int32_t id; float x, y; };
    std::vector<PointerPoint> points;
    // key
    int32_t keyAction = 0, keyCode = 0;
};

struct AInputQueue {
    std::deque<AInputEvent*> queue;
};

struct AAudioStream {
    int32_t channels = 1;
    aaudio_data_callback_result_t (*callback)(AAudioStream*, void*, void*, int32_t) = nullptr;
    void* user = nullptr;
    bool running = false;
    std::thread thread;
    std::atomic<bool> stop{false};
};
struct AAudioStreamBuilder {
    int32_t channels = 1;
    int32_t sampleRate = 44100;
    aaudio_data_callback_result_t (*callback)(AAudioStream*, void*, void*, int32_t) = nullptr;
    void* user = nullptr;
};

namespace fake {
namespace {

std::mutex g_mutex;
AInputQueue* g_queue = nullptr;
AAudioStream* g_stream = nullptr;

std::atomic<int> g_swaps{0};
std::atomic<int> g_uploads{0};
std::atomic<int> g_audioFrames{0};
std::atomic<bool> g_glReady{false};
std::atomic<uint32_t> g_hash{2166136261u};
int g_vpX = 0, g_vpY = 0, g_vpW = 0, g_vpH = 0;
std::string g_assetRoot;

// Один слот под активное событие: AInputQueue_getEvent отдаёт указатель,
// которым распоряжается система до finishEvent.
AInputEvent g_eventStorage;

}  // namespace

void setAssetRoot(const std::string& path) { g_assetRoot = path; }

void* makeWindow(int width, int height) {
    ANativeWindow* w = new ANativeWindow();
    w->width = width;
    w->height = height;
    return w;
}

void resizeWindow(void* window, int width, int height) {
    if (!window) return;
    ANativeWindow* w = (ANativeWindow*)window;
    w->width = width;
    w->height = height;
}

void* makeAssetManager(const std::string& root) {
    AAssetManager* mgr = new AAssetManager();
    mgr->root = root;
    return mgr;
}

void createInputQueue() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_queue) g_queue = new AInputQueue();
}

void* inputQueueHandle() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_queue;
}

void destroyInputQueue() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_queue) return;
    for (AInputEvent* e : g_queue->queue) delete e;
    delete g_queue;
    g_queue = nullptr;
}

namespace {
void push(AInputEvent* event) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_queue) { delete event; return; }
    g_queue->queue.push_back(event);
}

AInputEvent* motionBase(int32_t action) {
    AInputEvent* e = new AInputEvent();
    e->type = FAKE_EVENT_MOTION;
    e->action = action;
    return e;
}
}  // namespace

void touchDown(int32_t id, float x, float y) {
    AInputEvent* e = motionBase(AMOTION_EVENT_ACTION_DOWN);
    e->points.push_back({id, x, y});
    push(e);
}

void touchDown2(int32_t id, float x, float y) {
    // POINTER_DOWN: индекс пальца в старших битах, как у Android.
    AInputEvent* e = motionBase(AMOTION_EVENT_ACTION_POINTER_DOWN | (1 << 8));
    e->points.push_back({id, x, y});
    push(e);
}

void touchMove(int32_t id, float x, float y) {
    AInputEvent* e = motionBase(AMOTION_EVENT_ACTION_MOVE);
    e->points.push_back({id, x, y});
    push(e);
}

void touchUp(int32_t id) {
    AInputEvent* e = motionBase(AMOTION_EVENT_ACTION_UP);
    e->points.push_back({id, 0.0f, 0.0f});
    push(e);
}

void keyEvent(int32_t keycode, bool down) {
    AInputEvent* e = new AInputEvent();
    e->type = FAKE_EVENT_KEY;
    e->keyAction = down ? AKEY_EVENT_ACTION_DOWN : AKEY_EVENT_ACTION_UP;
    e->keyCode = keycode;
    push(e);
}

int swapCount() { return g_swaps.load(); }
int textureUploads() { return g_uploads.load(); }
int audioFramesMixed() { return g_audioFrames.load(); }
bool audioRunning() { return g_stream && g_stream->running; }
bool glReady() { return g_glReady.load(); }
uint32_t lastFrameHash() { return g_hash.load(); }
void resetFrameHash() { g_hash = 2166136261u; }

void lastViewport(int& x, int& y, int& w, int& h) {
    std::lock_guard<std::mutex> lock(g_mutex);
    x = g_vpX; y = g_vpY; w = g_vpW; h = g_vpH;
}

}  // namespace fake

// ── Логи ───────────────────────────────────────────────────────────────────
int __android_log_print(int prio, const char* tag, const char* fmt, ...) {
    (void)prio; (void)tag;
    va_list args;
    va_start(args, fmt);
    const int n = vprintf(fmt, args);
    va_end(args);
    printf("\n");
    return n;
}

// ── Активность ─────────────────────────────────────────────────────────────
void ANativeActivity_setWindowFlags(ANativeActivity*, uint32_t, uint32_t) {}

// ── Окно ───────────────────────────────────────────────────────────────────
int32_t ANativeWindow_getWidth(ANativeWindow* w) { return w ? w->width : 0; }
int32_t ANativeWindow_getHeight(ANativeWindow* w) { return w ? w->height : 0; }

// ── Looper: события появляются, только когда стенд их положил ─────────────
ALooper* ALooper_prepare(int) {
    static ALooper looper;
    return &looper;
}

int ALooper_pollOnce(int /*timeoutMillis*/, int* outFd, int* outEvents, void** outData) {
    if (outFd) *outFd = 0;
    if (outData) *outData = nullptr;
    std::lock_guard<std::mutex> lock(fake::g_mutex);
    if (fake::g_queue && !fake::g_queue->queue.empty()) {
        if (outEvents) *outEvents = ALOOPER_EVENT_INPUT;
        return 1;
    }
    if (outEvents) *outEvents = 0;
    return 0;   // «ничего не пришло», как при таймауте 0
}

// ── Очередь ввода ──────────────────────────────────────────────────────────
int AInputQueue_getEvent(AInputQueue* q, AInputEvent** outEvent) {
    std::lock_guard<std::mutex> lock(fake::g_mutex);
    if (!q || q->queue.empty()) { *outEvent = nullptr; return -1; }
    AInputEvent* e = q->queue.front();
    q->queue.pop_front();
    // Копируем в стабильный слот: игра держит указатель до finishEvent.
    fake::g_eventStorage = *e;
    delete e;
    *outEvent = &fake::g_eventStorage;
    return 0;
}

int AInputQueue_preDispatchEvent(AInputQueue*, AInputEvent*) { return 0; }
void AInputQueue_finishEvent(AInputQueue*, AInputEvent*, int) {}
void AInputQueue_attachLooper(AInputQueue*, ALooper*, int, ALooper_callbackFunc, void*) {}
void AInputQueue_detachLooper(AInputQueue*) {}

int32_t AInputEvent_getType(const AInputEvent* e) { return e ? e->type : 0; }

int32_t AMotionEvent_getAction(const AInputEvent* e) { return e ? e->action : 0; }
size_t AMotionEvent_getPointerCount(const AInputEvent* e) {
    return e ? e->points.size() : 0;
}
int32_t AMotionEvent_getPointerId(const AInputEvent* e, size_t i) {
    return e && i < e->points.size() ? e->points[i].id : -1;
}
float AMotionEvent_getX(const AInputEvent* e, size_t i) {
    return e && i < e->points.size() ? e->points[i].x : 0.0f;
}
float AMotionEvent_getY(const AInputEvent* e, size_t i) {
    return e && i < e->points.size() ? e->points[i].y : 0.0f;
}

int32_t AKeyEvent_getAction(const AInputEvent* e) { return e ? e->keyAction : 0; }
int32_t AKeyEvent_getKeyCode(const AInputEvent* e) { return e ? e->keyCode : 0; }

// ── Ассеты: читаются с диска, как из APK ───────────────────────────────────
AAsset* AAssetManager_open(AAssetManager* mgr, const char* name, int) {
    if (!mgr || !name) return nullptr;
    std::string path = mgr->root + "/" + name;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return nullptr;
    fclose(f);
    AAsset* a = new AAsset();
    a->path = path;
    return a;
}

int AAsset_read(AAsset* asset, void* buf, size_t count) {
    if (!asset) return -1;
    FILE* f = fopen(asset->path.c_str(), "rb");
    if (!f) return -1;
    fseek(f, (long)asset->offset, SEEK_SET);
    const size_t n = fread(buf, 1, count, f);
    fclose(f);
    asset->offset += n;
    return (int)n;
}

void AAsset_close(AAsset* asset) { delete asset; }

AAssetDir* AAssetManager_openDir(AAssetManager* mgr, const char* dirName) {
    if (!mgr) return nullptr;
    std::string path = mgr->root;
    if (dirName && dirName[0]) path += std::string("/") + dirName;
    DIR* d = opendir(path.c_str());
    if (!d) return nullptr;
    AAssetDir* dir = new AAssetDir();
    while (dirent* entry = readdir(d)) {
        if (entry->d_name[0] == '.') continue;
        dir->names.push_back(entry->d_name);
    }
    closedir(d);
    return dir;
}

const char* AAssetDir_getNextFileName(AAssetDir* dir) {
    if (!dir || dir->cursor >= dir->names.size()) return nullptr;
    return dir->names[dir->cursor++].c_str();
}

void AAssetDir_close(AAssetDir* dir) { delete dir; }

// ── EGL: всё «получается», кадры считаются ────────────────────────────────
namespace {
struct FakeDisplay { int d = 0; };
FakeDisplay g_display;
int g_surface = 0, g_context = 0;
}  // namespace

EGLDisplay eglGetDisplay(EGLNativeDisplayType) { return &g_display; }
EGLBoolean eglInitialize(EGLDisplay d, EGLint* major, EGLint* minor) {
    if (major) *major = 1;
    if (minor) *minor = 4;
    fake::g_glReady = (d != nullptr);
    return EGL_TRUE;
}
EGLBoolean eglTerminate(EGLDisplay) {
    fake::g_glReady = false;
    return EGL_TRUE;
}
EGLBoolean eglChooseConfig(EGLDisplay, const EGLint*, EGLConfig* configs,
                                      EGLint configSize, EGLint* numConfig) {
    if (configs && configSize > 0) configs[0] = (EGLConfig)(void*)&g_surface;
    if (numConfig) *numConfig = 1;
    return EGL_TRUE;
}
EGLSurface eglCreateWindowSurface(EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint*) {
    return (EGLSurface)(void*)&g_surface;
}
EGLContext eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint*) {
    return (EGLContext)(void*)&g_context;
}
EGLBoolean eglMakeCurrent(EGLDisplay, EGLSurface, EGLSurface, EGLContext) { return EGL_TRUE; }
EGLBoolean eglSwapBuffers(EGLDisplay, EGLSurface) {
    ++fake::g_swaps;
    return EGL_TRUE;
}
EGLBoolean eglSwapInterval(EGLDisplay, EGLint) { return EGL_TRUE; }
EGLBoolean eglDestroySurface(EGLDisplay, EGLSurface) { return EGL_TRUE; }
EGLBoolean eglDestroyContext(EGLDisplay, EGLContext) { return EGL_TRUE; }

// ── GLES2: шейдеры «собираются», кадр хешируется ──────────────────────────
GLuint glCreateShader(GLenum) { static GLuint id = 1; return id++; }
void glShaderSource(GLuint, GLsizei, const char* const*, const GLint*) {}
void glCompileShader(GLuint) {}
void glGetShaderiv(GLuint, GLenum pname, GLint* params) {
    if (params) *params = (pname == GL_COMPILE_STATUS) ? GL_TRUE : 0;
}
void glGetShaderInfoLog(GLuint, GLsizei, GLsizei* length, char* log) {
    if (length) *length = 0;
    if (log) log[0] = 0;
}
void glDeleteShader(GLuint) {}
GLuint glCreateProgram() { static GLuint id = 100; return id++; }
void glAttachShader(GLuint, GLuint) {}
void glLinkProgram(GLuint) {}
void glGetProgramiv(GLuint, GLenum pname, GLint* params) {
    if (params) *params = (pname == GL_LINK_STATUS) ? GL_TRUE : 0;
}
void glGetProgramInfoLog(GLuint, GLsizei, GLsizei* length, char* log) {
    if (length) *length = 0;
    if (log) log[0] = 0;
}
void glDeleteProgram(GLuint) {}
void glUseProgram(GLuint) {}
GLint glGetAttribLocation(GLuint, const char*) { return 0; }
GLint glGetUniformLocation(GLuint, const char*) { return 1; }
void glGenTextures(GLsizei n, GLuint* textures) {
    static GLuint id = 1000;
    for (GLsizei i = 0; i < n; ++i) textures[i] = id++;
}
void glBindTexture(GLenum, GLuint) {}
void glDeleteTextures(GLsizei, const GLuint*) {}
void glTexParameteri(GLenum, GLenum, GLint) {}
void glActiveTexture(GLenum) {}
void glClear(GLbitfield) {}
void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat) {}
void glEnableVertexAttribArray(GLuint) {}
void glDisableVertexAttribArray(GLuint) {}
void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) {}
void glDrawArrays(GLenum, GLint, GLsizei) {}
void glUniform1i(GLint, GLint) {}

void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    std::lock_guard<std::mutex> lock(fake::g_mutex);
    fake::g_vpX = x; fake::g_vpY = y; fake::g_vpW = w; fake::g_vpH = h;
}

namespace {
void hashFrame(const void* pixels, int w, int h) {
    const uint8_t* p = (const uint8_t*)pixels;
    uint32_t hash = 2166136261u;
    const size_t bytes = (size_t)w * h * 4;
    for (size_t i = 0; i < bytes; i += 97) {
        hash ^= p[i];
        hash *= 16777619u;
    }
    fake::g_hash = hash;
}
}  // namespace

void glTexImage2D(GLenum, GLint, GLint, GLsizei width, GLsizei height, GLint,
                             GLenum, GLenum, const void* pixels) {
    ++fake::g_uploads;
    if (pixels) hashFrame(pixels, width, height);
}

void glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei width, GLsizei height,
                                GLenum, GLenum, const void* pixels) {
    ++fake::g_uploads;
    if (pixels) hashFrame(pixels, width, height);
}

// ── AAudio: поток действительно считает звук, пока игра не на паузе ───────
aaudio_result_t AAudio_createStreamBuilder(AAudioStreamBuilder** builder) {
    *builder = new AAudioStreamBuilder();
    return AAUDIO_OK;
}

void AAudioStreamBuilder_setDirection(AAudioStreamBuilder*, int32_t) {}
void AAudioStreamBuilder_setSharingMode(AAudioStreamBuilder*, int32_t) {}
void AAudioStreamBuilder_setPerformanceMode(AAudioStreamBuilder*, int32_t) {}
void AAudioStreamBuilder_setFormat(AAudioStreamBuilder*, int32_t) {}
void AAudioStreamBuilder_setSampleRate(AAudioStreamBuilder*, int32_t) {}
void AAudioStreamBuilder_setChannelCount(AAudioStreamBuilder* b, int32_t count) {
    if (b) b->channels = count;
}
void AAudioStreamBuilder_setDataCallback(
    AAudioStreamBuilder* b, aaudio_data_callback_result_t (*cb)(AAudioStream*, void*, void*, int32_t),
    void* user) {
    if (b) { b->callback = cb; b->user = user; }
}

aaudio_result_t AAudioStreamBuilder_openStream(AAudioStreamBuilder* b, AAudioStream** stream) {
    if (!b || !b->callback) return -1;
    AAudioStream* s = new AAudioStream();
    s->channels = b->channels;
    s->callback = b->callback;
    s->user = b->user;
    *stream = s;
    return AAUDIO_OK;
}

void AAudioStreamBuilder_delete(AAudioStreamBuilder* b) { delete b; }

aaudio_result_t AAudioStream_requestStart(AAudioStream* s) {
    if (!s) return -1;
    s->stop = false;
    s->running = true;
    // Как настоящий AAudio: колбэк зовут в своём потоке, примерно по 512 кадров.
    s->thread = std::thread([s] {
        std::vector<int16_t> buffer(512 * 2);
        while (!s->stop.load()) {
            const int32_t frames = 512;
            s->callback(s, s->user, buffer.data(), frames);
            fake::g_audioFrames += frames;
            std::this_thread::sleep_for(std::chrono::milliseconds(11));
        }
    });
    fake::g_stream = s;
    return AAUDIO_OK;
}

aaudio_result_t AAudioStream_requestStop(AAudioStream* s) {
    if (!s) return -1;
    s->stop = true;
    if (s->thread.joinable()) s->thread.join();
    s->running = false;
    return AAUDIO_OK;
}

aaudio_result_t AAudioStream_close(AAudioStream* s) {
    if (!s) return -1;
    AAudioStream_requestStop(s);
    if (fake::g_stream == s) fake::g_stream = nullptr;
    delete s;
    return AAUDIO_OK;
}

int32_t AAudioStream_getChannelCount(AAudioStream* s) {
    return s ? s->channels : 1;
}
int32_t AAudioStream_getSampleRate(AAudioStream*) { return 44100; }
