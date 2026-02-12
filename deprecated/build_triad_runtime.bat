@echo off
setlocal
cd /d "%~dp0"

if not exist build mkdir build

set CLANG=D:\msys64\clang64\bin\clang.exe

if not exist "%CLANG%" (
  echo [TRIAD] ERROR: clang not found at "%CLANG%"
  exit /b 1
)

echo [TRIAD] Building triad runtime probe...
"%CLANG%" -std=c2x -Wall -Wextra -O2 ^
  tools/triad_runtime_probe.c ^
  src/stygian_triad.c ^
  -I include ^
  -o build/triad_runtime_probe.exe

if %ERRORLEVEL% NEQ 0 (
  echo [TRIAD] Build FAILED
  exit /b 1
)

echo [TRIAD] Build SUCCESS: build/triad_runtime_probe.exe

if "%~1"=="" (
  echo [TRIAD] Run example:
  echo   build\triad_runtime_probe.exe tools\wavelet_bench\triad_out\stygian_emoji_master_t2.triad
  exit /b 0
)

echo [TRIAD] Running probe...
build\triad_runtime_probe.exe %*
exit /b %ERRORLEVEL%
