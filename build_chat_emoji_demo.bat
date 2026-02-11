@echo off
setlocal
cd /d "%~dp0"

if not exist build mkdir build

set CLANG=D:\msys64\clang64\bin\clang.exe
if not exist "%CLANG%" (
  echo [ChatEmoji] ERROR: clang not found at "%CLANG%"
  exit /b 1
)

if not exist shaders\build\stygian.vert.glsl (
  echo [ChatEmoji] Missing shader outputs. Run build_shaders.bat first.
  exit /b 1
)
if not exist shaders\build\stygian.frag.glsl (
  echo [ChatEmoji] Missing shader outputs. Run build_shaders.bat first.
  exit /b 1
)

echo [ChatEmoji] Building chat emoji demo...
"%CLANG%" -std=c2x -Wall -O2 ^
  examples/chat_emoji_demo.c ^
  widgets/stygian_widgets.c ^
  src/stygian.c ^
  src/stygian_memory.c ^
  src/stygian_triad.c ^
  src/stygian_unicode.c ^
  src/stygian_color.c ^
  src/stygian_icc.c ^
  src/stygian_mtsdf.c ^
  src/stygian_clipboard.c ^
  tools/wavelet_bench/third_party/lz4.c ^
  backends/stygian_ap_gl.c ^
  window/platform/stygian_win32.c ^
  -I include ^
  -I window ^
  -I backends ^
  -I widgets ^
  -I tools/wavelet_bench/third_party ^
  -o build/chat_emoji_demo.exe ^
  -luser32 -lgdi32 -ldwmapi -lopengl32 -lz -lzstd -D_CRT_SECURE_NO_WARNINGS

if %ERRORLEVEL% NEQ 0 (
  echo [ChatEmoji] Build FAILED
  exit /b 1
)

echo [ChatEmoji] Build SUCCESS: build/chat_emoji_demo.exe
set /p RUN_NOW=Run now? [y/N]: 
if /I "%RUN_NOW%"=="y" build\chat_emoji_demo.exe
exit /b 0
