@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File compile\windows\build.ps1 -Group mini_apps_all %*
exit /b %ERRORLEVEL%