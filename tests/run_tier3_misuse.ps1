param(
  [switch]$Rebuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if ($Rebuild -or -not (Test-Path "build\tier3_misuse.exe")) {
  powershell -NoProfile -ExecutionPolicy Bypass -File compile\run.ps1 -Target tier3_misuse
  if ($LASTEXITCODE -ne 0) {
    throw "tier3_misuse build failed"
  }
}

& "build\tier3_misuse.exe"
exit $LASTEXITCODE
