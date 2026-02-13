@echo off
setlocal
cd /d "%~dp0\..\.."

if not exist shaders (
  echo [Shaders] ERROR: shaders directory not found
  exit /b 1
)

echo [Shaders] Building OpenGL and Vulkan shader outputs...
pushd shaders
call compile.bat
if %ERRORLEVEL% NEQ 0 (
  popd
  echo [Shaders] Build FAILED
  exit /b 1
)
popd

echo [Shaders] Build SUCCESS
exit /b 0

