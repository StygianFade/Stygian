param(
  [switch]$Rebuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if ($Rebuild -or -not (Test-Path "build\tier2_runtime.exe")) {
  powershell -NoProfile -ExecutionPolicy Bypass -File compile\run.ps1 -Target tier2_runtime
  if ($LASTEXITCODE -ne 0) {
    throw "tier2_runtime build failed"
  }
}

& "build\tier2_runtime.exe"
exit $LASTEXITCODE
