@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File compile\windows\build.ps1 -Target calculator_mini_vk %*
exit /b %ERRORLEVEL%