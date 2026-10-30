@echo off
setlocal enabledelayedexpansion

set "BD=%~dp0"
set "SRC=%BD%src"
set "BUILD=%BD%build"
set "LOG=%BUILD%\build.log"
set "RC=%BUILD%\resource.rc"
set "RESOBJ=%BUILD%\resource.o"
set "TMPEXE=%BUILD%\ShadowSurvivors.exe"

if not exist "%BUILD%" mkdir "%BUILD%"
> "%LOG%" echo Build started
>> "%LOG%" echo Date: %date% %time%

:: --- version: src\version.h is the only source of truth ---
:: This script used to prefer a local version.txt counter, and that counter drifted
:: far behind the header (a build reported 2.9.78 while the game shipped 2.9.82).
:: The header decides now. An explicit argument still overrides it.
set "VER="
if not "%~1"=="" set "VER=%~1"
if not defined VER for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%SRC%\version.h"') do set "VER=%%~v"

set VER=!VER:"=!
for /f "tokens=* delims= " %%v in ("!VER!") do set "VER=%%v"
if "!VER!"=="" (
    echo ERROR: cannot read GAME_VERSION from src\version.h >> "%LOG%"
    echo ERROR: cannot read GAME_VERSION from src\version.h
    pause
    exit /b 1
)

echo !VER!| findstr /r /c:"^[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$" >nul
if errorlevel 1 (
    echo ERROR: bad version "!VER!" - expected x.y.z >> "%LOG%"
    echo ERROR: bad version "!VER!" - expected x.y.z
    pause
    exit /b 1
)

:: --- parse version parts ---
for /f "tokens=1-3 delims=." %%a in ("!VER!") do (
    set "V1=%%a"
    set "V2=%%b"
    set "V3=%%c"
)
set /a "VCODE=!V1! * 1000 + !V2! * 100 + !V3!"
set "EXE=%BD%ShadowSurvivors_!VER!.exe"

echo Version (src\version.h): !VER! >> "%LOG%"
echo Exe: ShadowSurvivors_!VER!.exe >> "%LOG%"

:: --- check windres ---
where windres >nul 2>&1
if errorlevel 1 (
    echo ERROR: windres not found >> "%LOG%"
    echo Install MinGW-w64 and add bin to PATH >> "%LOG%"
    pause
    exit /b 1
)
echo [OK] windres >> "%LOG%"

:: --- check g++ ---
where g++ >nul 2>&1
if errorlevel 1 (
    echo ERROR: g++ not found >> "%LOG%"
    echo Install MinGW-w64 and add bin to PATH >> "%LOG%"
    pause
    exit /b 1
)
echo [OK] g++ >> "%LOG%"

:: --- check source ---
if not exist "!SRC!\main_win32.cpp" (
    echo ERROR: main_win32.cpp not found in !SRC! >> "%LOG%"
    pause
    exit /b 1
)
if not exist "!SRC!\icon.ico" (
    echo ERROR: icon.ico not found in !SRC! >> "%LOG%"
    pause
    exit /b 1
)
echo [OK] source files >> "%LOG%"

:: --- generate resource.rc (never committed: build/ is a scratch directory) ---
:: Forward slashes keep windres from treating path backslashes as escapes.
set "RCSRC=!SRC:\=/!"
(
    echo #include ^<windows.h^>
    echo 1 ICON "!RCSRC!/icon.ico"
    echo.
    echo 1 VERSIONINFO
    echo FILEVERSION !V1!,!V2!,!V3!,0
    echo PRODUCTVERSION !V1!,!V2!,!V3!,0
    echo BEGIN
    echo     BLOCK "StringFileInfo"
    echo     BEGIN
    echo         BLOCK "041904B0"
    echo         BEGIN
    echo             VALUE "CompanyName", "Shadow Forge Indie"
    echo             VALUE "FileDescription", "Shadow Survivors !VER! - Action Survival Roguelike"
    echo             VALUE "FileVersion", "!VER!"
    echo             VALUE "InternalName", "ShadowSurvivors"
    echo             VALUE "OriginalFilename", "ShadowSurvivors_!VER!.exe"
    echo             VALUE "ProductName", "Shadow Survivors"
    echo             VALUE "ProductVersion", "!VER!"
    echo         END
    echo     END
    echo     BLOCK "VarFileInfo"
    echo     BEGIN
    echo         VALUE "Translation", 0x0419, 1200
    echo     END
    echo END
) > "!RC!"
echo [OK] resource.rc written >> "%LOG%"

:: --- compile resources ---
windres "!RC!" -O coff -o "!RESOBJ!" >> "%LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: windres failed >> "%LOG%"
    pause
    exit /b 1
)
echo [OK] windres compile >> "%LOG%"

:: --- compile game (into build/, so a failed build never destroys the shipped exe) ---
g++ -O3 -std=c++17 -I"!SRC!" -DGAME_VERSION=\"!VER!\" -DGAME_VERSION_NUM=!VCODE! "!SRC!\main_win32.cpp" "!RESOBJ!" -o "!TMPEXE!" -mwindows -static -static-libgcc -static-libstdc++ -lwinmm -lgdi32 -luser32 -ldwmapi -pthread >> "%LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: g++ compile failed >> "%LOG%"
    echo ERROR: g++ compile failed - see build\build.log
    pause
    exit /b 1
)
echo [OK] g++ compile >> "%LOG%"

move /y "!TMPEXE!" "%EXE%" >nul
if errorlevel 1 (
    echo ERROR: cannot write %EXE% ^(is the game running?^) >> "%LOG%"
    echo ERROR: cannot write %EXE% ^(is the game running?^)
    pause
    exit /b 1
)
for %%F in ("%EXE%") do echo Size: %%~zF bytes >> "%LOG%"
:: Одна и та же точка входа после каждой сборки: в корне рядом со старыми
:: ShadowSurvivors_2.9.xx.exe оставался прежний ShadowSurvivors.exe, и запускали
:: как раз его — игра «показывала» старую версию при новой сборке в папке.
copy /y "%EXE%" "%BD%ShadowSurvivors.exe" >nul
echo BUILD SUCCESS: ShadowSurvivors_!VER!.exe >> "%LOG%"

:: The version is not bumped here any more: it is bumped in src\version.h when a
:: patch ships, so the exe name, the window title and the chronicle always agree.
echo [%date% %time%] Build finished >> "%LOG%"

echo BUILD SUCCESS: ShadowSurvivors_!VER!.exe
echo Запускать: ShadowSurvivors.exe ^(скопирован из этой сборки^)
pause
exit /b 0
