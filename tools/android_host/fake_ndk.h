// ── Поддельный NDK: Android-слой игры, но на компьютере ─────────────────────
//
// Настоящий APK собрать здесь нечем, зато можно исполнить src/main_android.cpp
// целиком: он обращается к системе через ~60 функций (EGL, GLES2, AAudio,
// ALooper, AInputQueue, ANativeWindow, ассеты). Здесь они и реализованы —
// по-настоящему, а не заглушками: события ввода проходят через те же
// AInputQueue_getEvent, что и на телефоне, кадр уходит в glTexSubImage2D,
// звук считается в том же аудио-колбэке, ассеты читаются с диска.
//
// Что это даёт: проверяется весь Android-путь (жизненный цикл, поток игры,
// нажатия, пауза, сейв при выходе), кроме собственно телефона. Чего это не
// даёт: драйверов GPU и AAudio — их поведение здесь идеально.
//
// Интерфейс для стенда — в fake_ndk.h.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace fake {

// ── Управление «системой» из стенда ────────────────────────────────────
void setAssetRoot(const std::string& path);   // каталог android/.../assets
void* makeWindow(int width, int height);      // то, что система отдала в onNativeWindowCreated
void resizeWindow(void* window, int width, int height);   // поворот/сплит
void* makeAssetManager(const std::string& root);
void createInputQueue();                      // как onInputQueueCreated
void* inputQueueHandle();                     // та же очередь для колбэка активности
void destroyInputQueue();

// Касания в пикселях окна (так их отдаёт Android).
void touchDown(int32_t id, float x, float y);
void touchMove(int32_t id, float x, float y);
void touchUp(int32_t id);
void touchDown2(int32_t id, float x, float y);  // второй палец (POINTER_DOWN)
void keyEvent(int32_t keycode, bool down);

// ── Что видно снаружи ──────────────────────────────────────────────────
int swapCount();                    // сколько кадров дошло до экрана
int textureUploads();               // сколько раз кадр ушёл в текстуру
int audioFramesMixed();             // сколько кадров звука посчитал микшер
bool audioRunning();                // открыт ли поток AAudio
void lastViewport(int& x, int& y, int& w, int& h);
uint32_t lastFrameHash();           // хеш последнего залитого кадра
void resetFrameHash();
bool glReady();                     // поднялся ли «EGL»

}  // namespace fake
