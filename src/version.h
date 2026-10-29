#pragma once
// Single source of truth for the build version.
//
// build_windows.bat always compiles with
//     -DGAME_VERSION="x.y.z" -DGAME_VERSION_NUM=xyzz
// so the window title, the in-game chronicle and the .exe metadata can never
// drift apart again. The fallback below only applies to plain `g++` builds
// (IDE, syntax check, the headless tests).
#ifndef GAME_VERSION
#define GAME_VERSION "2.10.19"
#endif
#ifndef GAME_VERSION_NUM
// 2.9.80 -> 2*1000 + 9*100 + 80
#define GAME_VERSION_NUM 3019
#endif
