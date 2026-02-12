@echo off
setlocal
cd /d "%~dp0"

call build_text_editor_mini.bat || exit /b 1
call build_calculator_mini.bat || exit /b 1
call build_calendar_mini.bat || exit /b 1
call build_perf_pathological_suite.bat || exit /b 1

call build_text_editor_mini_vk.bat || exit /b 1
call build_calculator_mini_vk.bat || exit /b 1
call build_calendar_mini_vk.bat || exit /b 1
call build_perf_pathological_suite_vk.bat || exit /b 1

echo [mini_apps_all] Build SUCCESS (GL + VK parity)
exit /b 0
