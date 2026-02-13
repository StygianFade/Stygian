# Compile Runners

`targets.json` is the single source of truth for Stygian mini-app targets.

## Runners

- Unified dispatcher:
  - Windows: `compile/run.ps1`
  - Linux/macOS: `compile/run.sh`
- Windows: `compile/windows/build.ps1`
- Linux: `compile/linux/build.sh`
- macOS: `compile/macos/build.sh`

## First Build (Windows)

1. Build shader outputs:
   - `compile\windows\build_shaders.bat`
2. Build quick-start target (default wrapper):
   - `compile\windows\build.bat`
3. Run:
   - `build\quickwindow.exe`

Optional quick tests:

- Borderless: `compile\windows\build_quickwindow_borderless.bat`
- Custom titlebar: `compile\windows\build_quickwindow_custom_titlebar.bat`
- All quick smoke targets: `compile\windows\build_quick_smoke.bat`

## Backend Rules (Strict)

Backend setup must match across all layers:

1. Source backend (`StygianConfig.backend`)
2. Source window render flag (`STYGIAN_WINDOW_OPENGL` or `STYGIAN_WINDOW_VULKAN`)
3. Target backend in `compile/targets.json` (`"gl"` or `"vk"`)

For quickwindow:

- OpenGL: `powershell -File compile/run.ps1 -Target quickwindow`
- Vulkan: `powershell -File compile/run.ps1 -Target quickwindow_vk`

## Examples

- Quick-start target: `powershell -File compile/run.ps1 -Target quickwindow`
- Quick-start Vulkan target: `powershell -File compile/run.ps1 -Target quickwindow_vk`
- Quick smoke group: `powershell -File compile/run.ps1 -Group quick_smoke`
- Unified Windows target: `powershell -File compile/run.ps1 -Target text_editor_mini`
- Unified Windows group: `powershell -File compile/run.ps1 -Group mini_apps_all`
- Linux/macOS target: `compile/run.sh --target text_editor_mini`
- Linux/macOS group: `compile/run.sh --group mini_apps_all`
- Direct Windows runner target: `powershell -File compile/windows/build.ps1 -Target text_editor_mini`
- Direct Windows runner group: `powershell -File compile/windows/build.ps1 -Group mini_apps_all`
- Linux target: `compile/linux/build.sh text_editor_mini`
- macOS target: `compile/macos/build.sh text_editor_mini`

## Add Your Own Example

1. Create your file in `examples/` (for example, `examples/my_app.c`).
2. Add a target entry in `compile/targets.json` under `targets`:
   - `backend`: `gl` or `vk`
   - `entry_source`: example source path
   - `output_stem`: output exe name
3. Build with:
   - `powershell -File compile/run.ps1 -Target <your_target_name>`

## Notes

- Windows runner validates shader outputs unless `-NoShaderCheck` is passed.
- Linux/macOS runners require `jq` to parse `targets.json`.
- CI workflow builds the tier test targets on Windows hosted runners using
  `-NoShaderCheck`.
- Runtime tier execution (`tests/run_all.ps1`) is available as a manual
  workflow dispatch path (`run_runtime=true`) and runs best-effort.
