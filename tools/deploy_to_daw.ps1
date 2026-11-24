param(
    [string]$TargetHost = "DAW-LAPTOP",
    [string]$BuildDir = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $repoRoot "build-msvc\Doomgazer_artefacts\Release"
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $repoRoot $BuildDir
}
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)

function Get-CopiedBytes([string]$Path) {
    $item = Get-Item -LiteralPath $Path
    if (-not $item.PSIsContainer) {
        return [int64]$item.Length
    }
    $sum = Get-ChildItem -LiteralPath $Path -Recurse -File | Measure-Object -Property Length -Sum
    if ($null -eq $sum.Sum) {
        return [int64]0
    }
    return [int64]$sum.Sum
}

function Test-FileInUse([System.Management.Automation.ErrorRecord]$ErrorRecord) {
    $message = $ErrorRecord.Exception.Message
    if ($message -match "used by another process") {
        return $true
    }
    $current = $ErrorRecord.Exception
    while ($null -ne $current) {
        if ($current.HResult -eq -2147024864) {
            return $true
        }
        $current = $current.InnerException
    }
    return $false
}

function Copy-Required([string]$Label, [string]$Source, [string]$Share) {
    $problems = @()
    if (-not (Test-Path -LiteralPath $Share)) {
        $problems += "$Label share is missing: $Share"
    }
    if (-not (Test-Path -LiteralPath $Source)) {
        $problems += "$Label artefact is missing: $Source"
    }
    if ($problems.Count -gt 0) {
        throw ($problems -join [Environment]::NewLine)
    }

    $leaf = Split-Path -Leaf $Source
    $destination = Join-Path $Share $leaf
    try {
        Copy-Item -LiteralPath $Source -Destination $Share -Recurse -Force
    } catch {
        if (Test-FileInUse $_) {
            throw @"
$Label is in use: $Source
Unload Doomgazer in Bitwig and REAPER, quit the laptop standalone, and rerun.
$($_.Exception.Message)
"@
        }
        throw "$Label copy failed: $Source -> $Share. $($_.Exception.Message)"
    }

    $copied = Get-Item -LiteralPath $destination
    $bytes = Get-CopiedBytes $destination
    $when = $copied.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss")
    Write-Host ("  -> {0}  {1} bytes  {2}  {3}" -f $Label, $bytes, $when, $destination) -ForegroundColor Green
}

Write-Host ">>> Deploying DOOMGAZER to $TargetHost from $BuildDir" -ForegroundColor Cyan

if (-not (Test-Path -LiteralPath $BuildDir)) {
    throw "Build directory not found: $BuildDir"
}

$vst3 = Join-Path $BuildDir "VST3\Doomgazer.vst3"
$clap = Join-Path $BuildDir "CLAP\Doomgazer.clap"
$standalone = Join-Path $BuildDir "Standalone\Doomgazer.exe"

Copy-Required "VST3" $vst3 "\\$TargetHost\VST3"
Copy-Required "CLAP" $clap "\\$TargetHost\CLAP"
Copy-Required "Standalone" $standalone "\\$TargetHost\Standalone"

$presets = Join-Path $repoRoot "presets"
if (-not (Test-Path -LiteralPath $presets)) {
    Write-Host "  -> No repo presets/ folder. Factory presets are compiled in."
} else {
    $presetShare = "\\$TargetHost\Presets"
    if (-not (Test-Path -LiteralPath $presetShare)) {
        throw "Presets share is missing: $presetShare"
    }
    try {
        Copy-Item -Path (Join-Path $presets "*") -Destination $presetShare -Recurse -Force
    } catch {
        if (Test-FileInUse $_) {
            throw @"
Presets are in use: $presets
Unload Doomgazer in Bitwig and REAPER, quit the laptop standalone, and rerun.
$($_.Exception.Message)
"@
        }
        throw "Presets copy failed: $presets -> $presetShare. $($_.Exception.Message)"
    }
    Write-Host "  -> Presets synced to $presetShare" -ForegroundColor Green
}

Write-Host "Local standalone: $standalone"
Write-Host "Laptop standalone: C:\Program Files\Doomgazer\Doomgazer.exe"
Write-Host ">>> Deployment pass finished. Rescan plugins in Bitwig and REAPER." -ForegroundColor Green
