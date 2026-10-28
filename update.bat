@echo off
setlocal
cd /d "%~dp0"

echo === Shadow Survivors: обновление ===
echo.

rem Версия берётся из src\version.h: локального счётчика больше нет, поэтому
rem обновление ничего не восстанавливает и не затирает.

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
