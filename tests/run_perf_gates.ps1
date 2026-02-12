param(
  [ValidateSet("gl", "vk", "both")]
  [string]$Backend = "both",
  [ValidateSet("aggressive", "baseline")]
  [string]$Profile = "aggressive",
  [ValidateSet("auto", "igpu", "dgpu")]
  [string]$DeviceClass = "auto",
  [int]$Seconds = 6,
  [switch]$Rebuild,
  [switch]$FailOnWarn
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$scenarios = @("idle", "overlay", "sparse", "clip", "scroll", "text")

$buildProfiles = @{
  "gl" = @{
    BuildScript = "build_perf_pathological_suite.bat"
    ExePath     = "build\perf_pathological_suite.exe"
  }
  "vk" = @{
    BuildScript = "build_perf_pathological_suite_vk.bat"
    ExePath     = "build\perf_pathological_suite_vk.exe"
  }
}

$thresholds = @{
  "baseline" = @{
    "igpu" = @{
      "idle"    = @{ max_render = 6;  max_upload = 25000  }
      "overlay" = @{ min_render = 18; max_render = 50; max_build = 8;  max_upload = 30000  }
      "sparse"  = @{ min_render = 18; max_render = 50; max_build = 14; max_upload = 2500000 }
      "clip"    = @{ min_render = 18; max_render = 50; max_build = 14; max_upload = 50000  }
      "scroll"  = @{ min_render = 14; max_render = 50; max_build = 24; max_upload = 1500000 }
      "text"    = @{ min_render = 14; max_render = 50; max_build = 24; max_upload = 120000  }
    }
    "dgpu" = @{
      "idle"    = @{ max_render = 8;  max_upload = 25000  }
      "overlay" = @{ min_render = 18; max_render = 60; max_build = 8;  max_upload = 30000  }
      "sparse"  = @{ min_render = 18; max_render = 60; max_build = 14; max_upload = 2500000 }
      "clip"    = @{ min_render = 18; max_render = 60; max_build = 12; max_upload = 50000  }
      "scroll"  = @{ min_render = 14; max_render = 60; max_build = 22; max_upload = 1500000 }
      "text"    = @{ min_render = 14; max_render = 60; max_build = 20; max_upload = 120000  }
    }
  }
  "aggressive" = @{
    "igpu" = @{
      "idle"    = @{ max_render = 3;  max_upload = 20000  }
      "overlay" = @{ min_render = 26; max_render = 33; max_build = 2.2; max_upload = 22000  }
      "sparse"  = @{ min_render = 26; max_render = 33; max_build = 6.5; max_upload = 2200000 }
      "clip"    = @{ min_render = 26; max_render = 33; max_build = 1.2; max_upload = 20000  }
      "scroll"  = @{ min_render = 22; max_render = 33; max_build = 12.0; max_upload = 1400000 }
      "text"    = @{ min_render = 22; max_render = 33; max_build = 2.5; max_upload = 70000  }
    }
    "dgpu" = @{
      "idle"    = @{ max_render = 3;  max_upload = 20000  }
      "overlay" = @{ min_render = 26; max_render = 36; max_build = 1.5; max_upload = 22000  }
      "sparse"  = @{ min_render = 26; max_render = 36; max_build = 5.0; max_upload = 2200000 }
      "clip"    = @{ min_render = 26; max_render = 36; max_build = 0.8; max_upload = 20000  }
      "scroll"  = @{ min_render = 22; max_render = 36; max_build = 10.0; max_upload = 1400000 }
      "text"    = @{ min_render = 22; max_render = 36; max_build = 1.8; max_upload = 70000  }
    }
  }
}

function Invoke-BuildIfNeeded {
  param(
    [string]$Name,
    [hashtable]$ProfileDef,
    [bool]$ForceRebuild
  )
  $exe = Join-Path $root $ProfileDef.ExePath
  if ($ForceRebuild -or -not (Test-Path $exe)) {
    Write-Host "[gate] building $Name via $($ProfileDef.BuildScript)"
    Push-Location $root
    try {
      & cmd /c $ProfileDef.BuildScript
      if ($LASTEXITCODE -ne 0) {
        throw "build failed for $Name"
      }
    } finally {
      Pop-Location
    }
  }
}

function Parse-PerfLines {
  param([string[]]$Lines)
  $rows = @()
  foreach ($line in $Lines) {
    if (-not $line.StartsWith("PERFCASE ")) {
      continue
    }
    $row = @{}
    $matches = [regex]::Matches($line, "(\w+)=([^\s]+)")
    foreach ($m in $matches) {
      $row[$m.Groups[1].Value] = $m.Groups[2].Value
    }
    if ($row.Count -gt 0) {
      $rows += [pscustomobject]@{
        scenario      = $row["scenario"]
        backend       = $row["backend"]
        second        = [int]$row["second"]
        render        = [int]$row["render"]
        eval          = [int]$row["eval"]
        gpu_ms        = [double]$row["gpu_ms"]
        build_ms      = [double]$row["build_ms"]
        submit_ms     = [double]$row["submit_ms"]
        present_ms    = [double]$row["present_ms"]
        upload_bytes  = [double]$row["upload_bytes"]
        upload_ranges = [double]$row["upload_ranges"]
        cmd_applied   = [int]$row["cmd_applied"]
        cmd_drops     = [int]$row["cmd_drops"]
      }
    }
  }
  return $rows
}

function Get-Median {
  param([double[]]$Values)
  if (-not $Values -or $Values.Count -eq 0) {
    return 0.0
  }
  $sorted = @($Values | Sort-Object)
  $mid = [int]($sorted.Count / 2)
  if (($sorted.Count % 2) -eq 0) {
    return (($sorted[$mid - 1] + $sorted[$mid]) / 2.0)
  }
  return [double]$sorted[$mid]
}

function Detect-DeviceClass {
  param(
    [string[]]$Lines,
    [string]$BackendName,
    [string]$ForcedDevice
  )
  if ($ForcedDevice -ne "auto") {
    return $ForcedDevice
  }
  foreach ($line in $Lines) {
    if ($line -match "Renderer:\s*(.+)$") {
      $gpu = $Matches[1]
      if ($gpu -match "Intel|Iris|HD Graphics") { return "igpu" }
      if ($gpu -match "NVIDIA|GeForce|RTX|GTX|Radeon RX") { return "dgpu" }
    }
    if ($line -match "Selected GPU:\s*(.+)$") {
      $gpu = $Matches[1]
      if ($gpu -match "Intel|Iris|HD Graphics") { return "igpu" }
      if ($gpu -match "NVIDIA|GeForce|RTX|GTX|Radeon RX") { return "dgpu" }
    }
  }
  if ($BackendName -eq "vk") { return "dgpu" }
  return "igpu"
}

function Get-ScenarioSummary {
  param(
    [string]$Name,
    [string]$BackendName,
    [string[]]$OutputLines
  )
  $rows = @(Parse-PerfLines -Lines $OutputLines | Where-Object { $_.scenario -eq $Name -and $_.backend -eq $BackendName })
  if ($rows.Count -eq 0) {
    return $null
  }

  $stableRows = @($rows | Where-Object { $_.second -ge 2 })
  if ($stableRows.Count -eq 0) {
    $stableRows = $rows
  }

  return [pscustomobject]@{
    scenario       = $Name
    backend        = $BackendName
    samples        = $stableRows.Count
    avg_render     = ($stableRows | Measure-Object -Property render -Average).Average
    median_render  = Get-Median -Values @($stableRows | ForEach-Object { [double]$_.render })
    avg_eval       = ($stableRows | Measure-Object -Property eval -Average).Average
    avg_gpu_ms     = ($stableRows | Measure-Object -Property gpu_ms -Average).Average
    avg_build_ms   = ($stableRows | Measure-Object -Property build_ms -Average).Average
    avg_submit_ms  = ($stableRows | Measure-Object -Property submit_ms -Average).Average
    avg_present_ms = ($stableRows | Measure-Object -Property present_ms -Average).Average
    avg_upload_b   = ($stableRows | Measure-Object -Property upload_bytes -Average).Average
    max_cmd_drops  = ($stableRows | Measure-Object -Property cmd_drops -Maximum).Maximum
  }
}

function Evaluate-Summary {
  param(
    [pscustomobject]$Summary,
    [hashtable]$Rule
  )
  $fails = @()
  $warns = @()

  if ($Summary.max_cmd_drops -gt 0) {
    $fails += "cmd_drops=$($Summary.max_cmd_drops)"
  }

  $renderForGate = $Summary.avg_render
  if ($Summary.PSObject.Properties.Match("median_render").Count -gt 0) {
    $renderForGate = $Summary.median_render
  }

  if ($Rule.ContainsKey("min_render")) {
    if ($renderForGate -lt $Rule.min_render) {
      $fails += "gate_render=$([math]::Round($renderForGate,2)) < min_render=$($Rule.min_render)"
    } elseif ($renderForGate -lt ($Rule.min_render * 1.03)) {
      $warns += "gate_render near floor ($([math]::Round($renderForGate,2)))"
    }
  }

  if ($Rule.ContainsKey("max_render")) {
    if ($renderForGate -gt $Rule.max_render) {
      $fails += "gate_render=$([math]::Round($renderForGate,2)) > max_render=$($Rule.max_render)"
    } elseif ($renderForGate -gt ($Rule.max_render * 0.97)) {
      $warns += "gate_render near ceiling ($([math]::Round($renderForGate,2)))"
    }
  }

  if ($Rule.ContainsKey("max_build")) {
    if ($Summary.avg_build_ms -gt $Rule.max_build) {
      $fails += "avg_build_ms=$([math]::Round($Summary.avg_build_ms,2)) > max_build=$($Rule.max_build)"
    } elseif ($Summary.avg_build_ms -gt ($Rule.max_build * 0.90)) {
      $warns += "avg_build_ms near limit ($([math]::Round($Summary.avg_build_ms,2)))"
    }
  }

  if ($Rule.ContainsKey("max_upload")) {
    if ($Summary.avg_upload_b -gt $Rule.max_upload) {
      $fails += "avg_upload_b=$([math]::Round($Summary.avg_upload_b,0)) > max_upload=$($Rule.max_upload)"
    } elseif ($Summary.avg_upload_b -gt ($Rule.max_upload * 0.90)) {
      $warns += "avg_upload_b near limit ($([math]::Round($Summary.avg_upload_b,0)))"
    }
  }

  if ($Rule.ContainsKey("max_submit")) {
    if ($Summary.avg_submit_ms -gt $Rule.max_submit) {
      $fails += "avg_submit_ms=$([math]::Round($Summary.avg_submit_ms,2)) > max_submit=$($Rule.max_submit)"
    }
  }

  if ($Rule.ContainsKey("max_present")) {
    if ($Summary.avg_present_ms -gt $Rule.max_present) {
      $fails += "avg_present_ms=$([math]::Round($Summary.avg_present_ms,2)) > max_present=$($Rule.max_present)"
    }
  }

  $status = "GOOD"
  if ($fails.Count -gt 0) {
    $status = "BAD"
  } elseif ($warns.Count -gt 0) {
    $status = "WARN"
  }

  $score = 100 - ($fails.Count * 25) - ($warns.Count * 10)
  if ($score -lt 0) { $score = 0 }

  return [pscustomobject]@{
    status   = $status
    score    = $score
    failures = $fails
    warnings = $warns
  }
}

$backends = if ($Backend -eq "both") { @("gl", "vk") } else { @($Backend) }
$hasBad = $false
$hasWarn = $false

foreach ($backendName in $backends) {
  $buildDef = $buildProfiles[$backendName]
  Invoke-BuildIfNeeded -Name $backendName -ProfileDef $buildDef -ForceRebuild $Rebuild.IsPresent
  $exe = Join-Path $root $buildDef.ExePath
  $backendDevice = $null
  $goodCount = 0
  $warnCount = 0
  $badCount = 0

  foreach ($scenario in $scenarios) {
    Write-Host "[gate] run backend=$backendName scenario=$scenario seconds=$Seconds profile=$Profile"
    Push-Location $root
    try {
      $output = & $exe --scenario $scenario --seconds $Seconds --no-perf 2>&1
    } finally {
      Pop-Location
    }

    if (-not $backendDevice) {
      $backendDevice = Detect-DeviceClass -Lines $output -BackendName $backendName -ForcedDevice $DeviceClass
      Write-Host "[gate] detected device backend=$backendName => $backendDevice"
    }

    $ruleSet = $thresholds[$Profile][$backendDevice]
    if (-not $ruleSet) {
      Write-Host "PERF_HEALTH backend=$backendName device=$backendDevice profile=$Profile scenario=$scenario verdict=BAD score=0 issues=""missing-threshold-set"""
      $badCount++
      $hasBad = $true
      continue
    }

    $summary = Get-ScenarioSummary -Name $scenario -BackendName $backendName -OutputLines $output
    if ($null -eq $summary) {
      Write-Host "PERF_HEALTH backend=$backendName device=$backendDevice profile=$Profile scenario=$scenario verdict=BAD score=0 issues=""missing-PERFCASE"""
      $badCount++
      $hasBad = $true
      continue
    }

    $evaluation = Evaluate-Summary -Summary $summary -Rule $ruleSet[$scenario]
    $issues = @()
    if ($evaluation.failures.Count -gt 0) { $issues += $evaluation.failures }
    if ($evaluation.warnings.Count -gt 0) { $issues += $evaluation.warnings }
    if ($issues.Count -eq 0) { $issues = @("none") }

    Write-Host ("PERF_HEALTH backend={0} device={1} profile={2} scenario={3} verdict={4} score={5} render={6:N2} eval={7:N2} build_ms={8:N3} gpu_ms={9:N3} upload_b={10:N0} issues=""{11}""" -f `
      $backendName, $backendDevice, $Profile, $scenario, $evaluation.status, $evaluation.score,
      $summary.avg_render, $summary.avg_eval, $summary.avg_build_ms, $summary.avg_gpu_ms,
      $summary.avg_upload_b, ($issues -join "; "))

    if ($evaluation.status -eq "BAD") {
      $badCount++
      $hasBad = $true
    } elseif ($evaluation.status -eq "WARN") {
      $warnCount++
      $hasWarn = $true
    } else {
      $goodCount++
    }
  }

  Write-Host ("PERF_HEALTH_SUMMARY backend={0} device={1} profile={2} good={3} warn={4} bad={5}" -f `
    $backendName, $backendDevice, $Profile, $goodCount, $warnCount, $badCount)
}

if ($hasBad) {
  Write-Host "[gate] FAIL: performance health has BAD scenarios."
  exit 1
}
if ($FailOnWarn -and $hasWarn) {
  Write-Host "[gate] FAIL: warnings present with -FailOnWarn enabled."
  exit 1
}

Write-Host "[gate] PASS: performance health is acceptable."
exit 0
