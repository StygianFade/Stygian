param(
  [string]$Target,
  [string]$Group,
  [string]$Clang = "",
  [string]$VulkanSdk = "",
  [switch]$NoShaderCheck
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$runner = Join-Path $PSScriptRoot "windows/build.ps1"
if (-not (Test-Path $runner)) {
  throw "missing runner: $runner"
}

$args = @()
if ($Target) {
  $args += "-Target"
  $args += $Target
}
if ($Group) {
  $args += "-Group"
  $args += $Group
}
if ($Clang) {
  $args += "-Clang"
  $args += $Clang
}
if ($VulkanSdk) {
  $args += "-VulkanSdk"
  $args += $VulkanSdk
}
if ($NoShaderCheck) {
  $args += "-NoShaderCheck"
}

if (-not $Target -and -not $Group) {
  throw "specify -Target <name> or -Group <name>"
}

powershell -NoProfile -ExecutionPolicy Bypass -File $runner @args
exit $LASTEXITCODE
