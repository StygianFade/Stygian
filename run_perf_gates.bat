@echo off
setlocal
cd /d "%~dp0"

set PSH=powershell -NoProfile -ExecutionPolicy Bypass

if "%1"=="" (
  %PSH% -File tests\run_perf_gates.ps1 -Backend both -Seconds 6
) else (
  %PSH% -File tests\run_perf_gates.ps1 %*
)

exit /b %ERRORLEVEL%
