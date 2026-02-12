@echo off
REM Build Jello Test with OpenGL backend using Clang
setlocal
cd /d "%~dp0"

if not exist build mkdir build

set CLANG=D:\msys64\clang64\bin\clang.exe

echo [Jello Test] Building with OpenGL backend...

%CLANG% -std=c2x -Wall -O0 -g ^
  examples/jello_test.c ^
  src/stygian.c ^
src/stygian_triad.c ^
  src/stygian_mtsdf.c ^
  backends/stygian_ap_gl.c ^
  window/platform/stygian_win32.c ^
  -o build/jello_test.exe ^
  -lopengl32 -luser32 -lgdi32 -ldwmapi

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed
    exit /b 1
)

echo [SUCCESS] Build complete. Run build\jello_test.exe

