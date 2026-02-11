@echo off
REM Build Stygian demo using clang

setlocal
cd /d "%~dp0"

if not exist build mkdir build

set CLANG=D:\msys64\clang64\bin\clang.exe

echo [Stygian] Building demo (with console)...
%CLANG% -std=c2x -Wall -O0 -g ^
  examples/demo.c ^
  src/stygian.c ^
  src/stygian_triad.c ^
  src/stygian_unicode.c ^
  src/stygian_color.c ^
  src/stygian_icc.c ^
  src/stygian_mtsdf.c ^
  backends/stygian_ap_gl.c ^
  window/platform/stygian_win32.c ^
  -o build/stygian_demo.exe ^
  -lopengl32 -luser32 -lgdi32 -ldwmapi

if %ERRORLEVEL% NEQ 0 (
    echo [Stygian] Build FAILED
    exit /b 1
)



echo [Stygian] Build SUCCESS
echo [Stygian] Configured for OpenGL.

endlocal

