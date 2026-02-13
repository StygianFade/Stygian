@echo off
setlocal
cd /d "%~dp0\..\.."
powershell -NoProfile -ExecutionPolicy Bypass -File compile\windows\build.ps1 -Target quickwindow_custom_titlebar %*
exit /b %ERRORLEVEL%
