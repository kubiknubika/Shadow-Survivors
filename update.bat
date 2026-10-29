@echo off
chcp 866 >nul
rem ============================================================================
rem  Shadow Survivors updater.
rem
rem  Файл записан в cp866 - иначе вместо русского текста в консоли кракозябры:
rem  cmd.exe читает .bat в кодовой странице консоли, а не в UTF-8. Из-за этого
rem  раньше вывод ломался, и куски строк выполнялись как команды.
rem  Русский текст встречается только в echo и rem: все команды - ASCII.
rem  Многострочные if ( ... ) с русским внутри ломаются, как в build_windows.bat.
rem
rem  Любой выход завершается паузой и записью build\update.log, чтобы окно
rem  не исчезало раньше, чем видна причина.
rem
rem  Сеть, ветка и pull не обрывают работу: если обновиться не удалось,
rem  апдейтер собирает то, что уже лежит на диске, - игра всё равно запустится.
rem ============================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

rem Версия самого апдейтера: по ней видно, свежий файл скачан или старый.
set "SELF=2.10.15"
set "TARGET=main"
set "REMOTE=origin"
set "SRC=%cd%\src"
set "BUILD=%cd%\build"
set "LOG=%BUILD%\update.log"
set "STEP=%BUILD%\update_step.log"
set "KEEP=%BUILD%\update.local.bat"
set "HOLD=1"
set "STASHED="
if /i "%~1"=="/nopause" set "HOLD=0"

if exist "%BUILD%" goto have_build
mkdir "%BUILD%" >nul 2>&1
:have_build
> "%LOG%" echo === Shadow Survivors updater ===
set "HERE=%cd%"
>> "%LOG%" echo %DATE% %TIME%   folder: !HERE!
>> "%LOG%" echo версия апдейтера: !SELF!

echo === Shadow Survivors: обновление ===
echo версия апдейтера: !SELF!
echo.

rem --- проверки окружения ---------------------------------------------------
if exist "%SRC%\version.h" goto have_src
echo ОШИБКА: рядом нет папки src\version.h.
echo Запусти update.bat из корня игры, а не из другой папки.
goto fail
:have_src

where git >nul 2>&1
if not errorlevel 1 goto have_git
echo ОШИБКА: git не найден.
echo Установи Git for Windows и запусти update.bat снова.
goto fail
:have_git

for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_BEFORE=%%~v"
for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD 2^>nul') do set "BRANCH=%%b"
for /f "delims=" %%c in ('git rev-parse --short HEAD 2^>nul') do set "HEAD_BEFORE=%%c"
if defined BRANCH goto have_repo
echo ОШИБКА: эта папка не git-репозиторий, обновлять нечего.
echo Скачай игру так:
echo   git clone https://github.com/kubiknubika/Shadow-Survivors.git
goto fail
:have_repo
set "BRANCH_FROM=!BRANCH!"
echo Ветка: !BRANCH!, коммит !HEAD_BEFORE!, версия !VER_BEFORE!
>> "%LOG%" echo current: branch=!BRANCH! commit=!HEAD_BEFORE! version=!VER_BEFORE!
echo.

rem --- 1/4: свежие данные с GitHub -----------------------------------------
echo [1/4] git fetch --all --prune
git fetch --all --prune > "%STEP%" 2>&1
if not errorlevel 1 goto fetched
echo ОШИБКА: не удалось связаться с GitHub. Проверь сеть и адрес remote:
echo   git remote -v
type "%STEP%"
echo Собираю то, что уже скачано, без обновления.
>> "%LOG%" echo offline: fetch failed
goto offline_build
:fetched

git show !REMOTE!/!TARGET!:src/version.h > "%STEP%.h" 2>nul
if not errorlevel 1 goto have_remote
echo ВНИМАНИЕ: на !REMOTE! нет ветки !TARGET!. Доступны такие:
git branch -r
echo Собираю то, что уже скачано, без обновления.
>> "%LOG%" echo offline: no remote branch
goto offline_build
:have_remote
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%STEP%.h"') do set "VER_REMOTE=%%~v"
for /f "delims=" %%c in ('git rev-parse --short !REMOTE!/!TARGET!') do set "HEAD_REMOTE=%%c"
del "%STEP%.h" >nul 2>&1
echo На GitHub в !TARGET!: версия !VER_REMOTE!, коммит !HEAD_REMOTE!
>> "%LOG%" echo remote: !TARGET!=!HEAD_REMOTE! version=!VER_REMOTE!
echo.

rem --- 2/4: переключение на ветку обновления ---------------------------------
echo [2/4] ветка !TARGET!
if /i "!BRANCH!"=="!TARGET!" goto on_target
echo Ты на ветке !BRANCH!. Апдейтер собирает ветку !TARGET! - переключаюсь.
echo Незакоммиченные правки в файлах на !BRANCH! не удаляются.
git switch !TARGET! > "%STEP%" 2>&1
if not errorlevel 1 goto switched
git switch -c !TARGET! --track !REMOTE!/!TARGET! > "%STEP%.c" 2>&1
if not errorlevel 1 goto switched

rem Отказано. Обычная причина: этот самый файл скачан с GitHub руками, поэтому
rem он отличается от версии в ветке, и git не даёт его перезаписать.
echo Переключение отклонено. Git сообщил:
type "%STEP%"
del "%STEP%.c" >nul 2>&1
set "BLOCKERS="
for /f "tokens=1,*" %%a in ('git status --porcelain 2^>nul') do call :note_change "%%b"
if defined BLOCKERS goto switch_blocked
rem Мешает только update.bat. Скачанный экземпляр сохраняем, ветку чистим,
rem переключаемся, а скачанный возвращаем после pull.
if not exist "update.bat" goto retry_switch
copy /y "update.bat" "%KEEP%" >nul 2>&1
git checkout -- update.bat > "%STEP%" 2>&1
if not errorlevel 1 goto cleaner_done
git checkout HEAD -- update.bat > "%STEP%" 2>&1
if not errorlevel 1 goto cleaner_done
del "update.bat" >nul 2>&1
:cleaner_done
echo update.bat отличается от версии в !BRANCH!: сохранил его в build\update.local.bat,
echo чтобы правки в ветке не мешали переключению, и верну обратно после pull.
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
echo Переключиться не дают изменённые файлы:
git status --short
echo.
echo Прячу их в git stash и пробую снова - ничего не пропадёт.
git stash push -u -m "update.bat: autosave before update" > "%STEP%" 2>&1
if errorlevel 1 goto switch_give_up
type "%STEP%"
set "STASHED=1"
goto retry_switch

:switch_give_up
echo ВНИМАНИЕ: спрятать правки не удалось, ветку не меняю.
echo Собираю то, что лежит на диске - игра обновится в следующий раз.
>> "%LOG%" echo offline: switch blocked
goto offline_build

:switched
set "BRANCH=!TARGET!"
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_BEFORE=%%~v"
for /f "delims=" %%c in ('git rev-parse --short HEAD') do set "HEAD_BEFORE=%%c"
echo Переключился.
echo.
:on_target

rem --- 3/4: обновление кода ------------------------------------------------
echo [3/4] git pull --ff-only
set "UPSTREAM="
for /f "delims=" %%u in ('git rev-parse --abbrev-ref --symbolic-full-name @{u} 2^>nul') do set "UPSTREAM=%%u"
if defined UPSTREAM goto have_upstream
echo У !TARGET! нет upstream - привязываю к !REMOTE!/!TARGET!.
git branch --set-upstream-to=!REMOTE!/!TARGET! > "%STEP%" 2>&1
if errorlevel 1 goto fail_step
:have_upstream
rem Скачанный руками update.bat новее того, что лежит в ветке. Если обновление
rem тронет этот файл, локальная копия остановит pull, поэтому откладываем её
rem тем же способом и возвращаем перед сборкой.
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
echo update.bat отличается от версии в !TARGET!: отложил его на время pull, верну после.
:pull_now
git pull --ff-only > "%STEP%" 2>&1
type "%STEP%"
if not errorlevel 1 goto pulled
echo.
echo ВНИМАНИЕ: pull не удался - обновление кода не прошло.
set "AHEAD="
for /f "delims=" %%n in ('git rev-list --count !REMOTE!/!TARGET!..HEAD 2^>nul') do set "AHEAD=%%n"
if not defined AHEAD goto no_ahead
if "!AHEAD!"=="0" goto no_ahead
echo В твоей ветке есть коммиты, которых нет в !REMOTE!/!TARGET!: !AHEAD! - поэтому ff-only не проходит.
echo   git log --oneline !REMOTE!/!TARGET!..HEAD
:no_ahead
echo   git status          посмотреть состояние файлов
echo   git stash -u        убрать правки на время обновления
echo Собираю текущий код, чтобы игра всё равно запускалась.
>> "%LOG%" echo offline: pull failed
goto build
:pulled

set "VER_AFTER="
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_AFTER=%%~v"
for /f "delims=" %%c in ('git rev-parse --short HEAD') do set "HEAD_AFTER=%%c"

if not "!HEAD_BEFORE!"=="!HEAD_AFTER!" goto moved
echo.
echo Обновлять нечего: !TARGET! уже на коммите !HEAD_AFTER!, версия !VER_AFTER!.
if /i "!VER_AFTER!"=="!VER_REMOTE!" goto matched
goto drifted
:matched
echo Локальная копия совпадает с !TARGET! на GitHub.
echo Если это не то, что ты ждал - посмотри, что нового в рабочей ветке:
echo   git log --oneline !REMOTE!/!TARGET! -3
goto build
:drifted
echo На GitHub в !TARGET! уже !VER_REMOTE!, а локально !VER_AFTER! -
echo файлы расходятся с репозиторием:
echo   git status
echo   git diff !REMOTE!/!TARGET! --stat
goto build
:moved
echo Обновлено: !HEAD_BEFORE! -^> !HEAD_AFTER!, версия !VER_AFTER!
>> "%LOG%" echo updated: !HEAD_BEFORE! -^> !HEAD_AFTER! version=!VER_AFTER!
goto build

:offline_build
rem Сюда попадаем, если обновиться не удалось: версию и коммит берём с диска.
set "VER_AFTER="
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_AFTER=%%~v"
for /f "delims=" %%c in ('git rev-parse --short HEAD') do set "HEAD_AFTER=%%c"
echo.
echo Обновление кода пропущено - собираю версию, которая уже лежит на диске.
>> "%LOG%" echo offline: version=!VER_AFTER! commit=!HEAD_AFTER!

rem --- 4/4: сборка ---------------------------------------------------------
:build
if not exist "%KEEP%" goto no_restore
copy /y "%KEEP%" "update.bat" >nul 2>&1
del "%KEEP%" >nul 2>&1
echo.
echo Скачанный update.bat возвращён - он новее версии в !TARGET!.
echo Если следующее обновление его снова затрёт:  git checkout -- update.bat
:no_restore
echo.
echo [4/4] сборка !VER_AFTER!
call "%~dp0build_windows.bat"
if errorlevel 1 goto fail

echo.
echo Готово. Версия !VER_AFTER!, ветка !BRANCH!, коммит !HEAD_AFTER!.
>> "%LOG%" echo done: version=!VER_AFTER! branch=!BRANCH! commit=!HEAD_AFTER!
call :put_back
goto done

rem Возвращает игрока на его ветку и достаёт правки из stash, если апдейтер
rem их прятал. Вызывается и при успехе, и при ошибке.
:put_back
if /i "!BRANCH!"=="!BRANCH_FROM!" goto put_back_stash
git switch !BRANCH_FROM! > "%STEP%" 2>&1
if errorlevel 1 goto put_back_manual
set "BRANCH=!BRANCH_FROM!"
echo Вернулся на твою ветку !BRANCH_FROM!.
goto put_back_stash
:put_back_manual
echo Вернись на свою ветку вручную: git switch !BRANCH_FROM!
:put_back_stash
if not defined STASHED goto put_back_done
git stash pop > "%STEP%" 2>&1
if errorlevel 1 goto put_back_conflict
echo Правки возвращены из stash.
set "STASHED="
goto put_back_done
:put_back_conflict
echo Правки остались в stash: git stash list, потом git stash pop.
echo Ничего не потеряно - файлы лежат в stash.
set "STASHED="
:put_back_done
goto :eof

rem Вызывается для каждого изменённого пути: блокирует переключение всё,
rem кроме самого апдейтера. cmd кладёт первый токен в первую переменную,
rem поэтому путь здесь - вторая переменная; старая версия сравнивала пустой
rem остаток с update.bat и всегда находила помеху.
:note_change
set "CHANGED=%~1"
if not defined CHANGED goto :eof
if /i "!CHANGED!"=="update.bat" goto :eof
set "BLOCKERS=1"
goto :eof

:fail_step
type "%STEP%"
:fail
if exist "%KEEP%" copy /y "%KEEP%" "update.bat" >nul 2>&1
if exist "%KEEP%" del "%KEEP%" >nul 2>&1
echo.
echo Обновление не завершено. Подробности в файле: !LOG!
call :put_back
>> "%LOG%" echo FAILED - see the messages above
if "!HOLD!"=="1" pause
exit /b 1

:done
if "!HOLD!"=="1" pause
exit /b 0
