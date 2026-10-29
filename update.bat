@echo off
chcp 866 >nul
rem ============================================================================
rem  Shadow Survivors updater.
rem
rem  Текст файла - cp866, и первой строкой стоит chcp 866: русские сообщения
rem  читаются в консоли как есть, без ромбиков. Раньше файл был в UTF-8, а
rem  cmd.exe читает .bat построчно в кодовой странице консоли - отсюда были
rem  крякозябры и строка, которую cmd принимал за команду.
rem  Логика - однострочные if и goto, без блоков if ( ... ), как в build_windows.bat.
rem
rem  Every exit path pauses and writes build\update.log, so the window can no
rem  longer vanish before the reason is readable.
rem ============================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

rem Bump this with the version: it is the fastest way to tell a freshly
rem downloaded updater from a cached old one in the console and in the log.
set "SELF=2.10.12"
set "TARGET=main"
set "REMOTE=origin"
set "SRC=%cd%\src"
set "BUILD=%cd%\build"
set "LOG=%BUILD%\update.log"
set "STEP=%BUILD%\update_step.log"
set "KEEP=%BUILD%\update.local.bat"
set "HOLD=1"
if /i "%~1"=="/nopause" set "HOLD=0"

if exist "%BUILD%" goto have_build
mkdir "%BUILD%" >nul 2>&1
:have_build
> "%LOG%" echo === Shadow Survivors updater ===
>> "%LOG%" echo %DATE% %TIME%   folder: %cd%
>> "%LOG%" echo версия апдейтера !SELF!

echo === Shadow Survivors: обновление ===
echo версия апдейтера !SELF!
echo.

if exist "%SRC%\version.h" goto have_src
echo ОШИБКА: рядом со скриптом нет src\version.h.
echo Запускайте update.bat из корня папки с игрой.
goto fail
:have_src

where git >nul 2>&1
if not errorlevel 1 goto have_git
echo ОШИБКА: git не найден.
echo Установите Git for Windows и запустите update.bat снова.
goto fail
:have_git

for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_BEFORE=%%~v"
for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD 2^>nul') do set "BRANCH=%%b"
for /f "delims=" %%c in ('git rev-parse --short HEAD 2^>nul') do set "HEAD_BEFORE=%%c"
if defined BRANCH goto have_repo
echo ОШИБКА: папка не является git-копией, обновлять нечего.
echo Склонируйте игру заново:
echo   git clone https://github.com/kubiknubika/Shadow-Survivors.git
goto fail
:have_repo

set "BRANCH_FROM=!BRANCH!"
echo Сейчас: ветка !BRANCH!, коммит !HEAD_BEFORE!, версия !VER_BEFORE!
>> "%LOG%" echo current: branch=!BRANCH! commit=!HEAD_BEFORE! version=!VER_BEFORE!
echo.

echo [1/4] git fetch --all --prune
git fetch --all --prune > "%STEP%" 2>&1
if not errorlevel 1 goto fetched
echo ОШИБКА: нет связи с GitHub. Проверьте сеть и адрес remote:
echo   git remote -v
type "%STEP%"
goto fail
:fetched

git show !REMOTE!/!TARGET!:src/version.h > "%STEP%.h" 2>nul
if not errorlevel 1 goto have_remote
echo ОШИБКА: на !REMOTE! нет ветки !TARGET!. Есть такие:
git branch -r
goto fail
:have_remote
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%STEP%.h"') do set "VER_REMOTE=%%~v"
for /f "delims=" %%c in ('git rev-parse --short !REMOTE!/!TARGET!') do set "HEAD_REMOTE=%%c"
del "%STEP%.h" >nul 2>&1
echo На GitHub в !TARGET!: версия !VER_REMOTE!, коммит !HEAD_REMOTE!
>> "%LOG%" echo remote: !TARGET!=!HEAD_REMOTE! version=!VER_REMOTE!
echo.

echo [2/4] ветка !TARGET!
if /i "!BRANCH!"=="!TARGET!" goto on_target
echo Вы на ветке !BRANCH!. Готовая игра собирается из !TARGET! - переключаюсь.
echo Ваши коммиты и файлы останутся в !BRANCH! - ничего не удаляется.
git switch !TARGET! > "%STEP%" 2>&1
if not errorlevel 1 goto switched
git switch -c !TARGET! --track !REMOTE!/!TARGET! > "%STEP%.c" 2>&1
if not errorlevel 1 goto switched

rem Refused. The usual culprit: this very file was downloaded by hand, so it
rem differs from the branch and git will not overwrite it during the switch.
echo Переключение отклонено. Git говорит:
type "%STEP%"
del "%STEP%.c" >nul 2>&1
set "BLOCKERS="
for /f "tokens=1,*" %%a in ('git status --porcelain 2^>nul') do call :note_change "%%b"
if defined BLOCKERS goto switch_blocked
rem Only update.bat is dirty. Keep the downloaded copy, clean the branch copy,
rem switch, and put the downloaded one back after the pull.
if not exist "update.bat" goto retry_switch
copy /y "update.bat" "%KEEP%" >nul 2>&1
git checkout -- update.bat > "%STEP%" 2>&1
if not errorlevel 1 goto cleaner_done
git checkout HEAD -- update.bat > "%STEP%" 2>&1
if not errorlevel 1 goto cleaner_done
del "update.bat" >nul 2>&1
:cleaner_done
echo Ваш update.bat новее версии в !BRANCH!: сохранил его в build\update.local.bat,
echo вернул версию из ветки для переключения и верну вашу обратно после.
:retry_switch
git switch !TARGET! > "%STEP%" 2>&1
if not errorlevel 1 goto switched
git switch -c !TARGET! --track !REMOTE!/!TARGET! > "%STEP%.c" 2>&1
if not errorlevel 1 goto switched
type "%STEP%"
del "%STEP%.c" >nul 2>&1
goto switch_blocked

:switch_blocked
echo.
echo ОШИБКА: не могу переключиться на !TARGET!. Изменены файлы:
git status --short
echo.
echo Сохраните их и запустите update.bat снова:
echo   git stash -u       убрать правки в сторону
echo   update.bat         запустить этот файл снова
echo   git stash pop      вернуть правки
echo Апдейтер ничего не удалил и не перезаписал.
echo На git старше 2.23 вместо switch пишите checkout.
goto fail

:switched
set "BRANCH=!TARGET!"
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_BEFORE=%%~v"
for /f "delims=" %%c in ('git rev-parse --short HEAD') do set "HEAD_BEFORE=%%c"
echo Переключено.
echo.
:on_target

echo [3/4] git pull --ff-only
set "UPSTREAM="
for /f "delims=" %%u in ('git rev-parse --abbrev-ref --symbolic-full-name @{u} 2^>nul') do set "UPSTREAM=%%u"
if defined UPSTREAM goto have_upstream
echo У !TARGET! нет upstream - привязываю к !REMOTE!/!TARGET!.
git branch --set-upstream-to=!REMOTE!/!TARGET! > "%STEP%" 2>&1
if errorlevel 1 goto fail_step
:have_upstream
rem The hand-downloaded updater is newer than the one in !TARGET!. If a future
rem update touches update.bat, a local copy would block the pull, so put it
rem aside the same way and copy it back before the build.
if exist "!KEEP!" goto pull_now
set "BLOCKERS="
for /f "tokens=1,*" %%a in ('git status --porcelain 2^>nul') do call :note_change "%%b"
if defined BLOCKERS goto pull_now
for /f "tokens=2" %%a in ('git status --porcelain -- update.bat 2^>nul') do if /i "%%a"=="update.bat" copy /y "update.bat" "!KEEP!" >nul 2>&1
if not exist "!KEEP!" goto pull_now
git checkout -- update.bat > "%STEP%" 2>&1
if not errorlevel 1 goto updater_aside
del "update.bat" >nul 2>&1
:updater_aside
echo Ваш update.bat новее версии в !TARGET!: отложил на время pull, верну после.
:pull_now
git pull --ff-only > "%STEP%" 2>&1
type "%STEP%"
if not errorlevel 1 goto pulled
echo.
echo ОШИБКА: pull не прошёл, сборки не было. Старый exe на месте.
set "AHEAD="
for /f "delims=" %%n in ('git rev-list --count !REMOTE!/!TARGET!..HEAD 2^>nul') do set "AHEAD=%%n"
if not defined AHEAD goto no_ahead
if "!AHEAD!"=="0" goto no_ahead
echo Своих коммитов поверх !REMOTE!/!TARGET!: !AHEAD! - их и не пускает ff-only.
echo   git log --oneline !REMOTE!/!TARGET!..HEAD
:no_ahead
echo   git status          незакоммиченные правки
echo   git stash -u        убрать их на время обновления
goto fail
:pulled

set "VER_AFTER="
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_AFTER=%%~v"
for /f "delims=" %%c in ('git rev-parse --short HEAD') do set "HEAD_AFTER=%%c"

if not "!HEAD_BEFORE!"=="!HEAD_AFTER!" goto moved
echo.
echo ВНИМАНИЕ: обновлений не было - !TARGET! на коммите !HEAD_AFTER!, версия !VER_AFTER!.
if /i not "!VER_AFTER!"=="!VER_REMOTE!" goto drifted
echo Это последняя версия в !TARGET! на GitHub.
echo Если в чате названа другая - она ещё в рабочей ветке и не влита:
echo   git log --oneline !REMOTE!/!TARGET! -3
goto build
:drifted
echo На GitHub в !TARGET! уже !VER_REMOTE!, локально !VER_AFTER! -
echo файлы разошлись с репозиторием:
echo   git status
echo   git diff !REMOTE!/!TARGET! --stat
goto build
:moved
echo Обновлено: !HEAD_BEFORE! -^> !HEAD_AFTER!, версия !VER_AFTER!
>> "%LOG%" echo updated: !HEAD_BEFORE! -^> !HEAD_AFTER! version=!VER_AFTER!

:build
if not exist "%KEEP%" goto no_restore
copy /y "%KEEP%" "update.bat" >nul 2>&1
del "%KEEP%" >nul 2>&1
echo.
echo Ваш скачанный update.bat сохранён - он новее версии в !TARGET!.
echo Если следующее обновление снова откажется:  git checkout -- update.bat
:no_restore
echo.
echo [4/4] сборка !VER_AFTER!
call "%~dp0build_windows.bat"
if errorlevel 1 goto fail

echo.
echo Готово. Версия !VER_AFTER!, ветка !BRANCH!, коммит !HEAD_AFTER!.
if /i not "!BRANCH_FROM!"=="!TARGET!" echo Back to your work branch with: git switch !BRANCH_FROM!
>> "%LOG%" echo done: version=!VER_AFTER! branch=!BRANCH! commit=!HEAD_AFTER!
goto done

rem Called for every changed path: anything except this updater counts as a
rem blocker. cmd puts token 1 into the first variable whatever tokens= says, so
rem the path here is the second variable - the previous version compared the
rem empty remainder with "update.bat" and always found a blocker, even when the
rem only change was this very file.
:note_change
if "%~1"=="" goto :eof
if /i "%~1"=="update.bat" goto :eof
if /i "%~1"=="\"update.bat\"" goto :eof
set "BLOCKERS=1"
goto :eof

:fail_step
type "%STEP%"
:fail
if exist "%KEEP%" copy /y "%KEEP%" "update.bat" >nul 2>&1
if exist "%KEEP%" del "%KEEP%" >nul 2>&1
echo.
echo Обновление НЕ завершено. Лог: %LOG%
>> "%LOG%" echo FAILED - see the messages above
if "!HOLD!"=="1" pause
exit /b 1

:done
if "!HOLD!"=="1" pause
exit /b 0
