@echo off
REM Stygian Shader Compilation Script
REM Uses shaderc (glslc) for both OpenGL and Vulkan
REM Run this after modifying any .glsl file

setlocal

REM Find glslc from Vulkan SDK
set GLSLC=
if defined VULKAN_SDK set GLSLC=%VULKAN_SDK%\Bin\glslc.exe
if not defined GLSLC if exist "D:\SystemTools\VulkanSDK\1.4\Bin\glslc.exe" set GLSLC=D:\SystemTools\VulkanSDK\1.4\Bin\glslc.exe
if not defined GLSLC set GLSLC=glslc

REM Check if glslc exists
"%GLSLC%" --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] glslc not found. Install Vulkan SDK or add to PATH.
    exit /b 1
)

REM Create build directory
if not exist build mkdir build

echo [Shaderc] Compiling shaders...

REM Preprocess for OpenGL (with -DSTYGIAN_GL define)
REM Then strip #line and #extension directives that OpenGL GLSL doesn't support
"%GLSLC%" -E -DSTYGIAN_GL stygian.vert 2>&1 | findstr /V /R /C:"^#line" /C:"^#extension" > build\stygian.vert.glsl
if errorlevel 1 goto :error
"%GLSLC%" -E -DSTYGIAN_GL stygian.frag 2>&1 | findstr /V /R /C:"^#line" /C:"^#extension" > build\stygian.frag.glsl
if errorlevel 1 goto :error

REM Compile to SPIR-V for Vulkan (no extra define needed)
"%GLSLC%" stygian.vert -o build/stygian.vert.spv 2>&1
if errorlevel 1 goto :error
"%GLSLC%" stygian.frag -o build/stygian.frag.spv 2>&1
if errorlevel 1 goto :error

echo [Shaderc] SUCCESS - Output in build/
exit /b 0

:error
echo [Shaderc] FAILED
exit /b 1
