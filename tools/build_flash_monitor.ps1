[CmdletBinding()]
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [string]$CliPath = "arduino-cli",
    [string]$Esp32CoreVersion = "3.3.11",
    [string]$CliStateRoot = "",
    [int]$BootstrapTimeoutSeconds = 180,
    [string[]]$TestCommands = @("help", "version", "device-info", "uptime", "heap", "wifi-status", "exit", "quit"),
    [int]$TestTimeoutSeconds = 2,
    [switch]$Clean,
    [switch]$ResetCliState,
    [switch]$SkipUpload,
    [switch]$SkipTest,
    [switch]$OpenMonitor,
    [switch]$SkipMonitor
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$sketch = Join-Path $repo "firmware\arduino\esp32shell"
$build = Join-Path $repo ".build\arduino"
if ([string]::IsNullOrWhiteSpace($CliStateRoot)) {
    $CliStateRoot = Join-Path ([System.IO.Path]::GetTempPath()) "esp32shell-arduino-cli"
}
$cliState = $CliStateRoot
$cliConfig = Join-Path $cliState "arduino-cli.yaml"
$cliData = Join-Path $cliState "data"
$cliDownloads = Join-Path $cliState "downloads"
$cliUser = Join-Path $cliState "user"
$partitions = Join-Path $sketch "partitions.csv"

$cli = Get-Command $CliPath -ErrorAction SilentlyContinue
if (-not $cli -and $CliPath -eq "arduino-cli") {
    $installedCli = Join-Path ${env:ProgramFiles} "Arduino CLI\arduino-cli.exe"
    if (Test-Path -LiteralPath $installedCli) {
        $cli = Get-Command $installedCli
    }
}
if (-not $cli) {
    throw "Arduino CLI was not found. Install it and ensure '$CliPath' is on PATH."
}
if (-not (Test-Path -LiteralPath $partitions)) {
    throw "Partition table not found: $partitions"
}

if ($Clean -and (Test-Path -LiteralPath $build)) {
    Remove-Item -LiteralPath $build -Recurse -Force
}
if ($ResetCliState -and (Test-Path -LiteralPath $cliState)) {
    Remove-Item -LiteralPath $cliState -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $build | Out-Null
New-Item -ItemType Directory -Force -Path $cliData,$cliDownloads,$cliUser | Out-Null

function Convert-ToCliPath {
    param([string]$PathValue)
    return $PathValue.Replace('\', '/')
}

function Invoke-CliBootstrapCommand {
    param(
        [string[]]$Arguments,
        [int]$TimeoutSeconds
    )

    $stdoutPath = Join-Path $cliState "bootstrap.stdout.log"
    $stderrPath = Join-Path $cliState "bootstrap.stderr.log"
    $argumentText = ($Arguments | ForEach-Object {
        if ($_ -match '[\s"]') { '"' + $_.Replace('"', '\"') + '"' } else { $_ }
    }) -join ' '
    $process = Start-Process -FilePath $cli.Source -ArgumentList $argumentText -WorkingDirectory $repo `
        -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill()
        throw "Arduino CLI bootstrap command timed out after $TimeoutSeconds seconds. See $stdoutPath and $stderrPath."
    }
    $output = @()
    if (Test-Path -LiteralPath $stdoutPath) { $output += Get-Content -LiteralPath $stdoutPath }
    if (Test-Path -LiteralPath $stderrPath) { $output += Get-Content -LiteralPath $stderrPath }
    $output | ForEach-Object { Write-Host $_ }
    if ($process.ExitCode -ne 0) {
        throw "Arduino CLI bootstrap command failed with exit code $($process.ExitCode). See $stderrPath."
    }
    return $output
}

if (-not (Test-Path -LiteralPath $cliConfig)) {
    $configText = @"
directories:
  data: '$(Convert-ToCliPath $cliData)'
  downloads: '$(Convert-ToCliPath $cliDownloads)'
  user: '$(Convert-ToCliPath $cliUser)'
board_manager:
  additional_urls:
    - https://espressif.github.io/arduino-esp32/package_esp32_index.json
"@
    Set-Content -LiteralPath $cliConfig -Value $configText -Encoding utf8
}

Write-Host "Using Arduino CLI: $($cli.Source)"
Write-Host "Using writable CLI state: $cliState"
$cliGlobalArgs = @("--config-file", $cliConfig)
Write-Host "Checking Arduino ESP32 core $Esp32CoreVersion..."
$coreList = Invoke-CliBootstrapCommand -Arguments ($cliGlobalArgs + @("core", "list")) -TimeoutSeconds 30
$coreListText = $coreList -join "`n"
if (-not ($coreListText -match "esp32:esp32")) {
    Write-Host "Installing Arduino ESP32 core $Esp32CoreVersion into the writable CLI state..."
    [void](Invoke-CliBootstrapCommand -Arguments ($cliGlobalArgs + @("core", "update-index")) -TimeoutSeconds $BootstrapTimeoutSeconds)
    [void](Invoke-CliBootstrapCommand -Arguments ($cliGlobalArgs + @("core", "install", "esp32:esp32@$Esp32CoreVersion")) -TimeoutSeconds $BootstrapTimeoutSeconds)
}

& $cli.Source @cliGlobalArgs compile `
    --fqbn esp32:esp32:esp32s3 `
    --board-options "FlashSize=32M,PSRAM=opi" `
    --build-path $build `
    --warnings all `
    $sketch
if ($LASTEXITCODE -ne 0) { throw "Arduino CLI compile failed with exit code $LASTEXITCODE." }

$flashArgsPath = Join-Path $build "flash_args"
if (-not (Test-Path -LiteralPath $flashArgsPath)) {
    throw "Build verification failed: flash_args was not produced."
}
$flashArgs = Get-Content -LiteralPath $flashArgsPath -Raw
if ($flashArgs -notmatch "--flash-size\s+32MB") {
    throw "Build verification failed: flash_args does not specify --flash-size 32MB."
}
$partitionBinary = Get-ChildItem -LiteralPath $build -Filter "*.partitions.bin" -File | Select-Object -First 1
$firmwareBinary = Get-ChildItem -LiteralPath $build -Filter "*.ino.bin" -File | Select-Object -First 1
if (-not $partitionBinary -or -not $firmwareBinary) {
    throw "Build verification failed: firmware or partition binary is missing."
}

Write-Host "Build verified: $($firmwareBinary.Name), $($partitionBinary.Name), flash size 32MB"

function Invoke-UartTerminalTest {
    param(
        [string]$SerialPortName,
        [int]$SerialBaudRate,
        [string[]]$Commands,
        [int]$TimeoutSeconds
    )

    $serial = New-Object System.IO.Ports.SerialPort(
        $SerialPortName,
        $SerialBaudRate,
        [System.IO.Ports.Parity]::None,
        8,
        [System.IO.Ports.StopBits]::One
    )
    $serial.DtrEnable = $false
    $serial.RtsEnable = $false
    $serial.ReadTimeout = 100
    $serial.WriteTimeout = 1000

    try {
        $serial.Open()
        Start-Sleep -Milliseconds 500
        $serial.DiscardInBuffer()
        $serial.DiscardOutBuffer()

        foreach ($command in $Commands) {
            $serial.WriteLine($command)
            $expected = switch ($command) {
                "help" { "Commands:"; break }
                "version" { "esp32shell"; break }
                "device-info" { "chip="; break }
                "uptime" { "ms"; break }
                "heap" { "free_heap="; break }
                "wifi-status" { "wifi="; break }
                "exit" { "serial monitor remains active"; break }
                "quit" { "serial monitor remains active"; break }
                default { throw "No expected output is defined for UART test command '$command'." }
            }

            $output = New-Object System.Text.StringBuilder
            $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
            while ([DateTime]::UtcNow -lt $deadline) {
                if ($serial.BytesToRead -gt 0) {
                    [void]$output.Append($serial.ReadExisting())
                    if ($output.ToString().Contains($expected)) { break }
                }
                Start-Sleep -Milliseconds 50
            }
            $text = $output.ToString()
            Write-Host ("UART test [{0}]: {1}" -f $command, ($text.Trim() -replace "\r?\n", " | "))
            if (-not $text.Contains($expected)) {
                throw "UART test failed for '$command'; expected output containing '$expected'."
            }
        }
        Write-Host "UART tests passed; COM port will now be disconnected."
    }
    finally {
        if ($serial.IsOpen) { $serial.Close() }
        $serial.Dispose()
    }
}

if (-not $SkipUpload) {
    & $cli.Source @cliGlobalArgs upload `
        --fqbn esp32:esp32:esp32s3 `
        --port $Port `
        --input-dir $build
    if ($LASTEXITCODE -ne 0) { throw "Arduino CLI upload failed with exit code $LASTEXITCODE." }
}

if (-not $SkipTest) {
    Invoke-UartTerminalTest `
        -SerialPortName $Port `
        -SerialBaudRate $BaudRate `
        -Commands $TestCommands `
        -TimeoutSeconds $TestTimeoutSeconds
}

if ($OpenMonitor -and -not $SkipMonitor) {
    Write-Host "Opening UART shell on $Port at $BaudRate baud. Press Ctrl-C to exit the monitor."
    & $cli.Source @cliGlobalArgs monitor --port $Port --config "baudrate=$BaudRate"
    if ($LASTEXITCODE -ne 0) { throw "Arduino CLI monitor exited with code $LASTEXITCODE." }
}
