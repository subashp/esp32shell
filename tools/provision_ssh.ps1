[CmdletBinding()]
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [Parameter(Mandatory = $true)][string]$WifiSsid,
    [Parameter(Mandatory = $true)][string]$WifiPassword,
    [Parameter(Mandatory = $true)][string]$SshUsername,
    [Parameter(Mandatory = $true)][string]$SshPassword,
    [string]$OpenSslPath = "openssl"
)

$ErrorActionPreference = "Stop"
if ($SshPassword.Length -lt 12 -or $SshPassword -notmatch '[A-Z]' -or
    $SshPassword -notmatch '[a-z]' -or $SshPassword -notmatch '\d') {
    throw "SSH password must be at least 12 characters and contain upper, lower, and digit characters."
}
if ($SshPassword -eq 'ESP32S3-WROOM-2') {
    throw "Do not use the board model as the SSH password. Provide a separate strong password."
}

$openssl = Get-Command $OpenSslPath -ErrorAction SilentlyContinue
if (-not $openssl) { throw "OpenSSL was not found. Install it or pass -OpenSslPath." }
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("esp32shell-ssh-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tempRoot | Out-Null
$pemPath = Join-Path $tempRoot "host-key.pem"
$derPath = Join-Path $tempRoot "host-key.der"

try {
    # OpenSSL 3 prints normal status text such as 'writing RSA key' on stderr.
    # Merge and discard that informational stream; preserve the exit-code check.
    & $openssl.Source genrsa -traditional -out $pemPath 2048 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "OpenSSL RSA key generation failed." }
    & $openssl.Source rsa -in $pemPath -outform DER -out $derPath 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "OpenSSL DER conversion failed." }
    $hostKeyHex = ([BitConverter]::ToString([IO.File]::ReadAllBytes($derPath))).Replace('-', '')

    $serial = New-Object System.IO.Ports.SerialPort($Port, $BaudRate,
        [IO.Ports.Parity]::None, 8, [IO.Ports.StopBits]::One)
    $serial.DtrEnable = $false
    $serial.RtsEnable = $false
    $serial.ReadTimeout = 100
    $serial.WriteTimeout = 5000
    try {
        $serial.Open()
        Start-Sleep -Milliseconds 500
        $serial.DiscardInBuffer()
        $serial.DiscardOutBuffer()

        function Send-Command([string]$Command, [string]$Expected) {
            $serial.WriteLine($Command)
            $text = New-Object Text.StringBuilder
            $deadline = [DateTime]::UtcNow.AddSeconds(8)
            while ([DateTime]::UtcNow -lt $deadline) {
                if ($serial.BytesToRead -gt 0) {
                    [void]$text.Append($serial.ReadExisting())
                    if ($text.ToString().Contains($Expected)) { return }
                }
                Start-Sleep -Milliseconds 50
            }
            throw "Device did not confirm '$($Command.Split(' ')[0])'. Output: $($text.ToString())"
        }

        Send-Command "wifi-config $WifiSsid $WifiPassword" "wifi configuration accepted"
        Send-Command "config-set ssh_username $SshUsername" "configuration updated"
        Send-Command "config-set ssh_password $SshPassword" "configuration updated"
        Send-Command "config-set ssh_host_key $hostKeyHex" "configuration updated"
        Write-Host "Provisioning completed on $Port. The DER host key remains in protected NVS; no key file was kept."
    }
    finally {
        if ($serial.IsOpen) { $serial.Close() }
        $serial.Dispose()
    }
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
