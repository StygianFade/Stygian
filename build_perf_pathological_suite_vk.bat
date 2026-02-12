@echo off
setlocal
cd /d "%~dp0"

if not exist build mkdir build

set CLANG=D:\msys64\clang64\bin\clang.exe
set VULKAN_SDK=D:\SystemTools\VulkanSDK\1.4
if not exist "%CLANG%" (
  echo [perf_pathological_suite_vk] ERROR: clang not found at "%CLANG%"
  exit /b 1
)

if not exist shaders\build\stygian.vert.glsl (
  echo [perf_pathological_suite_vk] Missing shader outputs. Run build_shaders.bat first.
  exit /b 1
)
if not exist shaders\build\stygian.frag.glsl (
  echo [perf_pathological_suite_vk] Missing shader outputs. Run build_shaders.bat first.
  exit /b 1
)

echo [perf_pathological_suite_vk] Building Vulkan suite...
"%CLANG%" -std=c2x -Wall -O0 -g ^
  -I"%VULKAN_SDK%\Include" ^
  -I include -I window -I backends -I widgets -I layout ^
  -DSTYGIAN_DEMO_VULKAN -DSTYGIAN_VULKAN ^
  examples/perf_pathological_suite.c ^
  examples/mini_perf_harness.c ^
  widgets/stygian_widgets.c ^
  src/stygian.c ^
  src/stygian_memory.c ^
  src/stygian_triad.c ^
  src/stygian_unicode.c ^
  src/stygian_color.c ^
  src/stygian_icc.c ^
  src/stygian_mtsdf.c ^
  src/stygian_clipboard.c ^
  layout/stygian_layout.c ^
  layout/stygian_tabs.c ^
  backends/stygian_ap_vk.c ^
  window/platform/stygian_win32.c ^
  -o build/perf_pathological_suite_vk.exe ^
  -L"%VULKAN_SDK%\Lib" ^
  -lvulkan-1 -luser32 -lgdi32 -ldwmapi -lopengl32 -lz -lzstd ^
  -D_CRT_SECURE_NO_WARNINGS

if %ERRORLEVEL% NEQ 0 (
  echo [perf_pathological_suite_vk] Build FAILED
  exit /b 1
)

echo [perf_pathological_suite_vk] Build SUCCESS: build/perf_pathological_suite_vk.exe
exit /b 0
