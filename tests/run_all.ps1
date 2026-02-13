param(
  [switch]$Rebuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$runners = @(
  "tests/run_tier1_safety.ps1",
  "tests/run_tier2_runtime.ps1",
  "tests/run_tier3_misuse.ps1"
)

foreach ($runner in $runners) {
  Write-Host "[run_all] running $runner"
  if ($Rebuild) {
    powershell -NoProfile -ExecutionPolicy Bypass -File $runner -Rebuild
  } else {
    powershell -NoProfile -ExecutionPolicy Bypass -File $runner
  }
  if ($LASTEXITCODE -ne 0) {
    throw "$runner failed with code $LASTEXITCODE"
  }
}

Write-Host "[run_all] all tiers passed"
exit 0
