@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo === Shadow Survivors: обновление ===
echo.

rem Версия берётся из src\version.h: локального счётчика больше нет.
rem Скрипт печатает ветку, коммит и версию до и после pull, чтобы не собирать
rem старую копию молча: раньше работа шла в ветке, а git pull тянул main.

set "SRC=%cd%\src"
set "VER_BEFORE="
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_BEFORE=%%~v"
for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD') do set "BRANCH=%%b"
for /f "delims=" %%c in ('git rev-parse --short HEAD') do set "HEAD_BEFORE=%%c"

echo Ветка: !BRANCH!   коммит: !HEAD_BEFORE!   версия: !VER_BEFORE!
echo.

echo [1/3] git fetch
git fetch --all --prune
if errorlevel 1 goto failed

echo [2/3] git pull --ff-only
git pull --ff-only
if errorlevel 1 goto failed

set "VER_AFTER="
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_AFTER=%%~v"
for /f "delims=" %%c in ('git rev-parse --short HEAD') do set "HEAD_AFTER=%%c"

if "!HEAD_BEFORE!"=="!HEAD_AFTER!" goto nothing_new
echo Обновлено: !HEAD_BEFORE! -^> !HEAD_AFTER!, версия !VER_AFTER!
goto build

:nothing_new
echo.
echo ВНИМАНИЕ: обновлений не было. Ветка !BRANCH! уже на коммите !HEAD_AFTER!, версия !VER_AFTER!.
echo Если в чате названа более новая версия, вы на другой ветке.
echo Переключитесь на main:  git switch main
echo Список веток:           git branch -a
echo.

:build
echo [3/3] сборка !VER_AFTER!
call "%~dp0build_windows.bat"
if errorlevel 1 goto failed

echo.
echo Готово. Версия !VER_AFTER!, ветка !BRANCH!, коммит !HEAD_AFTER!.
exit /b 0

:failed
echo.
echo ОШИБКА: обновление не завершено. Лог сборки: build\build.log
exit /b 1
