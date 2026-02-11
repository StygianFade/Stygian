@echo off
REM Build Tab System Test with OpenGL backend

setlocal
cd /d "%~dp0"

if not exist build mkdir build

set CLANG=D:\msys64\clang64\bin\clang.exe

echo [Tab Test] Building with OpenGL backend...
%CLANG% -std=c2x -Wall -O0 -g ^
  examples/tabs_test.c ^
  src/stygian.c ^
src/stygian_triad.c ^
  src/stygian_mtsdf.c ^
  layout/stygian_layout.c ^
  layout/stygian_tabs.c ^
  widgets/stygian_widgets.c ^
  backends/stygian_ap_gl.c ^
  window/platform/stygian_win32.c ^
  -o build/tabs_test.exe ^
  -lopengl32 -luser32 -lgdi32 -ldwmapi

if %ERRORLEVEL% NEQ 0 (
  echo [Tab Test] Build FAILED
  exit /b 1
)

echo [Tab Test] Build SUCCESS

