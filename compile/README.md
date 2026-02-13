# Compile Runners

`targets.json` is the single source of truth for Stygian mini-app targets.

## Runners

- Unified dispatcher:
  - Windows: `compile/run.ps1`
  - Linux/macOS: `compile/run.sh`
- Windows: `compile/windows/build.ps1`
- Linux: `compile/linux/build.sh`
- macOS: `compile/macos/build.sh`

## Examples

- Windows target: `powershell -File compile/run.ps1 -Target text_editor_mini`
- Windows group: `powershell -File compile/run.ps1 -Group mini_apps_all`
- Linux/macOS target: `compile/run.sh --target text_editor_mini`
- Linux/macOS group: `compile/run.sh --group mini_apps_all`
- Windows target: `powershell -File compile/windows/build.ps1 -Target text_editor_mini`
- Windows group: `powershell -File compile/windows/build.ps1 -Group mini_apps_all`
- Linux target: `compile/linux/build.sh text_editor_mini`
- macOS target: `compile/macos/build.sh text_editor_mini`

## Notes

- Windows runner validates shader outputs unless `-NoShaderCheck` is passed.
- Linux/macOS runners require `jq` to parse `targets.json`.
