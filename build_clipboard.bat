@echo off
setlocal

set LLVM_PATH=C:\Program Files\LLVM\bin
set PATH=%LLVM_PATH%;%PATH%

echo [Clipboard Test] Building with OpenGL backend...

clang -O0 -g ^
    examples/clipboard_test.c ^
    widgets/stygian_widgets.c ^
    widgets/ide/file_explorer.c ^
    widgets/ide/debug_tools.c ^
    window/platform/stygian_win32.c ^
    backends/stygian_ap_gl.c ^
    src/stygian.c ^
src/stygian_triad.c ^
    src/stygian_mtsdf.c ^
    src/stygian_clipboard.c ^
    -I include -I window -I backends -I widgets ^
    -o build/clipboard_test.exe ^
    -luser32 -lgdi32 -ldwmapi -lopengl32 ^
    -D_CRT_SECURE_NO_WARNINGS

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Build complete. Output: build/clipboard_test.exe
endlocal

