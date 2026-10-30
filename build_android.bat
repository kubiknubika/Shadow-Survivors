@echo off
rem ===========================================================================
rem  build_android.bat - сборка APK для Android из этого же репозитория.
rem
rem  Двойной щелчок собирает debug-APK и кладёт его в build\ рядом с exe.
rem  Подробная инструкция, требования и разбор ошибок - ANDROID_BUILD.md.
rem
rem  Аргументы (любой порядок):
rem     release   - собрать release вместо debug
rem     install   - после сборки поставить APK на подключённый телефон (adb)
rem     clean     - сначала очистить прошлую сборку
rem
rem  Файл хранится в UTF-8, поэтому первым делом переключаем кодовую
rem  страницу консоли: иначе русский текст превращается в мусор.
rem ===========================================================================

for /f "tokens=2 delims=:" %%c in ('chcp') do set "CP_OLD=%%c"
chcp 65001 >nul 2>&1
setlocal enabledelayedexpansion

set "BD=%~dp0"
set "ANDROID=%BD%android"
set "BUILD=%BD%build"
set "LOG=%BUILD%\android_build.log"

if not exist "%BUILD%" mkdir "%BUILD%"
> "%LOG%" echo Android build started
>> "%LOG%" echo Date: %date% %time%

rem --------------------------------------------------------------- аргументы
set "TASK=assembleDebug"
set "DO_INSTALL=0"
set "CLEANARG="
:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="release" set "TASK=assembleRelease"
if /i "%~1"=="debug"   set "TASK=assembleDebug"
if /i "%~1"=="install" set "DO_INSTALL=1"
if /i "%~1"=="clean"   set "CLEANARG=clean"
shift
goto parse_args
:args_done

if not exist "%ANDROID%\settings.gradle" (
    echo ОШИБКА: не нашёл каталог android\ рядом с этим файлом.
    echo Положите build_android.bat в корень репозитория Shadow Survivors.
    >> "%LOG%" echo ERROR: android\settings.gradle not found
    goto fail
)

rem ------------------------------------------------------------------ версия
rem Номер версии берётся из src\version.h - того же файла, что и у exe:
rem иначе APK, заголовок окна и хроника в игре разошлись бы.
set "VER="
for /f "tokens=3" %%v in ('findstr /c:"#define GAME_VERSION " "%BD%src\version.h"') do set "VER=%%~v"
if "!VER!"=="" (
    echo ОШИБКА: не смог прочитать GAME_VERSION из src\version.h
    >> "%LOG%" echo ERROR: cannot read GAME_VERSION
    goto fail
)
for /f "tokens=1-3 delims=." %%a in ("!VER!") do (
    set "V1=%%a"
    set "V2=%%b"
    set "V3=%%c"
)
set /a "VCODE=!V1! * 1000 + !V2! * 100 + !V3!"
echo Версия из src\version.h: !VER! ^(код !VCODE!^)
>> "%LOG%" echo Version: !VER! code !VCODE!

rem -------------------------------------------------------------------- Java
rem Android Gradle Plugin 8.x требует JDK 17. Если JAVA_HOME не задан,
rem пробуем найти JDK, встроенный в Android Studio - он там всегда есть.
if not defined JAVA_HOME (
    if exist "%ProgramFiles%\Android\Android Studio\jbr\bin\java.exe" set "JAVA_HOME=%ProgramFiles%\Android\Android Studio\jbr"
)
if not defined JAVA_HOME (
    if exist "%ProgramFiles%\Android\Android Studio\jre\bin\java.exe" set "JAVA_HOME=%ProgramFiles%\Android\Android Studio\jre"
)
if not defined JAVA_HOME (
    where java >nul 2>&1
    if errorlevel 1 (
        echo ОШИБКА: не нашёл Java. Нужен JDK 17 или новее.
        echo Поставьте JDK 17 ^(Adoptium/Temurin^) или Android Studio:
        echo у Studio JDK уже внутри, bat найдёт его сам.
        >> "%LOG%" echo ERROR: java not found
        goto fail
    )
)
set "JAVA_EXE=java"
if defined JAVA_HOME set "JAVA_EXE=%JAVA_HOME%\bin\java.exe"
set "JVER=unknown"
for /f "tokens=3" %%j in ('"!JAVA_EXE!" -version 2^>^&1 ^| findstr /i /c:"version"' ) do set "JVER=%%~j"
echo Java: !JVER!
>> "%LOG%" echo Java: !JVER!
set "JMAJOR=0"
set "JMAJOR_N=0"
for /f "tokens=1,2 delims=._-" %%a in ("!JVER!") do (
    set "JMAJ=%%a"
    set "JMIN=%%b"
)
if "!JMAJ!"=="1" (set "JMAJOR=!JMIN!") else (set "JMAJOR=!JMAJ!")
rem set /a на нечисловой строке печатает ошибку, поэтому значение по умолчанию
rem уже стоит: разбор не должен ронять сборку.
set /a "JMAJOR_N=!JMAJOR!+0" >nul 2>&1
if !JMAJOR_N! LSS 17 (
    echo ВНИМАНИЕ: Java !JVER! старше 17. Gradle 8 может не запуститься.
    echo Если сборка упадёт - поставьте JDK 17 и повторите.
    >> "%LOG%" echo WARNING: java older than 17
)

rem --------------------------------------------------------------------- SDK
rem Ищем Android SDK: ANDROID_HOME, ANDROID_SDK_ROOT, стандартный путь Studio.
if not defined ANDROID_HOME if defined ANDROID_SDK_ROOT set "ANDROID_HOME=%ANDROID_SDK_ROOT%"
if not defined ANDROID_HOME (
    if exist "%LOCALAPPDATA%\Android\Sdk\platforms" set "ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk"
)
if not defined ANDROID_HOME (
    echo ОШИБКА: не нашёл Android SDK.
    echo Поставьте Android Studio ^(она ставит SDK^) или cmdline-tools,
    echo затем задайте переменную ANDROID_HOME на папку SDK.
    >> "%LOG%" echo ERROR: Android SDK not found
    goto fail
)
set "SDKDIR=!ANDROID_HOME:\=/!"
echo SDK: !SDKDIR!
>> "%LOG%" echo SDK: !SDKDIR!
> "%ANDROID%\local.properties" echo sdk.dir=!SDKDIR!

if not exist "!SDKDIR!/platforms" (
    echo ВНИМАНИЕ: в SDK нет папки platforms - Gradle попробует скачать её сам.
)

rem ------------------------------------------------------------------ Gradle
rem Три варианта: обёртка в android\, gradle в PATH, своя копия в build\.
set "GRADLE_CMD="
if exist "%ANDROID%\gradlew.bat" set "GRADLE_CMD=%ANDROID%\gradlew.bat"
if not defined GRADLE_CMD (
    rem Именно полный путь: call "gradle" с кавычками ищет файл с именем gradle
    rem и не находит ни gradle.bat, ни gradle.exe.
    for /f "delims=" %%g in ('where gradle.bat 2^>nul') do if not defined GRADLE_CMD set "GRADLE_CMD=%%g"
)
if not defined GRADLE_CMD (
    if exist "%BUILD%\gradle-dist\gradle-8.2\bin\gradle.bat" set "GRADLE_CMD=%BUILD%\gradle-dist\gradle-8.2\bin\gradle.bat"
)
if not defined GRADLE_CMD (
    echo Gradle не установлен - скачиваю 8.2 ^(~130 МБ, только в первый раз^).
    >> "%LOG%" echo Downloading Gradle 8.2
    powershell -NoProfile -ExecutionPolicy Bypass -Command "$ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri 'https://services.gradle.org/distributions/gradle-8.2-bin.zip' -OutFile '%BUILD%\gradle-8.2-bin.zip'; Expand-Archive -Force -Path '%BUILD%\gradle-8.2-bin.zip' -DestinationPath '%BUILD%\gradle-dist'" >> "%LOG%" 2>&1
    if exist "%BUILD%\gradle-dist\gradle-8.2\bin\gradle.bat" (
        set "GRADLE_CMD=%BUILD%\gradle-dist\gradle-8.2\bin\gradle.bat"
    ) else (
        echo ОШИБКА: не смог скачать Gradle.
        echo Поставьте Gradle 8.2+ вручную и добавьте его в PATH,
        echo либо соберите через Android Studio ^(см. ANDROID_BUILD.md^).
        >> "%LOG%" echo ERROR: gradle download failed
        goto fail
    )
)
echo Gradle: !GRADLE_CMD!
>> "%LOG%" echo Gradle: !GRADLE_CMD!

rem ------------------------------------------------------------------- сборка
echo.
echo Собираю %TASK% ... журнал: build\android_build.log
>> "%LOG%" echo ---- gradle %CLEANARG% %TASK% ----
set "GRADLE_ARGS=-p "%ANDROID%" -PappVersion=!VER! -PappVersionCode=!VCODE! --no-daemon --console=plain --stacktrace"
if defined CLEANARG set "GRADLE_ARGS=!GRADLE_ARGS! clean"
call "!GRADLE_CMD!" !GRADLE_ARGS! %TASK% >> "%LOG%" 2>&1
if errorlevel 1 (
    echo.
    echo ОШИБКА: Gradle не собрал APK. Последние строки журнала:
    echo.
    powershell -NoProfile -Command "Get-Content -LiteralPath '%LOG%' -Tail 24"
    echo.
    echo Полный журнал: build\android_build.log
    echo Разбор частых ошибок: ANDROID_BUILD.md, раздел «Если не собралось».
    goto fail
)

rem ---------------------------------------------------------------------- APK
set "APK="
if /i "%TASK%"=="assembleDebug" (
    for %%f in ("%ANDROID%\app\build\outputs\apk\debug\*.apk") do set "APK=%%~ff"
) else (
    for %%f in ("%ANDROID%\app\build\outputs\apk\release\*.apk") do set "APK=%%~ff"
)
if defined APK if not exist "!APK!" set "APK="
if not defined APK (
    echo ОШИБКА: сборка прошла, но APK не найден в app\build\outputs\apk\.
    >> "%LOG%" echo ERROR: apk not found
    goto fail
)
set "SUFFIX=debug"
if /i "%TASK%"=="assembleRelease" set "SUFFIX=release"
set "APKOUT=%BUILD%\ShadowSurvivors_!VER!_!SUFFIX!.apk"
copy /y "!APK!" "!APKOUT!" >nul
if errorlevel 1 (
    echo ОШИБКА: не смог скопировать APK в build\.
    >> "%LOG%" echo ERROR: cannot copy apk
    goto fail
)
for %%F in ("!APKOUT!") do set "APKSIZE=%%~zF"
>> "%LOG%" echo APK: !APKOUT! ^(!APKSIZE! bytes^)
echo.
echo ГОТОВО: APK собран.
echo   !APKOUT!
echo   размер: !APKSIZE! байт, версия !VER!, %TASK%

rem ------------------------------------------------------------------- adb
if "%DO_INSTALL%"=="1" (
    set "ADB="
    for /f "delims=" %%a in ('where adb.exe 2^>nul') do if not defined ADB set "ADB=%%a"
    if not defined ADB if exist "!SDKDIR!/platform-tools/adb.exe" set "ADB=!SDKDIR!/platform-tools/adb.exe"
    if not defined ADB (
        echo ВНИМАНИЕ: adb не найден - установите APK вручную.
    ) else (
        echo Ставлю на подключённый телефон ...
        "!ADB!" install -r "!APKOUT!" >> "%LOG%" 2>&1
        if errorlevel 1 (
            echo ВНИМАНИЕ: adb install не сработал. Проверьте: телефон подключён,
            echo отладка по USB включена, на телефоне разрешена установка.
            echo Последние строки журнала:
            powershell -NoProfile -Command "Get-Content -LiteralPath '%LOG%' -Tail 10"
        ) else (
            echo Установлено. Запускайте «Пепел Тьмы» на телефоне.
        )
    )
) else (
    echo Установка: build_android.bat install  ^(или скопируйте APK на телефон^).
)

>> "%LOG%" echo [%date% %time%] Build finished
goto done

:fail
echo.
echo Сборка НЕ завершена. Журнал: build\android_build.log
echo Инструкция и разбор ошибок: ANDROID_BUILD.md
:done
if defined CP_OLD chcp !CP_OLD! >nul 2>&1
echo.
pause
endlocal
exit /b 0
