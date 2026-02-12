@echo off
setlocal
cd /d "%~dp0"

if not exist build mkdir build

set CLANG=D:\msys64\clang64\bin\clang.exe
if not exist "%CLANG%" (
  echo [text_editor_mini] ERROR: clang not found at "%CLANG%"
  exit /b 1
)

if not exist shaders\build\stygian.vert.glsl (
  echo [text_editor_mini] Missing shader outputs. Run build_shaders.bat first.
  exit /b 1
)
if not exist shaders\build\stygian.frag.glsl (
  echo [text_editor_mini] Missing shader outputs. Run build_shaders.bat first.
  exit /b 1
)

echo [text_editor_mini] Building...
"%CLANG%" -std=c2x -Wall -O0 -g ^
  examples/text_editor_mini.c ^
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
  backends/stygian_ap_gl.c ^
  window/platform/stygian_win32.c ^
  -I include -I window -I backends -I widgets -I layout ^
  -o build/text_editor_mini.exe ^
  -luser32 -lgdi32 -ldwmapi -lopengl32 -lz -lzstd -D_CRT_SECURE_NO_WARNINGS

if %ERRORLEVEL% NEQ 0 (
  echo [text_editor_mini] Build FAILED
  exit /b 1
)

echo [text_editor_mini] Build SUCCESS: build/text_editor_mini.exe
exit /b 0
