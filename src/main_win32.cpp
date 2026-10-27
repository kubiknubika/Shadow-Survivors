#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <time.h>
#include <exception>
#include <string>
#include "game.h"

static Game g_game;
static Renderer g_renderer;
static BITMAPINFO g_bmi;
static WINDOWPLACEMENT g_wpPrev = { sizeof(g_wpPrev) };

static int g_winW = 1280;
static int g_winH = 720;
static int g_viewX = 0, g_viewY = 0, g_viewW = 1280, g_viewH = 720;

static void updateViewportRect(int clientW, int clientH) {
    // Keep a stable logical pixel density while matching the client aspect ratio.
    // Landscape uses a 720px reference height; portrait uses a 720px reference width.
    if (clientW < 240 || clientH < 240) return;
    g_winW = clientW; g_winH = clientH;
    g_viewX = 0; g_viewY = 0; g_viewW = g_winW; g_viewH = g_winH;
    const double aspect = (double)g_winW / (double)g_winH;
    int logicalW, logicalH;
    if (aspect >= 1.0) {
        logicalH = 720;
        logicalW = std::max(640, ((int)(logicalH * aspect + 8.0) / 16) * 16);
    } else {
        logicalW = 720;
        logicalH = std::max(720, ((int)(logicalW / aspect + 8.0) / 16) * 16);
    }
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
        msg += "Exception code: 0x";
    char buf[16];
    sprintf(buf, "%08lX", pException->ExceptionRecord->ExceptionCode);
    msg += buf;
    msg += "\n";
    if (pException->ExceptionRecord->ExceptionAddress) {
        msg += "Exception address: 0x";
        sprintf(buf, "%p", pException->ExceptionRecord->ExceptionAddress);
        msg += buf;
        msg += "\n";
    }
    }
    msg += "\nSee crash.log for details.";
    writeCrashLog(msg);
    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Перехват критических ошибок Windows (access violation, segfault и т.д.)
    SetUnhandledExceptionFilter(CrashHandler);

    try {
    writeCrashLog("ShadowSurvivors starting...\nWinMain entered");
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

    HWND hwnd = CreateWindowExW(
        0,
        L"ShadowSurvivorsWndClass",
        L"Пепел Тьмы: Выжившие 2.9.53 — F11: полный экран, F12: арена музыки босса",
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

    g_game.init();
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    LARGE_INTEGER freq, lastCounter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&lastCounter);

    HDC hdc = GetDC(hwnd);
    SetStretchBltMode(hdc, HALFTONE);

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

        g_game.update(dt);
        g_game.render(g_renderer);

        // Present the full logical canvas at the client aspect ratio; world view is never cropped.
        StretchDIBits(
            hdc,
            g_viewX, g_viewY, g_viewW, g_viewH,
            0, 0, SCREEN_W, SCREEN_H,
            g_renderer.pixels,
            &g_bmi,
            DIB_RGB_COLORS,
            SRCCOPY
        );

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
