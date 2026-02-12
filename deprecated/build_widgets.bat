@echo off
REM Build Stygian Widgets Demo with OpenGL backend

setlocal
cd /d "%~dp0"

if not exist build mkdir build

set CLANG=D:\msys64\clang64\bin\clang.exe

echo [Widgets Demo] Building with OpenGL backend...
%CLANG% -std=c2x -Wall -O0 -g ^
  examples/widgets_demo.c ^
  src/stygian.c ^
src/stygian_triad.c ^
  src/stygian_mtsdf.c ^
  layout/stygian_tabs.c ^
  layout/stygian_docking.c ^
  widgets/stygian_widgets.c ^
  widgets/ide/file_explorer.c ^
  widgets/ide/debug_tools.c ^
  widgets/game/viewport_scene.c ^
  widgets/game/assets_console.c ^
  backends/stygian_ap_gl.c ^
  window/platform/stygian_win32.c ^
  -o build/widgets_demo.exe ^
  -lopengl32 -luser32 -lgdi32 -ldwmapi

if %ERRORLEVEL% NEQ 0 (
    echo [Widgets Demo] Build FAILED
    exit /b 1
)

echo [Widgets Demo] Build SUCCESS
echo [Widgets Demo] Exit code: %ERRORLEVEL%
pause
endlocal

