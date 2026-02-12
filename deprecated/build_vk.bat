@echo off
REM Build Stygian demo with Vulkan backend
REM Requires Vulkan SDK at D:\SystemTools\VulkanSDK\1.4

setlocal
cd /d "%~dp0"

if not exist build mkdir build

set CLANG=D:\msys64\clang64\bin\clang.exe
set VULKAN_SDK=D:\SystemTools\VulkanSDK\1.4

echo [Stygian VK] Building demo with Vulkan backend...
"%CLANG%" -std=c2x -Wall -O0 -g ^
  -I"%VULKAN_SDK%\Include" ^
  -I include ^
  -I window ^
  -I backends ^
  -DSTYGIAN_DEMO_VULKAN ^
  -DSTYGIAN_VULKAN ^
  examples/demo.c ^
  src/stygian.c ^
  src/stygian_memory.c ^
  src/stygian_triad.c ^
  src/stygian_unicode.c ^
  src/stygian_color.c ^
  src/stygian_icc.c ^
  src/stygian_mtsdf.c ^
  backends/stygian_ap_vk.c ^
  window/platform/stygian_win32.c ^
  -o build/stygian_demo_vk.exe ^
  -L"%VULKAN_SDK%\Lib" ^
  -lvulkan-1 -luser32 -lgdi32 -ldwmapi -lopengl32 -D_CRT_SECURE_NO_WARNINGS

if %ERRORLEVEL% NEQ 0 (
    echo [Stygian VK] Build FAILED
    exit /b 1
)

echo [Stygian VK] Build SUCCESS
set /p RUN_NOW=Run now? [y/N]: 
if /I "%RUN_NOW%"=="y" build\stygian_demo_vk.exe
endlocal

