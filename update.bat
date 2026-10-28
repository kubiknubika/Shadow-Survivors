@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul 2>&1
cd /d "%~dp0"

echo === Shadow Survivors: обновление ===
echo.

rem Готовая игра всегда собирается из main. Работа идёт в ветках arena/... : они живут
rem до merge, а потом удаляются на GitHub. Прежний скрипт делал git pull текущей ветки
rem и ломался двумя способами:
rem   * на живой рабочей ветке pull отвечал "Already up to date" — main ушёл вперёд,
rem     а скрипт молча собирал старую версию;
rem   * после удаления ветки pull падал с "no such ref was fetched" и до сборки
rem     не доходил вовсе.
rem Теперь цель — origin/main: рабочая копия переключается на него, а свои правки
rem остаются в прежней ветке.

set "TARGET=main"
set "REMOTE=origin"
set "SRC=%cd%\src"

where git >nul 2>&1
if errorlevel 1 (
    echo ОШИБКА: git не найден. Установите Git for Windows и повторите.
    goto failed
)
if not exist "%SRC%\version.h" (
    echo ОШИБКА: рядом со скриптом нет папки src. Запускайте update.bat из корня игры.
    goto failed
)

for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_BEFORE=%%~v"
for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD 2^>nul') do set "BRANCH=%%b"
for /f "delims=" %%c in ('git rev-parse --short HEAD 2^>nul') do set "HEAD_BEFORE=%%c"

if not defined BRANCH (
    echo ОШИБКА: папка не является git-копией, обновлять нечего.
    echo Склонируйте игру заново:
    echo   git clone https://github.com/kubiknubika/Shadow-Survivors.git
    goto failed
)

set "BRANCH_FROM=!BRANCH!"
echo Сейчас: ветка !BRANCH!, коммит !HEAD_BEFORE!, версия !VER_BEFORE!
echo.

echo [1/4] git fetch --all --prune
git fetch --all --prune
if errorlevel 1 (
    echo ОШИБКА: нет связи с GitHub. Проверьте сеть и адрес:  git remote -v
    goto failed
)

rem Версию на GitHub печатаем всегда: именно её сравнивают с версией из чата.
git show !REMOTE!/!TARGET!:src/version.h > "%TEMP%\ss_remote_version.h" 2>nul
if errorlevel 1 (
    echo ОШИБКА: в !REMOTE! нет ветки !TARGET!. Что есть:  git branch -r
    goto failed
)
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%TEMP%\ss_remote_version.h"') do set "VER_REMOTE=%%~v"
for /f "delims=" %%c in ('git rev-parse --short !REMOTE!/!TARGET!') do set "HEAD_REMOTE=%%c"
del "%TEMP%\ss_remote_version.h" >nul 2>&1
echo На GitHub в !TARGET!: версия !VER_REMOTE!, коммит !HEAD_REMOTE!
echo.

echo [2/4] ветка !TARGET!
if /i "!BRANCH!"=="!TARGET!" goto pull
echo Вы на ветке !BRANCH! — это рабочая ветка, готовая игра собирается из !TARGET!.
echo Переключаюсь: git switch !TARGET! ничего не удаляет, ваши правки останутся в !BRANCH!.
echo.
git switch !TARGET!
if errorlevel 1 (
    echo.
    echo ОШИБКА: переключиться на !TARGET! не вышло: мешают незакоммиченные правки
    echo или локальной ветки !TARGET! нет. По шагам:
    echo   git status
    echo   git stash -u
    echo   git switch -c !TARGET! --track !REMOTE!/!TARGET!
    echo   git stash pop
    echo На git старше 2.23 вместо switch пишите checkout.
    goto failed
)
set "BRANCH=!TARGET!"
rem Меряем заново уже на !TARGET!: сравнение ниже должно говорить о pull, а не о смене ветки.
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_BEFORE=%%~v"
for /f "delims=" %%c in ('git rev-parse --short HEAD') do set "HEAD_BEFORE=%%c"
echo.

:pull
echo [3/4] git pull --ff-only
set "UPSTREAM="
for /f "delims=" %%u in ('git rev-parse --abbrev-ref --symbolic-full-name @{u} 2^>nul') do set "UPSTREAM=%%u"
if not defined UPSTREAM (
    echo У ветки !TARGET! нет upstream — привязываю к !REMOTE!/!TARGET!.
    git branch --set-upstream-to=!REMOTE!/!TARGET!
    if errorlevel 1 goto failed
)
git pull --ff-only
if errorlevel 1 (
    echo.
    echo ОШИБКА: pull не прошёл — обновление остановлено, старая сборка осталась рабочей.
    set "AHEAD="
    for /f "delims=" %%n in ('git rev-list --count !REMOTE!/!TARGET!..HEAD 2^>nul') do set "AHEAD=%%n"
    if defined AHEAD if not "!AHEAD!"=="0" (
        echo Своих коммитов поверх !REMOTE!/!TARGET!: !AHEAD! — их и не пускает --ff-only.
        echo   git log --oneline !REMOTE!/!TARGET!..HEAD
    )
    echo   git status         незакоммиченные правки
    echo   git stash -u       убрать их на время обновления
    goto failed
)

set "VER_AFTER="
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_AFTER=%%~v"
for /f "delims=" %%c in ('git rev-parse --short HEAD') do set "HEAD_AFTER=%%c"

if "!HEAD_BEFORE!"=="!HEAD_AFTER!" goto nothing_new
echo Обновлено: !HEAD_BEFORE! -^> !HEAD_AFTER!, версия !VER_AFTER!
goto build

:nothing_new
echo.
echo ВНИМАНИЕ: обновлений не было — !TARGET! уже на коммите !HEAD_AFTER!, версия !VER_AFTER!.
if /i "!VER_AFTER!"=="!VER_REMOTE!" (
    echo Это последняя версия в !TARGET! на GitHub.
    echo Если в чате названа другая — она ещё в рабочей ветке и в !TARGET! не влита:
    echo   git log --oneline !REMOTE!/!TARGET! -3
) else (
    echo На GitHub в !TARGET! уже !VER_REMOTE!, локально !VER_AFTER! — файлы разошлись с репозиторием:
    echo   git status
    echo   git diff !REMOTE!/!TARGET! --stat
)
echo.

:build
echo [4/4] сборка !VER_AFTER!
call "%~dp0build_windows.bat"
if errorlevel 1 goto failed

echo.
echo Готово. Версия !VER_AFTER!, ветка !BRANCH!, коммит !HEAD_AFTER!.
if /i not "!BRANCH_FROM!"=="!TARGET!" (
    echo Вернуться в рабочую ветку:  git switch !BRANCH_FROM!
)
exit /b 0

:failed
echo.
echo ОШИБКА: обновление не завершено. Лог сборки: build\build.log
exit /b 1
