@echo off
rem ============================================================================
rem  Shadow Survivors updater.
rem
rem  Deliberately plain: ASCII only, no "if ( ... )" blocks, no chcp.
rem  A .bat is parsed by cmd.exe line by line in the console codepage, and that
rem  is exactly where a double-clicked window dies without saying anything: a
rem  block that spans lines, or a file full of Cyrillic read as cp866.
rem  Everything below is a single-line if + goto, and every message is ASCII.
rem
rem  Every exit path pauses and writes build\update.log, so the window can no
rem  longer vanish before the reason is readable.
rem ============================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "TARGET=main"
set "REMOTE=origin"
set "SRC=%cd%\src"
set "BUILD=%cd%\build"
set "LOG=%BUILD%\update.log"
set "STEP=%BUILD%\update_step.log"
set "HOLD=1"
if /i "%~1"=="/nopause" set "HOLD=0"

if exist "%BUILD%" goto have_build
mkdir "%BUILD%" >nul 2>&1
:have_build
> "%LOG%" echo === Shadow Survivors updater ===
>> "%LOG%" echo %DATE% %TIME%   folder: %cd%

echo === Shadow Survivors: update ===
echo.

if exist "%SRC%\version.h" goto have_src
echo ERROR: src\version.h not found next to this file.
echo Run update.bat from the root of the game folder.
goto fail
:have_src

where git >nul 2>&1
if not errorlevel 1 goto have_git
echo ERROR: git is not in PATH.
echo Install Git for Windows, then run update.bat again.
goto fail
:have_git

for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_BEFORE=%%~v"
for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD 2^>nul') do set "BRANCH=%%b"
for /f "delims=" %%c in ('git rev-parse --short HEAD 2^>nul') do set "HEAD_BEFORE=%%c"
if defined BRANCH goto have_repo
echo ERROR: this folder is not a git clone - there is nothing to update.
echo Clone the game again:
echo   git clone https://github.com/kubiknubika/Shadow-Survivors.git
goto fail
:have_repo

set "BRANCH_FROM=!BRANCH!"
echo Current: branch !BRANCH!, commit !HEAD_BEFORE!, version !VER_BEFORE!
>> "%LOG%" echo current: branch=!BRANCH! commit=!HEAD_BEFORE! version=!VER_BEFORE!
echo.

echo [1/4] git fetch --all --prune
git fetch --all --prune > "%STEP%" 2>&1
if not errorlevel 1 goto fetched
echo ERROR: cannot reach GitHub. Check network and remote address:
echo   git remote -v
type "%STEP%"
goto fail
:fetched

git show !REMOTE!/!TARGET!:src/version.h > "%STEP%.h" 2>nul
if not errorlevel 1 goto have_remote
echo ERROR: branch !TARGET! was not found on !REMOTE!. Branches that exist:
git branch -r
goto fail
:have_remote
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%STEP%.h"') do set "VER_REMOTE=%%~v"
for /f "delims=" %%c in ('git rev-parse --short !REMOTE!/!TARGET!') do set "HEAD_REMOTE=%%c"
del "%STEP%.h" >nul 2>&1
echo On GitHub, !TARGET!: version !VER_REMOTE!, commit !HEAD_REMOTE!
>> "%LOG%" echo remote: !TARGET!=!HEAD_REMOTE! version=!VER_REMOTE!
echo.

echo [2/4] branch !TARGET!
if /i "!BRANCH!"=="!TARGET!" goto on_target
echo You are on !BRANCH!. The release is built from !TARGET!, so switching over.
echo Your own commits and files stay in !BRANCH! - nothing is deleted.
git switch !TARGET! > "%STEP%" 2>&1
if not errorlevel 1 goto switched
echo ERROR: could not switch to !TARGET! - uncommitted changes or no local branch.
type "%STEP%"
echo Try these steps:
echo   git status
echo   git stash -u
echo   git switch -c !TARGET! --track !REMOTE!/!TARGET!
echo   git stash pop
echo On git older than 2.23 write checkout instead of switch.
goto fail
:switched
set "BRANCH=!TARGET!"
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_BEFORE=%%~v"
for /f "delims=" %%c in ('git rev-parse --short HEAD') do set "HEAD_BEFORE=%%c"
echo Switched.
echo.
:on_target

echo [3/4] git pull --ff-only
set "UPSTREAM="
for /f "delims=" %%u in ('git rev-parse --abbrev-ref --symbolic-full-name @{u} 2^>nul') do set "UPSTREAM=%%u"
if defined UPSTREAM goto have_upstream
echo No upstream on !TARGET! - linking it to !REMOTE!/!TARGET!.
git branch --set-upstream-to=!REMOTE!/!TARGET! > "%STEP%" 2>&1
if errorlevel 1 goto fail_step
:have_upstream
git pull --ff-only > "%STEP%" 2>&1
type "%STEP%"
if not errorlevel 1 goto pulled
echo.
echo ERROR: pull failed, nothing was built. The old exe is still in place.
set "AHEAD="
for /f "delims=" %%n in ('git rev-list --count !REMOTE!/!TARGET!..HEAD 2^>nul') do set "AHEAD=%%n"
if not defined AHEAD goto no_ahead
if "!AHEAD!"=="0" goto no_ahead
echo Your own commits on top of !REMOTE!/!TARGET!: !AHEAD! - ff-only refuses those.
echo   git log --oneline !REMOTE!/!TARGET!..HEAD
:no_ahead
echo   git status          uncommitted changes
echo   git stash -u        set them aside for the update
goto fail
:pulled

set "VER_AFTER="
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER_AFTER=%%~v"
for /f "delims=" %%c in ('git rev-parse --short HEAD') do set "HEAD_AFTER=%%c"

if not "!HEAD_BEFORE!"=="!HEAD_AFTER!" goto moved
echo.
echo NOTE: nothing new - !TARGET! is at !HEAD_AFTER!, version !VER_AFTER!.
if /i not "!VER_AFTER!"=="!VER_REMOTE!" goto drifted
echo This is the newest version in !TARGET! on GitHub.
echo If another version was promised, it is still in a work branch, not merged:
echo   git log --oneline !REMOTE!/!TARGET! -3
goto build
:drifted
echo On GitHub !TARGET! is already !VER_REMOTE!, locally !VER_AFTER! -
echo the files disagree with the repository:
echo   git status
echo   git diff !REMOTE!/!TARGET! --stat
goto build
:moved
echo Updated: !HEAD_BEFORE! -^> !HEAD_AFTER!, version !VER_AFTER!
>> "%LOG%" echo updated: !HEAD_BEFORE! -^> !HEAD_AFTER! version=!VER_AFTER!

:build
echo.
echo [4/4] build !VER_AFTER!
call "%~dp0build_windows.bat"
if errorlevel 1 goto fail

echo.
echo Done. Version !VER_AFTER!, branch !BRANCH!, commit !HEAD_AFTER!.
if /i not "!BRANCH_FROM!"=="!TARGET!" echo Back to your work branch with: git switch !BRANCH_FROM!
>> "%LOG%" echo done: version=!VER_AFTER! branch=!BRANCH! commit=!HEAD_AFTER!
goto done

:fail_step
type "%STEP%"
:fail
echo.
echo Update did NOT complete. Log: %LOG%
>> "%LOG%" echo FAILED - see the messages above
if "!HOLD!"=="1" pause
exit /b 1

:done
if "!HOLD!"=="1" pause
exit /b 0
