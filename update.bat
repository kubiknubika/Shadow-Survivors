@echo off
setlocal
cd /d "%~dp0"

echo === Shadow Survivors: обновление ===
echo.

rem 1. Раньше version.txt хранился в репозитории и переписывался сборкой, из-за чего
rem    git pull ругался на локальные изменения. Теперь файл не отслеживается:
rem    возвращаем ему состояние из репозитория (если он ещё отслеживается) и
rem    спокойно обновляемся. Если файла нет — команда просто ничего не делает.
git checkout -- version.txt >nul 2>&1

echo [1/2] git pull
git pull
if errorlevel 1 goto failed

echo.
echo [2/2] сборка
call "%~dp0build_windows.bat"
if errorlevel 1 goto failed

echo.
echo Готово.
exit /b 0

:failed
echo.
echo ОШИБКА: обновление не завершено. Лог сборки: build_log.txt
exit /b 1
