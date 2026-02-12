@echo off
setlocal
cd /d "%~dp0"

if not exist build mkdir build

REM Use MSYS2 clang (match root build.bat)
set CLANG=D:\msys64\clang64\bin\clang.exe
set PATH=D:\msys64\clang64\bin;%PATH%
set PS=powershell -NoProfile -ExecutionPolicy Bypass -Command
set BUILD_EST=10

if not exist "%CLANG%" (
    echo [ERROR] clang not found at "%CLANG%"
    exit /b 1
)

echo [JIT Nodes Test] Using clang: %CLANG%
%CLANG% --version >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] clang failed to run. Check MSYS2 DLLs on PATH.
    exit /b 1
)

if not exist shaders\build\stygian.vert.glsl (
    echo [ERROR] Missing shader outputs. Run build_shaders.bat first.
    exit /b 1
)
if not exist shaders\build\stygian.frag.glsl (
    echo [ERROR] Missing shader outputs. Run build_shaders.bat first.
    exit /b 1
)

echo [JIT Nodes Test] Building with OpenGL backend...
echo.
set /p BUILD_CHOICE=Build which? [1=v1, 2=v2, 3=both]: 
if "%BUILD_CHOICE%"=="" set BUILD_CHOICE=3

set BUILD_V1=0
set BUILD_V2=0
if "%BUILD_CHOICE%"=="1" set BUILD_V1=1
if "%BUILD_CHOICE%"=="2" set BUILD_V2=1
if "%BUILD_CHOICE%"=="3" (
  set BUILD_V1=1
  set BUILD_V2=1
)

if "%BUILD_V1%"=="1" (
  %PS% "$sw=[Diagnostics.Stopwatch]::StartNew(); $est=%BUILD_EST%; $args=@('-O2','examples/jit_nodes.c','widgets/stygian_widgets.c','widgets/ide/file_explorer.c','widgets/ide/debug_tools.c','window/platform/stygian_win32.c','backends/stygian_ap_gl.c','src/stygian.c','src/stygian_triad.c','src/stygian_unicode.c','src/stygian_color.c','src/stygian_icc.c','src/stygian_mtsdf.c','src/stygian_clipboard.c','-I','include','-I','window','-I','backends','-I','widgets','-o','build/jit_nodes.exe','-luser32','-lgdi32','-ldwmapi','-lopengl32','-D_CRT_SECURE_NO_WARNINGS'); $p=Start-Process -FilePath '%CLANG%' -ArgumentList $args -NoNewWindow -PassThru; while(-not $p.HasExited){ $pct=[Math]::Min(99,[int]($sw.Elapsed.TotalSeconds/$est*100)); $n=[int]($pct/2); $bar=('#'*$n)+('-'*(50-$n)); Write-Host -NoNewline (\"`rBuild v1: [\"+$bar+\"] \"+$pct+\"%%\"); Start-Sleep -Milliseconds 80 }; Write-Host -NoNewline \"`rBuild v1: [##################################################] 100%%`n\"; $sw.Stop(); Write-Host (\"Build v1 time: {0:n2}s\" -f $sw.Elapsed.TotalSeconds); exit $p.ExitCode"
  if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
)

if "%BUILD_V2%"=="1" (
  %PS% "$sw=[Diagnostics.Stopwatch]::StartNew(); $est=%BUILD_EST%; $args=@('-O2','examples/jit_nodes_v2.c','widgets/stygian_widgets.c','widgets/ide/file_explorer.c','widgets/ide/debug_tools.c','window/platform/stygian_win32.c','backends/stygian_ap_gl.c','src/stygian.c','src/stygian_triad.c','src/stygian_unicode.c','src/stygian_color.c','src/stygian_icc.c','src/stygian_mtsdf.c','src/stygian_clipboard.c','-I','include','-I','window','-I','backends','-I','widgets','-o','build/jit_nodes_v2.exe','-luser32','-lgdi32','-ldwmapi','-lopengl32','-D_CRT_SECURE_NO_WARNINGS'); $p=Start-Process -FilePath '%CLANG%' -ArgumentList $args -NoNewWindow -PassThru; while(-not $p.HasExited){ $pct=[Math]::Min(99,[int]($sw.Elapsed.TotalSeconds/$est*100)); $n=[int]($pct/2); $bar=('#'*$n)+('-'*(50-$n)); Write-Host -NoNewline (\"`rBuild v2: [\"+$bar+\"] \"+$pct+\"%%\"); Start-Sleep -Milliseconds 80 }; Write-Host -NoNewline \"`rBuild v2: [##################################################] 100%%`n\"; $sw.Stop(); Write-Host (\"Build v2 time: {0:n2}s\" -f $sw.Elapsed.TotalSeconds); exit $p.ExitCode"
  if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
)

if "%BUILD_V1%"=="1" if "%BUILD_V2%"=="1" (
  echo [SUCCESS] Build complete. Outputs: build/jit_nodes.exe, build/jit_nodes_v2.exe
  echo.
  set /p RUN_CHOICE=Run now? [1=v1, 2=v2, Enter=skip]: 
  if "%RUN_CHOICE%"=="1" (
      build\jit_nodes.exe
  ) else if "%RUN_CHOICE%"=="2" (
      build\jit_nodes_v2.exe
  )
) else if "%BUILD_V1%"=="1" (
  echo [SUCCESS] Build complete. Output: build/jit_nodes.exe
  echo.
  set /p RUN_ONE=Run v1 now? [y/N]: 
  if /I "%RUN_ONE%"=="y" build\jit_nodes.exe
) else if "%BUILD_V2%"=="1" (
  echo [SUCCESS] Build complete. Output: build/jit_nodes_v2.exe
  echo.
  set /p RUN_ONE=Run v2 now? [y/N]: 
  if /I "%RUN_ONE%"=="y" build\jit_nodes_v2.exe
)
endlocal

