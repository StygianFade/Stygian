@echo off
REM Build shaders first, then widgets stress harness, widgets full test, chat emoji demo.
setlocal
cd /d "%~dp0"

echo [All Three] Step 1/4: Shaders...
call build_shaders.bat
if %ERRORLEVEL% NEQ 0 (
  echo [All Three] Shaders FAILED - aborting.
  exit /b 1
)

echo [All Three] Step 2/4: Widgets stress harness...
call build_widgets_stress_harness.bat
if %ERRORLEVEL% NEQ 0 (
  echo [All Three] Widgets stress harness FAILED - aborting.
  exit /b 1
)

echo [All Three] Step 3/4: Widgets full test...
call build_widgets_full_test.bat
if %ERRORLEVEL% NEQ 0 (
  echo [All Three] Widgets full test FAILED - aborting.
  exit /b 1
)

echo [All Three] Step 4/4: Chat emoji demo...
echo n | call build_chat_emoji_demo.bat
if %ERRORLEVEL% NEQ 0 (
  echo [All Three] Chat emoji demo FAILED - aborting.
  exit /b 1
)

echo [All Three] All four builds SUCCESS.
exit /b 0
