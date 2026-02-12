@echo off
REM Build Layout Test with Vulkan backend

setlocal
cd /d "%~dp0"

if not exist build mkdir build

set CLANG=D:\msys64\clang64\bin\clang.exe
set VULKAN_SDK=D:\SystemTools\VulkanSDK\1.4

echo [Layout Test] Building with OpenGL backend...
%CLANG% -std=c2x -Wall -O0 -g ^
  -I"%VULKAN_SDK%\Include" ^
  examples/layout_test.c ^
  src/stygian.c ^
src/stygian_triad.c ^
  src/stygian_mtsdf.c ^
  layout/stygian_layout.c ^
  layout/stygian_tabs.c ^
  widgets/stygian_widgets.c ^
  backends/stygian_ap_gl.c ^
  window/platform/stygian_win32.c ^
  -o build/layout_test.exe ^
  -lopengl32 -luser32 -lgdi32 -ldwmapi

if %ERRORLEVEL% NEQ 0 (
  echo [Layout Test] Build FAILED
  exit /b 1
)

echo [Layout Test] Build SUCCESS

