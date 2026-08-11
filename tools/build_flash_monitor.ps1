[CmdletBinding()]
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [string]$CliPath = "arduino-cli",
    [string[]]$TestCommands = @("help", "version", "device-info", "uptime", "heap", "wifi-status", "exit", "quit"),
    [int]$TestTimeoutSeconds = 2,
    [switch]$Clean,
    [switch]$SkipUpload,
    [switch]$SkipTest,
    [switch]$OpenMonitor,
    [switch]$SkipMonitor
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$sketch = Join-Path $repo "firmware\arduino\esp32shell"
$build = Join-Path $repo ".build\arduino"
$partitions = Join-Path $sketch "partitions.csv"

$cli = Get-Command $CliPath -ErrorAction SilentlyContinue
if (-not $cli) {
    throw "Arduino CLI was not found. Install it and ensure '$CliPath' is on PATH."
}
if (-not (Test-Path -LiteralPath $partitions)) {
    throw "Partition table not found: $partitions"
}

if ($Clean -and (Test-Path -LiteralPath $build)) {
    Remove-Item -LiteralPath $build -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $build | Out-Null

& $cli.Source compile `
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
    & $cli.Source upload `
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
    & $cli.Source monitor --port $Port --config "baudrate=$BaudRate"
    if ($LASTEXITCODE -ne 0) { throw "Arduino CLI monitor exited with code $LASTEXITCODE." }
}
