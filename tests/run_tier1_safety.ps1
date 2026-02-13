param(
  [switch]$Rebuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if ($Rebuild -or -not (Test-Path "build\tier1_safety.exe")) {
  powershell -NoProfile -ExecutionPolicy Bypass -File compile\run.ps1 -Target tier1_safety
  if ($LASTEXITCODE -ne 0) {
    throw "tier1_safety build failed"
  }
}

& "build\tier1_safety.exe"
exit $LASTEXITCODE
