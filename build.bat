@echo off
setlocal
cd /d "%~dp0"
REM Legacy entrypoint: build the primary mini app target.
powershell -NoProfile -ExecutionPolicy Bypass -File compile\windows\build.ps1 -Target text_editor_mini %*
exit /b %ERRORLEVEL%