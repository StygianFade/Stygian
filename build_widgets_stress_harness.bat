@echo off
setlocal
cd /d "%~dp0"

if not exist build mkdir build

set CLANG=D:\msys64\clang64\bin\clang.exe
set VULKAN_SDK=D:\SystemTools\VulkanSDK\1.4

if not exist shaders\build\stygian.vert.glsl (
  echo [Widgets Stress Harness] Missing shader outputs. Run build_shaders.bat first.
  exit /b 1
)
if not exist shaders\build\stygian.frag.glsl (
  echo [Widgets Stress Harness] Missing shader outputs. Run build_shaders.bat first.
  exit /b 1
)

echo [Widgets Stress Harness] Building OpenGL test app...
%CLANG% -std=c2x -Wall -O0 -g ^
  -I"%VULKAN_SDK%\Include" ^
  examples/widgets_stress_harness.c ^
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
  -o build/widgets_stress_harness.exe ^
  -lopengl32 -luser32 -lgdi32 -ldwmapi -lz -lzstd

if %ERRORLEVEL% NEQ 0 (
  echo [Widgets Stress Harness] Build FAILED
  exit /b 1
)

echo [Widgets Stress Harness] Build SUCCESS: build\widgets_stress_harness.exe
exit /b 0
