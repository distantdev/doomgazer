param(
    [string]$TargetHost = "DAW-LAPTOP",
    [string]$Config = "Release",
    [string]$BuildDir = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $repoRoot "build-msvc"
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $repoRoot $BuildDir
}
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)

$expectedGenerator = "Visual Studio 18 2026"

function Get-CMakeCacheValue([string]$CachePath, [string]$Key) {
    $pattern = "^" + [regex]::Escape($Key) + "=(.*)$"
    $match = Select-String -LiteralPath $CachePath -Pattern $pattern | Select-Object -First 1
    if ($null -eq $match) {
        return ""
    }
    return $match.Matches[0].Groups[1].Value.Trim()
}

if (-not $SkipBuild) {
    $cache = Join-Path $BuildDir "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cache)) {
        Write-Host ">>> Configuring $BuildDir with $expectedGenerator" -ForegroundColor Cyan
        & cmake -S $repoRoot -B $BuildDir -G $expectedGenerator -A x64 -DDOOMGAZER_BUILD_PLUGIN=ON
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    } else {
        $generator = Get-CMakeCacheValue $cache "CMAKE_GENERATOR:INTERNAL"
        if ($generator -ne $expectedGenerator) {
            throw "CMake generator is '$generator', expected '$expectedGenerator'. Refusing to reconfigure $BuildDir. Do not pass -G Ninja."
        }
        $plugin = Get-CMakeCacheValue $cache "DOOMGAZER_BUILD_PLUGIN:BOOL"
        if ($plugin -ne "ON") {
            throw "DOOMGAZER_BUILD_PLUGIN is '$plugin' in $BuildDir. Deploy from a plugin-on tree only."
        }
    }

    Write-Host ">>> Building $Config Doomgazer_VST3 Doomgazer_Standalone Doomgazer_CLAP" -ForegroundColor Cyan
    & cmake --build $BuildDir --config $Config --target Doomgazer_VST3 Doomgazer_Standalone Doomgazer_CLAP
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$artefacts = Join-Path $BuildDir "Doomgazer_artefacts\$Config"
& (Join-Path $PSScriptRoot "deploy_to_daw.ps1") -TargetHost $TargetHost -BuildDir $artefacts
if (-not $?) {
    exit 1
}
