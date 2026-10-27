@echo off
setlocal enabledelayedexpansion

chcp 1251 >nul

set "BD=%~dp0"
set "LOG=%BD%build.log"
set "VERFILE=%BD%version.txt"
set "SRC=%BD%src"

:: --- start log ---
echo Build started > "%LOG%"
echo Date: %date% %time% >> "%LOG%"

:: --- read version ---
set "VER=2.9.70"
if exist "%VERFILE%" (
    set /p "VER=" < "%VERFILE%"
)
if "!VER!"=="" set "VER=2.9.70"

echo Version: !VER! >> "%LOG%"
echo Exe: ShadowSurvivors_!VER!.exe >> "%LOG%"

:: --- parse version parts ---
for /f "tokens=1-3 delims=." %%a in ("!VER!") do (
    set "V1=%%a"
    set "V2=%%b"
    set "V3=%%c"
)

set "EXE=ShadowSurvivors_!VER!.exe"

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

:: --- clean old ---
if exist "%EXE%" del /f /q "%EXE%"
if exist "!SRC!\resource.o" del /f /q "!SRC!\resource.o"
echo [OK] cleaned >> "%LOG%"

:: --- generate resource.rc ---
(
    echo #include ^<windows.h^>
    echo 1 ICON "icon.ico"
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
    echo             VALUE "OriginalFilename", "ShadowSurvivors.exe"
    echo             VALUE "ProductName", "Shadow Survivors"
    echo             VALUE "ProductVersion", "!VER!"
    echo         END
    echo     END
    echo     BLOCK "VarFileInfo"
    echo     BEGIN
    echo         VALUE "Translation", 0x0419, 1200
    echo     END
    echo END
) > "!SRC!\resource.rc"
echo [OK] resource.rc written >> "%LOG%"

:: --- compile resources ---
pushd "!SRC!"
windres resource.rc -O coff -o resource.o >> "%LOG%" 2>&1
if errorlevel 1 (
    popd
    echo ERROR: windres failed >> "%LOG%"
    pause
    exit /b 1
)
popd
echo [OK] windres compile >> "%LOG%"

:: --- compile game ---
g++ -O3 -std=c++17 -I"!SRC!" "!SRC!\main_win32.cpp" "!SRC!\resource.o" -o "%EXE%" -mwindows -static -static-libgcc -static-libstdc++ -lwinmm -lgdi32 -luser32 -ldwmapi -lkernel32 >> "%LOG%" 2>&1
if errorlevel 1 (
    echo ERROR: g++ compile failed >> "%LOG%"
    pause
    exit /b 1
)

:: --- success ---
echo BUILD SUCCESS: %EXE% >> "%LOG%"

set /a "V3=!V3! + 1"
set "NVER=!V1!.!V2!.!V3!"

echo!NVER!>"%VERFILE%"
echo Next version: !NVER! >> "%LOG%"
echo [%date% %time%] Build finished >> "%LOG%"

pause
exit /b 0
