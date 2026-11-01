#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <thread>
#include <mmsystem.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <time.h>
#include <exception>
#include <string>
#include <cstring>
#include <cstdlib>
#include "version.h"
#include "game.h"

static Game g_game;
static Renderer g_renderer;
static BITMAPINFO g_bmi;
static WINDOWPLACEMENT g_wpPrev = { sizeof(g_wpPrev) };

static int g_winW = 1280;
static int g_winH = 720;
// Ключи командной строки: на слабом железе быстрее задать ступень один раз,
// чем искать её в настройках мышью. Пустое значение — не задано.
static int g_forceThreads = 0;
static int g_forceGraphics = -1;
static bool g_useHalftone = true;
static int g_viewX = 0, g_viewY = 0, g_viewW = 1280, g_viewH = 720;

static void updateViewportRect(int clientW, int clientH) {
    // Keep a stable logical pixel density while matching the client aspect ratio.
    // Landscape uses a 720px reference height; portrait uses a 720px reference width.
    if (clientW < 240 || clientH < 240) return;
    g_winW = clientW; g_winH = clientH;
    g_viewX = 0; g_viewY = 0; g_viewW = g_winW; g_viewH = g_winH;
    int logicalW, logicalH;
    logicalScreenSize(g_winW, g_winH, logicalW, logicalH);
    if (SCREEN_W != logicalW || SCREEN_H != logicalH) {
        SCREEN_W = logicalW; SCREEN_H = logicalH;
        g_renderer.resize(SCREEN_W, SCREEN_H);
        g_game.mouseX = SCREEN_W / 2; g_game.mouseY = SCREEN_H / 2;
        g_bmi.bmiHeader.biWidth = SCREEN_W;
        g_bmi.bmiHeader.biHeight = -SCREEN_H;
    }
}

static void toggleFullscreen(HWND hwnd) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (dwStyle & WS_OVERLAPPEDWINDOW) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(hwnd, &g_wpPrev) &&
            GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP,
                mi.rcMonitor.left, mi.rcMonitor.top,
                mi.rcMonitor.right - mi.rcMonitor.left,
                mi.rcMonitor.bottom - mi.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            g_game.isFullscreen = true;
        }
    } else {
        SetWindowLong(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &g_wpPrev);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        g_game.isFullscreen = false;
    }
}

static void mapMouseToCanvas(int mx, int my) {
    if (g_viewW <= 0 || g_viewH <= 0) return;
    g_game.mouseMovedSinceLaunch = true;
    int cx = ((mx - g_viewX) * SCREEN_W) / g_viewW;
    int cy = ((my - g_viewY) * SCREEN_H) / g_viewH;
    g_game.mouseX = std::max(0, std::min(SCREEN_W - 1, cx));
    g_game.mouseY = std::max(0, std::min(SCREEN_H - 1, cy));
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE: {
            updateViewportRect(LOWORD(lParam), HIWORD(lParam));
            return 0;
        }
        case WM_MOUSEMOVE: {
            mapMouseToCanvas(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        }
        case WM_MOUSEWHEEL: {
            g_game.mouseWheelDelta += (int)(short)HIWORD(wParam);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            mapMouseToCanvas(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            g_game.mouseLeftDown = true;
            g_game.mouseClicked = true;
            return 0;
        }
        case WM_LBUTTONUP: {
            mapMouseToCanvas(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            g_game.mouseLeftDown = false;
            return 0;
        }
        case WM_RBUTTONDOWN: {
            mapMouseToCanvas(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            g_game.mouseRightDown = true;
            return 0;
        }
        case WM_RBUTTONUP: {
            mapMouseToCanvas(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            g_game.mouseRightDown = false;
            return 0;
        }
        case WM_SYSKEYDOWN: {
            if (wParam == VK_RETURN && (lParam & (1 << 29))) {
                toggleFullscreen(hwnd);
                return 0;
            }
            // F10 can arrive as a system key; reserve it for the tenth music option, but leave Alt+F10 alone.
            if (wParam == VK_F10 && !(lParam & (1 << 29))) {
                if (!(lParam & (1 << 30))) g_game.onKeyPress((int)wParam);
                return 0;
            }
            break;
        }
        case WM_KEYDOWN: {
            bool repeat = (lParam & (1 << 30)) != 0;
            // Virtual keys work identically in both EN (QWERTY) and RU (ЙЦУКЕН) layouts!
            if (wParam == 'W') g_game.keyW = true;
            if (wParam == 'A') g_game.keyA = true;
            if (wParam == 'S') g_game.keyS = true;
            if (wParam == 'D') g_game.keyD = true;
            if (wParam == VK_UP)    g_game.keyUp = true;
            if (wParam == VK_DOWN)  g_game.keyDown = true;
            if (wParam == VK_LEFT)  g_game.keyLeft = true;
            if (wParam == VK_RIGHT) g_game.keyRight = true;

            if (!repeat) {
                if (wParam == VK_F11) {
                    toggleFullscreen(hwnd);
                } else if (wParam == VK_F3) {
                    g_game.debugOverlay = !g_game.debugOverlay; // F3: technical overlay, Minecraft style
                } else if (wParam == VK_SHIFT) {
                    g_game.onKeyPress(32); // Shift also triggers Dash!
                } else if (wParam == VK_LEFT && g_game.state == STATE_MENU) {
                    g_game.cycleHero(-1);
                    g_game.sound.play(SFX_CLICK, 0.02f);
                } else if (wParam == VK_RIGHT && g_game.state == STATE_MENU) {
                    g_game.cycleHero(1);
                    g_game.sound.play(SFX_CLICK, 0.02f);
                } else {
                    g_game.onKeyPress((int)wParam);
                }
            }
            return 0;
        }
        case WM_KEYUP: {
            if (wParam == 'W') g_game.keyW = false;
            if (wParam == 'A') g_game.keyA = false;
            if (wParam == 'S') g_game.keyS = false;
            if (wParam == 'D') g_game.keyD = false;
            if (wParam == VK_UP)    g_game.keyUp = false;
            if (wParam == VK_DOWN)  g_game.keyDown = false;
            if (wParam == VK_LEFT)  g_game.keyLeft = false;
            if (wParam == VK_RIGHT) g_game.keyRight = false;
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_CLOSE:
        case WM_DESTROY:
            g_game.writeSave();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static std::wstring windowTitle() {
    // GAME_VERSION is injected by build_windows.bat, so the title can never go stale again.
    const char* utf8 = "Пепел Тьмы: Выжившие " GAME_VERSION " — F3: отладка, F11: полный экран, F12: арена музыки босса";
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len <= 1) return L"Shadow Survivors";
    std::wstring wide((size_t)(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &wide[0], len);
    return wide;
}

static void writeCrashLog(const std::string& msg) {
    FILE* f = fopen("crash.log", "w");
    if (f) {
        fprintf(f, "%s\n", msg.c_str());
        fclose(f);
    }
    MessageBoxA(NULL, msg.c_str(), "ShadowSurvivors — ERROR", MB_ICONERROR | MB_OK);
}

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* pException) {
    std::string msg = "ShadowSurvivors crashed with an unhandled exception.\n\n";
    if (pException && pException->ExceptionRecord) {
        // buf must hold a full 64-bit address ("0x" + 16 hex digits + NUL).
        char buf[32];
        msg += "Exception code: 0x";
        sprintf(buf, "%08lX", (unsigned long)pException->ExceptionRecord->ExceptionCode);
        msg += buf;
        msg += "\n";
        if (pException->ExceptionRecord->ExceptionAddress) {
            msg += "Exception address: 0x";
            sprintf(buf, "%p", (void*)pException->ExceptionRecord->ExceptionAddress);
            msg += buf;
            msg += "\n";
        }
    }
    msg += "\nSee crash.log for details.";
    writeCrashLog(msg);
    return EXCEPTION_EXECUTE_HANDLER;
}

// Третий параметр — командная строка. Он был безымянным, а ниже читался как
// lpCmdLine: Windows-сборка падала на этом с 2.10.48 («lpCmdLine» was not
// declared), и никто этого не видел, потому что стенды собирают другой TU.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    // Перехват критических ошибок Windows (access violation, segfault и т.д.)
    SetUnhandledExceptionFilter(CrashHandler);

    try {
    srand((unsigned int)time(NULL));

    // Enable High-DPI awareness so the adaptive logical canvas stays crisp on every monitor.
    HMODULE hUser32 = LoadLibraryA("user32.dll");
    if (hUser32) {
        typedef BOOL(WINAPI* SetProcessDPIAwareFunc)();
        SetProcessDPIAwareFunc setDPI = (SetProcessDPIAwareFunc)GetProcAddress(hUser32, "SetProcessDPIAware");
        if (setDPI) setDPI();
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"ShadowSurvivorsWndClass";
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    RegisterClassExW(&wc);

    RECT wr = {0, 0, SCREEN_W, SCREEN_H};
    DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&wr, style, FALSE);
    int winW = wr.right - wr.left;
    int winH = wr.bottom - wr.top;

    // Center window on primary monitor
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int winX = std::max(0, (scrW - winW) / 2);
    int winY = std::max(0, (scrH - winH) / 2);

    const std::wstring title = windowTitle();
    HWND hwnd = CreateWindowExW(
        0,
        L"ShadowSurvivorsWndClass",
        title.c_str(),
        style,
        winX, winY, winW, winH,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 1;

    memset(&g_bmi, 0, sizeof(g_bmi));
    g_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_bmi.bmiHeader.biWidth = SCREEN_W;
    g_bmi.bmiHeader.biHeight = -SCREEN_H; // Top-down DIB
    g_bmi.bmiHeader.biPlanes = 1;
    g_bmi.bmiHeader.biBitCount = 32;
    g_bmi.bmiHeader.biCompression = BI_RGB;

    // Ключи: ShadowSurvivors.exe --threads 4 --graphics 3
    //   --threads N   1..8  сколько потоков рисуют кадр (0 — по числу ядер)
    //   --graphics N  0..4  АВТО / высокое / среднее / быстрое / эконом
    if (lpCmdLine && lpCmdLine[0]) {
        for (const char* p = lpCmdLine; *p; ++p) {
            if (strncmp(p, "--threads", 9) == 0) g_forceThreads = atoi(p + 9);
            else if (strncmp(p, "--graphics", 10) == 0) g_forceGraphics = atoi(p + 10);
        }
        if (g_forceThreads < 0) g_forceThreads = 0;
        if (g_forceThreads > 8) g_forceThreads = 8;
        if (g_forceGraphics >= 0 && g_forceGraphics <= 4) g_game.options.graphicsLevel = g_forceGraphics;
    }

    g_game.init();
    // Кадр рисуется полосами: по одному потоку на полосу, максимум четыре.
    // На одном ядре или в системе, где неизвестно число ядер, остаётся
    // однопоточный путь — он же и запасной, если полосы не годятся.
    {
        unsigned cores = std::thread::hardware_concurrency();
        if (g_forceThreads > 0) g_game.renderThreads = g_forceThreads;
        else {
            if (cores < 2) cores = 1;
            if (cores > 4) cores = 4;
            g_game.renderThreads = (int)cores;
        }
    }
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    LARGE_INTEGER freq, lastCounter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&lastCounter);

    HDC hdc = GetDC(hwnd);
    // HALFTONE нужен только когда кадр действительно растягивается: это
    // фильтр в софте, и на 1:1 он всё равно гоняет буфер через свой конвейер.
    // 1:1 отдаём напрямую через SetDIBitsToDevice (см. ниже).
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);

    MSG msg;
    bool running = true;
    while (running && !g_game.requestExit) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running || g_game.requestExit) break;

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - lastCounter.QuadPart) / (float)freq.QuadPart;
        lastCounter = now;

        // Время кадра меряется вокруг самой работы, а не вокруг ожидания
        // вертикальной синхронизации: иначе при 60 Гц «АВТО» всегда видело бы
        // 16.7 мс и не смогло бы отличить быстрый кадр от медленного.
        LARGE_INTEGER workStart;
        QueryPerformanceCounter(&workStart);
        g_game.update(dt);
        g_game.render(g_renderer);
        LARGE_INTEGER workEnd;
        QueryPerformanceCounter(&workEnd);
        // Ступень графики в режиме «АВТО» двигается по времени кадра: на телефоне
        // через эмулятор это единственная честная мерка.
        g_game.observeFrameTime((float)(workEnd.QuadPart - workStart.QuadPart) * 1000.0f / (float)freq.QuadPart);

        // Present the full logical canvas at the client aspect ratio; world view is never cropped.
        if (g_viewX == 0 && g_viewY == 0 && g_viewW == SCREEN_W && g_viewH == SCREEN_H) {
            // Окно ровно в логический кадр (например 1280×720): растяжения нет,
            // значит и HALFTONE не нужен — отдаём строки как есть.
            SetDIBitsToDevice(hdc, 0, 0, SCREEN_W, SCREEN_H, 0, 0, 0, SCREEN_H,
                              g_renderer.pixels, &g_bmi, DIB_RGB_COLORS);
        } else {
            // HALFTONE — фильтр в софте: он сглаживает край, но гонит весь кадр
            // через свой конвейер. Когда окно заметно больше логического кадра и
            // ступень графики уже снижена, дешевле отдать без сглаживания: на
            // телефоне это экономит несколько миллисекунд на каждом кадре.
            const bool bigUpscale = g_viewW > (SCREEN_W * 3) / 2 || g_viewH > (SCREEN_H * 3) / 2;
            const bool wantHalftone = !(bigUpscale && g_game.resolvedGraphicsTier() >= 2);
            if (wantHalftone != g_useHalftone) {
                g_useHalftone = wantHalftone;
                SetStretchBltMode(hdc, g_useHalftone ? HALFTONE : COLORONCOLOR);
                if (g_useHalftone) SetBrushOrgEx(hdc, 0, 0, NULL);
            }
            StretchDIBits(
                hdc,
                g_viewX, g_viewY, g_viewW, g_viewH,
                0, 0, SCREEN_W, SCREEN_H,
                g_renderer.pixels,
                &g_bmi,
                DIB_RGB_COLORS,
                SRCCOPY
            );
        }

        // DWM frame pacing follows the active monitor refresh rate (including 120/144/165 Hz).
        // This synchronizes the GDI/DIB presentation to compositor vblank and avoids tearing.
        if (FAILED(DwmFlush())) Sleep(1);
    }

    ReleaseDC(hwnd, hdc);
    g_game.writeSave();
    g_game.sound.shutdown();
    return 0;
    } catch (const std::exception& e) {
        std::string msg = "ShadowSurvivors crashed:\n\n";
        msg += e.what();
        msg += "\n\nSee crash.log for details.";
        MessageBoxA(NULL, msg.c_str(), "ShadowSurvivors — ERROR", MB_ICONERROR | MB_OK);
        
        FILE* f = fopen("crash.log", "w");
        if (f) {
            fprintf(f, "Caught C++ exception: %s\n", e.what());
            fclose(f);
        }
        return 1;
    } catch (...) {
        std::string msg = "ShadowSurvivors crashed with an unknown exception.\n\nSee crash.log for details.";
        MessageBoxA(NULL, msg.c_str(), "ShadowSurvivors — ERROR", MB_ICONERROR | MB_OK);
        
        FILE* f = fopen("crash.log", "w");
        if (f) {
            fprintf(f, "Caught unknown C++ exception\n");
            fclose(f);
        }
        return 1;
    }

}
