[CmdletBinding()]
param(
    [string]$Port = "COM4",
    [string]$ConfigPath = "",
    [string]$SshUsername = "root",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
    $ConfigPath = Join-Path $repo "config\esp32shell.local.psd1"
}
if (-not (Test-Path -LiteralPath $ConfigPath)) {
    throw "Create an ignored local config first: Copy-Item .\config\esp32shell.local.psd1.example .\config\esp32shell.local.psd1"
}
$config = Import-PowerShellDataFile -LiteralPath $ConfigPath
$profiles = @($config.WifiProfiles)
if ($profiles.Count -eq 0 -or [string]::IsNullOrWhiteSpace([string]$profiles[0].Ssid)) {
    throw "ConfigPath must contain at least one WifiProfiles entry with Ssid and Password."
}

Write-Host "Stage 1/3: flashing the Arduino bootstrap image and provisioning Wi-Fi over $Port..."
$buildScript = Join-Path $repo "tools\build_flash_monitor.ps1"
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $buildScript `
    -Port $Port -ConfigPath $ConfigPath -Clean -SkipTest -SkipMonitor
if ($LASTEXITCODE -ne 0) { throw "Arduino bootstrap failed with exit code $LASTEXITCODE." }

Write-Host "Stage 2/3: provisioning SSH credentials over the USB shell..."
$provisionScript = Join-Path $repo "tools\provision_ssh.ps1"
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $provisionScript `
    -Port $Port -WifiSsid ([string]$profiles[0].Ssid) -SshUsername $SshUsername
if ($LASTEXITCODE -ne 0) { throw "SSH provisioning failed with exit code $LASTEXITCODE." }

Write-Host "Stage 3/3: building and flashing the ESP-IDF image; NVS credentials are preserved."
$buildIdf = Join-Path $repo "tools\build_espidf_ssh.cmd"
$flashIdf = Join-Path $repo "tools\flash_espidf_ssh.cmd"
if ($Clean) { & cmd.exe /c $buildIdf --clean } else { & cmd.exe /c $buildIdf }
if ($LASTEXITCODE -ne 0) { throw "ESP-IDF build failed with exit code $LASTEXITCODE." }
& cmd.exe /c $flashIdf $Port
if ($LASTEXITCODE -ne 0) { throw "ESP-IDF flash failed with exit code $LASTEXITCODE." }

Write-Host "Onboarding complete. The ESP-IDF image now connects using the provisioned Wi-Fi profile and starts SSH when an IP is acquired."
