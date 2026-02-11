@echo off
REM Build Complete Widget Test with OpenGL backend

setlocal
cd /d "%~dp0"

if not exist build mkdir build

set CLANG=D:\msys64\clang64\bin\clang.exe
set VULKAN_SDK=D:\SystemTools\VulkanSDK\1.4

if not exist shaders\build\stygian.vert.glsl (
  echo [Widget Full Test] Missing shader outputs. Run build_shaders.bat first.
  exit /b 1
)
if not exist shaders\build\stygian.frag.glsl (
  echo [Widget Full Test] Missing shader outputs. Run build_shaders.bat first.
  exit /b 1
)

echo [Widget Full Test] Building with OpenGL backend...
%CLANG% -std=c2x -Wall -O0 -g ^
  -I"%VULKAN_SDK%\Include" ^
  examples/widgets_full_test.c ^
  src/stygian.c ^
  src/stygian_memory.c ^
  src/stygian_triad.c ^
  src/stygian_unicode.c ^
  src/stygian_color.c ^
  src/stygian_icc.c ^
  src/stygian_clipboard.c ^
  src/stygian_mtsdf.c ^
  layout/stygian_layout.c ^
  layout/stygian_tabs.c ^
  widgets/stygian_widgets.c ^
  backends/stygian_ap_gl.c ^
  window/platform/stygian_win32.c ^
  -o build/widgets_full_test.exe ^
  -lopengl32 -luser32 -lgdi32 -ldwmapi -lz -lzstd

if %ERRORLEVEL% NEQ 0 (
  echo [Widget Full Test] Build FAILED
  exit /b 1
)

echo [Widget Full Test] Build SUCCESS

