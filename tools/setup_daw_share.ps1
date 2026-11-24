# Run this on the DAW laptop. It prepares the SMB shares that tools/deploy_to_daw.ps1 copies into.
# Right-click, Run with PowerShell, or:  pwsh -File .\tools\setup_daw_share.ps1
param(
    [string]$Account = "",
    [string]$PresetsPath = "",
    [string]$ComputerName = "DAW-LAPTOP",
    [switch]$Reboot
)

$ErrorActionPreference = "Stop"

function Test-IsAdmin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if ([string]::IsNullOrWhiteSpace($Account)) {
    $Account = "$env:USERDOMAIN\$env:USERNAME"
}
if ([string]::IsNullOrWhiteSpace($PresetsPath)) {
    $PresetsPath = Join-Path $env:APPDATA "Doomgazer\Presets"
}

if (-not (Test-IsAdmin)) {
    $hostExe = (Get-Command pwsh -ErrorAction SilentlyContinue).Source
    if (-not $hostExe) {
        $hostExe = (Get-Command powershell -ErrorAction SilentlyContinue).Source
    }
    $argList = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $PSCommandPath,
        "-Account", $Account,
        "-PresetsPath", $PresetsPath,
        "-ComputerName", $ComputerName
    )
    if ($Reboot) {
        $argList += "-Reboot"
    }
    Write-Host "Requesting administrator approval..." -ForegroundColor Cyan
    $proc = Start-Process -FilePath $hostExe -Verb RunAs -ArgumentList $argList -Wait -PassThru
    exit $proc.ExitCode
}

function Ensure-Folder([string]$Path) {
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Grant-Modify([string]$Path, [string]$Who) {
    & icacls $Path /grant "${Who}:(OI)(CI)M" /T /C | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "icacls failed for $Path ($Who), exit $LASTEXITCODE"
    }
}

function Ensure-Share([string]$Name, [string]$Path, [string]$Who) {
    $existing = Get-SmbShare -Name $Name -ErrorAction SilentlyContinue
    if ($null -ne $existing -and $existing.Path -ne $Path) {
        Remove-SmbShare -Name $Name -Force
        $existing = $null
    }
    if ($null -eq $existing) {
        New-SmbShare -Name $Name -Path $Path -ChangeAccess $Who -FullAccess "Administrators" | Out-Null
    } else {
        Grant-SmbShareAccess -Name $Name -AccountName $Who -AccessRight Change -Force | Out-Null
    }
}

Write-Host ">>> Doomgazer DAW share setup" -ForegroundColor Cyan
Write-Host "    Account: $Account"

$vst3 = "C:\Program Files\Common Files\VST3"
$clap = "C:\Program Files\Common Files\CLAP"
$standalone = "C:\Program Files\Doomgazer"

Ensure-Folder $vst3
Ensure-Folder $clap
Ensure-Folder $PresetsPath
Ensure-Folder $standalone

foreach ($folder in @($vst3, $clap, $PresetsPath, $standalone)) {
    Grant-Modify $folder $Account
    Grant-Modify $folder "Administrators"
}

Ensure-Share "VST3" $vst3 $Account
Ensure-Share "CLAP" $clap $Account
Ensure-Share "Presets" $PresetsPath $Account
Ensure-Share "Standalone" $standalone $Account
Write-Host "  -> Shares VST3, CLAP, Presets, Standalone are ready." -ForegroundColor Green

Get-NetConnectionProfile | ForEach-Object {
    if ($_.NetworkCategory -eq "Public") {
        Set-NetConnectionProfile -InterfaceIndex $_.InterfaceIndex -NetworkCategory Private
        Write-Host "  -> $($_.Name) set to Private." -ForegroundColor Green
    }
}

foreach ($group in @("File and Printer Sharing", "Network Discovery")) {
    $rules = Get-NetFirewallRule -DisplayGroup $group -ErrorAction SilentlyContinue
    if ($rules) {
        $rules | Set-NetFirewallRule -Profile Private -Enabled True -ErrorAction SilentlyContinue
        Write-Host "  -> Firewall group enabled: $group" -ForegroundColor Green
    } else {
        Write-Warning "Firewall group not found: $group"
    }
}

foreach ($serviceName in @("LanmanServer", "fdPHost", "FDResPub")) {
    $service = Get-Service -Name $serviceName -ErrorAction SilentlyContinue
    if ($null -eq $service) {
        continue
    }
    Set-Service -Name $serviceName -StartupType Automatic
    if ($service.Status -ne "Running") {
        Start-Service -Name $serviceName
    }
}

$lsa = "HKLM:\SYSTEM\CurrentControlSet\Control\Lsa"
$lanman = "HKLM:\SYSTEM\CurrentControlSet\Services\LanmanServer\Parameters"
New-ItemProperty -Path $lsa -Name "everyoneincludesanonymous" -PropertyType DWord -Value 0 -Force | Out-Null
New-ItemProperty -Path $lsa -Name "restrictanonymous" -PropertyType DWord -Value 1 -Force | Out-Null
New-ItemProperty -Path $lanman -Name "restrictnullsessaccess" -PropertyType DWord -Value 1 -Force | Out-Null
Write-Host "  -> Password-protected sharing is on." -ForegroundColor Green

$renamePending = $false
if ($env:COMPUTERNAME -ne $ComputerName) {
    Rename-Computer -NewName $ComputerName -Force
    $renamePending = $true
    Write-Host "  -> Computer will be named $ComputerName after reboot." -ForegroundColor Yellow
} else {
    Write-Host "  -> Computer name is already $ComputerName." -ForegroundColor Green
}

Write-Host ""
Write-Host "From the desktop, connect as ${Account} and run:" -ForegroundColor Cyan
Write-Host "  dir \\$ComputerName\VST3"
Write-Host "  .\tools\deploy_to_daw.ps1 -TargetHost $ComputerName"

if ($renamePending -and $Reboot) {
    Write-Host "Rebooting so the new computer name takes effect..." -ForegroundColor Yellow
    Restart-Computer -Force
} elseif ($renamePending) {
    Write-Host "Reboot the DAW laptop before deploying. Re-run with -Reboot to restart now." -ForegroundColor Yellow
}
